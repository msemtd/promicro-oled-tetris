#define MSEPRODUCT "ProMicro OLED Tetris"
#define MSEVERSION "v1.0"
#ifdef __AVR__
  #include <avr/power.h>
#endif

#define THELP Serial.println
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "deb8.h"

#define OLED_RESET 4
Adafruit_SSD1306 display(OLED_RESET);

#if (SSD1306_LCDHEIGHT != 32)
#error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif

const int RXLED = 17;

// Digital I/O for all 16 possible buttons/joypad on the Pro-Micro
//const uint8_t d_map[16] = {2,3,4,5,6,7,8,9,
//                           10,16,14,15,18,19,20,21};
// We only need 8 inputs so use an 8-bit state var and deboucer
const uint8_t d_map[8] = {10,16,14,15,18,19,20,21};
// Here we want just the top 8 inputs
static uint8_t d_inputs = 0;
static debounce8_t d_deb;

// Inputs on the NES controller
#define BTN_UP 1
#define BTN_DOWN 2
#define BTN_RIGHT 4
#define BTN_LEFT 8
#define BTN_B 16
#define BTN_A 32
#define BTN_START 64
#define BTN_SELECT 128

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

    - define and draw tetronimoes
    - simplified bitmap versions of all 7 tetronimoes
    - perhaps have expanded bitmaps for speed but these would use more memory
    - progmem doesn't seem to allow me the correct addressing
    - allow debug serial controls to select and rotate new piece
    - random 7-system bagshuffle as per https://tetris.wiki/Random_Generator
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
#define spawn_cy TFIELDH
#define FULL_ROW (0x3FF)

/*
    Level params from https://tetris.wiki/Tetris_(Game_Boy)
    Game Boy runs at 59.73 frames per second.
    soft-drop 1/3G
    ARE: 2 frames (tetromino is invisible for first frame after it spawns)
    ARE+line clear: 93 frames
    DAS: 24 frames (1/9G)
    Speed levels:
*/
uint8_t level_frames[] = {53, 49, 45, 41, 37, 33, 28, 22, 17, 11, 10, 9, 8, 7, 6, 6, 5, 5, 4, 4, 3};
uint16_t game_lines = 0;
uint32_t game_score = 0;
uint8_t game_level = 0;
int game_gravity = 1;
enum gameState { ATTRACT_MODE, GAME_ON, GAME_OVER, HIGH_SCORES } game_state = ATTRACT_MODE;

#include "Tetrominoes.h"

// map from cell x,y to pixel coords
int8_t curr_tetr_cx = 0;
int8_t curr_tetr_cy = TFIELDH;
uint8_t curr_tetr_w = 3;
uint8_t curr_tetr = 0;
uint8_t curr_tetr_rot = 0;

// field contents in cell rows
uint16_t field_cells[TFIELDH];
// bag of 7 shuffled tetronimoes...
uint8_t tet_bag[7];
uint8_t tet_bix;
// Flags for jobs that need doing...
#define FLAG_SELECT_NEXT_TET 1
#define FLAG_CHECK_COMPLETE_LINES 2
uint8_t flags = 0;

void setup() {
    TX_RX_LED_INIT;
    pinMode(RXLED, OUTPUT);
    TXLED1;
    RXLED1;
    inputs_setup();

    // by default, we'll generate the high voltage from the 3.3v line internally! (neat!)
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 128x32)
    consoleInput.reserve(DIAG_INPUT_MAX);
    Serial.begin(9600);     // serial over the USB when connected
    // Serial1.begin(9600);    // serial UART direct onto the ProMicro pins
    version();
    game_state = ATTRACT_MODE;
    // TODO proper modes - just start for now...
    restart();
}

void restart() {
    randomSeed(analogRead(0));
    display.clearDisplay();
    display.setRotation(3);
    tet_bix = 99; // force a new bag
    curr_tetr = next_tet();
    tetris_field_clear();
    flags = 0;
    game_lines = 0;
    game_score = 0;
    draw_all();
}

