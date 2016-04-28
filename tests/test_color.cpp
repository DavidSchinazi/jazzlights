#include "catch.hpp"
#include "DFSparks_Color.h"

using namespace dfsparks;
TEST_CASE( "RGBA color model", "color") {
   for(int i=0; i<1000; ++i) {
      uint8_t r = rand() % 255;
      uint8_t g = rand() % 255;
      uint8_t b = rand() % 255;
      uint8_t a = rand() % 255;
      uint32_t color = rgba(r,g,b,a);
      REQUIRE(chan_red(color)==r);
      REQUIRE(chan_green(color)==g);
      REQUIRE(chan_blue(color)==b);
      REQUIRE(chan_alpha(color)==a);
   }
}

TEST_CASE( "HSL color model", "color") {
   REQUIRE(dfsparks::rgb(255,0,255) == 0xff00ff);
#if 0

   REQUIRE(dfsparks::hsl(0,255,255) == 0xff0000);
   REQUIRE(dfsparks::hsl(2,255,255) == 0xff0f00);
   REQUIRE(dfsparks::hsl(5,255,255) == 0xff1e00);
   REQUIRE(dfsparks::hsl(7,255,255) == 0xff2d00);
   REQUIRE(dfsparks::hsl(10,255,255) == 0xff3d00);
   REQUIRE(dfsparks::hsl(12,255,255) == 0xff4c00);
   REQUIRE(dfsparks::hsl(15,255,255) == 0xff5b00);
   REQUIRE(dfsparks::hsl(17,255,255) == 0xff6b00);
   REQUIRE(dfsparks::hsl(20,255,255) == 0xff7a00);
   REQUIRE(dfsparks::hsl(22,255,255) == 0xff8900);
   REQUIRE(dfsparks::hsl(25,255,255) == 0xff9900);
   REQUIRE(dfsparks::hsl(28,255,255) == 0xffa800);
   REQUIRE(dfsparks::hsl(30,255,255) == 0xffb700);
   REQUIRE(dfsparks::hsl(33,255,255) == 0xffc600);
   REQUIRE(dfsparks::hsl(35,255,255) == 0xffd600);
   REQUIRE(dfsparks::hsl(38,255,255) == 0xffe500);
   REQUIRE(dfsparks::hsl(40,255,255) == 0xfff400);
   REQUIRE(dfsparks::hsl(43,255,255) == 0xf9ff00);
   REQUIRE(dfsparks::hsl(45,255,255) == 0xeaff00);
   REQUIRE(dfsparks::hsl(48,255,255) == 0xdbff00);
   REQUIRE(dfsparks::hsl(51,255,255) == 0xcbff00);
   REQUIRE(dfsparks::hsl(53,255,255) == 0xbcff00);
   REQUIRE(dfsparks::hsl(56,255,255) == 0xadff00);
   REQUIRE(dfsparks::hsl(58,255,255) == 0x9eff00);
   REQUIRE(dfsparks::hsl(61,255,255) == 0x8eff00);
   REQUIRE(dfsparks::hsl(63,255,255) == 0x7fff00);
   REQUIRE(dfsparks::hsl(66,255,255) == 0x70ff00);
   REQUIRE(dfsparks::hsl(68,255,255) == 0x60ff00);
   REQUIRE(dfsparks::hsl(71,255,255) == 0x51ff00);
   REQUIRE(dfsparks::hsl(73,255,255) == 0x42ff00);
   REQUIRE(dfsparks::hsl(76,255,255) == 0x33ff00);
   REQUIRE(dfsparks::hsl(79,255,255) == 0x23ff00);
   REQUIRE(dfsparks::hsl(81,255,255) == 0x14ff00);
   REQUIRE(dfsparks::hsl(84,255,255) == 0x5ff00);
   REQUIRE(dfsparks::hsl(86,255,255) == 0xff0a);
   REQUIRE(dfsparks::hsl(89,255,255) == 0xff19);
   REQUIRE(dfsparks::hsl(91,255,255) == 0xff28);
   REQUIRE(dfsparks::hsl(94,255,255) == 0xff38);
   REQUIRE(dfsparks::hsl(96,255,255) == 0xff47);
   REQUIRE(dfsparks::hsl(99,255,255) == 0xff56);
   REQUIRE(dfsparks::hsl(102,255,255) == 0xff66);
   REQUIRE(dfsparks::hsl(104,255,255) == 0xff75);
   REQUIRE(dfsparks::hsl(107,255,255) == 0xff84);
   REQUIRE(dfsparks::hsl(109,255,255) == 0xff93);
   REQUIRE(dfsparks::hsl(112,255,255) == 0xffa3);
   REQUIRE(dfsparks::hsl(114,255,255) == 0xffb2);
   REQUIRE(dfsparks::hsl(117,255,255) == 0xffc1);
   REQUIRE(dfsparks::hsl(119,255,255) == 0xffd1);
   REQUIRE(dfsparks::hsl(122,255,255) == 0xffe0);
   REQUIRE(dfsparks::hsl(124,255,255) == 0xffef);
   REQUIRE(dfsparks::hsl(127,255,255) == 0xffff);
   REQUIRE(dfsparks::hsl(130,255,255) == 0xefff);
   REQUIRE(dfsparks::hsl(132,255,255) == 0xe0ff);
   REQUIRE(dfsparks::hsl(135,255,255) == 0xd1ff);
   REQUIRE(dfsparks::hsl(137,255,255) == 0xc1ff);
   REQUIRE(dfsparks::hsl(140,255,255) == 0xb2ff);
   REQUIRE(dfsparks::hsl(142,255,255) == 0xa3ff);
   REQUIRE(dfsparks::hsl(145,255,255) == 0x93ff);
   REQUIRE(dfsparks::hsl(147,255,255) == 0x84ff);
   REQUIRE(dfsparks::hsl(150,255,255) == 0x75ff);
   REQUIRE(dfsparks::hsl(153,255,255) == 0x66ff);
   REQUIRE(dfsparks::hsl(155,255,255) == 0x56ff);
   REQUIRE(dfsparks::hsl(158,255,255) == 0x47ff);
   REQUIRE(dfsparks::hsl(160,255,255) == 0x38ff);
   REQUIRE(dfsparks::hsl(163,255,255) == 0x28ff);
   REQUIRE(dfsparks::hsl(165,255,255) == 0x19ff);
   REQUIRE(dfsparks::hsl(168,255,255) == 0xaff);
   REQUIRE(dfsparks::hsl(170,255,255) == 0x500ff);
   REQUIRE(dfsparks::hsl(173,255,255) == 0x1400ff);
   REQUIRE(dfsparks::hsl(175,255,255) == 0x2300ff);
   REQUIRE(dfsparks::hsl(178,255,255) == 0x3200ff);
   REQUIRE(dfsparks::hsl(181,255,255) == 0x4200ff);
   REQUIRE(dfsparks::hsl(183,255,255) == 0x5100ff);
   REQUIRE(dfsparks::hsl(186,255,255) == 0x6000ff);
   REQUIRE(dfsparks::hsl(188,255,255) == 0x7000ff);
   REQUIRE(dfsparks::hsl(191,255,255) == 0x7f00ff);
   REQUIRE(dfsparks::hsl(193,255,255) == 0x8e00ff);
   REQUIRE(dfsparks::hsl(196,255,255) == 0x9e00ff);
   REQUIRE(dfsparks::hsl(198,255,255) == 0xad00ff);
   REQUIRE(dfsparks::hsl(201,255,255) == 0xbc00ff);
   REQUIRE(dfsparks::hsl(204,255,255) == 0xcc00ff);
   REQUIRE(dfsparks::hsl(206,255,255) == 0xdb00ff);
   REQUIRE(dfsparks::hsl(209,255,255) == 0xea00ff);
   REQUIRE(dfsparks::hsl(211,255,255) == 0xf900ff);
   REQUIRE(dfsparks::hsl(214,255,255) == 0xff00f4);
   REQUIRE(dfsparks::hsl(216,255,255) == 0xff00e5);
   REQUIRE(dfsparks::hsl(219,255,255) == 0xff00d6);
   REQUIRE(dfsparks::hsl(221,255,255) == 0xff00c6);
   REQUIRE(dfsparks::hsl(224,255,255) == 0xff00b7);
   REQUIRE(dfsparks::hsl(226,255,255) == 0xff00a8);
   REQUIRE(dfsparks::hsl(229,255,255) == 0xff0098);
   REQUIRE(dfsparks::hsl(232,255,255) == 0xff0089);
   REQUIRE(dfsparks::hsl(234,255,255) == 0xff007a);
   REQUIRE(dfsparks::hsl(237,255,255) == 0xff006b);
   REQUIRE(dfsparks::hsl(239,255,255) == 0xff005b);
   REQUIRE(dfsparks::hsl(242,255,255) == 0xff004c);
   REQUIRE(dfsparks::hsl(244,255,255) == 0xff003d);
   REQUIRE(dfsparks::hsl(247,255,255) == 0xff002d);
   REQUIRE(dfsparks::hsl(249,255,255) == 0xff001e);
   REQUIRE(dfsparks::hsl(252,255,255) == 0xff000f);
   REQUIRE(dfsparks::hsl(255,255,255) == 0xff0000);
#endif
}
