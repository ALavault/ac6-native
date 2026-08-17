#include "ac6demo_native/ipc.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::abort();
    }
}

#if defined(__unix__) || defined(__APPLE__)
void send_frame(int fd, std::string_view value) {
    const auto size = static_cast<std::uint32_t>(value.size());
    const std::array<unsigned char, 4U> prefix{
        static_cast<unsigned char>((size >> 24U) & 0xffU),
        static_cast<unsigned char>((size >> 16U) & 0xffU),
        static_cast<unsigned char>((size >> 8U) & 0xffU),
        static_cast<unsigned char>(size & 0xffU)};
    require(::send(fd, prefix.data(), prefix.size(), 0) ==
                static_cast<ssize_t>(prefix.size()), "IPC request prefix sent");
    require(::send(fd, value.data(), value.size(), 0) ==
                static_cast<ssize_t>(value.size()), "IPC request body sent");
}

std::string receive_frame(int fd) {
    std::array<unsigned char, 4U> prefix{};
    require(::recv(fd, prefix.data(), prefix.size(), MSG_WAITALL) ==
                static_cast<ssize_t>(prefix.size()), "IPC response prefix received");
    const std::uint32_t size = (static_cast<std::uint32_t>(prefix[0]) << 24U) |
        (static_cast<std::uint32_t>(prefix[1]) << 16U) |
        (static_cast<std::uint32_t>(prefix[2]) << 8U) | prefix[3];
    require(size > 0U && size <= 65536U, "IPC response length bounded");
    std::string value(size, '\0');
    require(::recv(fd, value.data(), size, MSG_WAITALL) == static_cast<ssize_t>(size),
            "IPC response body received");
    return value;
}

void test_session() {
    int sockets[2] = {-1, -1};
    require(::socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0, "IPC socketpair created");
    int child_result = -1;
    std::thread child([&] { child_result = ac6demo_native::run_platform_ipc(sockets[1]); });
    constexpr std::string_view token = "0123456789abcdef";
    send_frame(sockets[0], "{\"op\":\"start\",\"token\":\"0123456789abcdef\"}");
    require(receive_frame(sockets[0]) == "{\"ok\":true,\"token\":\"0123456789abcdef\"}",
            "IPC start acknowledgement exact");
    send_frame(sockets[0], "{\"op\":\"step\",\"token\":\"0123456789abcdef\",\"xinput\":{\"buttons\":16,\"connected\":true,\"left_stick\":{\"x\":0,\"y\":0},\"left_trigger\":0,\"right_stick\":{\"x\":0,\"y\":0},\"right_trigger\":0}}");
    require(receive_frame(sockets[0]) == "{\"ok\":true,\"present\":0,\"tick\":1,\"token\":\"0123456789abcdef\"}",
            "IPC step acknowledgement exact");
    send_frame(sockets[0], "{\"op\":\"observe\",\"token\":\"0123456789abcdef\"}");
    require(receive_frame(sockets[0]).find("\"tick\":1") != std::string::npos,
            "IPC observe tick exact");
    send_frame(sockets[0], "{\"op\":\"stop\",\"token\":\"0123456789abcdef\"}");
    require(receive_frame(sockets[0]) == "{\"ok\":true,\"token\":\"0123456789abcdef\"}",
            "IPC stop acknowledgement exact");
    ::close(sockets[0]);
    child.join();
    require(child_result == 0, "IPC session exits cleanly");
    (void)token;
}

void test_oversize_rejected() {
    int sockets[2] = {-1, -1};
    require(::socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0, "oversize socketpair created");
    int child_result = -1;
    std::thread child([&] { child_result = ac6demo_native::run_platform_ipc(sockets[1]); });
    const std::uint32_t size = 65537U;
    const std::array<unsigned char, 4U> prefix{
        static_cast<unsigned char>((size >> 24U) & 0xffU),
        static_cast<unsigned char>((size >> 16U) & 0xffU),
        static_cast<unsigned char>((size >> 8U) & 0xffU),
        static_cast<unsigned char>(size & 0xffU)};
    require(::send(sockets[0], prefix.data(), prefix.size(), 0) ==
                static_cast<ssize_t>(prefix.size()), "oversize prefix sent");
    ::shutdown(sockets[0], SHUT_RDWR);
    ::close(sockets[0]);
    child.join();
    require(child_result == 2, "oversize frame closes IPC child");
}
#endif

}  // namespace

int main() {
#if defined(__unix__) || defined(__APPLE__)
    test_session();
    test_oversize_rejected();
#endif
    std::cout << "IPC tests passed\n";
    return 0;
}
