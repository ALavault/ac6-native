// AC6 cycle-321 regression for the cycle-320 auto-reset-event hypothesis.
//
// Cycle 320 observed two guest threads asleep on the same auto-reset event
// (handle 0xF80000A8) and proposed, as leading root cause, that the POSIX
// implementation lets a later waiter steal a signal intended for an
// already-sleeping eligible waiter. It explicitly required a targeted SDK
// regression before changing the runtime. This is that regression.
//
// It replicates PosixConditionBase::Wait and PosixCondition<Event> from
// rexglue-sdk/src/core/threading_posix.cpp exactly as they stand: Signal()
// sets a boolean under the mutex and calls notify_all(); every woken waiter
// races for the mutex and the winner clears the boolean via post_execution().
//
// Build: g++ -std=c++20 -O2 -pthread ac6_auto_reset_event_regression.cpp
//
// The claim under test is falsifiable: if signal theft is the freeze
// mechanism, the eligible waiter must be able to reach zero progress while
// signals continue to arrive.
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <thread>

namespace {

// Verbatim shape of PosixCondition<Event> with manual_reset_ == false.
class RaceEvent {
 public:
  void Signal() {
    auto lock = std::unique_lock<std::mutex>(mutex_);
    signal_ = true;
    cond_.notify_all();
  }
  bool Wait(std::chrono::milliseconds timeout) {
    auto lock = std::unique_lock<std::mutex>(mutex_);
    const bool executed =
        signal_ ? true
                : cond_.wait_for(lock, timeout, [this] { return signal_; });
    if (executed) {
      signal_ = false;  // post_execution() for an auto-reset event
    }
    return executed;
  }

 private:
  std::mutex mutex_;
  std::condition_variable cond_;
  bool signal_ = false;
};

struct Outcome {
  int eligible_wakeups = 0;
  int ineligible_wakeups = 0;
};

// Cycle-320 guest shape: the shared value stays 0, so only the main waiter is
// eligible; the worker wakes, sees the value is not 1, and waits again.
// `reentry_delay` makes the eligible waiter slower to re-enter the wait, which
// is the condition most favourable to theft.
Outcome Run(int signals, std::chrono::microseconds reentry_delay) {
  RaceEvent event;
  std::atomic<int> eligible{0};
  std::atomic<int> ineligible{0};
  std::atomic<bool> stop{false};

  std::thread worker([&] {
    while (!stop.load()) {
      if (event.Wait(std::chrono::milliseconds(2))) {
        ineligible.fetch_add(1);
      }
    }
  });
  std::thread main_waiter([&] {
    while (!stop.load()) {
      if (event.Wait(std::chrono::milliseconds(2))) {
        eligible.fetch_add(1);
      }
      if (reentry_delay.count() != 0) {
        std::this_thread::sleep_for(reentry_delay);
      }
    }
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  for (int i = 0; i < signals; ++i) {
    event.Signal();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  stop.store(true);
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  worker.join();
  main_waiter.join();
  return {eligible.load(), ineligible.load()};
}

}  // namespace

int main() {
  constexpr int kSignals = 200;
  bool starved = false;
  std::printf("AC6_AUTO_RESET_EVENT_REGRESSION signals=%d\n", kSignals);
  for (const int delay_us : {0, 50, 500, 5000}) {
    const auto outcome = Run(kSignals, std::chrono::microseconds(delay_us));
    std::printf("  eligible_reentry_delay_us=%-5d eligible=%-4d ineligible=%d\n",
                delay_us, outcome.eligible_wakeups, outcome.ineligible_wakeups);
    if (outcome.eligible_wakeups == 0) {
      starved = true;
    }
  }
  // A stolen signal delays the eligible waiter until the next signal; a signal
  // delivered with no waiter present is retained in the boolean, so none is
  // lost. Starvation to zero is the only outcome that would support the
  // cycle-320 attribution.
  std::printf("%s\n", starved
                          ? "AC6_AUTO_RESET_EVENT_REGRESSION SUPPORTS_THEFT"
                          : "AC6_AUTO_RESET_EVENT_REGRESSION REFUTES_THEFT");
  return starved ? 1 : 0;
}
