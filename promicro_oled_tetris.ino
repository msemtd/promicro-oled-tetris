#define MSEPRODUCT "ProMicro OLED Tetris"
#define MSEVERSION "v1.0"
#ifdef __AVR__
  #include <avr/power.h>
#endif

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define OLED_RESET 4
Adafruit_SSD1306 display(OLED_RESET);

#if (SSD1306_LCDHEIGHT != 32)
#error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif

const int RXLED = 17;

// When using a console line reading mode, append non-line-ending char to input 
// string (if not already overflowed).
// Both CR and LF can terminate the line and ESC (0x1b) can clear the line.
#define DIAG_INPUT_MAX 128
String consoleInput = "";
bool consoleLineMode = false;
bool consoleDebug = true;

/*

    The Tetris magic - follow the guidelines at https://tetris.wiki/Tetris_Guideline
    
    On the little 128x32 SSD1306 OLED the playing field has the top on the left
    and the bottom on the right.
    
    Luckily the Adafruit GFX library can rotate the coords for us
    https://learn.adafruit.com/adafruit-gfx-graphics-library/overview
    
    We can fit a playing field that is the standard 10 columns wide when 
    using 3x3 pixel cells. There is plenty of room for a standard 22-24 rows.

    "Columns are conventionally numbered from left to right, and rows from bottom to top."

    We will translate between cell coordinates and pixel coordinates using 
    various game field pixel offsets.
    
    The ghost of the active tetromino can be shown as a shadow line in a 2 pixel gap under the field border.

    For input we will eventually have some buttons but for now we can use serial input.
    When in game mode then single keypresses will be read from the serial terminal.
    
    TASK: define and draw tetronimoes
    - simplified bitmap versions of all 7 tetronimoes
    - perhaps have expanded bitmaps for speed but these would use more memory
    - progmem doesn't seem to allow me the correct addressing
    TASK: allow debug serial controls to select and rotate new piece
    - no real controls yet
    TASK: random 7-system bagshuffle as per https://tetris.wiki/Random_Generator
    - a Fisher-Yates shuffle of a bag of 7
    TASK: game flow and speed
    TASK: rotation and wall bumps

*/


#define TFIELDW 10
#define TFIELDH 26
#define TCELLSZ  3
// bottom gap of 2 for a space and ghost line
#define TBOTGAP 2
#define TFIELDBASE (127 - TBOTGAP)
#define TFIELDTOP (TFIELDBASE - (TFIELDH * TCELLSZ))
// spawn box top left above field in cell units
#define spawn_cx 3
#define spawn_cy 24

#include "Tetrominoes.h"

// map from cell x,y to pixel coords
int8_t curr_tetr_cx = 0;
int8_t curr_tetr_cy = TFIELDH;
uint8_t curr_tetr_w = 3;
uint8_t curr_tetr = 0;
uint8_t curr_tetr_rot = 0;

// field contents in cell rows
uint16_t field_cells[TFIELDH];

void setup() {
    // by default, we'll generate the high voltage from the 3.3v line internally! (neat!)
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 128x32)
    TX_RX_LED_INIT;
    pinMode(RXLED, OUTPUT);
    TXLED1;
    RXLED1;
    consoleInput.reserve(DIAG_INPUT_MAX);
    Serial.begin(9600);     // serial over the USB when connected
    // Serial1.begin(9600);    // serial UART direct onto the ProMicro pins
    version();
    randomSeed(analogRead(0));
    display.clearDisplay();
    display.setRotation(3);
    tetris_tests();
}

void version() {
    Serial.println(MSEPRODUCT " " MSEVERSION);
}

void loop() {
    static uint32_t now;
    now = millis();
    // button debounce and collect
    //if(now - getLast >= getFreq) {
    //    getLast = now;
    //    testCrank();
    //}
    while(Serial.available()){
        proc_console_input(Serial.read());
    }
}

