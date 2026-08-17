#pragma once

#include <cstdint>
#include <vector>

struct GuestNotification final {
  std::uint32_t id{};
  std::uint32_t param{};
};
struct GuestNotifyListener final {
  std::uint64_t mask{};
  std::uint32_t max_version{};
  // XNotifyGetNext: "Only one notification is stored for each notification ID.
  // Therefore, pParam always represents the most recent state for a given
  // notification ID." So this is keyed by id, not a plain queue; insertion
  // order is kept only to make delivery deterministic, which the XDK does not
  // require ("Notifications may not be returned in the same order as they are
  // received").
  std::vector<GuestNotification> pending{};
};
