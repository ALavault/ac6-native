#include "ac6demo_native/ipc.hpp"

#include "ac6demo_native/platform.hpp"

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <regex>
#include <string>
#include <string_view>

#if defined(__unix__) || defined(__APPLE__)
#include <cerrno>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace ac6demo_native {
namespace {

constexpr std::size_t kMaxFrameBytes = 64U * 1024U;

#if defined(__unix__) || defined(__APPLE__)
bool read_all(int fd, std::byte* output, std::size_t length) {
    while (length != 0U) {
        const ssize_t count = ::recv(fd, output, length, 0);
        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            return false;
        }
        output += count;
        length -= static_cast<std::size_t>(count);
    }
    return true;
}

bool write_all(int fd, const std::byte* input, std::size_t length) {
    while (length != 0U) {
        const ssize_t count = ::send(fd, input, length, MSG_NOSIGNAL);
        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            return false;
        }
        input += count;
        length -= static_cast<std::size_t>(count);
    }
    return true;
}

bool read_frame(int fd, std::string* message) {
    std::array<std::byte, 4U> prefix{};
    if (message == nullptr || !read_all(fd, prefix.data(), prefix.size())) {
        return false;
    }
    std::uint32_t length = 0U;
    for (const auto byte : prefix) {
        length = (length << 8U) | std::to_integer<std::uint8_t>(byte);
    }
    if (length == 0U || length > kMaxFrameBytes) {
        return false;
    }
    message->assign(length, '\0');
    return read_all(fd, reinterpret_cast<std::byte*>(message->data()), length);
}

bool write_frame(int fd, std::string_view message) {
    if (message.empty() || message.size() > kMaxFrameBytes) {
        return false;
    }
    const auto length = static_cast<std::uint32_t>(message.size());
    const std::array<std::byte, 4U> prefix{
        std::byte((length >> 24U) & 0xffU), std::byte((length >> 16U) & 0xffU),
        std::byte((length >> 8U) & 0xffU), std::byte(length & 0xffU)};
    return write_all(fd, prefix.data(), prefix.size()) &&
           write_all(fd, reinterpret_cast<const std::byte*>(message.data()), message.size());
}
#endif

bool token_safe(std::string_view token) {
    return !token.empty() && token.size() <= 64U &&
           token.find_first_not_of("0123456789abcdef") == std::string_view::npos;
}

template <typename T>
bool parse_integer(const std::ssub_match& match, T minimum, T maximum, T* output) {
    if (output == nullptr || !match.matched) {
        return false;
    }
    const std::string value = match.str();
    T parsed_value{};
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), parsed_value);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() ||
        parsed_value < minimum || parsed_value > maximum) {
        return false;
    }
    *output = parsed_value;
    return true;
}

struct Request {
    enum class Operation { start, step, observe, stop } operation{};
    std::string token;
    XInputFrame input{};
};

