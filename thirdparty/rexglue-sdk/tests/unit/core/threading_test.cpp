#include <catch2/catch_test_macros.hpp>
#include <native/thread.h>

#include <atomic>
#include <chrono>
#include <thread>

using namespace std::chrono_literals;

TEST_CASE("auto-reset event reserves a signal for an existing waiter", "[threading]") {
  auto event = rex::thread::Event::CreateAutoResetEvent(false);
  auto ready = rex::thread::Event::CreateManualResetEvent(false);
  std::atomic result = rex::thread::WaitResult::kFailed;

  std::thread waiter([&] {
    ready->Set();
    result = rex::thread::Wait(event.get(), false, 1s);
  });

  REQUIRE(rex::thread::Wait(ready.get(), false, 1s) == rex::thread::WaitResult::kSuccess);
  std::this_thread::sleep_for(10ms);
  event->Set();

  CHECK(rex::thread::Wait(event.get(), false, 0ms) == rex::thread::WaitResult::kTimeout);
  waiter.join();
  CHECK(result == rex::thread::WaitResult::kSuccess);
}
