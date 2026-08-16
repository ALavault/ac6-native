#pragma once

#include "ac6demo/runtime_error.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string_view>

namespace ac6demo {

enum class DemoLanguage : std::uint8_t { English, Japanese };

struct XmaFrame final {
  std::string_view pack;
  DemoLanguage language{DemoLanguage::English};
  std::uint64_t timestamp{};
  std::int16_t volume_db{};
  std::span<const std::byte> compressed;
};

struct MediaStats final {
  std::uint64_t submitted{};
  std::uint64_t decoded{};
  std::uint64_t bytes{};
  std::uint64_t last_timestamp{};
};

class XmaAudioService final {
 public:
  using Decode = std::function<void(const XmaFrame&)>;
  using Submit = std::function<void(const XmaFrame&)>;

  XmaAudioService(Decode decoder, Submit sink)
      : decoder_(std::move(decoder)), sink_(std::move(sink)) {}

  void submit(const XmaFrame& frame);
  [[nodiscard]] const MediaStats& stats() const noexcept { return stats_; }

 private:
  static bool allowed_pack(std::string_view pack, DemoLanguage language) noexcept;

  Decode decoder_;
  Submit sink_;
  MediaStats stats_{};
};

// The concrete FFmpeg/SDL3 adapters are injected at the boundary. The core
// has no silent PCM fallback and can therefore be tested without those host
// libraries while keeping their buffers and timestamps explicit.
class FfmpegXmaDecoder final {
 public:
  explicit FfmpegXmaDecoder(XmaAudioService::Decode decode) : decode_(std::move(decode)) {}
  void operator()(const XmaFrame& frame) const { decode_(frame); }

 private:
  XmaAudioService::Decode decode_;
};

class Sdl3AudioSink final {
 public:
  explicit Sdl3AudioSink(XmaAudioService::Submit submit) : submit_(std::move(submit)) {}
  void operator()(const XmaFrame& frame) const { submit_(frame); }

 private:
  XmaAudioService::Submit submit_;
};

}  // namespace ac6demo
