// AC6 cycle 323 — regression for the guest condition-variable ping-pong.
//
// Cycle 321 refuted "auto-reset signal theft" with a harness that held the
// shared value frozen at 0 and pumped 200 independent signals. Under that
// model a stolen signal only ever costs throughput, because another signal is
// always coming. The real guest protocol has no such spare signal: each state
// change is announced exactly once, and the two waiters want *different*
// values, so a consumed-by-the-wrong-thread wake is permanent.
//
// This harness replays the real protocol instead of an abstraction of it.
//
// Guest protocol, recovered from the generated corpus (module default.xex,
// SHA-256 acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde):
//
//   object 0x82870818 = 0x82870780 + 152
//     +0  mutant handle   (created by sub_82346010 via sub_82391130)
//     +4  event handle    (created by sub_82346010 via CreateEvent(0,0,0,0)
//                          -> bManualReset = 0, i.e. auto-reset)
//     +16 shared 64-bit value  (initialised to 0 by sub_8233A730/sub_8233AE30)
//
//   sub_82346108(obj, wanted):            // wait_for_value
//       loop { NtWaitForSingleObject(obj+0);            // take mutant
//              if (load64(obj+16) == wanted) { NtReleaseMutant(obj+0); return; }
//              NtSignalAndWaitForSingleObjectEx(obj+0, obj+4, ...); }
//
//   sub_823460B0(obj, value):              // set_value
//       NtWaitForSingleObject(obj+0);
//       store64(obj+16, value);
//       NtReleaseMutant(obj+0);
//       NtSetEvent(obj+4);
//
//   main thread  sub_8233BA78: wait_for_value(0) ... set_value(1)
//                              (via sub_8233AB00 and sub_8233AAF0)
//   worker       sub_8233AD70: wait_for_value(1) ... set_value(0)
//
// Falsifiable criterion: if the POSIX auto-reset event at rexglue HEAD is
// sufficient for this protocol, the ping-pong completes its iteration target.
// If the setter thread can consume the wake it just posted, the pair reaches a
// state where the shared value is 0, the main thread wants 0, both threads are
// asleep, and no further signal will ever be posted -- exactly the state
// measured in the runtime.
//
// Build:
//   g++ -std=c++20 -O2 -pthread ac6_condition_pingpong_regression.cpp \
//       -o ac6_condition_pingpong_regression

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <new>
#include <string>
#include <thread>

#include <signal.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

enum class WaitResult { kSuccess, kTimeout };

// ---------------------------------------------------------------------------
// Verbatim replication of rexglue-sdk/src/core/threading_posix.cpp at HEAD.
// The Linux robust-mutex branch of PosixConditionBase::Wait is elided; it
// affects error recovery only, not wake ownership, which is what is under test.
// ---------------------------------------------------------------------------

class PosixConditionBase {
 public:
  virtual ~PosixConditionBase() = default;
  virtual bool Signal() = 0;

  virtual WaitResult Wait(std::chrono::milliseconds timeout) {
    bool executed;
    auto predicate = [this] { return this->signaled(); };
    std::unique_lock<std::mutex> lock(mutex_);
    if (predicate()) {
      executed = true;
    } else {
      if (timeout == std::chrono::milliseconds::max()) {
        cond_.wait(lock, predicate);
        executed = true;
      } else {
        executed = cond_.wait_for(lock, timeout, predicate);
      }
    }
    if (executed) {
      post_execution();
      return WaitResult::kSuccess;
    }
    return WaitResult::kTimeout;
  }

 protected:
  virtual bool signaled() const = 0;
  virtual void post_execution() = 0;
  std::condition_variable cond_;
  std::mutex mutex_;
};

// PosixCondition<Event> exactly as shipped at HEAD.
class EventHead : public PosixConditionBase {
 public:
  EventHead(bool manual_reset, bool initial_state)
      : signal_(initial_state), manual_reset_(manual_reset) {}

  bool Signal() override {
    auto lock = std::unique_lock<std::mutex>(mutex_);
    signal_ = true;
    cond_.notify_all();
    return true;
  }

 private:
  bool signaled() const override { return signal_; }
  void post_execution() override {
    if (!manual_reset_) {
      signal_ = false;
    }
  }
  bool signal_;
  const bool manual_reset_;
};

