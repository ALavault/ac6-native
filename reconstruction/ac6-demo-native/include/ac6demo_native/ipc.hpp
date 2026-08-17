#pragma once

namespace ac6demo_native {

[[nodiscard]] int run_platform_ipc(int fd);
[[nodiscard]] int maybe_run_platform_ipc(int argc, char** argv);

}  // namespace ac6demo_native
