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

#define DIAG_INPUT_MAX 128
String consoleInput = "";

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
#define TFIELDH 22
#define TCELLSZ  3
// bottom gap of 2 for a space and ghost line
#define TBOTGAP 2
#define TFIELDBASE (127 - TBOTGAP)
#define TFIELDTOP (TFIELDBASE - (TFIELDH * TCELLSZ))

#include "Tetrominoes.h"

// map from cell x,y to pixel coords
uint8_t curr_tetr_cx = 0;
uint8_t curr_tetr_cy = TFIELDH;
uint8_t curr_tetr_w = 3;
uint8_t curr_tetr = 0;
uint8_t curr_tetr_rot = 0;

// spawn box top left above field in cell units
uint8_t spawn_cx = 3;
uint8_t spawn_cy = 24;

// field contents in cell rows
uint16_t field_cells[24];

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

void proc_console_input(int data) {
    Serial.println(data, DEC);
    // these are all just single character commands for testing
    // TODO simple serial menu system - perhaps nested
    if(data == 'v') {
        version();
    }
    if(data == 'h') {
        Serial.println("h = help");
        version();
        Serial.print("I0 = ");
        Serial.println((uint32_t)(void *)I0, HEX);
        for(int i = 0; i < 4; i++) {
           Serial.print(" I0[");
           Serial.print(i);
           Serial.print("]=");
           Serial.println(I0[i],BIN);
        }
    }
    if(data == 'c') {
        Serial.println("c = clear");
        display.clearDisplay();
        display.display();
    }
    if(data == 'w') {
        // choose next tetronimo
        curr_tetr++;
        curr_tetr %= 7;
        select_new_tet(curr_tetr);
    }
    if(data == 'r') {
        Serial.println("r = redraw");
        tetris_tests();
    }
    if(data == '0') {
        testcells();
    }
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
    uint8_t *bm = tet_bms[curr_tetr][curr_tetr_rot];
    uint8_t bsize = curr_tetr_w;
    draw_square_cell_bm(bm, bsize, curr_tetr_cx, curr_tetr_cy);
    display.display();
}

void draw_square_cell_bm(uint8_t *bm, uint8_t bsize, uint8_t ocx, uint8_t ocy) {
    // Draw a simplified square bitmap with given top left cell origin.
    // The bitmaps are right justified and a maximum of 8 bits wide.
    // TODO: undraw!
    // Iterate rows of the bitmap from top...
    for(uint8_t r = 0; r < bsize; r++) {
        // grab the byte...
        uint8_t b = bm[r];
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
    display.println("Tetris");
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
