#include <monte-toad/core/any.hpp>

mt::core::Any::~Any() {
  this->Clear();
}

mt::core::Any::Any(mt::core::Any && other)
  : data(other.data)
  , dealloc(other.dealloc)
{
  other.data = nullptr;
  other.dealloc = nullptr;
}

void mt::core::Any::Clear() {
  if (data) { dealloc(data); }
  data = nullptr;
  dealloc = nullptr;
}

mt::core::Any & mt::core::Any::operator=(mt::core::Any && other) {
  data = other.data;
  dealloc = other.dealloc;
  other.data = nullptr;
  other.dealloc = nullptr;
  return *this;
}
