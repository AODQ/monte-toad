#pragma once

#include <monte-toad/core/any.hpp>
#include <monte-toad/core/enum.hpp>

namespace mt::core {
  struct KernelDispatchInfo {
    mt::KernelDispatchTiming timing;
    size_t dispatchPluginIdx;
    mt::core::Any userdata;
  };
}
