#ifndef DFSPARKS_PLAYLIST_DEFAULT_H
#define DFSPARKS_PLAYLIST_DEFAULT_H
#include "dfsparks/playlist.h"
#include "dfsparks/effects/flame.h"
#include "dfsparks/effects/simple.h"
namespace dfsparks {

// Add new effects here
inline const Playlist& defaultPlaylist() {
  static auto pl = makePlaylist(
    functionalEffect("radiaterainbow", renderRainbow),
    functionalEffect("threesine", renderTreesine),
    functionalEffect("plasma", renderPlasma),
    functionalEffect("rider", renderRider),
    Flame("flame"),
    functionalEffect("glitterr", renderGlitter),
    functionalEffect("slantbars", renderSlantbars)
  );
  return pl;
}

} // namespace dfsparks
#endif /* DFSPARKS_PLAYLIST_DEFAULT_H */
