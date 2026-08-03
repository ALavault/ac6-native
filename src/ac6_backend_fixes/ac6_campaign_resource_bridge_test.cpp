#include "../ac6_campaign_resource_bridge.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <vector>

namespace {

void StoreBe16(uint8_t* p, uint16_t value) {
  p[0] = uint8_t(value >> 8);
  p[1] = uint8_t(value);
}

void StoreBe32(uint8_t* p, uint32_t value) {
  p[0] = uint8_t(value >> 24);
  p[1] = uint8_t(value >> 16);
  p[2] = uint8_t(value >> 8);
  p[3] = uint8_t(value);
}

std::array<uint8_t, 128> CampaignFhm() {
  std::array<uint8_t, 128> data{};
  data[0] = 'F';
  data[1] = 'H';
  data[2] = 'M';
  data[3] = ' ';
  data[5] = 1;
  StoreBe16(data.data() + 6, 0x10);
  StoreBe32(data.data() + 0x10, 5);
  constexpr uint32_t offsets[5] = {0x70, 0x5F720, 0x5F7A0, 0x5F7A0,
                                    0x60000};
  constexpr uint32_t sizes[5] = {0x5F6A6, 0x80, 0, 0, 0xEB000};
  for (uint32_t i = 0; i < 5; ++i) {
    StoreBe32(data.data() + 0x14 + i * 4, offsets[i]);
    StoreBe32(data.data() + 0x14 + (5 + i) * 4, sizes[i]);
  }
  return data;
}

}  // namespace

int main() {
  assert(ac6::campaign_resource::ShouldSelectFirstCampaignMission(0, 1, 0));
  assert(!ac6::campaign_resource::ShouldSelectFirstCampaignMission(1, 1, 0));
  assert(!ac6::campaign_resource::ShouldSelectFirstCampaignMission(0, 2, 0));
  assert(!ac6::campaign_resource::ShouldSelectFirstCampaignMission(0, 1, 1));

  constexpr uint32_t parent = 0xB8CD0000;
  constexpr uint32_t leaf = 0xB8D30000;
  auto data = CampaignFhm();

  const auto match = ac6::campaign_resource::InspectFhmParent(
      data.data(), data.size(), parent, leaf, 0xEB000);
  assert(match.valid);
  assert(match.member_count == 5);
  assert(match.size == 0x14B000);

  data[0] = 'X';
  assert(!ac6::campaign_resource::InspectFhmParent(
              data.data(), data.size(), parent, leaf, 0xEB000)
              .valid);
  data = CampaignFhm();
  assert(!ac6::campaign_resource::InspectFhmParent(
              data.data(), data.size(), parent, leaf, 0xEA000)
              .valid);
  StoreBe32(data.data() + 0x14 + 6 * 4, 0);
  assert(!ac6::campaign_resource::InspectFhmParent(
              data.data(), data.size(), parent, leaf, 0xEB000)
              .valid);

  std::array<uint8_t, 0x1080> wrapped{};
  wrapped[0] = 'F';
  wrapped[1] = 'H';
  wrapped[2] = 'M';
  wrapped[3] = ' ';
  wrapped[5] = 1;
  StoreBe16(wrapped.data() + 6, 0x10);
  StoreBe32(wrapped.data() + 0x10, 1);
  StoreBe32(wrapped.data() + 0x14, 0x1000);
  StoreBe32(wrapped.data() + 0x18, 0x14B000);
  data = CampaignFhm();
  StoreBe32(data.data() + 0x14 + 8 * 4, 4);
  for (std::size_t i = 0; i < data.size(); ++i) {
    wrapped[0x1000 + i] = data[i];
  }
  const auto wrapper = ac6::campaign_resource::InspectCampaignWrapper(
      wrapped.data(), 0x14C000);
  assert(wrapper.valid);
  assert(wrapper.inner_offset == 0x1000);
  assert(wrapper.inner_size == 0x14B000);
  assert(wrapper.inner_member_count == 5);

  StoreBe32(wrapped.data() + 0x1000 + 0x14 + 8 * 4, 0);
  assert(!ac6::campaign_resource::InspectCampaignWrapper(
              wrapped.data(), 0x14C000)
              .valid);

  std::vector<uint8_t> ntxr(0x104, 0);
  ntxr[0] = 'N';
  ntxr[1] = 'T';
  ntxr[2] = 'X';
  ntxr[3] = 'R';
  ntxr[4] = 2;
  ntxr[5] = 0x0C;
  ntxr[7] = 1;
  assert(ac6::campaign_resource::HasEmptyNtxrDirectory(ntxr.data(),
                                                        ntxr.size()));
  ntxr[0x103] = 1;
  assert(!ac6::campaign_resource::HasEmptyNtxrDirectory(ntxr.data(),
                                                         ntxr.size()));

  return 0;
}
