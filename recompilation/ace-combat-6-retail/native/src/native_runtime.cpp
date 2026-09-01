#include "ac6/native_runtime.h"

#include <algorithm>
#include <string>
#include <utility>

namespace ac6::native {

std::unique_ptr<NativeRuntime> NativeRuntime::open(
    const std::filesystem::path& media, std::filesystem::path user_data) {
  const std::optional<MediaInput> parsed = parse_media_argument(media);
  if (!parsed.has_value()) return nullptr;
  return std::unique_ptr<NativeRuntime>(
      new NativeRuntime(*parsed, std::move(user_data)));
}

NativeRuntime::NativeRuntime(MediaInput media, std::filesystem::path user_data)
    : media_(std::move(media)),
      user_data_(std::move(user_data)),
      bus_(),
      bridge_(bus_),
      xenos_state_(),
      backend_(),
      guest_address_space_(),
      guest_memory_(16u * 1024u * 1024u) {}

NativeRuntime::~NativeRuntime() = default;

void NativeRuntime::bind_guest_vd() noexcept {
  native_guest_vd_service().bind(guest_address_space_.base(), bus_, bridge_,
                                  xenos_state_, backend_);
}

void NativeRuntime::fail(std::string message) noexcept {
  diagnostics_.state = RuntimeState::kFailed;
  diagnostics_.error = std::move(message);
}

bool NativeRuntime::boot() {
  if (diagnostics_.state != RuntimeState::kCreated) return false;
  std::error_code error;
  if ((media_.kind == MediaKind::kIso &&
       !std::filesystem::is_regular_file(media_.path, error)) ||
      (media_.kind == MediaKind::kAssetsDirectory &&
       !std::filesystem::is_directory(media_.path, error)) || error) {
    fail("media disappeared before boot");
    return false;
  }
  native_guest_media_service().bind(media_);
  XexImage image;
  XexMetadata metadata;
  if (media_.kind == MediaKind::kAssetsDirectory) {
    std::string xex_error;
    if (!load_xex_image_file(media_.path / "default.xex", image, &xex_error)) {
      fail("assets/default.xex is not qualified: " + xex_error);
      return false;
    }
    metadata = image.metadata;
  } else {
    std::vector<std::uint8_t> xex;
    std::string xdvdfs_error;
    if (!read_xdvdfs_file(media_.path, "default.xex", xex,
                          16u * 1024u * 1024u, nullptr, &xdvdfs_error) ||
        !load_xex_image_bytes(xex, image, &xdvdfs_error)) {
      fail("ISO default.xex is not qualified: " + xdvdfs_error);
      return false;
    }
    metadata = image.metadata;
  }
  if (!guest_address_space_.valid()) {
    fail("Xenon 32-bit guest address space is unavailable");
    return false;
  }
  if (!guest_address_space_.write(metadata.load_address, image.image)) {
    fail("decoded XEX image does not fit guest address space");
    return false;
  }
  guest_image_ = std::move(image.image);
  xex_metadata_ = metadata;
  saves_ = std::make_unique<AtomicSaveStore>(user_data_);
  if (user_data_.empty() || !saves_->valid()) {
    fail("user data root is invalid");
    return false;
  }
  diagnostics_.state = RuntimeState::kBooted;
  return true;
}

bool NativeRuntime::attach_replay(std::vector<ReplaySample> samples) {
  if (diagnostics_.state != RuntimeState::kBooted &&
      diagnostics_.state != RuntimeState::kRunning) {
    return false;
  }
  replay_ = std::make_unique<StrictInputReplay>(std::move(samples));
  return true;
}

bool NativeRuntime::submit_ring(std::span<const std::uint32_t> ring_words) {
  if ((diagnostics_.state != RuntimeState::kBooted &&
       diagnostics_.state != RuntimeState::kRunning) || ring_words.empty() ||
      ring_words.size() > (1u << 18u)) {
    return false;
  }
  const std::uint64_t ring_bytes = static_cast<std::uint64_t>(ring_words.size()) * 4u;
  std::uint32_t ring_size = 256u;
  while (ring_size <= ring_bytes && ring_size < (1u << 20u)) ring_size <<= 1u;
  if (ring_bytes >= ring_size || !bus_.write(MmioBus::kRingBase, 0x1000u) ||
      !bus_.write(MmioBus::kRingSize, ring_size) ||
      !bus_.write(MmioBus::kRingRead, 0u) ||
      !bus_.write(MmioBus::kRingWrite, static_cast<std::uint32_t>(ring_bytes))) {
    fail("ring setup is outside Xenos MMIO contract");
    return false;
  }
  std::vector<std::uint32_t> ring(ring_size / 4u, 0u);
  std::copy(ring_words.begin(), ring_words.end(), ring.begin());
  bridge_.set_ring_words(ring);
  std::vector<XenosCommand> commands;
  const DecodeResult decoded = bridge_.pump(xenos_state_, commands);
  if (!decoded.ok() || !backend_.submit(xenos_state_, commands)) {
    fail(decoded.ok() ? backend_.error() : decoded.error.detail);
    return false;
  }
  if (backend_.present_count() != diagnostics_.presented_frames) {
    diagnostics_.presented_frames = backend_.present_count();
    diagnostics_.state = RuntimeState::kRunning;
  }
  return true;
}

ServiceError NativeRuntime::poll_input(std::uint64_t tick, std::uint32_t user,
                                       ReplaySample& sample) noexcept {
  diagnostics_.guest_ticks = tick;
  if (diagnostics_.state != RuntimeState::kBooted &&
      diagnostics_.state != RuntimeState::kRunning) {
    return ServiceError::kBusy;
  }
  if (!replay_) return ServiceError::kPollMismatch;
  return replay_->poll(tick, user, sample);
}

ServiceError NativeRuntime::save(std::string_view name,
                                 std::span<const std::uint8_t> bytes) {
  if (!saves_ || (diagnostics_.state != RuntimeState::kBooted &&
                  diagnostics_.state != RuntimeState::kRunning)) {
    return ServiceError::kBusy;
  }
  return saves_->write(name, bytes);
}

ServiceError NativeRuntime::load(std::string_view name,
                                 std::vector<std::uint8_t>& bytes) const {
  if (!saves_ || (diagnostics_.state != RuntimeState::kBooted &&
                  diagnostics_.state != RuntimeState::kRunning)) {
    return ServiceError::kBusy;
  }
  return saves_->read(name, bytes);
}

bool NativeRuntime::shutdown() noexcept {
  if (diagnostics_.state == RuntimeState::kStopped) return true;
  if (diagnostics_.state == RuntimeState::kFailed) return false;
  if (replay_ && replay_->finalize() != ServiceError::kNone) {
    diagnostics_.state = RuntimeState::kFailed;
    diagnostics_.error = "input replay incomplete at shutdown";
    return false;
  }
  diagnostics_.state = RuntimeState::kStopped;
  return true;
}

}  // namespace ac6::native
