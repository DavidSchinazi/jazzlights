#ifndef UNISPARKS_PLAYLIST_DEFAULT_H
#define UNISPARKS_PLAYLIST_DEFAULT_H
#include "unisparks/playlist.h"
#include "unisparks/effects/flame.h"
#include "unisparks/effects/simple.h"
namespace unisparks {

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

} // namespace unisparks
#endif /* UNISPARKS_PLAYLIST_DEFAULT_H */
