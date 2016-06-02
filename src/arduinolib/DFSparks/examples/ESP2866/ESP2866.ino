#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <Adafruit_NeoPixel.h>
#include <DFSparks.h>

char ssid[] = "***";  //  your network SSID (name)
char pass[] = "***";  // your network password

const int STATUS_PIN = 2; // builtin led
const int RING_PIN = 5; // D1
const int BUTTON_PIN = 13; // D7

const int NUM_LEDS = 12;

// 
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, RING_PIN, NEO_GRB + NEO_KHZ800);

// A UDP instance to let us send and receive packets over UDP
DFSparks_WiFiUDPClient<WiFiUDP> client;

class Demo : public DFSparks_Matrix<NUM_LEDS,1> {
private: 
  void on_set_pixel_color(int x, int y, uint8_t red, uint8_t green, uint8_t blue) override {
         strip.setPixelColor(x, strip.Color(red/2,green/2,blue/2));     
  };
} demo;

void setup()
{
  Serial.begin(115200);
  Serial.println();
  Serial.println();

  // Initialize LED strip
  strip.begin();
  strip.show(); 
  strip.setPixelColor(0, 0xff0000);
  strip.show();


  pinMode(STATUS_PIN, OUTPUT);
  pinMode(RING_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT);

  // Connect to a WiFi network
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    
    // Display connecting pattern
    static int i = 0;
    strip.setPixelColor(++i % strip.numPixels(), 0x00ff00);
    strip.show();
  }
  Serial.println("");
  
  Serial.println("WiFi connected, my IP: ");
  Serial.println(WiFi.localIP());

  Serial.println("Starting demo");
  demo.begin(client);
}

void loop()
{
  static int prevButtonState = LOW;
  int buttonState = digitalRead(BUTTON_PIN);
  if (buttonState == HIGH && prevButtonState == LOW) {
      demo.play_next(); 
  } 
  prevButtonState = buttonState;
  //Serial.print("Start time:");
  //Serial.println(client.get_start_time());
  demo.render();
  strip.show();
  delay(10);
}