// Candidate: NT releases the *first* waiter queued on a synchronization event.
// Signal hands the release token to that waiter instead of publishing a shared
// boolean any running thread may claim.
class EventFifo : public PosixConditionBase {
 public:
  EventFifo(bool manual_reset, bool initial_state)
      : signal_(initial_state), manual_reset_(manual_reset) {}

  WaitResult Wait(std::chrono::milliseconds timeout) override {
    if (manual_reset_) {
      return PosixConditionBase::Wait(timeout);
    }
    std::unique_lock<std::mutex> lock(mutex_);
    if (signal_) {
      signal_ = false;
      return WaitResult::kSuccess;
    }
    bool released = false;
    waiters_.push_back(&released);
    auto predicate = [&released] { return released; };
    bool executed;
    if (timeout == std::chrono::milliseconds::max()) {
      cond_.wait(lock, predicate);
      executed = true;
    } else {
      executed = cond_.wait_for(lock, timeout, predicate);
    }
    if (!executed) {
      for (auto it = waiters_.begin(); it != waiters_.end(); ++it) {
        if (*it == &released) {
          waiters_.erase(it);
          break;
        }
      }
      return WaitResult::kTimeout;
    }
    return WaitResult::kSuccess;
  }

  bool Signal() override {
    auto lock = std::unique_lock<std::mutex>(mutex_);
    if (!manual_reset_ && !waiters_.empty()) {
      *waiters_.front() = true;
      waiters_.pop_front();
    } else {
      signal_ = true;
    }
    cond_.notify_all();
    return true;
  }

 private:
  bool signaled() const override { return signal_; }
  void post_execution() override {
    if (!manual_reset_) {
      signal_ = false;
    }
  }
  bool signal_;
  const bool manual_reset_;
  std::deque<bool*> waiters_;
};

// PosixCondition<Mutant> exactly as shipped at HEAD.
class Mutant : public PosixConditionBase {
 public:
  Mutant() : count_(0) {}

  bool Signal() override { return Release(); }

  bool Release() {
    if (owner_ == std::this_thread::get_id() && count_ > 0) {
      auto lock = std::unique_lock<std::mutex>(mutex_);
      --count_;
      if (count_ == 0) {
        cond_.notify_all();
      }
      return true;
    }
    return false;
  }

 private:
  bool signaled() const override {
    return count_ == 0 || owner_ == std::this_thread::get_id();
  }
  void post_execution() override {
    count_++;
    owner_ = std::this_thread::get_id();
  }
  uint32_t count_;
  std::thread::id owner_;
};

// rexglue SignalAndWait: signal, then wait. Not one atomic queue operation.
WaitResult SignalAndWait(PosixConditionBase* to_signal, PosixConditionBase* to_wait_on,
                         std::chrono::milliseconds timeout) {
  if (!to_signal->Signal()) {
    return WaitResult::kTimeout;
  }
  return to_wait_on->Wait(timeout);
}

// ---------------------------------------------------------------------------
// The guest object and its two guest entry points.
// ---------------------------------------------------------------------------

constexpr auto kForever = std::chrono::milliseconds::max();

struct GuestConditionObject {
  Mutant mutant;               // obj+0
  PosixConditionBase* event;   // obj+4
  uint64_t value = 0;          // obj+16, 64-bit, big-endian on the guest

  // sub_82346108
  void WaitForValue(uint64_t wanted) {
    for (;;) {
      mutant.Wait(kForever);
      if (value == wanted) {
        mutant.Release();
        return;
      }
      SignalAndWait(&mutant, event, kForever);
    }
  }

  // sub_823460B0
  void SetValue(uint64_t next) {
    mutant.Wait(kForever);
    value = next;
    mutant.Release();
    event->Signal();
  }
};

struct Shared {
  std::atomic<uint64_t> main_iterations;
  std::atomic<uint64_t> worker_iterations;
  std::atomic<uint64_t> value_snapshot;
};

struct Outcome {
  uint64_t main_iterations = 0;
  uint64_t worker_iterations = 0;
  uint64_t value_at_stop = 0;
  bool completed = false;
};

