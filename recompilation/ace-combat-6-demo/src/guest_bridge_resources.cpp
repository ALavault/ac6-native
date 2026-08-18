#include "ac6demo/guest_bridge.hpp"

#include "ac6demo/content.hpp"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <limits>
#include <tuple>
#include <vector>

namespace ac6demo {

void GuestBridge::enable_function_reachability(bool enabled) noexcept {
  function_reachability_enabled_ = enabled;
  if (!enabled) {
    function_reachability_.clear();
  }
}

void GuestBridge::record_function_entry(std::string_view generated_name) noexcept {
  if (!function_reachability_enabled_) {
    return;
  }
  const auto marker = generated_name.rfind("sub_");
  if (marker == std::string_view::npos || marker + 12U != generated_name.size()) {
    return;
  }
  std::uint32_t address{};
  const auto digits = generated_name.substr(marker + 4U);
  const auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(),
                                      address, 16);
  if (parsed.ec != std::errc{} || parsed.ptr != digits.data() + digits.size() ||
      address < 0x82090000U || address > 0x823767C3U || (address & 3U) != 0U) {
    return;
  }
  auto [position, inserted] = function_reachability_.try_emplace(address);
  auto &record = position->second;
  if (inserted) {
    record.address = address;
    record.first_tick = tick_;
  }
  record.last_tick = tick_;
  ++record.count;
}

std::vector<GuestFunctionReachability>
GuestBridge::function_reachability_snapshot() const {
  std::vector<GuestFunctionReachability> result;
  result.reserve(function_reachability_.size());
  for (const auto &[address, record] : function_reachability_) {
    (void)address;
    result.push_back(record);
  }
  return result;
}

bool read_guest_object_attributes_path(GuestMemory& memory,
                                       std::uint32_t object_attributes,
                                       std::string* path) {
  if (path == nullptr || !memory.mapped(object_attributes, 12U)) {
    return false;
  }
  const auto root_directory = memory.load_u32(object_attributes);
  const auto name_descriptor = memory.load_u32(object_attributes + 4U);
  const auto attributes = memory.load_u32(object_attributes + 8U);
  if (root_directory != 0xFFFFFFFDU || (attributes & ~0x40U) != 0U ||
      !memory.mapped(name_descriptor, 8U)) {
    return false;
  }
  const auto length = memory.load_u16(name_descriptor);
  const auto maximum_length = memory.load_u16(name_descriptor + 2U);
  const auto buffer = memory.load_u32(name_descriptor + 4U);
  if (length >= maximum_length || buffer == 0U || !memory.mapped(buffer, length)) {
    return false;
  }
  path->clear();
  path->reserve(length);
  for (std::uint32_t index = 0U; index < length; ++index) {
    path->push_back(static_cast<char>(memory.load_u8(buffer + index)));
  }
  return true;
}

bool validate_xgi_user_context_request(GuestMemory& memory,
                                       std::uint32_t caller_lr,
                                       std::uint32_t app, std::uint32_t message,
                                       std::uint32_t overlapped,
                                       std::uint32_t buffer,
                                       std::uint32_t length) {
  constexpr std::uint32_t kReachedApp = 0x000000FBU;
  constexpr std::uint32_t kReachedMessage = 0x000B0006U;
  constexpr std::uint32_t kReachedLength = 24U;
  constexpr std::uint32_t kReachedCallerLr = 0x821A55A0U;
  // sub_821A5550 (the sole caller at this LR) builds this 24-byte buffer on
  // its own stack at r1+80 with `stw r11,80(r1)` / `std r11,88(r1)` (zeroing
  // words 2-3) / `stw r10,96(r1)` / `stw r9,100(r1)` -- word index 1
  // (buffer+4, stack offset 84) is never stored to by that function, so it
  // carries whatever this stack slot held before the call, not part of the
  // message. Two live captures confirm this: 0x00000000 and, after forcing
  // menu_endMode's argument to 1, 0x18980054 (report
  // AC6_DEMO_...XAMUSERREADPROFILESETTINGS... follow-up). It is read here
  // for logging parity only and never compared.
  constexpr std::array<std::uint32_t, 6U> kReachedWords{
      0U, 0U, 0U, 0U, 0x00008001U, 0U};
  if (caller_lr != kReachedCallerLr || app != kReachedApp ||
      message != kReachedMessage || overlapped != 0U ||
      length != kReachedLength || buffer == 0U ||
      !memory.mapped(buffer, kReachedLength)) {
    return false;
  }
  for (std::size_t index = 0U; index < kReachedWords.size(); ++index) {
    if (index == 1U) {
      continue;  // uninitialized stack padding; see comment above.
    }
    const auto offset = static_cast<std::uint32_t>(index * 4U);
    if (memory.load_u32(buffer + offset) != kReachedWords[index]) {
      return false;
    }
  }
  return true;
}

