#include "jazzlights/layout_data.h"

#if IS_CAPTAIN_HAT

#include "jazzlights/layouts/pixelmap.h"

namespace jazzlights {
namespace {

constexpr Point pixelMap[] = {
    { 0.00, 0.00},
    { 1.00, 0.00},
    { 2.00, 0.00},
    { 3.00, 0.00},
    { 4.00, 0.00},
    { 5.00, 0.00},
    { 6.00, 0.00},
    { 7.00, 0.00},
    { 8.00, 0.00},
    { 9.00, 0.00},
    {10.00, 0.00},
    {11.00, 0.00},
    {12.00, 0.00},
    {13.00, 0.00},
    {14.00, 0.00},
    {15.00, 0.00},
    {16.00, 0.00},
    {17.00, 0.00},
    {18.00, 0.00},
    {19.00, 0.00},
    {20.00, 0.00},
    {21.00, 0.00},
    {22.00, 0.00},
    {23.00, 0.00},
    {22.00, 0.00},
    {21.00, 0.00},
    {20.00, 0.00},
    {19.00, 0.00},
    {18.00, 0.00},
    {17.00, 0.00},
    {16.00, 0.00},
    {15.00, 0.00},
    {14.00, 0.00},
    {13.00, 0.00},
    {12.00, 0.00},
    {11.00, 0.00},
    {10.00, 0.00},
    { 9.00, 0.00},
    { 8.00, 0.00},
    { 7.00, 0.00},
    { 6.00, 0.00},
    { 5.00, 0.00},
    { 4.00, 0.00},
    { 3.00, 0.00},
    { 2.00, 0.00},
    { 1.00, 0.00},
    { 0.00, 0.00},
    { 1.00, 0.00},
    EmptyPoint(),
    EmptyPoint(),  EmptyPoint(),  EmptyPoint(),  EmptyPoint(),  EmptyPoint(),  EmptyPoint(),  EmptyPoint(),
    EmptyPoint(),  EmptyPoint(),  EmptyPoint(),  EmptyPoint(),
};

static_assert(JL_LENGTH(pixelMap) == 60, "bad size");
PixelMap pixels(JL_LENGTH(pixelMap), pixelMap);

}  // namespace

const Layout* GetLayout() { return &pixels; }
const Layout* GetLayout2() { return nullptr; }

}  // namespace jazzlights

#endif  // IS_CAPTAIN_HAT
