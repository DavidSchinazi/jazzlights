#include "dfsparks/playlist.h"
namespace dfsparks {

const Effect* Playlist::find(const char *name) const {
  for(int i=0; i<size(); ++i) {
    const Effect* ef = operator[](i);
    if (!strcmp(ef->name(), name)) {
      return ef;
    }
  }
  return nullptr;   
}


} // namespace dfsparks