void GuestBridge::record_indirect_edge(std::uint32_t thread, std::uint32_t lr,
                                       std::uint32_t target,
                                       GuestRegisterSnapshot registers,
                                       std::optional<GuestVirtualDispatchSnapshot>
                                           virtual_dispatch) {
  const auto key = std::make_tuple(thread, lr, target);
  auto [position, inserted] = indirect_flow_edges_.try_emplace(key);
  auto& edge = position->second;
  if (inserted) {
    edge.kind = GuestControlFlowKind::Indirect;
    edge.thread = thread;
    edge.lr = lr;
    edge.target = target;
    edge.first_tick = tick_;
  }
  edge.last_tick = tick_;
  ++edge.count;
  edge.virtual_dispatch = virtual_dispatch;
  edge.registers = registers;
}

void GuestBridge::record_import_edge(std::uint32_t thread, std::uint32_t lr,
                                     std::string_view module,
                                     std::string_view name,
                                     std::uint16_t ordinal,
                                     GuestRegisterSnapshot registers) {
  const auto key = std::make_tuple(thread, lr, std::string(module), ordinal);
  auto [position, inserted] = import_flow_edges_.try_emplace(key);
  auto& edge = position->second;
  if (inserted) {
    edge.kind = GuestControlFlowKind::Import;
    edge.thread = thread;
    edge.lr = lr;
    edge.module = module;
    edge.name = name;
    edge.ordinal = ordinal;
    edge.first_tick = tick_;
  }
  edge.last_tick = tick_;
  ++edge.count;
  edge.registers = registers;
}

std::vector<GuestControlFlowEdge> GuestBridge::control_flow_snapshot() const {
  std::vector<GuestControlFlowEdge> result;
  result.reserve(indirect_flow_edges_.size() + import_flow_edges_.size());
  for (const auto& [key, edge] : indirect_flow_edges_) {
    (void)key;
    result.push_back(edge);
  }
  for (const auto& [key, edge] : import_flow_edges_) {
    (void)key;
    result.push_back(edge);
  }
  // The backing maps intentionally aggregate hot edges, but their iteration
  // order is unspecified. Sort the snapshot so two clean replays produce the
  // same frontier report independent of the container's bucket layout.
  std::sort(result.begin(), result.end(), [](const GuestControlFlowEdge& left,
                                             const GuestControlFlowEdge& right) {
    return std::tie(left.kind, left.thread, left.lr, left.target, left.module,
                    left.ordinal, left.name) <
           std::tie(right.kind, right.thread, right.lr, right.target,
                    right.module, right.ordinal, right.name);
  });
  return result;
}

std::uint32_t GuestBridge::open_guest_file(std::string_view xbox_path,
                                           std::uint32_t desired_access,
                                           std::uint32_t disposition,
                                           std::uint32_t* handle) {
  constexpr std::uint32_t kStatusSuccess = 0U;
  constexpr std::uint32_t kStatusAccessDenied = 0xC0000022U;
  constexpr std::uint32_t kStatusInvalidParameter = 0xC000000DU;
  constexpr std::uint32_t kStatusObjectNameNotFound = 0xC0000034U;
  constexpr std::uint32_t kFileOpen = 1U;
  constexpr std::uint32_t kWriteAccess =
      0x40000000U | 0x00000002U | 0x00000004U | 0x00000010U | 0x00000100U;
  if (handle == nullptr || vfs_ == nullptr) {
    return kStatusInvalidParameter;
  }
  if (disposition != kFileOpen || (desired_access & kWriteAccess) != 0U) {
    return kStatusAccessDenied;
  }
  const auto path = vfs_->resolve_if_qualified(xbox_path);
  if (!path.has_value()) {
    return kStatusObjectNameNotFound;
  }
  std::error_code error;
  const auto size = std::filesystem::file_size(*path, error);
  if (error || !std::filesystem::is_regular_file(*path, error) || error) {
    return kStatusObjectNameNotFound;
  }
  const auto issued = next_file_handle_;
  next_file_handle_ += 4U;
  files_.emplace(issued, GuestFile{*path, 0U, size});
  *handle = issued;
  return kStatusSuccess;
}

bool GuestBridge::close_guest_file(std::uint32_t handle) noexcept {
  return files_.erase(handle) == 1U;
}

bool GuestBridge::guest_file_size(std::uint32_t handle,
                                  std::uint64_t* size) const noexcept {
  const auto found = files_.find(handle);
  if (found == files_.end() || size == nullptr) {
    return false;
  }
  *size = found->second.size;
  return true;
}