int proc_console_line() {
    consoleInput.trim();
    if(consoleInput.length() == 0)
        return 0;
    if(consoleDebug) {
        Serial.print("Line mode: '");
        Serial.print(consoleInput);
        Serial.println("'");
    }
    // TODO actually have some line commands!
    if(consoleInput == "l") {
        Serial.print("Go to char mode...");
        consoleLineMode = false;
    }
    return 0;
}

int proc_console_input(int k) {
    // When using a console line reading mode, append non-line-ending char to input 
    // string (if not already overflowed).
    // Both CR and LF can terminate the line and ESC (0x1b) can clear the line.
    if(consoleLineMode) {
        switch(k) {
        case 10:
        case 13:
            proc_console_line();
            consoleInput = "";
            return 0;
        case 27:
            consoleInput = "";
            if(consoleDebug)
                Serial.println("ESC");
            return 0;
        default:
            // TODO backspace and arrow chars, useful stuff
            if(consoleInput.length() < DIAG_INPUT_MAX) {
                // ascii printable 0x20 to 0x7E
                if(k < 0x20 || k > 0x7E) {
                    // non-printable - do nothing
                    if(consoleDebug) {
                        Serial.print("non-ascii printable: ");
                        Serial.println(k, DEC);
                    }
                } else {
                    consoleInput += (char)k;
                }
            } else {
                if(consoleDebug) {
                    Serial.println("command tool long - cat sat on keyboard? discard data");
                }
                consoleInput = "";
            }
        }
        return 0;
    }
    // these are all just single character commands for testing
    // TODO simple serial menu system - perhaps nested
    if(consoleDebug) {
        Serial.print("serial input char mode: ");
        Serial.println(k, DEC);
    }
    switch(k) {
    case 10:
    case 13:
    case 27:
        return 0;
    case 'v':
        version();
        return 0;
    case 'h':
        return help();
    case 'c':
        Serial.println("c = clear");
        display.clearDisplay();
        display.display();
        return 0;
    case 'w':
        // choose next tetronimo
        curr_tetr++;
        curr_tetr %= 7;
        select_new_tet(curr_tetr);
        return 0;
    case 'r':
        Serial.println("r = redraw");
        tetris_tests();
        return 0;
    case '0':
        testcells();
        return 0;
    case 'l':
        Serial.print("Go to line mode...");
        consoleLineMode = true;
        return 0;
    case 'a':
    case 'd':
        int dx = (k == 'a' ? -1 : 1);
        return tet_move(dx, 0, 0);
    }
    return 0;
}

int help() {
    // this should return all the known commmands
    Serial.println("h = help");
    version();
    // what it does right now is run a little test...
    Serial.print("I0 = ");
    Serial.println((uint32_t)(void *)I0, HEX);
    for(int i = 0; i < 4; i++) {
        Serial.print(" I0[");
        Serial.print(i);
        Serial.print("]=");
        Serial.println(I0[i],BIN);
    }
    return 0;
}    

/* return upper left pixel location for cell in field */
uint8_t c2px(uint8_t cx) {
    // adjust for left field border
    uint8_t px = (cx * TCELLSZ) + 1;
    return px;
}

/* return upper left pixel location for cell in field */
uint8_t c2py(uint8_t cy) {
    // cell y is the row number starting a zero at the bottom
    // return top-left of cell location
    uint8_t py = TFIELDBASE - (cy * TCELLSZ) - TCELLSZ;
    return py;
}

void drawcell(uint8_t cx, uint8_t cy, bool set) {
    // draw the 9 pixels for the cell
    uint8_t px = c2px(cx);
    uint8_t py = c2py(cy);
    if(set) {
        display.drawRect(px, py, TCELLSZ, TCELLSZ, WHITE);
    } else {
        display.fillRect(px, py, TCELLSZ, TCELLSZ, BLACK);
    }
}

