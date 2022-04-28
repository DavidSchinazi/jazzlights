#include <Adafruit_NeoPixel.h>
#include <Unisparks.h>

char ssid[] = "***";  // your network SSID 
char pass[] = "***";  // your network password

const int STATUS_PIN = 2; // ESP12E builtin led
const int RING_PIN = 5; // D1
const int BUTTON_PIN = 0; // Builtin button
const int BUTTON_DOWN = LOW;
const int NUM_LEDS = 12;
const int BRIGHTNESS_DIVIDER = 8;

Adafruit_NeoPixel strip(NUM_LEDS, RING_PIN, NEO_GRB + NEO_KHZ800);

void renderPixel(int i, uint8_t r, uint8_t g, uint8_t b) {
    strip.setPixelColor(i, r/BRIGHTNESS_DIVIDER, g/BRIGHTNESS_DIVIDER, b/BRIGHTNESS_DIVIDER);       
}

Unisparks::Player player;
Unisparks::Matrix pixels(NUM_LEDS, 1);
Unisparks::Esp8266WiFi network(ssid, pass);

void setup()
{
  Serial.begin(115200);
  Serial.println();
  Serial.println();
  Serial.println("Starting up...");

  // Initialize LED strip
  strip.begin();
  strip.show(); 
  strip.setPixelColor(0, 0xff0000);
  strip.show();

  pinMode(STATUS_PIN, OUTPUT);
  pinMode(RING_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT);

  Unisparks::enableVerboseOutput();
  player.connect(&network);
  player.addStrand(pixels, renderPixel);
  player.begin();
}

void loop()
{
  static int prevButtonState = !BUTTON_DOWN;
  static int pressTs = 0; 
  int buttonState = digitalRead(BUTTON_PIN);
  if (buttonState == BUTTON_DOWN && prevButtonState != BUTTON_DOWN) {
    pressTs = millis();
  }
  else if (buttonState != BUTTON_DOWN && prevButtonState == BUTTON_DOWN) {
      if (millis() - pressTs < 500) { // short press
         player.next();     
      }
      else {
        ;//player.showStatus();
      }
  } 
  prevButtonState = buttonState;
  player.render(DISCONNECTED, millis());
  strip.show();
  delay(10);
}
