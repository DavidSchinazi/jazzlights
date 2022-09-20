#include "jazzlights/registry.h"
namespace jazzlights {
namespace internal {

Deleter::~Deleter() { delete next; }

Deleter* firstDeleter = nullptr;

}  // namespace internal
}  // namespace jazzlights