void testcells() {
    // a test for the cell coordinates and drawing routines
    tetris_field_border();
    for(uint8_t pass = 0; pass < 2; pass++) {
        for(uint8_t row = 0; row < TFIELDH; row++) {
            for(uint8_t col = 0; col < TFIELDW; col++) {
                drawcell(col, row, (pass ? BLACK:WHITE));
                display.display();
                //delay(200); no delay required! the display.display is slow enough!
            }
        }
        // the entire thing is really quick if we only update the display here with display.display();
    }
}

#define HIT_NOWT 0
#define HIT_FIELD 1
#define HIT_WALL_LEFT 2
#define HIT_WALL_RIGHT 3
#define HIT_FLOOR 4
#define HIT_CEILING 5

#define CELL_TEST 0
#define CELL_DRAW 1
#define CELL_CLEAR 2

/*
    Draw, undraw, or test for clash with field of a bitmap in cell coordinates, 
    given cell origin top left and the bitmap width and height.
    The bitmaps are right justified and a maximum of 8 bits wide.
    NB: We should only draw at this location if already tested for validity.
*/
int plot_cell_bm(uint8_t act, const uint8_t *bm, uint8_t w, uint8_t h, int8_t ocx, int8_t ocy) {
    int ret = HIT_NOWT;
    // Iterate rows of the bitmap from bottom to top...
    // NB: row is one more than the row index!
    for(uint8_t row = h; row > 0; row--) {
        // grab the byte...
        uint8_t b = bm[row-1];
        int8_t cy = ocy - row - 1;
        // Iterate bits in row from right to left
        for(uint8_t c = 0; c < w; c++) {
            int8_t cx = ocx + w - 1 - c;
            bool set = ((b >> c) & 0x01);
            // NB: test for clash before drawing!
            if(act == CELL_DRAW) {
                if(set) drawcell(cx, cy, set);
            } else if(act == CELL_CLEAR) {
                // when undrawing, clear occupied cells
                if(set) drawcell(cx, cy, !set);
            } else if(act == CELL_TEST) {
                if(!set) continue; // don't care
                // test for clash with field
                ret = tetris_field_test_cell(cx, cy);
                if(ret)
                    return ret;
            }
        }
    }
   return ret;
}

void tetris_field_clear() {
    memset(field_cells, 0, sizeof(field_cells));
}

// return: 0 = no clash, 1 = field block clash
// 2 = hit floor, 
int tetris_field_test_cell(int8_t cx, int8_t cy) {
    // If it is valid to test cell we return cell occupancy
    if(cx > 0 && cy > 0 && cx < TFIELDW && cy < TFIELDH)
        return (field_cells[cy] >> cx & 0x01);
    if(cx < 0)
        return HIT_WALL_LEFT;
    if(cx >= TFIELDW)
        return HIT_WALL_RIGHT;
    if(cy <= 0)
        return HIT_FLOOR;
    // if we get here we are looking at a cell off the top!
    return HIT_CEILING;
}

int tet_move(int8_t dx, int8_t dy, int8_t drot){
    // can current piece move or rotate?
    // it would be similar to attempted drawing of cells against field and side walls
    int8_t rot = (int)curr_tetr_rot + drot;
    int8_t cx = curr_tetr_cx + dx;
    int8_t cy = curr_tetr_cy + dy;
    
    Serial.print(" cx="); Serial.print(cx, DEC);
    Serial.print(" cy="); Serial.print(cy, DEC);
    Serial.print(" rot="); Serial.print(rot, DEC);
    while(rot < 0) rot+=4;
    while(rot > 3) rot-=4;
    const uint8_t *bm = tet_bms[curr_tetr][rot];
    int problem = plot_cell_bm(CELL_TEST, bm, curr_tetr_w, curr_tetr_w, cx, cy);
    Serial.print("plot_cell_bm test = ");
    Serial.println(problem, DEC);
    if(!problem) {
        plot_cell_bm(CELL_CLEAR, bm, curr_tetr_w, curr_tetr_w, curr_tetr_cx, curr_tetr_cy);
        curr_tetr_rot = (uint8_t)rot;
        curr_tetr_cx = cx;
        curr_tetr_cy = cy;
        plot_cell_bm(CELL_DRAW, bm, curr_tetr_w, curr_tetr_w, curr_tetr_cx, curr_tetr_cy);
        display.display();
    }
    return problem;
}

