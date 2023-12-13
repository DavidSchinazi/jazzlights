#include "jazzlights/layout_data.h"

#if JL_IS_CONFIG(VEST)

#include "jazzlights/layouts/pixelmap.h"

namespace jazzlights {
namespace {

constexpr Point pixelMap[] = {
    { 0.0,  0.0},
    { 0.0,  1.0},
    { 0.0,  2.0},
    { 0.0,  3.0},
    { 0.0,  4.0},
    { 0.0,  5.0},
    { 0.0,  6.0},
    { 0.0,  7.0},
    { 0.0,  8.0},
    { 0.0,  9.0},
    { 0.0, 10.0},
    { 0.0, 11.0},
    { 0.0, 12.0},
    { 0.0, 13.0},
    { 0.0, 14.0},
    { 0.0, 15.0},
    { 0.0, 16.0},
    { 0.0, 17.0},
    { 0.5, 18.0},
    { 1.0, 17.0},
    { 1.0, 16.0},
    { 1.0, 15.0},
    { 1.0, 14.0},
    { 1.0, 13.0},
    { 1.0, 12.0},
    { 1.0, 11.0},
    { 1.0, 10.0},
    { 1.0,  9.0},
    { 1.0,  8.0},
    { 1.0,  7.0},
    { 1.0,  6.0},
    { 1.0,  5.0},
    { 1.0,  4.0},
    { 1.0,  3.0},
    { 1.0,  2.0},
    { 1.0,  1.0},
    { 1.5,  0.0},
    { 2.0,  1.0},
    { 2.0,  2.0},
    { 2.0,  3.0},
    { 2.0,  4.0},
    { 2.0,  5.0},
    { 2.0,  6.0},
    { 2.0,  7.0},
    { 2.0,  8.0},
    { 2.0,  9.0},
    { 2.0, 10.0},
    { 2.0, 11.0},
    { 2.0, 12.0},
    { 2.0, 13.0},
    { 2.0, 14.0},
    { 2.0, 15.0},
    { 2.0, 16.0},
    { 2.0, 17.0},
    { 2.5, 18.0},
    { 3.0, 17.0},
    { 3.0, 16.0},
    { 3.0, 15.0},
    { 3.0, 14.0},
    { 3.0, 13.0},
    { 3.0, 12.0},
    { 3.0, 11.0},
    { 3.0, 10.0},
    { 3.0,  9.0},
    { 3.0,  8.0},
    { 3.0,  7.0},
    { 3.0,  6.0},
    { 3.0,  5.0},
    { 3.0,  4.0},
    { 3.0,  3.0},
    { 3.0,  2.0},
    { 3.0,  1.0},
    { 3.5,  0.0},
    { 4.0,  1.0},
    { 4.0,  2.0},
    { 4.0,  3.0},
    { 4.0,  4.0},
    { 4.0,  5.0},
    { 4.0,  6.0},
    { 4.0,  7.0},
    { 4.0,  8.0},
    { 4.0,  9.0},
    { 4.0, 10.0},
    { 4.0, 11.0},
    { 4.0, 12.0},
    { 4.0, 13.0},
    { 4.0, 14.0},
    { 4.0, 15.0},
    { 4.0, 16.0},
    { 4.0, 17.0},
    { 4.5, 18.0},
    { 5.0, 17.0},
    { 5.0, 16.0},
    { 5.0, 15.0},
    { 5.0, 14.0},
    { 5.0, 13.0},
    { 5.0, 12.0},
    { 5.0, 11.0},
    { 5.0, 10.0},
    { 5.0,  9.0},
    { 5.0,  8.0},
    { 5.0,  7.0},
    { 5.0,  6.0},
    { 5.0,  5.0},
    { 5.0,  4.0},
    { 5.0,  3.0},
    { 5.0,  2.0},
    { 5.0,  1.0},
    { 6.0,  0.0},
    { 6.0,  1.0},
    { 6.0,  2.0},
    { 6.0,  3.0},
    { 6.0,  4.0},
    { 6.0,  5.0},
    { 6.0,  6.0},
    { 6.0,  7.0},
    { 6.0,  8.0},
    { 6.0,  9.0},
    { 6.0, 10.0},
    { 6.0, 11.0},
    { 6.0, 12.0},
    { 6.0, 13.0},
    { 6.0, 14.0},
    { 6.0, 15.0},
    { 6.0, 16.0},
    { 6.0, 17.0},
    { 6.5, 18.0},
    { 7.0, 17.0},
    { 7.0, 16.0},
    { 7.0, 15.0},
    { 7.0, 14.0},
    { 7.0, 13.0},
    { 7.0, 12.0},
    { 7.0, 11.0},
    { 7.0, 10.0},
    { 7.0,  9.0},
    { 7.0,  8.0},
    { 7.0,  7.0},
    { 7.0,  6.0},
    { 7.0,  5.0},
    { 7.0,  4.0},
    { 7.0,  3.0},
    { 7.0,  2.0},
    { 7.0,  1.0},
    { 7.5,  0.0},
    { 8.0,  1.0},
    { 8.0,  2.0},
    { 8.0,  3.0},
    { 8.0,  4.0},
    { 8.0,  5.0},
    { 8.0,  6.0},
    { 8.0,  7.0},
    { 8.0,  8.0},
    { 8.0,  9.0},
    { 8.0, 10.0},
    { 8.0, 11.0},
    { 8.0, 12.0},
    { 8.0, 13.0},
    { 8.0, 14.0},
    { 8.0, 15.0},
    { 8.0, 16.0},
    { 8.0, 17.0},
    { 8.5, 18.0},
    { 9.0, 17.0},
    { 9.0, 16.0},
    { 9.0, 15.0},
    { 9.0, 14.0},
    { 9.0, 13.0},
    { 9.0, 12.0},
    { 9.0, 11.0},
    { 9.0, 10.0},
    { 9.0,  9.0},
    { 9.0,  8.0},
    { 9.0,  7.0},
    { 9.0,  6.0},
    { 9.0,  5.0},
    { 9.0,  4.0},
    { 9.0,  3.0},
    { 9.0,  2.0},
    { 9.0,  1.0},
    { 9.5,  0.0},
    {10.0,  1.0},
    {10.0,  2.0},
    {10.0,  3.0},
    {10.0,  4.0},
    {10.0,  5.0},
    {10.0,  6.0},
    {10.0,  7.0},
    {10.0,  8.0},
    {10.0,  9.0},
    {10.0, 10.0},
    {10.0, 11.0},
    {10.0, 12.0},
    {10.0, 13.0},
    {10.0, 14.0},
    {10.0, 15.0},
    {10.0, 16.0},
    {10.0, 17.0},
    {10.5, 18.0},
    {11.0, 17.0},
    {11.0, 16.0},
    {11.0, 15.0},
    {11.0, 14.0},
    {11.0, 13.0},
    {11.0, 12.0},
    {11.0, 11.0},
    {11.0, 10.0},
    {11.0,  9.0},
    {11.0,  8.0},
    {11.0,  7.0},
    {11.0,  6.0},
    {11.0,  5.0},
    {11.0,  4.0},
    {11.0,  3.0},
    {11.0,  2.0},
    {11.0,  1.0},
    {11.5,  0.0},
    {12.0,  1.0},
    {12.0,  2.0},
    {12.0,  3.0},
    {12.0,  4.0},
    {12.0,  5.0},
    {12.0,  6.0},
    {12.0,  7.0},
    {12.0,  8.0},
    {12.0,  9.0},
    {12.0, 10.0},
    {12.0, 11.0},
    {12.0, 12.0},
    {12.0, 13.0},
    {12.0, 14.0},
    {12.0, 15.0},
    {12.0, 16.0},
    {12.0, 17.0},
    {12.5, 18.0},
    {13.0, 17.0},
    {13.0, 16.0},
    {13.0, 15.0},
    {13.0, 14.0},
    {13.0, 13.0},
    {13.0, 12.0},
    {13.0, 11.0},
    {13.0, 10.0},
    {13.0,  9.0},
    {13.0,  8.0},
    {13.0,  7.0},
    {13.0,  6.0},
    {13.0,  5.0},
    {13.0,  4.0},
    {13.0,  3.0},
    {13.0,  2.0},
    {13.0,  1.0},
    {14.0,  0.0},
    {14.0,  1.0},
    {14.0,  2.0},
    {14.0,  3.0},
    {14.0,  4.0},
    {14.0,  5.0},
    {14.0,  6.0},
    {14.0,  7.0},
    {14.0,  8.0},
    {14.0,  9.0},
    {14.0, 10.0},
    {14.0, 11.0},
    {14.0, 12.0},
    {14.0, 13.0},
    {14.0, 14.0},
    {14.0, 15.0},
    {14.0, 16.0},
    {14.0, 17.0},
    {14.5, 18.0},
    {15.0, 17.0},
    {15.0, 16.0},
    {15.0, 15.0},
    {15.0, 14.0},
    {15.0, 13.0},
    {15.0, 12.0},
    {15.0, 11.0},
    {15.0, 10.0},
    {15.0,  9.0},
    {15.0,  8.0},
    {15.0,  7.0},
    {15.0,  6.0},
    {15.0,  5.0},
    {15.0,  4.0},
    {15.0,  3.0},
    {15.0,  2.0},
    {15.0,  1.0},
    {15.5,  0.0},
    {16.0,  1.0},
    {16.0,  2.0},
    {16.0,  3.0},
    {16.0,  4.0},
    {16.0,  5.0},
    {16.0,  6.0},
    {16.0,  7.0},
    {16.0,  8.0},
    {16.0,  9.0},
    {16.0, 10.0},
    {16.0, 11.0},
    {16.0, 12.0},
    {16.0, 13.0},
    {16.0, 14.0},
    {16.0, 15.0},
    {16.0, 16.0},
    {16.0, 17.0},
    {16.5, 18.0},
    {17.0, 17.0},
    {17.0, 16.0},
    {17.0, 15.0},
    {17.0, 14.0},
    {17.0, 13.0},
    {17.0, 12.0},
    {17.0, 11.0},
    {17.0, 10.0},
    {17.0,  9.0},
    {17.0,  8.0},
    {17.0,  7.0},
    {17.0,  6.0},
    {17.0,  5.0},
    {17.0,  4.0},
    {17.0,  3.0},
    {17.0,  2.0},
    {17.0,  1.0},
    {17.5,  0.0},
    {18.0,  1.0},
    {18.0,  2.0},
    {18.0,  3.0},
    {18.0,  4.0},
    {18.0,  5.0},
    {18.0,  6.0},
    {18.0,  7.0},
    {18.0,  8.0},
    {18.0,  9.0},
    {18.0, 10.0},
    {18.0, 11.0},
    {18.0, 12.0},
    {18.0, 13.0},
    {18.0, 14.0},
    {18.0, 15.0},
    {18.0, 16.0},
    {18.0, 17.0},
    {18.5, 18.0},
    {19.0, 17.0},
    {19.0, 16.0},
    {19.0, 15.0},
    {19.0, 14.0},
    {19.0, 13.0},
    {19.0, 12.0},
    {19.0, 11.0},
    {19.0, 10.0},
    {19.0,  9.0},
    {19.0,  8.0},
    {19.0,  7.0},
    {19.0,  6.0},
    {19.0,  5.0},
    {19.0,  4.0},
    {19.0,  3.0},
    {19.0,  2.0},
    {19.0,  1.0},
};

static_assert(JL_LENGTH(pixelMap) == 360, "bad size");
PixelMap pixels(JL_LENGTH(pixelMap), pixelMap);

}  // namespace

const Layout* GetLayout() { return &pixels; }
const Layout* GetLayout2() { return nullptr; }

}  // namespace jazzlights

#endif  // VEST
