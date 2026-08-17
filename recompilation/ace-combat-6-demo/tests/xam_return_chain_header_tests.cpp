#include "../src/guest_bridge/xam_return_chain_trace.hpp"

#include <array>
#include <cassert>
#include <cstdlib>
#include <thread>
#include <vector>

namespace {

using namespace ac6demo::guest_bridge_detail;

constexpr std::uint32_t kThread = 7U;

void arm() {
  reset_xam_return_chain_for_tests();
  setenv("AC6_DEMO_WATCH_XAM_RETURN_CHAIN", "1", 1);
  initialize_xam_return_chain_watch();
  assert(xam_return_chain_watch_enabled_fast());
  constexpr std::array<std::uint8_t, 16U> payload{
      0x00U, 0x01U, 0x02U, 0x03U, 0x04U, 0x05U, 0x06U, 0x07U,
      0x08U, 0x09U, 0x0AU, 0x0BU, 0x0CU, 0x0DU, 0x0EU, 0x0FU};
  arm_xam_return_chain(kXamReturnCaller, 252U, kThread, 0U, 0U,
                       kXamReturnControllerObject, 0U, payload.data());
  assert(xam_return_chain_state.phase.load() == 1U);
  assert(xam_return_chain_state.thread == kThread);
  assert(xam_return_chain_state.result == 0U);
  assert(xam_return_chain_state.output == kXamReturnControllerObject);
  assert(xam_return_chain_state.state16 == payload);
}

void test_off_hot_path() {
  unsetenv("AC6_DEMO_WATCH_XAM_RETURN_CHAIN");
  reset_xam_return_chain_for_tests();
  initialize_xam_return_chain_watch();
  assert(!xam_return_chain_watch_enabled_fast());
  constexpr std::array<std::uint8_t, 16U> payload{};
  arm_xam_return_chain(kXamReturnCaller, 252U, kThread, 0U, 0U, 0x1000U,
                       0U, payload.data());
  assert(xam_return_chain_state.phase.load() == 0U);
  assert(xam_return_chain_state.accesses.load() == 0U);
}

void test_arm_identity_rejections() {
  constexpr std::array<std::uint8_t, 16U> payload{};
  for (const auto bad : {1U, 2U, 3U, 4U}) {
    reset_xam_return_chain_for_tests();
    setenv("AC6_DEMO_WATCH_XAM_RETURN_CHAIN", "1", 1);
    initialize_xam_return_chain_watch();
    arm_xam_return_chain(bad == 1U ? kXamReturnCaller + 4U : kXamReturnCaller,
                         1U, kThread, 0U, 0U,
                         bad == 3U ? kXamReturnControllerObject + 4U
                                   : kXamReturnControllerObject,
                         bad == 2U ? 1U : 0U,
                         bad == 4U ? nullptr : payload.data());
    assert(xam_return_chain_state.phase.load() == 0U);
  }
}

void test_thread_affinity_and_concurrency() {
  arm();
  for (unsigned i = 0U; i < 100U; ++i) {
    assert(!xam_return_chain_claim(kThread + 1U));
  }
  assert(xam_return_chain_state.accesses.load() == 0U);

  std::vector<std::thread> workers;
  for (unsigned i = 0U; i < 8U; ++i) {
    workers.emplace_back([] {
      for (unsigned attempt = 0U; attempt < 64U; ++attempt) {
        (void)xam_return_chain_claim(kThread + 1U);
      }
    });
  }
  for (auto &worker : workers) worker.join();
  assert(xam_return_chain_state.accesses.load() == 0U);

  workers.clear();
  for (unsigned i = 0U; i < 8U; ++i) {
    workers.emplace_back([] {
      for (unsigned attempt = 0U; attempt < 64U; ++attempt) {
        (void)xam_return_chain_claim(kThread);
      }
    });
  }
  for (auto &worker : workers) worker.join();
  assert(xam_return_chain_state.accesses.load() == kXamReturnMaxAccesses);
  assert(xam_return_chain_state.phase.load() == 2U);
  (void)xam_return_chain_claim(kThread);
  assert(xam_return_chain_state.accesses.load() == kXamReturnMaxAccesses);
}

void test_scalar_vector_and_target() {
  arm();
  record_xam_return_chain("load8", 0x1000U, 1U, 0x12U, 1U, kThread, 0U,
                          "f", 10U);
  record_xam_return_chain("load16", 0x1002U, 2U, 0x1234U, 2U, kThread, 0U,
                          "f", 11U);
  record_xam_return_chain("load32", 0x1004U, 4U, 0x12345678U, 3U, kThread,
                          0U, "f", 12U);
  record_xam_return_chain("load64", 0x1008U, 8U, 0x123456789ABCDEF0ULL, 4U,
                          kThread, 0U, "f", 13U);
  assert(xam_return_chain_state.accesses.load() == 4U);

  arm();
  constexpr std::array<std::uint8_t, 16U> vector{
      0x00U, 0x11U, 0x22U, 0x33U, 0x44U, 0x55U, 0x66U, 0x77U,
      0x88U, 0x99U, 0xAAU, 0xBBU, 0xCCU, 0xDDU, 0xEEU, 0xFFU};
  record_xam_return_chain_bytes("load128", 0x2000U, 16U, vector.data(), 5U,
                                kThread, 0U, "f", 14U);
  record_xam_return_chain_bytes("store128", 0x2010U, 16U, vector.data(), 6U,
                                kThread, 0U, "f", 15U);
  assert(xam_return_chain_state.accesses.load() == 2U);

  arm();
  record_xam_return_chain("store32", kXamReturnExclusiveAddress, 4U,
                          0x829D15BCU, 7U, kThread, 0U,
                          "__imp__sub_822F5E58", 3948U);
  assert(xam_return_chain_state.accesses.load() == 1U);
  assert(xam_return_chain_state.phase.load() == 2U);
  record_xam_return_chain("load8", 0x3000U, 1U, 1U, 8U, kThread, 0U, "f",
                          16U);
  assert(xam_return_chain_state.accesses.load() == 1U);
}

void test_exact_bound_and_atomic_contract() {
  arm();
  for (std::uint32_t i = 0U; i < kXamReturnMaxAccesses; ++i) {
    record_xam_return_chain("load8", 0x4000U + i, 1U, i, i, kThread, 0U,
                            "f", 20U);
  }
  assert(xam_return_chain_state.accesses.load() == kXamReturnMaxAccesses);
  assert(xam_return_chain_state.phase.load() == 2U);
  record_xam_return_chain("load8", 0x5000U, 1U, 1U, 100U, kThread, 0U, "f",
                          21U);
  assert(xam_return_chain_state.accesses.load() == kXamReturnMaxAccesses);

  arm();
  record_xam_return_chain_atomic("stwcx", kXamReturnExclusiveAddress, 4U,
                                 0x12345678U, true, 8U, kThread, 0U,
                                 "f", 22U);
  assert(xam_return_chain_state.accesses.load() == 1U);
  assert(xam_return_chain_state.phase.load() == 2U);

  arm();
  record_xam_return_chain_atomic("stdcx", kXamReturnExclusiveAddress, 8U,
                                 0x123456789ABCDEF0ULL, false, 9U, kThread,
                                 0U, "f", 23U, kXamReturnExclusivePc);
  assert(xam_return_chain_state.accesses.load() == 1U);
  assert(xam_return_chain_state.phase.load() == 2U);
}

}  // namespace

int main() {
  test_off_hot_path();
  test_arm_identity_rejections();
  test_thread_affinity_and_concurrency();
  test_scalar_vector_and_target();
  test_exact_bound_and_atomic_contract();
  return 0;
}
