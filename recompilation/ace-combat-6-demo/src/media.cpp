#include "ac6demo/media.hpp"

#include <algorithm>

namespace ac6demo {

bool XmaAudioService::allowed_pack(std::string_view pack, DemoLanguage language) noexcept {
  if (pack == "bgmpack.bin") {
    return true;
  }
  if (language == DemoLanguage::English) {
    return pack == "demopack_eng.bin" || pack == "voicepack_eng.bin";
  }
  return pack == "demopack_jpn.bin" || pack == "voicepack_jpn.bin";
}

void XmaAudioService::submit(const XmaFrame& frame) {
  if (!allowed_pack(frame.pack, frame.language) || frame.compressed.empty() || !decoder_ ||
      !sink_) {
    throw RuntimeTrap("unqualified XMA packet or missing FFmpeg/SDL3 boundary", frame.timestamp);
  }
  ++stats_.submitted;
  stats_.bytes += frame.compressed.size();
  stats_.last_timestamp = frame.timestamp;
  decoder_(frame);
  sink_(frame);
  ++stats_.decoded;
}

}  // namespace ac6demo
