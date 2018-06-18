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

  pl.render();

  REQUIRE(pl.effectCount() > 2);
  REQUIRE(pl.effectIndex() == 0);
  REQUIRE(!strcmp(pl.effectName(), "rainbow"));

  pl.render(); // check that we don't crash

  //    pl.begin();    // should do nothing
  //    pl.begin(); // should do nothing

  REQUIRE(pl.effectIndex() == 0);
  pl.next();
  REQUIRE(pl.effectIndex() == 1);
  pl.next();
  REQUIRE(pl.effectIndex() == 2);
  pl.prev();
  REQUIRE(pl.effectIndex() == 1);
  pl.jump(0);
  REQUIRE(pl.effectIndex() == 0);
  pl.jump(2);
  REQUIRE(pl.effectIndex() == 2);
}

TEST_CASE("Sequence", "[player]") {
  // enableVerboseOutput();

  auto ef = sequence(1000, solid(RED), 1000, solid(GREEN));
  EffectInfo efi = {&ef, "seqtest", true};

  Matrix layout{1, 1};
  TestRenderer renderer;
  Strand s = {&layout, &renderer};

  PlayerOptions opts;
  opts.customEffects = &efi;
  opts.customEffectCount = 1;
  opts.strands = &s;
  opts.strandCount = 1;

  Player pl;
  pl.begin(opts);
  pl.play("seqtest");

  pl.render(0);
  REQUIRE(renderer.colors[0] == RgbColor(0xff0000));

  pl.render(500);
  REQUIRE(renderer.colors[0] == RgbColor(0xff0000));

  pl.render(600);
  REQUIRE(renderer.colors[0] == RgbColor(0x00ff00));
}
