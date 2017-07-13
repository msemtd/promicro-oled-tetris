#define MSEPRODUCT "ProMicro OLED Tetris"
#define MSEVERSION "v1.0"
#ifdef __AVR__
  #include <avr/power.h>
#endif
/*

    The Tetris magic - follow the guidelines at https://tetris.wiki/Tetris_Guideline
    
    On the little 128x32 SSD1306 OLED the playing field has the top on the left
    and the bottom on the right.
    
    Luckily the Adafruit GFX library can rotate the coords for us
    https://learn.adafruit.com/adafruit-gfx-graphics-library/overview
    
    We can fit a playing field that is the standard 10 cells wide when 
    using 3x3 pixel blocks with a border.
    
    The ghost of the active tetromino can be on the bottom line in the 2 pixel gap under the field.

    For input we will eventually have some buttons but for now we can use serial input.
    When in game mode then single keypresses will be read from the serial terminal.
    
    TODO define and draw tetronimoes
    simplified bitmap versions of all 7
    TODO random bagshuffle
    
    
    
    

*/


#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "Tetrominoes.h"

#define OLED_RESET 4
Adafruit_SSD1306 display(OLED_RESET);

#if (SSD1306_LCDHEIGHT != 32)
#error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif

const int RXLED = 17;
                        
#define DIAG_INPUT_MAX 128
String consoleInput = "";

#define TFIELDW 10
#define TFIELDH 22
#define TCELLSZ  3
// bottom gap of 2 for a space and ghost line
#define TBOTGAP 2
#define TFIELDBASE (127 - TBOTGAP)
#define TFIELDTOP (TFIELDBASE - (TFIELDH * TCELLSZ))


// map from cell x,y to pixel coords
uint8_t curr_tetr_cx = 0;
uint8_t curr_tetr_cy = TFIELDH;
uint8_t curr_tetr_w = 3;

uint8_t curr_tetr = 0;

void setup() {
    // by default, we'll generate the high voltage from the 3.3v line internally! (neat!)
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 128x32)
    TX_RX_LED_INIT;
    pinMode(RXLED, OUTPUT);
    TXLED0;
    RXLED0;
    consoleInput.reserve(DIAG_INPUT_MAX);
    Serial.begin(9600);     // serial over the USB when connected
    // Serial1.begin(9600);    // serial UART direct onto the ProMicro pins
    Serial.print("Starting " MSEPRODUCT " " MSEVERSION "\n\n");
    tetris_tests();
}

void loop() {
    static uint32_t now;
    now = millis();
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
    if(data == 'w') {
        // choose next tetronimo
        curr_tetr++;
        curr_tetr %= 7;
        select_new_tet(curr_tetr);
    }
    if(data == 'c') {
        Serial.println("c = clear");
        display.fillScreen(BLACK);
        display.display();
    }
    if(data == 'r') {
        Serial.println("r = redraw");
        tetris_tests();
    }
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
    display.display();
    
    
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

