#include "ac6/retail_locator_payload.h"

#include <cstring>

namespace ac6::retail {

void copy_locator_payload(RetailLocatorPayload& destination,
                          const RetailLocatorPayload& source) noexcept {
  // std::memcpy requires non-overlapping objects. The only possible overlap
  // for two complete RetailLocatorPayload objects is identity, which is
  // already the requested result.
  if (&destination == &source) {
    return;
  }
  std::memcpy(destination.bytes.data(), source.bytes.data(),
              kRetailLocatorPayloadBytes);
}

}  // namespace ac6::retail
