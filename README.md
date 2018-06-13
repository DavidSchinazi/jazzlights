# Unisparks Library

[![Build Status](https://travis-ci.org/unisparks/unisparks.svg?branch=master)](https://travis-ci.org/unisparks/unisparks)

Unisparks is a library for generating network-synchronized LED patterns. 
You can use it in wearables, costumes, accessories, even robots and art cars. The library was originally developed
to control the shiny skin of the famous DiscoFish art car and furry-but-not-less-shiny vests of its crew. It is
also the core of LED control software on our new [TechnoGecko](https://www.technogecko.org/) art car.

If you're going to Burning Man and want to build a costume (or an art installation, or maybe even an art car)
that synchronizes with our art cars - this is the library to use. This document will explain you how.

![Image](extras/docs/discofish.jpg)

## Getting started

Here is an example of how to use the library to 
drive some [NeoPixels](https://www.adafruit.com/index.php?main_page=category&cPath=168) with ESP8266 board:

```c++
    #include <Unisparks.h>
    #include <Adafruit_NeoPixel.h>

    const int NUM_LEDS = 12;
    const int LED_PIN = 5; 
 
    Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

    void renderPixel(int i, uint8_t r, uint8_t g, uint8_t b) {
        strip.setPixelColor(i, r, g, b);       
    }

    Unisparks::Player player;
    Unisparks::Matrix pixels(NUM_LEDS, 1);
    Unisparks::Esp8266WiFi network("myssid", "mypassword");

    void setup()
    {
      Serial.begin(115200);
      strip.begin();
      player.begin(pixels, renderPixel, network);
 
      pinMode(LED_PIN, OUTPUT);
    }

    void loop()
    {
      player.render();
      strip.show();
      delay(10);
    }
```

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md).
