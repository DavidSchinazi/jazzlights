// Copyright 2016, Igor Chernyshev.

#include "effects/wearable.h"

#include "util/lock.h"
#include "util/logging.h"
#include "util/led_layout.h"
#include "stdio.h"

static dfsparks::UdpSocketNetwork network;

// TODO: We can get a big performance boost by switching to LED-based effects as most
// pixels in a matrix are not actually used, so there's no need to calculate (and then
// interpolate) them
struct FishPixels : public dfsparks::Pixels {
  // The effects look better when they fit completely on one side of the fish.
  // Since we have 500x50 image covering both sides, we'll just mirror the effect
  // folding at 250px
  static constexpr int sizeMax = 250;

  FishPixels(RgbaImage &img) : img(img) {}
  int getNumberOfPixels() const final { return width()*height(); };
  int getWidth() const final { return img.width() < sizeMax ? img.width() : sizeMax; }
  int getHeight() const final { return img.height() < sizeMax ? img.height() : sizeMax; }
  void getCoords(int i, int *x, int *y) const final { *y = i / width(); *x = i % width(); }
  void doSetColor(int index, dfsparks::RgbaColor color) final {
      int x,y;
      getCoords(index, &x, &y);
      for(int i = 0; i < img.width()/width(); ++i ) {
        for(int j = 0; j < img.height()/height(); ++j) {
          uint8_t *d = img.data() + 4*(img.width()*(y + 250*j) + x + 250*i);
          d[0] = color.red;
          d[1] = color.green;
          d[2] = color.blue;
          d[3] = color.alpha;          }
      }
  }
  RgbaImage &img;
};


WearableEffect::WearableEffect() {
}

WearableEffect::~WearableEffect() {
  delete player;
  delete pixels;
}


void WearableEffect::DoInitialize() {}

void WearableEffect::Destroy() {
  CHECK(false);
}

void WearableEffect::SetEffect(int id) {
  (void) id;

  Autolock l(lock_);
  if (player && id > 0) {
    player->play(id, dfsparks::Player::HIGH_PRIORITY);
  }
  effect = id;
}

void WearableEffect::ApplyOnImage(RgbaImage* dst, bool* is_done) {
  (void) is_done;
  if (effect < 0) {
    return;
  }

  Autolock l(lock_);
  if (!player) {
    pixels = new FishPixels(*dst);
    player = new dfsparks::NetworkPlayer(*pixels, network);
    player->shuffleAll();
    static bool haveServer = false;
    if (!haveServer) {
      player->serve();
      haveServer = true;
    }
  }
  network.poll(); 
  player->render();
}
