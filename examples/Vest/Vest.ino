//   RGB Vest Demo Code
//
//   Use Version 3.0 or later https://github.com/FastLED/FastLED
//   ZIP file https://github.com/FastLED/FastLED/archive/master.zip
//

// you need this line or the feather huzzah or LoLin nodecmu pins will not be mapped properly
#define FASTLED_ESP8266_RAW_PIN_ORDER
#include <FastLED.h>
#include <DFSparks.h>

using namespace dfsparks;

char ssid[] = "***";  //  your network SSID (name)
char pass[] = "***";  // your network password

// Data output to LEDs is on pin 5
#define LED_PIN  5

// Vest color order (Green/Red/Blue)
#define COLOR_ORDER GRB
#define CHIPSET     WS2811 

// Button settings
#define NUMBUTTONS 3
#define MODEBUTTON 13
#define BRIGHTNESSBUTTON 14
#define WIFIBUTTON 12

#define BTN_IDLE 0
#define BTN_DEBOUNCING 1
#define BTN_PRESSED 2
#define BTN_RELEASED 3
#define BTN_LONGPRESS 4
#define BTN_LONGPRESSREAD 5

#define BTN_DEBOUNCETIME 20
#define BTN_LONGPRESSTIME 1000

// Global maximum brightness value, maximum 255
#define MAXBRIGHTNESS 120 
#define STARTBRIGHTNESS 40

// Vest dimensions
#define MATRIX_WIDTH 15
#define MATRIX_HEIGHT 16
#define LEDNUM 215 // == LAST_VISIBLE_LED, total number of physical leds

const int pixelMap[] = {
   216,216,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9,216,216,216,
    10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
    25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
   216,216, 40, 41, 42, 43, 44, 45, 46, 47,216,216,216,216,216,
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 
    63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 
    78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 
    93, 94, 95, 96, 97, 98, 99,100,101,102,103,104,105,106,107,
   108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,
   123,124,125,126,127,128,129,130,131,132,133,134,135,136,137,
   138,139,140,141,142,143,144,145,146,147,148,149,150,151,152,
   153,154,155,156,157,158,159,160,161,162,163,164,165,166,167,
   216,216,168,169,170,171,172,173,174,175,216,216,216,216,216,
   176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,
   191,192,193,194,195,196,197,198,199,200,201,202,203,204,205,
   216,216,206,207,208,209,210,211,212,213,214,215,216,216,216
};

struct VestPixels : public PixelMap {
  VestPixels() : PixelMap(MATRIX_WIDTH, MATRIX_HEIGHT, LEDNUM, pixelMap, true) {
  }

  void doSetColor(int i,RgbaColor color) final {
    leds[i] = CRGB(color.red, color.green, color.blue);       
  } 

  CRGB leds[MATRIX_WIDTH*MATRIX_HEIGHT]; 
  
} pixels;


uint8_t currentBrightness = STARTBRIGHTNESS;
unsigned long buttonEvents[NUMBUTTONS];
uint8_t buttonStatuses[NUMBUTTONS];
uint8_t buttonPins[NUMBUTTONS] = {MODEBUTTON, BRIGHTNESSBUTTON, WIFIBUTTON};


Esp8266Network network(ssid, pass);
NetworkPlayer player(pixels, network);

void updateButtons(uint32_t currentMillis) {
  for (int i = 0; i < NUMBUTTONS; i++) {
    switch (buttonStatuses[i]) {
      case BTN_IDLE:
        if (digitalRead(buttonPins[i]) == LOW) {
          buttonEvents[i] = currentMillis;
          buttonStatuses[i] = BTN_DEBOUNCING;
        }
        break;

      case BTN_DEBOUNCING:
        if (currentMillis - buttonEvents[i] > BTN_DEBOUNCETIME) {
          if (digitalRead(buttonPins[i]) == LOW) {
            buttonStatuses[i] = BTN_PRESSED;
          }
        }
        break;

      case BTN_PRESSED:
        if (digitalRead(buttonPins[i]) == HIGH) {
          buttonStatuses[i] = BTN_RELEASED;
        } else if (currentMillis - buttonEvents[i] > BTN_LONGPRESSTIME) {
          buttonStatuses[i] = BTN_LONGPRESS;
        }
        break;

      case BTN_RELEASED:
        break;

      case BTN_LONGPRESS:
        break;

      case BTN_LONGPRESSREAD:
        if (digitalRead(buttonPins[i]) == HIGH) {
          buttonStatuses[i] = BTN_IDLE;
        }
        break;
    }
  }
}

uint8_t buttonStatus(uint8_t buttonNum) {

  uint8_t tempStatus = buttonStatuses[buttonNum];
  if (tempStatus == BTN_RELEASED) {
    buttonStatuses[buttonNum] = BTN_IDLE;
  } else if (tempStatus == BTN_LONGPRESS) {
    buttonStatuses[buttonNum] = BTN_LONGPRESSREAD;
  }
  return tempStatus;
}

void doButtons(NetworkPlayer& player) {
  
  // Check the mode button (for switching between effects)
  switch (buttonStatus(0)) {
    case BTN_RELEASED: 
      player.next();
      break;

    case BTN_LONGPRESS: 
      break;
  }

  // Check the brightness adjust button
  switch (buttonStatus(1)) {
    case BTN_RELEASED: 
      currentBrightness += 51; // increase the brightness (wraps to lowest)
      FastLED.setBrightness(scale8(currentBrightness, MAXBRIGHTNESS));
      info("Brightness set to %d", currentBrightness);
      break;

    case BTN_LONGPRESS: // button was held down for a while
      currentBrightness = STARTBRIGHTNESS; // reset brightness to startup value
      FastLED.setBrightness(scale8(currentBrightness, MAXBRIGHTNESS));
      info("Brightness set to %d", currentBrightness);
      break;
  }

  // Check the wifi adjust button
  switch (buttonStatus(2)) {
    case BTN_RELEASED: 
      player.showStatus(!player.isShowingStatus());
      info("Toggled status, %d", player.isShowingStatus());
      break;

    case BTN_LONGPRESS: 
      break;
  }
}

void setup() {
  logLevel = debugLevel;
  
  Serial.begin(115200);

  pinMode(MODEBUTTON, INPUT_PULLUP);
  pinMode(BRIGHTNESSBUTTON, INPUT_PULLUP);
  pinMode(WIFIBUTTON, INPUT_PULLUP);

  // Write FastLED configuration data
  FastLED.addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(pixels.leds, sizeof(pixels.leds)/sizeof(*pixels.leds));
  FastLED.setBrightness(scale8(currentBrightness, MAXBRIGHTNESS));
}


void loop()
{
  uint32_t currentMillis = millis();
  updateButtons(currentMillis); // read, debounce, and process the buttons
  doButtons(player); // perform actions based on button state
  network.poll();
  player.render();
  FastLED.show();
  delay(10);
}