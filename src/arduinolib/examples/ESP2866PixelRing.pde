/*
 DiscoFish wearable based on ESP8266 + NeoPixel ring
 */

#include <SPI.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <DiscoFish_Wearables.h>

const char *ssid = "mynetwork";  //  your network SSID (name)
const char *ass = "mypassword";       // your network password

// A UDP instance to let us send and receive packets over UDP
WiFiUDP Udp;
DiscoFish_LightStrip DFLights(8);

void setup() {
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  // check for the presence of the shield:
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("WiFi shield not present");
    // don't continue:
    while (true);
  }

  String fv = WiFi.firmwareVersion();
  if (fv != "1.1.0") {
    Serial.println("Please upgrade the firmware");
  }

  // attempt to connect to Wifi network:
  int status = WL_IDLE_STATUS;
  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(ssid, pass);

    // wait 10 seconds for connection:
    delay(10000);
  }

  Serial.println("Connected to wifi");
  printWifiStatus();

  Serial.println("\nStarting connection to server...");
  Udp.begin(DISCOFISH_WEARABLES_PORT);
}

void loop() {
  if (Udp.parsePacket()) {
    char buf[DISCOFISH_WEARABLES_FRAME_SIZE]
    Udp.read(buf, sizeof(buf)); // read the packet into the buffer
    DFLights.loadFrame(buf, sizeof(buf));
    Serial.println("Frame received, r=%d, g=%d, b=%d", DFLights.getRed(0), 
      DFLights.getGreen(0), DFLights.getBlue(0));
  }
}
