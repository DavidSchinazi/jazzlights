#include "jazzlights/layout_data.h"

#if IS_GAUNTLET || IS_HAMMER || IS_FAIRY_WAND || IS_ROPELIGHT

#include "jazzlights/layouts/matrix.h"

namespace jazzlights {
namespace {

#if IS_GAUNTLET
Matrix pixels(/*w=*/20, /*h=*/15);
#endif  // IS_GAUNTLET

#if IS_HAMMER
Matrix pixels(/*w=*/20, /*h=*/1);
#endif  // IS_HAMMER

#if IS_FAIRY_WAND
Matrix pixels(/*w=*/3, /*h=*/3, /*resolution=*/1.0);
#endif  // IS_FAIRY_WAND

#if IS_ROPELIGHT
Matrix pixels(/*w=*/300, /*h=*/1);
#endif  // IS_ROPELIGHT

}  // namespace

const Layout* GetLayout() { return &pixels; }
const Layout* GetLayout2() { return nullptr; }

}  // namespace jazzlights

#endif
