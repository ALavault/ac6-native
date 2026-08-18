#include "ac6demo/session.hpp"

#include "ac6demo/xex_image.hpp"

#include <stdexcept>

namespace ac6demo {

void DeterministicScheduler::register_thread(std::uint32_t id) {
  if (id == 0U) {
    throw std::invalid_argument("scheduler thread id 0 is reserved");
  }
  threads_.insert(id);
  if (token_ == 0U) {
    token_ = *threads_.begin();
  }
}

void DeterministicScheduler::begin_tick() {
  if (tick_open_ || threads_.empty()) {
    throw std::logic_error("scheduler tick cannot begin");
  }
  tick_open_ = true;
  token_ = *threads_.begin();
}

void DeterministicScheduler::yield(std::uint32_t id) {
  if (!tick_open_ || threads_.find(id) == threads_.end() || token_ != id) {
    throw RuntimeTrap("host thread crossed guest boundary without scheduler token", tick_);
  }
  const auto next = threads_.upper_bound(id);
  token_ = next == threads_.end() ? *threads_.begin() : *next;
}

void DeterministicScheduler::finish_tick() {
  if (!tick_open_) {
    throw std::logic_error("scheduler tick is not open");
  }
  tick_open_ = false;
  ++tick_;
  token_ = *threads_.begin();
}

DemoSession::DemoSession(std::filesystem::path store, GraphicsBackend backend)
    : store_(std::move(store)), profile_{backend}, hooks_(memory_), guest_(memory_) {
  scheduler_.register_thread(1U);
  scheduler_.register_thread(2U);
  scheduler_.register_thread(3U);
}

void DemoSession::start(const std::filesystem::path& trace_path, bool frontend_only) {
  if (state_ != DemoSessionState::Stopped) {
    throw std::logic_error("demo session already started");
  }
  // The GuestMemory reservation spans the full 32-bit Xenon range. These
  // pages are the runtime bootstrap/TLS boundary only; the generated guest
  // will own game data and code mappings.
  memory_.map_zero(0x00010000U, 0x1000U);
  memory_.map_zero(0x7FFF0000U, 0x1000U);
  if (guest_.available() && !frontend_only) {
    mount_ = std::make_unique<VfsMount>(store_);
    guest_.attach_vfs(*mount_);
    const auto xex_path = mount_->resolve("game:/Default.xex");
    const auto image = load_xex_image(xex_path);
    memory_.map_bytes(image.base, image.bytes);
    guest_entry_point_ = image.entry_point;
    guest_.prepare(GuestBridge::ThreadImage{image.tls_address, image.tls_data_size,
                                            image.tls_raw_size, image.stack_size,
                                            image.system_flags});
  }
  guest_started_ = false;
  frontend_only_ = frontend_only;
  has_scheduler_snapshot_ = false;
  trace_.open(trace_path, profile_.backend);
  state_ = DemoSessionState::Frontend;
  trace_.append(0U, TraceDomain::Simulation,
                "{\"state\":\"frontend\",\"owner\":\"guest\"}");
  trace_.append(0U, TraceDomain::Objectives,
                "{\"state\":\"frontend\",\"synthetic_result\":false}");
}

void DemoSession::step(InputFrame input) {
  if (state_ == DemoSessionState::Stopped) {
    throw std::logic_error("demo session is not started");
  }
  scheduler_.begin_tick();
  hooks_.set_tick(scheduler_.tick());
  trace_.append(scheduler_.tick(), TraceDomain::Input,
                "{\"buttons\":" + std::to_string(input.buttons) +
                    ",\"left_trigger\":" + std::to_string(input.left_trigger) +
                    ",\"right_trigger\":" + std::to_string(input.right_trigger) +
                    ",\"lx\":" + std::to_string(input.left_x) +
                    ",\"ly\":" + std::to_string(input.left_y) +
                    ",\"rx\":" + std::to_string(input.right_x) +
                    ",\"ry\":" + std::to_string(input.right_y) +
                    ",\"connected\":" + (input.connected ? "true" : "false") + "}");
  guest_.set_tick(scheduler_.tick());
  guest_.set_input_frame(input);
  if (guest_.available() && !frontend_only_) {
    guest_.run_entry(guest_entry_point_);
    if (!guest_started_) {
      guest_started_ = true;
      state_ = DemoSessionState::Guest;
      trace_.append(scheduler_.tick(), TraceDomain::Simulation,
                    "{\"state\":\"guest\",\"owner\":\"recompiled\"}");
    }
  }
  const auto input_polls = guest_.consume_input_poll_stats();
  if (input_polls.state != 0U || input_polls.capabilities != 0U ||
      input_polls.vibration != 0U || input_polls.keystroke != 0U) {
    trace_.append(scheduler_.tick(), TraceDomain::Input,
                  "{\"xam_state_polls\":" + std::to_string(input_polls.state) +
                      ",\"xam_capability_polls\":" +
                      std::to_string(input_polls.capabilities) +
                      ",\"xam_vibration_calls\":" +
                      std::to_string(input_polls.vibration) +
                      ",\"xam_keystroke_polls\":" +
                      std::to_string(input_polls.keystroke) +
                      ",\"left_motor\":" + std::to_string(input_polls.left_motor) +
                      ",\"right_motor\":" + std::to_string(input_polls.right_motor) +
                      ",\"controller_snapshot\":" +
                      (input_polls.has_controller_snapshot ? "true" : "false") +
                      ",\"controller_object\":" +
                      std::to_string(input_polls.controller_object) +
                      ",\"controller_vtable\":" +
                      std::to_string(input_polls.controller_vtable) +
                      ",\"current_buttons\":" +
                      std::to_string(input_polls.current_buttons) +
                      ",\"pressed_buttons\":" +
                      std::to_string(input_polls.pressed_buttons) +
                      ",\"released_buttons\":" +
                      std::to_string(input_polls.released_buttons) +
                      ",\"previous_buttons\":" +
                      std::to_string(input_polls.previous_buttons) +
                      ",\"normalized_buttons\":" +
                      std::to_string(input_polls.normalized_buttons) +
                      ",\"logical_snapshot\":" +
                      (input_polls.has_logical_snapshot ? "true" : "false") +
                      ",\"logical_current\":" +
                      std::to_string(input_polls.logical_current) +
                      ",\"logical_previous\":" +
                      std::to_string(input_polls.logical_previous) +
                      ",\"logical_pressed\":" +
                      std::to_string(input_polls.logical_pressed) +
                      "}");
  }
  const auto guest_scheduler = guest_.scheduler_snapshot();
  if (!has_scheduler_snapshot_ || guest_scheduler != scheduler_snapshot_) {
    std::string waits = "[";
    for (std::uint32_t index = 0U; index < guest_scheduler.wait_count; ++index) {
      const auto& wait = guest_scheduler.waits[index];
      if (index != 0U) {
        waits += ',';
      }
      waits += "{\"id\":" + std::to_string(wait.id) +
               ",\"startup\":" + std::to_string(wait.startup) +
               ",\"entry\":" + std::to_string(wait.entry) +
               ",\"parameter\":" + std::to_string(wait.parameter) +
               ",\"callback\":" + std::to_string(wait.callback) +
               ",\"callback_parameter\":" +
               std::to_string(wait.callback_parameter) +
               ",\"last_indirect_target\":" +
               std::to_string(wait.last_indirect_target) +
               ",\"last_indirect_lr\":" +
               std::to_string(wait.last_indirect_lr) +
               ",\"indirect_call_count\":" +
               std::to_string(wait.indirect_call_count) +
               ",\"last_indirect_tick\":" +
               std::to_string(wait.last_indirect_tick) +
               ",\"kind\":" + std::to_string(wait.wait_kind) +
               ",\"key\":" + std::to_string(wait.wait_key) +
               ",\"wake_tick\":" + std::to_string(wait.wake_tick) +
               ",\"blocked\":" + (wait.blocked ? "true" : "false") +
               ",\"suspended\":" + (wait.suspended ? "true" : "false") +
               ",\"finished\":" + (wait.finished ? "true" : "false") + "}";
    }
    waits += ']';
    std::string publications{"["};
    for (std::uint32_t index = 0U;
         index < guest_scheduler.event_publication_count; ++index) {
      if (index != 0U) {
        publications += ',';
      }
      const auto& publication = guest_scheduler.event_publications[index];
      publications += "{\"key\":" + std::to_string(publication.key) +
                      ",\"thread\":" + std::to_string(publication.thread) +
                      ",\"lr\":" + std::to_string(publication.lr) +
                      ",\"kind\":" + std::to_string(publication.kind) + "}";
    }
    publications += ']';
    trace_.append(scheduler_.tick(), TraceDomain::Simulation,
                  "{\"scheduler_threads\":" +
                    std::to_string(guest_scheduler.thread_count) +
                    ",\"scheduler_runnable\":" +
                    std::to_string(guest_scheduler.runnable) +
                    ",\"scheduler_blocked\":" +
                    std::to_string(guest_scheduler.blocked) +
                    ",\"scheduler_finished\":" +
                    std::to_string(guest_scheduler.finished) +
                    ",\"kernel_events\":" +
                    std::to_string(guest_scheduler.event_count) +
                    ",\"kernel_mutants\":" +
                    std::to_string(guest_scheduler.mutant_count) +
                    ",\"kernel_semaphores\":" +
                    std::to_string(guest_scheduler.semaphore_count) +
                    ",\"event_set_count\":" +
                    std::to_string(guest_scheduler.event_set_count) +
                    ",\"last_event_set_handle\":" +
                    std::to_string(guest_scheduler.last_event_set_handle) +
                    ",\"last_event_set_thread\":" +
                    std::to_string(guest_scheduler.last_event_set_thread) +
                    ",\"event_publications\":" + publications +
                    ",\"primary_wait_kind\":" +
                    std::to_string(guest_scheduler.primary_wait_kind) +
                    ",\"primary_wait_key\":" +
                    std::to_string(guest_scheduler.primary_wait_key) +
                    ",\"primary_blocked\":" +
                    (guest_scheduler.primary_blocked ? "true" : "false") +
                    ",\"waits\":" + waits + "}");
    scheduler_snapshot_ = guest_scheduler;
    has_scheduler_snapshot_ = true;
  }
  scheduler_.yield(1U);
  scheduler_.yield(2U);
  scheduler_.yield(3U);
  trace_.append(scheduler_.tick(), TraceDomain::Simulation,
                "{\"guest_tick\":" + std::to_string(scheduler_.tick()) +
                    ",\"state\":\"" +
                    (state_ == DemoSessionState::Guest ? "guest" : "frontend") + "\"}");
  scheduler_.finish_tick();
}

void DemoSession::begin_xam_movie_record() {
  std::string error;
  if (!guest_.input_device().begin_xam_movie_record(error)) {
    throw RuntimeTrap("XAM movie record setup refused: " + error);
  }
}

void DemoSession::begin_xam_movie_replay(std::string_view movie) {
  std::string error;
  if (!guest_.input_device().begin_xam_movie_replay(movie, error)) {
    throw RuntimeTrap("XAM movie replay setup refused: " + error);
  }
}

std::string DemoSession::finalize_xam_movie() {
  std::string sealed_movie;
  std::string error;
  if (!guest_.input_device().finalize_xam_movie(sealed_movie, error)) {
    throw RuntimeTrap("XAM movie finalization refused: " + error,
                      scheduler_.tick());
  }
  return sealed_movie;
}

void DemoSession::stop() {
  if (state_ == DemoSessionState::Stopped) {
    return;
  }
  trace_.close();
  state_ = DemoSessionState::Stopped;
}

}  // namespace ac6demo
