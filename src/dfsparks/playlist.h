#ifndef DFSPARKS_PLAYLIST_H
#define DFSPARKS_PLAYLIST_H
#include "dfsparks/effect.h"
namespace dfsparks {

class Playlist {
public:
    virtual ~Playlist() = default;
    virtual int size() const = 0;
    virtual const Effect* operator[](int i) const = 0;
    const Effect* find(const char *name) const;
};

template <class... EffectTs>
struct BasicPlaylist : public Playlist {
  virtual const Effect* operator[](int i) const {return get(i);}
  virtual int size() const {return staticSize();}
  
  constexpr const Effect* get(int) const {return nullptr;}
  constexpr size_t staticMaxRendererSize() const {return 0;}
  constexpr size_t staticSize() const {return 0;}
};

template <class EffectT, class... EffectTs>
struct BasicPlaylist<EffectT, EffectTs...> : BasicPlaylist<EffectTs...> {
  BasicPlaylist(EffectT e, EffectTs... es) : BasicPlaylist<EffectTs...>(es...), effect(e) {}
  const Effect* operator[](int i) const override {return get(i);}
  int size() const override {return staticSize();}  
  constexpr const Effect* get(int i) const {return i==0 ? &effect : BasicPlaylist<EffectTs...>::get(i-1);}  
  constexpr size_t staticSize() const {return BasicPlaylist<EffectTs...>::staticSize() + 1;}

  EffectT effect;
};

template<typename... EffectTs>
BasicPlaylist<EffectTs...> makePlaylist(EffectTs... args) {
  return BasicPlaylist<EffectTs...>(args...);
}

} // namespace dfsparks
#endif /* DFSPARKS_PLAYLIST_H */