bool GuestBridge::set_guest_file_position(std::uint32_t handle,
                                          std::uint64_t position) noexcept {
  const auto found = files_.find(handle);
  if (found == files_.end()) {
    return false;
  }
  found->second.offset = position;
  return true;
}

std::uint32_t GuestBridge::read_guest_file(
    std::uint32_t handle, std::optional<std::uint64_t> offset,
    std::span<std::byte> output, std::uint32_t* bytes_read) {
  constexpr std::uint32_t kStatusSuccess = 0U;
  constexpr std::uint32_t kStatusInvalidHandle = 0xC0000008U;
  constexpr std::uint32_t kStatusEndOfFile = 0xC0000011U;
  constexpr std::uint32_t kStatusIoDeviceError = 0xC0000185U;
  if (bytes_read == nullptr) {
    return kStatusInvalidHandle;
  }
  *bytes_read = 0U;
  const auto found = files_.find(handle);
  if (found == files_.end()) {
    return kStatusInvalidHandle;
  }
  const auto position = offset.value_or(found->second.offset);
  if (output.empty()) {
    return kStatusSuccess;
  }
  if (position >= found->second.size) {
    return kStatusEndOfFile;
  }
  const auto available = found->second.size - position;
  const auto requested = static_cast<std::uint64_t>(output.size());
  const auto count = static_cast<std::size_t>(std::min(available, requested));
  std::ifstream stream(found->second.path, std::ios::binary);
  if (!stream.is_open() ||
      position > static_cast<std::uint64_t>(
                     std::numeric_limits<std::streamoff>::max())) {
    return kStatusIoDeviceError;
  }
  stream.seekg(static_cast<std::streamoff>(position));
  stream.read(reinterpret_cast<char*>(output.data()),
              static_cast<std::streamsize>(count));
  if (stream.gcount() != static_cast<std::streamsize>(count)) {
    return kStatusIoDeviceError;
  }
  *bytes_read = static_cast<std::uint32_t>(count);
  found->second.offset = position + count;
  return kStatusSuccess;
}

bool GuestBridge::register_xaudio_client(std::uint32_t callback,
                                         std::uint32_t callback_context,
                                         std::uint32_t* handle) noexcept {
  if (callback == 0U || handle == nullptr || xaudio_client_handle_ != 0U) {
    return false;
  }
  xaudio_client_handle_ = 0xE4000000U;
  xaudio_callback_ = callback;
  xaudio_callback_context_ = callback_context;
  *handle = xaudio_client_handle_;
  return true;
}

bool GuestBridge::unregister_xaudio_client(std::uint32_t handle) noexcept {
  if (handle == 0U || handle != xaudio_client_handle_) {
    return false;
  }
  xaudio_client_handle_ = 0U;
  xaudio_callback_ = 0U;
  xaudio_callback_context_ = 0U;
  return true;
}

bool GuestBridge::set_notify_ui_position(std::uint32_t position) noexcept {
  // static_exact: 0x8219BEB4 loads 6 before the reached call at 0x8219BEBC.
  // bridge_observed: the sole tick-220 call also carries r3=6. Other enum
  // values remain unqualified and therefore fail closed.
  if (position != 6U) {
    return false;
  }
  notify_ui_position_ = position;
  return true;
}

std::optional<std::uint32_t> GuestBridge::xam_user_signin_state(
    std::uint32_t user_index) const noexcept {
  // static_exact: 0x8219BA34 guards the title wrapper to user slots 0..3,
  // then 0x8219BA40 calls ordinal 528 and consumes states 1 and 2 explicitly.
  // bridge_observed: the first three calls enumerate slots 0, 0, then 1.
  // The qualified bridge profile owns slot zero only; unused slots are not
  // signed in. Returning Live state 2 would contradict the offline seam.
  if (user_index > 3U) {
    return std::nullopt;
  }
  return user_index == 0U ? 1U : 0U;
}

bool GuestBridge::write_xam_user_name(std::uint32_t user_index,
                                      std::uint32_t destination,
                                      std::uint32_t length) {
  constexpr std::string_view kOfflineUserName = "User";
  constexpr std::uint32_t kReachedBufferLength = 16U;
  if (user_index != 0U || length != kReachedBufferLength ||
      !memory_.mapped(destination, kOfflineUserName.size() + 1U)) {
    return false;
  }
  for (std::size_t index = 0U; index < kOfflineUserName.size(); ++index) {
    memory_.store_u8(destination + static_cast<std::uint32_t>(index),
                     static_cast<std::uint8_t>(kOfflineUserName[index]));
  }
  memory_.store_u8(destination +
                       static_cast<std::uint32_t>(kOfflineUserName.size()),
                   0U);
  return true;
}

