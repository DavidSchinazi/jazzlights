#include "unisparks/registry.hpp"
namespace unisparks {
namespace internal {

Deleter::~Deleter() {
    delete next;
}

Deleter* firstDeleter = nullptr;  

} // namespace internal
} // namespace unisparks



