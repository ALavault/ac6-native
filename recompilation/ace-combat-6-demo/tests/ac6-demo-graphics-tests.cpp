#include "ac6demo/graphics.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <iostream>

int main() {
  const std::array<std::uint32_t, 6> fetch{0x8A000002U, 0x1374A006U,
                                           0x0059E4FFU, 0x00001414U,
                                           0x00000000U, 0x00000200U};
  const auto swap =
      ac6demo::make_xenos_swap_packet(fetch, 0x1374A000U, 1280U, 720U);
  assert(swap[0] == 0x00054800U);
  assert(std::equal(fetch.begin(), fetch.end(), swap.begin() + 1));
  assert(swap[7] == 0xC0036400U);
  assert(swap[8] == 0x53574150U);
  assert(swap[9] == 0x1374A000U);
  assert(swap[10] == 1280U);
  assert(swap[11] == 720U);
  assert(swap[12] == 0x80000000U);
  assert(swap.back() == 0x80000000U);

  ac6demo::GraphicsProfile profile{ac6demo::GraphicsBackend::Headless};
  ac6demo::D3D9LtcgHle hle(profile);
  hle.begin_frame(0U);
  hle.set_render_state(7U, 1U);
  hle.create_resource(1U, ac6demo::XenosFormat::Rgba8, 1280U, 720U);
  const std::array<std::uint32_t, 2> shader = {0x01000000U, 0x02000000U};
  hle.audit_shader(2U, shader);
  hle.clear(0U);
  hle.draw(3U, 0U);
  hle.resolve(1U);
  hle.present(2U);
  assert(hle.stats().presents == 1U);

  ac6demo::VulkanBackend vulkan(
      ac6demo::GraphicsProfile{ac6demo::GraphicsBackend::Vulkan});
  vulkan.submit(hle);
  assert(vulkan.validated_presents() == 1U);

  bool rejected = false;
  try {
    hle.begin_frame(4U);
    hle.audit_shader(3U, std::array<std::uint32_t, 1>{0xff000000U});
  } catch (const ac6demo::RuntimeTrap &) {
    rejected = true;
  }
  assert(rejected);
  std::cout << "ac6-demo-graphics-tests: ok\n";
  return 0;
}
