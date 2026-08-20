#include "ac6demo/renderer_payload_version.hpp"

#include <cassert>
#include <stdexcept>
#include <string>

namespace {

std::string digest(char value) { return std::string(64U, value); }

} // namespace

int main() {
  ac6demo::RendererPayloadVersion version;
  const auto first = digest('0');
  const auto second = digest('a');

  assert(!version.initialized());
  assert(version.generation() == 0U);
  assert(version.needs_upload(first));

  version.validate_candidate(first);
  version.mark_uploaded(first);
  assert(version.initialized());
  assert(version.generation() == 1U);
  assert(version.digest() == first);
  assert(!version.needs_upload(first));
  assert(version.needs_upload(second));

  // Idempotent observation does not manufacture a new renderer generation.
  version.mark_uploaded(first);
  assert(version.generation() == 1U);

  version.mark_uploaded(second);
  assert(version.generation() == 2U);
  assert(version.digest() == second);

  // Invalid commits fail transactionally and preserve the last good state.
  for (const std::string &invalid : {std::string{}, std::string(63U, '0'),
                                    std::string(64U, 'A'),
                                    std::string(64U, 'g')}) {
    bool rejected = false;
    try {
      version.validate_candidate(invalid);
      version.mark_uploaded(invalid);
    } catch (const std::invalid_argument &) {
      rejected = true;
    }
    assert(rejected);
    assert(version.generation() == 2U);
    assert(version.digest() == second);
  }

  assert(ac6demo::RendererPayloadVersion::valid_sha256(first));
  assert(ac6demo::RendererPayloadVersion::valid_sha256(second));
  assert(!ac6demo::RendererPayloadVersion::valid_sha256("deadbeef"));

  version.reset();
  assert(!version.initialized());
  assert(version.generation() == 0U);
  assert(version.digest().empty());
  assert(version.needs_upload(first));
  return 0;
}