void version() {
    Serial.println(MSEPRODUCT " " MSEVERSION);
}

void loop() {
    static uint32_t now; // overflows every ~49.7 days
    static uint32_t t0; // last loop start
    now = millis();
    // 59.73 frames per second = 16.742005692281935375858027791729 ms per frame
    task_inputs();
    switch(game_state) {
    case GAME_ON: {
        // frame_update(now, t0);
        break;
    }
    default:
        break;
    }
    // quick hack for some controls
    button_action(d_inputs);
    check_complete_lines_and_clear();
    check_select_next_tex_and_clear();
    while(Serial.available()){
        proc_console_input(Serial.read());
    }
    t0 = now;
}

void button_action(uint8_t din) {
    // 8 button version
    static uint8_t dheld = 0;
    // 1. a button that is pressed and wasn't held
    // needs to be pressed and set as held
    // 2. a button that has just been released needs to be unheld
    // 3. For simple games we don't need to track button releases
    // Are we just looking at xor to see if changes have happened?
    uint8_t changes = din ^ dheld;
    uint8_t new_presses = changes & din;
    uint8_t new_releases = changes & ~din;
    dheld = din;
    // TODO test releases
    // action these...
    // TODO button_action2(din, changes, new_presses, new_releases)
    uint8_t don = new_presses;
    if(don & BTN_A) tet_move(0, 0, 1);
    if(don & BTN_B) tet_move(0, 0, -1);
    if(don & BTN_LEFT) tet_move(-1, 0, 0);
    if(don & BTN_RIGHT) tet_move(1, 0, 0);
    if(don & BTN_DOWN) tet_move(0, -1, 0);
    if(don & BTN_SELECT) {
        // choose next tetronimo
        select_tet(next_tet());
        return;
    }
    if(don & BTN_START) {
        proc_command('r');
        return;
    }
    if(din == (BTN_UP | BTN_START)) {
        testcells();
        return;
    }
    //

}
void inputs_setup(void) {
    for (int i = 0; i < 8; i++) {
        pinMode(d_map[i], INPUT_PULLUP);
    }
    d_inputs = inputs_read();
}

uint8_t inputs_read(void) {
    uint8_t d_samp = 0;
    for (int i = 0; i < 8; i++) {
        bitWrite(d_samp, i, (digitalRead(d_map[i]) ? 0 : 1));
    }
    return d_samp;
}

/*
 * The inputs are sampled every 5 ms (max)
 */
