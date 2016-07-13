#ifndef DFSPARKS_EFFECTS_H
#define DFSPARKS_EFFECTS_H
#include "dfsparks/effect.h"
#include "dfsparks/playlist.h"

namespace dfsparks {

struct EffectInfo;

class Repertoire {
public:
  Repertoire(Pixels& pixels);
  Repertoire(const Repertoire &) = delete;
  ~Repertoire();

  Repertoire &operator=(const Repertoire &) = delete;

  int size() const { return size_; }
  Effect *effect(int i) const;
  const char *name(int i) const;
  bool find(const char *name, Effect **effect, int *index) const;
  bool find(const Effect &ef, const char **name, int *index) const;

private:
  EffectInfo **effects_;
  int size_;
};

class Playlist {
public:
  Playlist(const Repertoire &repertoire, const char *names[] = nullptr);
  Playlist(const Repertoire &repertoire, double timeShare);
  Playlist(const Playlist &) = delete;
  ~Playlist();
  Playlist &operator=(const Playlist &) = delete;

  Effect &currentEffect() const { return *effects_[track_]; }
  int currentPosition() const { return track_; }

  Effect &next();
  Effect &prev();
  Effect &random();
  Effect &select(int i);

private:
  Effect **effects_ = nullptr;
  int track_ = 0;
  int size_ = 0;
};

} // namespace dfsparks
#endif /* DFSPARKS_EFFECTS_H */
