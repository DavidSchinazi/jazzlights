#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <DFSparks.h>

char ssid[] = "***";  //  your network SSID (name)
char pass[] = "***";  // your network password

byte packetBuffer[DFSPARKS_MAX_FRAME_SIZE]; // buffer to hold incoming packets

// A UDP instance to let us send and receive packets over UDP
WiFiUDP udp;

// An instance that parses network data and calculates RGB colors of each pixel
DFSparks_Strand strand;

void setup()
{
  Serial.begin(115200);
  Serial.println();
  Serial.println();

  // We start by connecting to a WiFi network
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  
  Serial.println("WiFi connected, my IP: ");
  Serial.println(WiFi.localIP());

  Serial.println("Starting UDP");
  udp.begin(DFSPARKS_UDP_PORT);
  Serial.print("Local port: ");
  Serial.println(udp.localPort());
  Serial.println();

  // initialize three LED strand
  strand.begin(3);
}

void loop()
{
  int cb = udp.parsePacket();
  if (cb) {
    udp.read(packetBuffer, sizeof(packetBuffer));
    if (strand.update(packetBuffer, sizeof(packetBuffer))) {
      Serial.print("invalid frame, length=");
      Serial.println(cb);
    }
    else {
      for(int i=0; i<strand.length(); ++i) {
        Serial.print("LED");
        Serial.print(i);
        Serial.print(": RGB(");
        Serial.print(strand.red(i));
        Serial.print(", ");
        Serial.print(strand.green(i));
        Serial.print(", ");
        Serial.print(strand.blue(i));
        Serial.print(") ");
      }
      Serial.println();  
    }
  }
  delay(10);
}