bool parse_request(std::string_view message, Request* output) {
    static const std::regex simple(
        R"json(^\{"op":"(start|observe|stop)","token":"([0-9a-f]{1,64})"\}$)json");
    static const std::regex step(
        R"json(^\{"op":"step","token":"([0-9a-f]{1,64})","xinput":\{"buttons":([0-9]+),"connected":(true|false),"left_stick":\{"x":(-?[0-9]+),"y":(-?[0-9]+)\},"left_trigger":([0-9]+),"right_stick":\{"x":(-?[0-9]+),"y":(-?[0-9]+)\},"right_trigger":([0-9]+)\}\}$)json");
    if (output == nullptr) {
        return false;
    }
    std::smatch match;
    const std::string owned(message);
    if (std::regex_match(owned, match, simple)) {
        output->token = match[2].str();
        output->operation = match[1].str() == "start" ? Request::Operation::start
            : match[1].str() == "observe" ? Request::Operation::observe
                                           : Request::Operation::stop;
        return token_safe(output->token);
    }
    if (!std::regex_match(owned, match, step) || !token_safe(match[1].str())) {
        return false;
    }
    std::uint64_t buttons = 0U;
    std::uint64_t left_trigger = 0U;
    std::uint64_t right_trigger = 0U;
    std::int64_t left_x = 0;
    std::int64_t left_y = 0;
    std::int64_t right_x = 0;
    std::int64_t right_y = 0;
    if (!parse_integer(match[2], std::uint64_t{0U}, std::uint64_t{0xffffU}, &buttons) ||
        !parse_integer(match[6], std::uint64_t{0U}, std::uint64_t{0xffU}, &left_trigger) ||
        !parse_integer(match[9], std::uint64_t{0U}, std::uint64_t{0xffU}, &right_trigger) ||
        !parse_integer(match[4], static_cast<std::int64_t>(-32768),
                       static_cast<std::int64_t>(32767), &left_x) ||
        !parse_integer(match[5], static_cast<std::int64_t>(-32768),
                       static_cast<std::int64_t>(32767), &left_y) ||
        !parse_integer(match[7], static_cast<std::int64_t>(-32768),
                       static_cast<std::int64_t>(32767), &right_x) ||
        !parse_integer(match[8], static_cast<std::int64_t>(-32768),
                       static_cast<std::int64_t>(32767), &right_y)) {
        return false;
    }
    output->operation = Request::Operation::step;
    output->token = match[1].str();
    output->input = {static_cast<std::uint16_t>(buttons),
                     static_cast<std::uint8_t>(left_trigger),
                     static_cast<std::uint8_t>(right_trigger),
                     static_cast<std::int16_t>(left_x),
                     static_cast<std::int16_t>(left_y),
                     static_cast<std::int16_t>(right_x),
                     static_cast<std::int16_t>(right_y), match[3].str() == "true"};
    return true;
}

std::string error_response(std::string_view token, std::string_view message) {
    return "{\"error\":\"" + std::string(message) + "\",\"ok\":false,\"token\":\"" +
           std::string(token) + "\"}";
}

std::string status_response(std::string_view token, const PlatformObservation& observation,
                            bool include_observation) {
    if (!include_observation) {
        return "{\"ok\":true,\"token\":\"" + std::string(token) + "\"}";
    }
    return "{\"ok\":true,\"present\":" + std::to_string(observation.present_count) +
           ",\"tick\":" + std::to_string(observation.tick) +
           ",\"token\":\"" + std::string(token) + "\"}";
}

}  // namespace

int run_platform_ipc(int fd) {
#if !defined(__unix__) && !defined(__APPLE__)
    (void)fd;
    return 2;
#else
    if (fd < 0) {
        return 2;
    }
    PlatformRuntime runtime;
    bool started = false;
    std::string token;
    int result = 0;
    while (true) {
        std::string message;
        if (!read_frame(fd, &message)) {
            result = 2;
            break;
        }
        Request request;
        if (!parse_request(message, &request) || (!token.empty() && request.token != token)) {
            static_cast<void>(write_frame(fd, error_response(token, "invalid IPC request")));
            result = 2;
            break;
        }
        std::string error;
        if (request.operation == Request::Operation::start) {
            if (started) {
                static_cast<void>(write_frame(fd, error_response(token, "session already started")));
                result = 3;
                break;
            }
            token = request.token;
            runtime.reset();
            started = true;
            if (!write_frame(fd, status_response(token, runtime.observe(), false))) {
                result = 2;
                break;
            }
        } else if (request.operation == Request::Operation::step) {
            if (!started || !runtime.step(request.input, &error) ||
                !write_frame(fd, status_response(token, runtime.observe(), true))) {
                if (!error.empty()) {
                    static_cast<void>(write_frame(fd, error_response(token, error)));
                }
                result = 3;
                break;
            }
        } else if (request.operation == Request::Operation::observe) {
            if (!started || !write_frame(fd, status_response(token, runtime.observe(), true))) {
                result = 3;
                break;
            }
        } else {
            if (!started || !write_frame(fd, status_response(token, runtime.observe(), false))) {
                result = 3;
            }
            break;
        }
    }
    ::close(fd);
    return result;
#endif
}

int maybe_run_platform_ipc(int argc, char** argv) {
    if (argc != 3 || argv == nullptr || argv[1] == nullptr || argv[2] == nullptr ||
        std::string_view(argv[1]) != "--emu-agent-ipc-fd") {
        return -1;
    }
    int fd = -1;
    const std::string_view value(argv[2]);
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), fd);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || fd < 0) {
        return 2;
    }
    return run_platform_ipc(fd);
}

}  // namespace ac6demo_native