void clear_spawn_area() {
    // clear spawn area - 4x4 cells
    display.fillRect(c2px(spawn_cx), c2py(spawn_cy), (TCELLSZ * 4), (TCELLSZ * 4), BLACK);
}
void select_new_tet(uint8_t tt) {
    display.setCursor(0,0);
    display.println("");
    display.setTextSize(1);
    display.setTextWrap(false);
    display.setTextColor(WHITE, BLACK);
    display.print((int)tt, DEC);
    display.print("=");
    display.write(tet_chars[tt]);
    curr_tetr = tt;
    curr_tetr_cx = spawn_cx;
    curr_tetr_cy = spawn_cy;
    curr_tetr_w = tet_box_size[tt];
    curr_tetr_rot = 0;
    // draw cells within field
    clear_spawn_area();
    // pull piece bitmap in current rotation
    const uint8_t *bm = tet_bms[curr_tetr][curr_tetr_rot];
    uint8_t bsize = curr_tetr_w;
    draw_square_cell_bm(bm, bsize, curr_tetr_cx, curr_tetr_cy);
    display.display();
}

void draw_square_cell_bm(const uint8_t *bm, uint8_t bsize, uint8_t ocx, uint8_t ocy) {
    // Draw a simplified square bitmap with given top left cell origin.
    // The bitmaps are right justified and a maximum of 8 bits wide.
    // TODO: undraw!
    // Iterate rows of the bitmap from top...
    for(uint8_t r = 0; r < bsize; r++) {
        // grab the byte...
        const uint8_t b = bm[r];
        // Iterate bits in row from right to left
        for(uint8_t c = 0; c < bsize; c++) {
            bool set = ((b >> c) & 0x01);
            uint8_t cx = ocx + bsize - 1 - c;
            uint8_t cy = ocy - r;
            drawcell(cx, cy, set);
        }
    }
    
    
}

void tetris_tests() {
    display.clearDisplay();
    display.setRotation(3);
    display.setTextSize(1);
    display.setTextWrap(false);
    display.setTextColor(WHITE);
    display.setCursor(0,0);
    display.println("Tetrs");
    tetris_field_clear();
    tetris_field_border();
    select_new_tet(curr_tetr);
    tetris_ghost();


    display.display();


}

void tetris_ghost() {
    // draw ghost of width of active tetromino, below field
    // cxy to pxy
    // cx2px()
    uint8_t x = (curr_tetr_cx * TCELLSZ) + 1;
    uint8_t y = 127;
    uint8_t w = (curr_tetr_w * TCELLSZ);
    display.drawFastHLine(x, y, w, WHITE);
}


void tetris_field_border() {
    display.drawFastVLine(0, TFIELDTOP,  TFIELDH * TCELLSZ, WHITE);
    display.drawFastVLine(31, TFIELDTOP, TFIELDH * TCELLSZ, WHITE);
    display.drawFastHLine(0, TFIELDBASE, 32, WHITE);
}

static uint8_t rand_int(uint8_t n) {
    return (uint8_t)random(n);
}

void fyshuffle(uint8_t *array, uint8_t n) {
    uint8_t i, j, tmp;
    for (i = n - 1; i > 0; i--) {
        j = rand_int(i + 1);
        tmp = array[j];
        array[j] = array[i];
        array[i] = tmp;
    }
}

uint8_t hexdig(uint8_t c) {
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'A' && c <= 'F') return c - 'A' + 10; 
    if(c >= 'a' && c <= 'f') return c - 'a' + 10; 
    return 0;
}

uint8_t hexdigs(uint8_t c1, uint8_t c2) {
    return (hexdig(c1) << 4) | hexdig(c2);
}
