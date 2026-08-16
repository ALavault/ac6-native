#include "ac6demo/emu_agent_ipc.hpp"

#include "ac6demo/content.hpp"
#include "ac6demo/session.hpp"

#include <array>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

#include <sys/socket.h>
#include <unistd.h>

namespace ac6demo {
namespace {

constexpr std::size_t kMaxFrameBytes = 64U * 1024U;

[[nodiscard]] bool read_all(int fd, std::byte *output, std::size_t length) {
  while (length != 0U) {
    const auto count = ::recv(fd, output, length, 0);
    if (count <= 0) {
      return false;
    }
    output += count;
    length -= static_cast<std::size_t>(count);
  }
  return true;
}

[[nodiscard]] bool write_all(int fd, const std::byte *input, std::size_t length) {
  while (length != 0U) {
    const auto count = ::send(fd, input, length, MSG_NOSIGNAL);
    if (count <= 0) {
      return false;
    }
    input += count;
    length -= static_cast<std::size_t>(count);
  }
  return true;
}

[[nodiscard]] bool read_frame(int fd, std::string &message) {
  std::array<std::byte, 4U> prefix{};
  if (!read_all(fd, prefix.data(), prefix.size())) {
    return false;
  }
  std::uint32_t length{};
  for (const auto byte : prefix) {
    length = (length << 8U) | static_cast<std::uint8_t>(byte);
  }
  if (length == 0U || length > kMaxFrameBytes) {
    return false;
  }
  message.resize(length);
  return read_all(fd, reinterpret_cast<std::byte *>(message.data()), length);
}

[[nodiscard]] bool write_frame(int fd, std::string_view message) {
  if (message.empty() || message.size() > kMaxFrameBytes) {
    return false;
  }
  const auto length = static_cast<std::uint32_t>(message.size());
  const std::array<std::byte, 4U> prefix{
      std::byte((length >> 24U) & 0xffU), std::byte((length >> 16U) & 0xffU),
      std::byte((length >> 8U) & 0xffU), std::byte(length & 0xffU)};
  return write_all(fd, prefix.data(), prefix.size()) &&
         write_all(fd, reinterpret_cast<const std::byte *>(message.data()), message.size());
}

[[nodiscard]] bool token_is_safe(std::string_view token) {
  return !token.empty() && token.size() <= 64U &&
         token.find_first_not_of("0123456789abcdef") == std::string_view::npos;
}

[[nodiscard]] bool as_unsigned(const std::ssub_match &match,
                               std::uint64_t maximum, std::uint64_t *output) {
  if (output == nullptr || !match.matched) {
    return false;
  }
  std::uint64_t value{};
  const auto text = match.str();
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
  if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() ||
      value > maximum) {
    return false;
  }
  *output = value;
  return true;
}

[[nodiscard]] bool as_signed(const std::ssub_match &match, std::int64_t minimum,
                             std::int64_t maximum, std::int64_t *output) {
  if (output == nullptr || !match.matched) {
    return false;
  }
  std::int64_t value{};
  const auto text = match.str();
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
  if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() ||
      value < minimum || value > maximum) {
    return false;
  }
  *output = value;
  return true;
}

struct Request final {
  enum class Operation { Start, Step, Observe, Stop } operation;
  std::string token;
  InputFrame input{};
};

[[nodiscard]] bool parse_request(const std::string &message, Request *output) {
  if (output == nullptr) {
    return false;
  }
  static const std::regex simple(
      R"json(^\{"op":"(start|observe|stop)","token":"([0-9a-f]{1,64})"\}$)json");
  static const std::regex step(
      R"json(^\{"op":"step","token":"([0-9a-f]{1,64})","xinput":\{"buttons":([0-9]+),"connected":(true|false),"left_stick":\{"x":(-?[0-9]+),"y":(-?[0-9]+)\},"left_trigger":([0-9]+),"right_stick":\{"x":(-?[0-9]+),"y":(-?[0-9]+)\},"right_trigger":([0-9]+)\}\}$)json");
  std::smatch match;
  if (std::regex_match(message, match, simple)) {
    output->token = match[2].str();
    const auto operation = match[1].str();
    output->operation = operation == "start" ? Request::Operation::Start
                      : operation == "observe" ? Request::Operation::Observe
                                               : Request::Operation::Stop;
    return token_is_safe(output->token);
  }
  if (!std::regex_match(message, match, step)) {
    return false;
  }
  std::uint64_t buttons{}, left_trigger{}, right_trigger{};
  std::int64_t left_x{}, left_y{}, right_x{}, right_y{};
  if (!as_unsigned(match[2], 0xffffU, &buttons) ||
      !as_unsigned(match[6], 0xffU, &left_trigger) ||
      !as_unsigned(match[9], 0xffU, &right_trigger) ||
      !as_signed(match[4], -32768, 32767, &left_x) ||
      !as_signed(match[5], -32768, 32767, &left_y) ||
      !as_signed(match[7], -32768, 32767, &right_x) ||
      !as_signed(match[8], -32768, 32767, &right_y)) {
    return false;
  }
  output->operation = Request::Operation::Step;
  output->token = match[1].str();
  output->input = InputFrame{static_cast<std::uint16_t>(buttons),
                             static_cast<std::uint8_t>(left_trigger),
                             static_cast<std::uint8_t>(right_trigger),
                             static_cast<std::int16_t>(left_x),
                             static_cast<std::int16_t>(left_y),
                             static_cast<std::int16_t>(right_x),
                             static_cast<std::int16_t>(right_y),
                             match[3].str() == "true"};
  return token_is_safe(output->token);
}

