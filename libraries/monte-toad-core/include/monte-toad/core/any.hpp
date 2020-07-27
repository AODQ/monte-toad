#pragma once

// simple non-templated/RTTI any

namespace mt::core {
  struct Any {
    Any() {}
    ~Any();
    Any(mt::core::Any &&);
    Any(mt::core::Any const &) = delete;

    void * data;
  };
}
