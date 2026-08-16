// Internal GuestBridge domain fragment; included only by guest_bridge.cpp.
namespace ac6demo {

bool generated_guest_available() noexcept { return true; }

GuestBridge::GuestBridge(GuestMemory& memory) : memory_(memory) {
  // The qualified bootstrap creates 16 guest workers. Reserve well beyond
  // that count so a vector relocation can never invalidate a live fiber's
  // GuestThread address while ExCreateThread is executing.
  threads_.reserve(64U);
}

GuestBridge::~GuestBridge() {
  destroy_guest_fiber(primary_thread_);
  for (auto& thread : threads_) {
    destroy_guest_fiber(thread);
  }
  if (active_bridge == this) {
    active_bridge = nullptr;
    reservations.clear();
    critical_sections.clear();
    kernel_critical_region_depth = 0U;
    tls_values.clear();
    tls_slots.clear();
    events.clear();
    notify_listeners.clear();
    timers.clear();
    mutants.clear();
    semaphores.clear();
    kernel_semaphores.clear();
    event_set_count = 0U;
    last_event_set_handle = 0U;
    last_event_set_thread = 0U;
    indirect_calls.clear();
    spinlock_owners.clear();
    guest_irql.clear();
    event_publications.fill(ac6demo::GuestEventPublicationSnapshot{});
    event_publication_count = 0U;
    graphics_interrupt_callback = 0U;
    graphics_interrupt_context = 0U;
    next_tls_slot = 0U;
    network_error = 10093;
    next_event_handle = 0xE0000000U;
    next_notify_handle = 0xE5000000U;
    next_timer_handle = 0xE6000000U;
    active_scheduler_context = nullptr;
  }
}
