#include "unisparks.hpp"
#include "catch.hpp"
using namespace unisparks;

void dummyrender(int, uint8_t, uint8_t, uint8_t) {
}

struct TestRenderer : Renderer {
  void render(InputStream<Color>& data) override {
    int i = 0;
    for (auto c : data) {
      if (i < 255) { colors[i++] = c.asRgb(); }
    }
  }
  RgbColor colors[255];
};


TEST_CASE("Player invariants", "[player]") {
  Player pl;
  Matrix bigPane{250, 50};

  pl.begin(bigPane, dummyrender);

  pl.render(timeMillis());
  pl.render(timeMillis()); // check that we don't crash

  //    pl.begin();    // should do nothing
  //    pl.begin(); // should do nothing
}

#if 0
TEST_CASE("Sequence", "[player]") {
  // enableVerboseOutput();
  info(">>>>> seqtest");
  auto ef = sequence(1000, solid(RED), 1000, solid(GREEN));
  Matrix layout{1, 1};
  TestRenderer renderer;

  Player pl;
  pl.addStrand(layout, renderer);
  pl.begin();

  pl.render(0);
  REQUIRE(renderer.colors[0] == RgbColor(0xff0000));

  pl.render(500);
  REQUIRE(renderer.colors[0] == RgbColor(0xff0000));

  pl.render(600);
  REQUIRE(renderer.colors[0] == RgbColor(0x00ff00));
}
#endif