std::uint32_t GuestBridge::allocate_address(std::uint32_t size) noexcept {
  const auto rounded = (static_cast<std::uint64_t>(size) + kGuestPageBytes - 1U) &
                       ~(static_cast<std::uint64_t>(kGuestPageBytes) - 1U);
  const auto address = next_allocation_;
  const auto next = static_cast<std::uint64_t>(address) + rounded;
  if (next > 0x80000000ULL) {
    return 0U;
  }
  next_allocation_ = static_cast<std::uint32_t>(next);
  return address;
}

std::uint32_t GuestBridge::ensure_xma_context_array() {
  // Xenia/ReXGlue-generic XMA_CONTEXT_DATA is 64 bytes and the generic
  // decoder reserves 320 contiguous contexts. This helper is deliberately
  // reachable only from the environment-gated PAL experiment in
  // audio_memory_dispatch.hpp; it is not a production audio implementation.
  constexpr std::uint32_t kContextBytes = 64U;
  constexpr std::uint32_t kContextCount = 320U;
  constexpr std::uint32_t kArrayBytes = kContextBytes * kContextCount;
  if (xma_context_array_address_ == 0U) {
    const auto array = allocate_address(kArrayBytes);
    if (array == 0U) {
      return 0U;
    }
    memory_.map_zero(array, kArrayBytes);
    record_allocation(array, kArrayBytes);
    xma_context_array_address_ = array;
  }
  if (!memory_.mapped(xma_context_array_address_, kArrayBytes) ||
      !owns_allocation(xma_context_array_address_, kArrayBytes)) {
    return 0U;
  }
  return xma_context_array_address_;
}

std::uint32_t GuestBridge::allocate_xma_context() {
  constexpr std::uint32_t kContextBytes = 64U;
  constexpr std::uint32_t kContextCount = 320U;
  const auto array = ensure_xma_context_array();
  if (array == 0U || xma_context_next_index_ >= kContextCount) {
    return 0U;
  }
  const auto context = array +
                       xma_context_next_index_ * kContextBytes;
  xma_context_active_[xma_context_next_index_] = true;
  ++xma_context_next_index_;
  return context;
}

bool GuestBridge::release_xma_context(std::uint32_t context) {
  constexpr std::uint32_t kContextBytes = 64U;
  constexpr std::uint32_t kContextCount = 320U;
  if (xma_context_array_address_ == 0U ||
      context < xma_context_array_address_) {
    return false;
  }
  const auto delta = context - xma_context_array_address_;
  if (delta % kContextBytes != 0U) {
    return false;
  }
  const auto index = delta / kContextBytes;
  if (index >= kContextCount || index >= xma_context_next_index_ ||
      !xma_context_active_[index] || !memory_.mapped(context, kContextBytes) ||
      !owns_allocation(context, kContextBytes)) {
    return false;
  }
  for (std::uint32_t offset = 0U; offset < kContextBytes; offset += 4U) {
    memory_.store_u32(context + offset, 0U);
  }
  xma_context_active_[index] = false;
  return true;
}

bool GuestBridge::owns_allocation(std::uint32_t address, std::size_t size) const noexcept {
  const auto end = static_cast<std::uint64_t>(address) + size;
  for (const auto& allocation : allocations_) {
    const auto allocation_end = static_cast<std::uint64_t>(allocation.address) +
                                allocation.size;
    if (address >= allocation.address && end <= allocation_end) {
      return true;
    }
  }
  return false;
}

void GuestBridge::record_allocation(std::uint32_t address, std::size_t size) {
  if (size == 0U) {
    return;
  }
  const auto end = static_cast<std::uint64_t>(address) + size;
  if (end > ac6demo::kGuestMemoryBytes) {
    throw std::invalid_argument("guest allocation exceeds 4 GiB");
  }

  allocations_.push_back(Allocation{address, size});
  std::sort(allocations_.begin(), allocations_.end(),
            [](const Allocation& left, const Allocation& right) {
              return left.address < right.address;
            });
  std::vector<Allocation> merged;
  merged.reserve(allocations_.size());
  for (const auto& allocation : allocations_) {
    if (merged.empty()) {
      merged.push_back(allocation);
      continue;
    }
    auto& previous = merged.back();
    const auto previous_end = static_cast<std::uint64_t>(previous.address) +
                              previous.size;
    const auto allocation_end = static_cast<std::uint64_t>(allocation.address) +
                                allocation.size;
    if (static_cast<std::uint64_t>(allocation.address) <= previous_end) {
      const auto merged_end = std::max(previous_end, allocation_end);
      previous.size = static_cast<std::size_t>(merged_end - previous.address);
    } else {
      merged.push_back(allocation);
    }
  }
  allocations_.swap(merged);
}

}  // namespace ac6demo