[[nodiscard]] std::string error_response(std::string_view token,
                                         std::string_view error) {
  return "{\"error\":\"" + std::string(error) + "\",\"ok\":false,\"token\":\"" +
         std::string(token) + "\"}";
}

[[nodiscard]] std::string ok_response(std::string_view token,
                                      const DemoSession *session,
                                      bool observation) {
  if (!observation) {
    return "{\"ok\":true,\"token\":\"" + std::string(token) + "\"}";
  }
  const auto tick = session == nullptr ? 0U : session->tick();
  const auto present = session == nullptr ? 0U : session->graphics_present_count();
  return "{\"ok\":true,\"present\":" + std::to_string(present) +
         ",\"tick\":" + std::to_string(tick) + ",\"token\":\"" +
         std::string(token) + "\"}";
}

} // namespace

int run_emu_agent_ipc_impl(int fd, bool frontend_only) {
  if (fd < 0) {
    return 2;
  }
  std::unique_ptr<DemoSession> session;
  std::filesystem::path trace;
  std::string token;
  int result = 0;
  while (true) {
    std::string message;
    if (!read_frame(fd, message)) {
      result = 2;
      break;
    }
    Request request{};
    if (!parse_request(message, &request) || (!token.empty() && request.token != token)) {
      static_cast<void>(write_frame(fd, error_response(token, "invalid IPC request")));
      result = 2;
      break;
    }
    try {
      if (request.operation == Request::Operation::Start) {
        if (!token.empty() || session != nullptr) {
          throw std::runtime_error("IPC session already started");
        }
        token = request.token;
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        trace = std::filesystem::temp_directory_path() /
                ("ac6-demo-emu-agent-" + std::to_string(stamp) + ".jsonl");
        session = std::make_unique<DemoSession>(DemoStore::default_path(), GraphicsBackend::Headless);
        session->start(trace, frontend_only);
        if (!write_frame(fd, ok_response(token, session.get(), false))) {
          result = 2;
          break;
        }
      } else if (request.operation == Request::Operation::Step) {
        if (session == nullptr) {
          throw std::runtime_error("IPC session is not started");
        }
        session->step(request.input);
        if (!write_frame(fd, ok_response(token, session.get(), true))) {
          result = 2;
          break;
        }
      } else if (request.operation == Request::Operation::Observe) {
        if (session == nullptr) {
          throw std::runtime_error("IPC session is not started");
        }
        if (!write_frame(fd, ok_response(token, session.get(), true))) {
          result = 2;
          break;
        }
      } else {
        if (session != nullptr) {
          session->stop();
        }
        static_cast<void>(write_frame(fd, ok_response(token, session.get(), false)));
        break;
      }
    } catch (const std::exception &error) {
      static_cast<void>(write_frame(fd, error_response(token, error.what())));
      result = 3;
      break;
    }
  }
  if (session != nullptr) {
    session->stop();
  }
  if (!trace.empty()) {
    std::error_code error;
    std::filesystem::remove(trace, error);
  }
  ::close(fd);
  return result;
}

int run_emu_agent_ipc(int fd) { return run_emu_agent_ipc_impl(fd, false); }

int run_emu_agent_ipc_frontend_test(int fd) {
  return run_emu_agent_ipc_impl(fd, true);
}

std::optional<int> maybe_run_emu_agent_ipc(int argc, char **argv) {
  if (argc != 3 || std::string_view(argv[1]) != "--emu-agent-ipc-fd") {
    return std::nullopt;
  }
  int fd{};
  const auto text = std::string_view(argv[2]);
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), fd);
  if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || fd < 0) {
    return 2;
  }
  return run_emu_agent_ipc(fd);
}

} // namespace ac6demo
