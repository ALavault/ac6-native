#pragma once

#include <optional>

namespace ac6demo {

// Internal-only child mode for the emu-agent's inherited AF_UNIX socket.
// It is intentionally not a user-facing CLI command.
[[nodiscard]] int run_emu_agent_ipc(int fd);
// CTest-only frontend cycle. It never mounts content or starts generated guest code.
[[nodiscard]] int run_emu_agent_ipc_frontend_test(int fd);
[[nodiscard]] std::optional<int> maybe_run_emu_agent_ipc(int argc,
                                                          char **argv);

} // namespace ac6demo
