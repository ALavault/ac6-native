#include "ac6demo/emu_agent_ipc.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <thread>

#include <sys/socket.h>
#include <unistd.h>

namespace {

bool write_all(int fd, const std::byte *bytes, std::size_t size) {
  while (size != 0U) {
    const auto written = ::send(fd, bytes, size, MSG_NOSIGNAL);
    if (written <= 0) {
      return false;
    }
    bytes += written;
    size -= static_cast<std::size_t>(written);
  }
  return true;
}

bool read_frame(int fd, std::string *result) {
  if (result == nullptr) {
    return false;
  }
  std::array<std::byte, 4> prefix{};
  if (::recv(fd, prefix.data(), prefix.size(), MSG_WAITALL) != 4) {
    return false;
  }
  std::uint32_t size{};
  for (const auto byte : prefix) {
    size = (size << 8U) | static_cast<std::uint8_t>(byte);
  }
  result->assign(size, '\0');
  return ::recv(fd, result->data(), result->size(), MSG_WAITALL) ==
         static_cast<ssize_t>(result->size());
}

bool send_frame(int fd, std::string_view payload) {
  const auto size = static_cast<std::uint32_t>(payload.size());
  const std::array<std::byte, 4> prefix{
      std::byte((size >> 24U) & 0xffU), std::byte((size >> 16U) & 0xffU),
      std::byte((size >> 8U) & 0xffU), std::byte(size & 0xffU)};
  return write_all(fd, prefix.data(), prefix.size()) &&
         write_all(fd, reinterpret_cast<const std::byte *>(payload.data()), payload.size());
}

} // namespace

int main() {
  int sockets[2]{};
  if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0) {
    return 1;
  }
  int status{};
  std::thread child([&] { status = ac6demo::run_emu_agent_ipc_frontend_test(sockets[1]); });
  std::string response;
  const auto expect = [&](std::string_view request, std::string_view expected) {
    return send_frame(sockets[0], request) && read_frame(sockets[0], &response) &&
           response == expected;
  };
  // This OFF build has no generated guest, so the real handler can exercise
  // its bounded frontend session without mounting retail/PAL data.
  if (!expect("{\"op\":\"start\",\"token\":\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\"}",
              "{\"ok\":true,\"token\":\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\"}") ||
      !expect("{\"op\":\"step\",\"token\":\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\",\"xinput\":{\"buttons\":32768,\"connected\":true,\"left_stick\":{\"x\":-1,\"y\":2},\"left_trigger\":3,\"right_stick\":{\"x\":4,\"y\":-5},\"right_trigger\":6}}",
              "{\"ok\":true,\"present\":0,\"tick\":1,\"token\":\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\"}") ||
      !expect("{\"op\":\"observe\",\"token\":\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\"}",
              "{\"ok\":true,\"present\":0,\"tick\":1,\"token\":\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\"}") ||
      !expect("{\"op\":\"stop\",\"token\":\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\"}",
              "{\"ok\":true,\"token\":\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\"}")) return 1;
  ::close(sockets[0]);
  child.join();
  if (status != 0 || ::socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0) {
    return 1;
  }
  std::thread malformed([&] { status = ac6demo::run_emu_agent_ipc_frontend_test(sockets[1]); });
  const std::array<std::byte, 4> oversized{std::byte{0}, std::byte{1},
                                           std::byte{0}, std::byte{1}};
  if (!write_all(sockets[0], oversized.data(), oversized.size())) return 1;
  ::shutdown(sockets[0], SHUT_WR);
  ::close(sockets[0]);
  malformed.join();
  return status == 2 ? 0 : 1;
}
