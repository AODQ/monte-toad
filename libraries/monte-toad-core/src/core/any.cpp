#include <monte-toad/core/any.hpp>

mt::core::Any::~Any() {
  if (data) { /* TODO report leaked memory */ }
}

mt::core::Any::Any(mt::core::Any && other) : data(other.data) {
  other.data = nullptr;
}

mt::core::Any & mt::core::Any::operator=(mt::core::Any && other) {
  data = other.data;
  other.data = nullptr;
  return *this;
}