// Each trial runs in its own process. A reproduced deadlock leaves two threads
// permanently asleep by construction, so there is no in-process way to reclaim
// them without perturbing the very wake ownership under test. The parent kills
// the child instead, and reads the counters out of shared memory.
Outcome RunPingPong(bool fifo, uint64_t target, std::chrono::milliseconds deadline) {
  void* raw = mmap(nullptr, sizeof(Shared), PROT_READ | PROT_WRITE,
                   MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  if (raw == MAP_FAILED) {
    std::perror("mmap");
    std::exit(3);
  }
  auto* shared = new (raw) Shared{};
  shared->main_iterations.store(0);
  shared->worker_iterations.store(0);
  shared->value_snapshot.store(0);

  pid_t child = fork();
  if (child == 0) {
    EventHead head_event(false, false);
    EventFifo fifo_event(false, false);
    static GuestConditionObject obj;
    obj.event = fifo ? static_cast<PosixConditionBase*>(&fifo_event)
                     : static_cast<PosixConditionBase*>(&head_event);

    // Worker: sub_8233AD70 — wait for 1, then report idle with 0.
    std::thread worker([&] {
      for (;;) {
        obj.WaitForValue(1);
        shared->worker_iterations.fetch_add(1, std::memory_order_relaxed);
        obj.SetValue(0);
      }
    });

    // Main: sub_8233BA78 — wait for worker idle, then hand it the next frame.
    for (uint64_t i = 0; i < target; ++i) {
      obj.WaitForValue(0);
      shared->main_iterations.fetch_add(1, std::memory_order_relaxed);
      shared->value_snapshot.store(obj.value, std::memory_order_relaxed);
      obj.SetValue(1);
    }
    shared->value_snapshot.store(obj.value, std::memory_order_relaxed);
    std::_Exit(0);
  }

  auto start = std::chrono::steady_clock::now();
  bool completed = false;
  int status = 0;
  while (std::chrono::steady_clock::now() - start < deadline) {
    pid_t reaped = waitpid(child, &status, WNOHANG);
    if (reaped == child) {
      completed = WIFEXITED(status) && WEXITSTATUS(status) == 0;
      child = 0;
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  if (child != 0) {
    kill(child, SIGKILL);
    waitpid(child, &status, 0);
  }

  Outcome outcome;
  outcome.main_iterations = shared->main_iterations.load();
  outcome.worker_iterations = shared->worker_iterations.load();
  outcome.value_at_stop = shared->value_snapshot.load();
  outcome.completed = completed;
  munmap(raw, sizeof(Shared));
  return outcome;
}

}  // namespace

int main(int argc, char** argv) {
  const uint64_t target = argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 20000;
  const int trials = argc > 2 ? std::atoi(argv[2]) : 3;
  const auto deadline = std::chrono::milliseconds(argc > 3 ? std::atoi(argv[3]) : 10000);

  std::printf("AC6 guest condition-variable ping-pong regression\n");
  std::printf("target iterations per trial: %llu, trials: %d, deadline: %lld ms\n\n",
              static_cast<unsigned long long>(target), trials,
              static_cast<long long>(deadline.count()));

  int head_completed = 0;
  int fifo_completed = 0;

  for (int variant = 0; variant < 2; ++variant) {
    const bool fifo = variant == 1;
    std::printf("%s\n", fifo ? "NT-ordered release (candidate)" : "rexglue HEAD auto-reset event");
    for (int trial = 0; trial < trials; ++trial) {
      auto outcome = RunPingPong(fifo, target, deadline);
      std::printf("  trial %d: main=%llu/%llu worker=%llu value_at_stop=%llu %s\n", trial + 1,
                  static_cast<unsigned long long>(outcome.main_iterations),
                  static_cast<unsigned long long>(target),
                  static_cast<unsigned long long>(outcome.worker_iterations),
                  static_cast<unsigned long long>(outcome.value_at_stop),
                  outcome.completed ? "COMPLETED" : "STALLED");
      if (outcome.completed) {
        (fifo ? fifo_completed : head_completed)++;
      }
    }
    std::printf("\n");
  }

  const bool head_stalls = head_completed < trials;
  const bool fifo_survives = fifo_completed == trials;

  std::printf("HEAD completed %d/%d, NT-ordered completed %d/%d\n", head_completed, trials,
              fifo_completed, trials);

  if (head_stalls && fifo_survives) {
    std::printf("AC6_CONDITION_PINGPONG_REGRESSION CONFIRMS_SELF_CONSUMED_WAKE\n");
    return 0;
  }
  if (!head_stalls && fifo_survives) {
    std::printf("AC6_CONDITION_PINGPONG_REGRESSION NO_DEFECT_REPRODUCED\n");
    return 1;
  }
  std::printf("AC6_CONDITION_PINGPONG_REGRESSION INCONCLUSIVE\n");
  return 2;
}
