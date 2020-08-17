#pragma once

// simple non-templated/RTTI any

namespace mt::core {
  struct Any {
    Any() : data{nullptr} {}
    ~Any();
    Any(mt::core::Any &&);
    mt::core::Any & operator=(mt::core::Any &&);
    Any(mt::core::Any const &) = delete;

    void Clear();

    void * data = nullptr;
    void (*dealloc)(void * data) = nullptr;
  };
}
