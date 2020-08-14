#include <monte-toad/core/integratordata.hpp>

#include <monte-toad/core/kerneldispatchinfo.hpp>

auto mt::core::IntegratorData::HasPreview() -> bool {
  return
      this->kernelDispatchers.size() > 0ul
   && this->kernelDispatchers.back().timing == mt::KernelDispatchTiming::Preview
 ;
}