void task_inputs(void) {
  static uint32_t last;
  uint32_t now = millis();
  if (now - last > 5) {
    last = now;
    uint8_t d_samp = inputs_read();
    d_samp = debouncer8(d_samp, &d_deb);
    if (d_samp != d_inputs) {
      d_inputs = d_samp;
      Serial.print("Inputs: ");
      Serial.println(d_inputs, BIN);
    }
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
    if(consoleInput.length() == 1) {
        return proc_command(consoleInput[0]);
    }
    // Line commands...
    if(consoleInput == "t1") {
        THELP("t1 = Test the shuffle");
        newbag();
        for(uint8_t i = 0; i < sizeof(tet_bag); i++) {
            Serial.print(tet_bag[i], DEC);
            Serial.print(" ");
        }
        Serial.println();
        return 0;
    }
    // Line commands...
    if(consoleInput == "t2") {
        THELP("t2 = Test the hex functions");
        Serial.print("Hex for 254 is ");
        Serial.print(hexnib(254, true));
        Serial.print(hexnib(254, false));
        Serial.println();
        return 0;
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
    return proc_command(k);
}

int help() {
    // this should return all the known commmands
    THELP("h/? = help");
    THELP("t1 = Test the shuffle");
    THELP("v = version");
    THELP("l = toggle line mode");
    THELP("d = console debug on/off");

    THELP("i = inputs");
    return 0;
}

int proc_command(int k) {
    // Single character commands...
    switch(k) {
    case 10:
    case 13:
    case 27:
        return 0;
    case 'v':
        version();
        return 0;
    case 'h':
    case '?':
        return help();
    case 'd':
        THELP("d = console debug on/off");
        consoleDebug = !consoleDebug;
        Serial.print("console debug is now ");
        Serial.println(consoleDebug);
        return 0;
    case 'l':
        THELP("l = toggle line mode");
        consoleLineMode = !consoleLineMode;
        Serial.print("line mode is now ");
        Serial.println(consoleLineMode);
        return 0;
    case 'i':
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
    case '0':
        THELP("0 = testcells");
        testcells();
        return 0;
    case 'c':
        THELP("c = clear");
        display.clearDisplay();
        display.display();
        return 0;
    case 'r':
        THELP("r = redraw");
        draw_all();
        return 0;
    case 'g':
        THELP("g = get game state to console");
        get_game_state();
        return 0;
    case 'q':
    case 'w':
        return tet_move((k == 'q' ? -1 : 1), 0, 0);
    case 'z': // down
        return tet_move(0, -1, 0);
    case 'x':
        // choose next tetronimo
        select_tet(next_tet());
        return 0;
    case ',': // rotate-left
        return tet_move(0, 0, -1);
    case '.': // rotate-right
        return tet_move(0, 0, 1);
    default:
        if(consoleDebug) {
            Serial.print("proc_command unused: ");
            Serial.println(k, DEC);
        }
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
#define CELL_FIELD_MERGE 3

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
            // bool set = ((b >> c) & 0x01);
            bool set = bitRead(b, c);
            // NB: test for clash before drawing!
            if(act == CELL_TEST) {
                if(!set) continue; // don't care
                // test for clash with field
                ret = tetris_field_test_cell(cx, cy);
                if(ret)
                    return ret;
            } else if(act == CELL_CLEAR) {
                // when undrawing, clear occupied cells
                if(set) drawcell(cx, cy, !set);
            } else if(act == CELL_DRAW) {
                if(set) drawcell(cx, cy, set);
            } else if(act == CELL_FIELD_MERGE) {
                if(set) {
                    // draw and add to field...
                    drawcell(cx, cy, set);
                    bitSet(field_cells[cy], cx);
                }
            }
        }
    }
   return ret;
}

void tetris_field_clear() {
    memset(field_cells, 0, sizeof(field_cells));
}

void get_game_state() {
    // Dump game state to console...
    // Show the field...
    for(uint8_t i = 0; i < TFIELDH; i++) {
        // Show rows from top down...
        uint8_t k = TFIELDH - i - 1;
        Serial.print("row[");
        if(k < 10) Serial.print('0');
        Serial.print(k, DEC);
        Serial.print("] = |");
        for(uint8_t j = 0; j < TFIELDW; j++) {
            Serial.print(bitRead(field_cells[k], j) ? '#' : ' ');
        }
        Serial.print("| = 0x");
        Serial.print(hexnib(field_cells[k], true));
        Serial.print(hexnib(field_cells[k], false));
        Serial.print(" = ");
        Serial.print(field_cells[k]);
        Serial.println();
    }
}

void check_select_next_tex_and_clear() {
    if(!bitRead(flags, FLAG_SELECT_NEXT_TET))
        return;
    select_tet(next_tet());
    bitClear(flags, FLAG_SELECT_NEXT_TET);
}

void check_complete_lines_and_clear() {
    if(!bitRead(flags, FLAG_CHECK_COMPLETE_LINES))
        return;
    uint8_t lines = check_complete_lines();
    if(lines) {
        Serial.print("Score lines ");
        Serial.println(lines);
        game_lines += lines;
        draw_field_blocks();
        draw_score();
        display.display();
    }
    bitClear(flags, FLAG_CHECK_COMPLETE_LINES);
}

uint8_t check_complete_lines() {
    // Go bottom up...
    uint8_t ins = 0;
    for(uint8_t i = 0; i < TFIELDH; i++) {
        if((field_cells[i] & FULL_ROW) == FULL_ROW) {
            // Full row should not be copied in...
        } else {
            field_cells[ins] = field_cells[i];
            ins++;
        }
    }
    return (TFIELDH - ins);
}

// return: 0 = no clash, 1 = field block clash
// 2 = hit floor,
int tetris_field_test_cell(int8_t cx, int8_t cy) {
    // If it is valid to test cell we return cell occupancy
    if(cx >= 0 && cy >= 0 && cx < TFIELDW && cy < TFIELDH)
        return bitRead(field_cells[cy], cx);
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
    int8_t rot = (int)curr_tetr_rot + drot;
    int8_t cx = curr_tetr_cx + dx;
    int8_t cy = curr_tetr_cy + dy;
    Serial.print(" cx="); Serial.print(cx, DEC);
    Serial.print(" cy="); Serial.print(cy, DEC);
    Serial.print(" rot="); Serial.print(rot, DEC);
    while(rot < 0) rot+=4; while(rot > 3) rot-=4;
    int problem = plot_cell_bm(CELL_TEST, tet_bms[curr_tetr][rot], curr_tetr_w, curr_tetr_w, cx, cy);
    Serial.print("plot_cell_bm test = "); Serial.println(problem, DEC);
    if(!problem) {
        // go ahead and move/rotate: undraw current tetromimo, rotation, position...
        plot_cell_bm(CELL_CLEAR, tet_bms[curr_tetr][curr_tetr_rot], curr_tetr_w, curr_tetr_w, curr_tetr_cx, curr_tetr_cy);
        curr_tetr_rot = (uint8_t)rot; curr_tetr_cx = cx; curr_tetr_cy = cy;
        plot_cell_bm(CELL_DRAW, tet_bms[curr_tetr][curr_tetr_rot], curr_tetr_w, curr_tetr_w, curr_tetr_cx, curr_tetr_cy);
        tetris_shadow();
        display.display();
    } else {
        if(dy < 0 && ((problem == HIT_FLOOR) || (problem == HIT_FIELD))) {
            Serial.println("landed!");
            // merge tetronimo to field - use plot_cell_bm?
            // TODO ensure that plot_cell_bm returns HIT_FIELD or HIT_FLOOR earlier than HIT_WALL_LEFT or HIT_WALL_RIGHT!!!!
            // Hmmm, the proposed rotation might cause it to hit the floor
            // but we just want to prevent the rotation rather than merge.
            // Perhaps we need to ensure that only one rotate or one move
            // is done for any one call to tet_move.
            plot_cell_bm(CELL_FIELD_MERGE, tet_bms[curr_tetr][curr_tetr_rot], curr_tetr_w, curr_tetr_w, curr_tetr_cx, curr_tetr_cy);
            display.display();
            // Avoid recursion here - schedule call to select_tet(next_tet())
            bitSet(flags, FLAG_SELECT_NEXT_TET);
            bitSet(flags, FLAG_CHECK_COMPLETE_LINES);
        }
    }
    return problem;
}

void select_tet(uint8_t tt) {
    // Select the current tetronimo and spawn...
    curr_tetr = tt;
    draw_tet_info(tt);
    curr_tetr_cx = spawn_cx;
    curr_tetr_cy = spawn_cy;
    curr_tetr_w = tet_box_size[tt];
    curr_tetr_rot = 0;
    // draw cells within field
    clear_spawn_area();
    if(0) {
        // old draw
        // pull piece bitmap in current rotation
        const uint8_t *bm = tet_bms[curr_tetr][curr_tetr_rot];
        uint8_t bsize = curr_tetr_w;
        draw_square_cell_bm(bm, bsize, curr_tetr_cx, curr_tetr_cy);
        display.display();
    } else {
        tet_move(0,0,0);
    }
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

void draw_field_blocks() {
    for(uint8_t row = 0; row < TFIELDH; row++) {
        for(uint8_t col = 0; col < TFIELDW; col++) {
            drawcell(col, row, (bitRead(field_cells[row], col) ? WHITE:BLACK));
        }
    }
}

void clear_spawn_area() {
    // clear spawn area - 4x4 cells
    display.fillRect(c2px(spawn_cx), c2py(spawn_cy), (TCELLSZ * 4), (TCELLSZ * 4), BLACK);
}

void draw_text_setup() {
    display.setCursor(0,0);
    display.setTextSize(1);
    display.setTextWrap(false);
    display.setTextColor(WHITE, BLACK);
}

void draw_tet_info(uint8_t tt) {
    draw_text_setup();
    display.println("");
    display.print((int)tt, DEC);
    display.print("=");
    display.write(tet_chars[tt]);
}

void draw_score() {
    draw_text_setup();
    display.println("");
    display.println("");
    display.print(game_lines, DEC);
}

void draw_all() {
    display.clearDisplay();
    display.setRotation(3);
    draw_text_setup();
    display.println("Tetrs");
    draw_score();
    tetris_field_border();
    draw_field_blocks();
    select_tet(curr_tetr);
    display.display();
}

void tetris_shadow() {
    // Draw shadow drop hint the width of active tetromino, below field...
    // cxy to pxy
    // cx2px()
    uint8_t x = (curr_tetr_cx * TCELLSZ) + 1;
    uint8_t y = 127;
    uint8_t w = (curr_tetr_w * TCELLSZ);
    display.drawFastHLine(0, y, 32, BLACK);
    display.drawFastHLine(x, y, w, WHITE);
}

void tetris_field_border() {
    display.drawFastVLine(0, TFIELDTOP,  TFIELDH * TCELLSZ, WHITE);
    display.drawFastVLine(31, TFIELDTOP, TFIELDH * TCELLSZ, WHITE);
    display.drawFastHLine(0, TFIELDBASE, 32, WHITE);
}

uint16_t score_system() {
    /*
    (from https://tetris.wiki/Scoring)
    Original Nintendo scoring system
    This score was used in Nintendo's versions of Tetris for NES, for Game Boy, and for Super NES.

    Level   Points for
    1 line  Points for
    2 lines Points for
    3 lines Points for
    4 lines
    0   40  100 300 1200
    1   80  200 600 2400
    2  120  300 900 3600
    ...
    9  400 1000 3000 12000
    n  40 * (n + 1) 100 * (n + 1)  300 * (n + 1)    1200 * (n + 1)
    For each piece, the game also awards the number of points equal to the number of grid spaces that the player has continuously soft dropped the piece. Unlike the points for lines, this does not increase per level.
    */
    // TODO
    return 0;
}

uint8_t next_tet() {
    // Get the next tetromino from the random bag...
    tet_bix++;
    if(tet_bix >= sizeof(tet_bag)) {
        newbag();
        tet_bix = 0;
    }
    return tet_bag[tet_bix];
}

//-----------------------------------------------------------------------------
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

void newbag() {
    for(uint8_t i = 0; i < sizeof(tet_bag); i++) {
        tet_bag[i] = i;
    }
    fyshuffle(tet_bag, sizeof(tet_bag));
}

//-----------------------------------------------------------------------------
uint8_t hexdig(uint8_t c) {
    // Hex input...
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'A' && c <= 'F') return c - 'A' + 10;
    if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0;
}

uint8_t hexdigs(uint8_t c1, uint8_t c2) {
    // Hex input...
    return (hexdig(c1) << 4) | hexdig(c2);
}

char hexnib(uint8_t b, bool upper) {
    // hex output...
    uint8_t t = b >> (upper ? 4 : 0);
    t = t & 0x0F;
    t += (t >= 10 ? 'A'-10 : '0');
    return (char)t;
}
