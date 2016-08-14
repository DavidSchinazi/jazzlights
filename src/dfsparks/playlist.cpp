#include "dfsparks/playlist.h"
#include "dfsparks/effects/blink.h"
#include "dfsparks/effects/flame.h"
#include "dfsparks/effects/glitter.h"
#include "dfsparks/effects/plasma.h"
#include "dfsparks/effects/rainbow.h"
#include "dfsparks/effects/rider.h"
#include "dfsparks/effects/slantbars.h"
#include "dfsparks/effects/threesine.h"
#include <assert.h>
#include <stdlib.h>

namespace dfsparks {

class NullEffect : public Effect {
  void doRender(Pixels&) final {}
};

struct EffectInfo {
  EffectInfo(Effect *e, const char *n) {
    effect = e;
    name = n;
  }
  ~EffectInfo() {
    delete effect;
  }
  Effect* effect; 
  const char *name;
};

Repertoire::Repertoire(Pixels& pixels) {
  EffectInfo* all[] = {
    new EffectInfo(new NullEffect(), "_none"),
    new EffectInfo(new Blink(), "slowblink"),
    new EffectInfo(new Rainbow(), "radiaterainbow"),
    new EffectInfo(new Threesine(), "threesine"),
    new EffectInfo(new Plasma(), "plasma"),
    new EffectInfo(new Rider(), "rider"),
    new EffectInfo(new Flame(pixels), "flame"),
    new EffectInfo(new Glitter(), "glitter"),
    new EffectInfo(new Slantbars(), "slantbars")
  };
  size_ = sizeof(all)/sizeof(*all);
  effects_ = new EffectInfo*[size_];
  memcpy(effects_, all, sizeof(all));  
}

Repertoire::~Repertoire() {
  for(int i=0; i<size_; ++i) {
    delete effects_[i];
  }
  delete[] effects_;
}

Effect* Repertoire::effect(int i) const {
  return i < 0 || i > size_ ? nullptr : effects_[i]->effect;
}

const char* Repertoire::name(int i) const {
  return i < 0 || i > size_ ? nullptr : effects_[i]->name;
}

bool Repertoire::find(const char *name, Effect** effect, int* index) const {
  for (int i = 0; i < size_; ++i) {
    if (!strcmp(effects_[i]->name, name)) {
      if (effect) {
        *effect = effects_[i]->effect;
      }
      if (index) {
        *index = i;          
      }
      return true;
    }      
  }
  return false;
}

bool Repertoire::find(const Effect &ef, const char** name, int* index) const {
  for (int i = 0; i < size_; ++i) {
    if (effects_[i]->effect == &ef) {
        if (name) {
          *name = effects_[i]->name;
        }
        if (index) {
          *index = i;          
        }
        return true;
    }      
  }
  return false;
}

Playlist::Playlist(const Repertoire& repertoire, const char *effectNames[]) {
  if (effectNames) {
    // Filter playlist by given names
    size_ = 0;
    int ni = 0;
    const char *name;
    while((name = effectNames[ni++])) {
      int ri;
      if (repertoire.find(name, nullptr, &ri)) {
        ++size_;  
      }
    }
    effects_ = new Effect*[size_];
    ni = 0;
    int pi = 0;
    while((name = effectNames[ni++])) {
      Effect *ef = nullptr;
      if (repertoire.find(name, &ef, nullptr)) {
        effects_[pi++] = ef;
      }
    }
    assert(pi == size_);
  } else {
    // Create default playlist
    size_ = 0;
    for(int i=0; i<repertoire.size(); ++i) {
      if (repertoire.name(i)[0] != '_') {
        ++size_;
      }
    }
    effects_ = new Effect*[size_];
    int pi = 0;
    for(int ri=0; ri<repertoire.size(); ++ri) {
      if (repertoire.name(ri)[0] != '_') {
        effects_[pi++] = repertoire.effect(ri);
      }
    }
    assert(pi == size_);    
  }
}

Playlist::Playlist(const Repertoire& repertoire, double timeShare) {
  size_ = 0;
  for(int i=0; i<repertoire.size(); ++i) {
    if (repertoire.name(i)[0] != '_') {
      ++size_;
    }
  }
  int usedSize = size_;
  size_ = size_ / timeShare;
  effects_ = new Effect*[size_];
  int pi = 0;
  for(int ri=0; ri<repertoire.size(); ++ri) {
    if (repertoire.name(ri)[0] != '_') {
      effects_[pi++] = repertoire.effect(ri);
    }
  }
  assert(pi == usedSize);    
  Effect *noEffect = repertoire.effect(0);
  while(pi < size_) {
    effects_[pi++] = noEffect;
  }
  assert(pi == size_);    
}

Playlist::~Playlist() {
  delete[] effects_;
}

Effect& Playlist::next() {
  track_ = track_ < size_ - 1 ? track_ + 1 : 0; 
  return currentEffect(); 
}

Effect& Playlist::prev() {
  track_ = track_ > 0 ? track_ - 1 : size_ - 1; 
  return currentEffect(); 
}

Effect& Playlist::random() {
  track_ = ::rand() % size_;
  return currentEffect(); 
}

Effect& Playlist::select(int i) {
  track_ = i % size_;
  if (track_ < 0) {
    track_ += size_;
  }
  return currentEffect(); 
}

} // namespace dfsparks
