// Internal GuestBridge domain fragment; included only by guest_bridge.cpp.
bool GuestBridge::create_guest_thread(std::uint32_t stack_size,
                                      std::uint32_t thread_id_pointer,
                                      std::uint32_t startup,
                                      std::uint32_t entry,
                                      std::uint32_t parameter,
                                      std::uint32_t creation_flags,
                                      std::uint32_t handle_pointer,
                                      std::uint32_t* thread_id) {
  (void)thread_id_pointer;
  if (handle_pointer == 0U || thread_id == nullptr ||
      entry < PPC_CODE_BASE || entry >= PPC_CODE_BASE + PPC_CODE_SIZE ||
      !memory_.mapped(entry, 1U)) {
    return false;
  }
  const auto object = allocate_address(0x200U);
  if (object == 0U) {
    return false;
  }
  std::uint32_t mapped_address{};
  std::size_t mapped_size{};
  if (!checked_page_range(object, 0x200U, &mapped_address, &mapped_size)) {
    return false;
  }
  memory_.map_zero(mapped_address, mapped_size);
  record_allocation(mapped_address, mapped_size);

  const auto requested_stack_size =
      std::max(stack_size, kGuestThreadStackSize);
  const auto stack_address = allocate_address(requested_stack_size);
  if (stack_address == 0U ||
      !checked_page_range(stack_address, requested_stack_size, &mapped_address,
                          &mapped_size)) {
    return false;
  }
  memory_.map_zero(mapped_address, mapped_size);
  record_allocation(mapped_address, mapped_size);

  const auto id = next_thread_id_++;
  const auto handle = next_thread_handle_;
  next_thread_handle_ += 4U;
  memory_.store_u32(handle_pointer, handle);
  GuestThread thread{handle, object, id, stack_size, startup, entry, parameter,
                     creation_flags,
                     mapped_address + static_cast<std::uint32_t>(mapped_size)};
  thread.suspended = (creation_flags & 0x1U) != 0U;
  threads_.push_back(std::move(thread));
  *thread_id = id;
  return true;
}

void GuestBridge::guest_fiber_trampoline(std::uintptr_t bridge_address,
                                         std::uintptr_t thread_address) {
  auto* bridge = reinterpret_cast<GuestBridge*>(bridge_address);
  auto* thread = reinterpret_cast<GuestThread*>(thread_address);
  bridge->execute_guest_thread(*thread);
}

void GuestBridge::initialize_guest_fiber(GuestThread& thread) {
  if (thread.fiber_state != nullptr) {
    return;
  }
  auto fiber = std::make_unique<GuestFiber>();
  fiber->stack.resize(kHostFiberStackSize);
  fiber->ppc = std::make_unique<PPCContext>();
  if (getcontext(&fiber->context) != 0) {
    throw RuntimeTrap("cannot create guest fiber context", tick_);
  }
  fiber->context.uc_stack.ss_sp = fiber->stack.data();
  fiber->context.uc_stack.ss_size = fiber->stack.size();
  fiber->context.uc_stack.ss_flags = 0;
  fiber->context.uc_link = active_scheduler_context;
  auto* context = fiber->ppc.get();
  context->r1.u32 = thread.id == kPrimaryGuestThreadId ? stack_top_ : thread.stack_top;
  context->r13.u32 = kGuestThreadBase;
  context->fpscr.loadFromHost();
  if (thread.id != kPrimaryGuestThreadId) {
    context->r3.u32 = thread.entry;
    context->r4.u32 = thread.parameter;
  } else {
    context->r3.u32 = thread.entry;
  }
  using Trampoline = void (*)();
  makecontext(&fiber->context, reinterpret_cast<Trampoline>(&GuestBridge::guest_fiber_trampoline),
              2, static_cast<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(this)),
              static_cast<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(&thread)));
  thread.fiber_state = fiber.release();
  thread.started = true;
}

void GuestBridge::destroy_guest_fiber(GuestThread& thread) noexcept {
  delete static_cast<GuestFiber*>(thread.fiber_state);
  thread.fiber_state = nullptr;
}

void GuestBridge::execute_guest_thread(GuestThread& thread) {
  auto* fiber = static_cast<GuestFiber*>(thread.fiber_state);
  if (fiber == nullptr || fiber->ppc == nullptr) {
    return;
  }
  const auto previous_thread_id = current_guest_thread_id;
  current_guest_thread_id = thread.id;
  try {
    const auto target = thread.id == kPrimaryGuestThreadId ? thread.entry : thread.startup;
    const auto function = lookup_guest_function(target);
    if (function == nullptr) {
      throw RuntimeTrap("guest fiber target is not in the qualified function map", tick_,
                        0, target);
    }
    function(*fiber->ppc, memory_.raw_base());
    thread.finished = true;
  } catch (const GuestThreadExit&) {
    // ExTerminateThread is the normal terminal edge of the Xbox wrapper.
    thread.finished = true;
  } catch (const GuestThreadBlocked&) {
    fiber->failure = std::make_exception_ptr(
        RuntimeTrap("guest fiber yielded without a qualified wait key", tick_));
  } catch (...) {
    fiber->failure = std::current_exception();
  }
  current_guest_thread_id = previous_thread_id;
  if (thread.finished && thread.handle != 0U) {
    wake_guest_waiters(kWaitThread, thread.handle);
  }
}

void GuestBridge::wake_expired_guest_threads() noexcept {
  const auto wake = [this](GuestThread& thread) {
    if (thread.blocked && thread.wake_tick != kNoWakeTick && tick_ >= thread.wake_tick) {
      thread.blocked = false;
      thread.wake_timed_out = true;
    }
  };
  wake(primary_thread_);
  for (auto& thread : threads_) {
    wake(thread);
  }
}

bool GuestBridge::block_current_guest_thread(std::uint8_t wait_kind,
                                             std::uint32_t wait_key,
                                             std::uint64_t wake_tick) {
  GuestThread* current = current_guest_thread_id == kPrimaryGuestThreadId
                             ? &primary_thread_
                             : nullptr;
  if (current == nullptr) {
    const auto found = std::find_if(
        threads_.begin(), threads_.end(),
        [](const GuestThread& thread) { return thread.id == current_guest_thread_id; });
    if (found != threads_.end()) {
      current = &*found;
    }
  }
  if (current == nullptr || current->fiber_state == nullptr ||
      active_scheduler_context == nullptr) {
    throw RuntimeTrap("guest wait crossed an inactive scheduler boundary", tick_);
  }
  auto* fiber = static_cast<GuestFiber*>(current->fiber_state);
  current->blocked = true;
  current->wake_timed_out = false;
  current->wait_kind = wait_kind;
  current->wait_key = wait_key;
  // Every guest wait is entered from inside an import, so the link register of
  // the import currently being dispatched is the guest call site of the wait.
  current->wait_lr = current_import_lr;
  current->wake_tick = wake_tick;
  if (swapcontext(&fiber->context, active_scheduler_context) != 0) {
    throw RuntimeTrap("guest fiber context switch failed", tick_);
  }
  const auto timed_out = current->wake_timed_out;
  current->blocked = false;
  current->wake_timed_out = false;
  current->wait_kind = 0U;
  current->wait_key = 0U;
  current->wake_tick = 0U;
  return timed_out;
}

void GuestBridge::yield_guest_thread_if_due() {
  if (active_scheduler_context == nullptr ||
      current_guest_thread_id == kSchedulerThreadId) {
    return;
  }
  if (++guest_memory_operations_since_yield < kGuestMemoryOperationsPerQuantum) {
    return;
  }
  guest_memory_operations_since_yield = 0U;

  constexpr std::uint32_t kRenderQueueBase = 0x82386CC0U;
  constexpr std::uint32_t kProducerOffset = 0x60D0U;
  constexpr std::uint32_t kConsumerOffset = 0x60D4U;
  if (memory_.mapped(kRenderQueueBase + kProducerOffset, 8U)) {
    const auto producer = memory_.load_u32(kRenderQueueBase + kProducerOffset);
    const auto consumer = memory_.load_u32(kRenderQueueBase + kConsumerOffset);
    if (!render_queue_observed_) {
      render_queue_observed_ = true;
    } else {
      if (producer != render_queue_last_producer_) {
        ++render_queue_producer_changes_;
        render_queue_last_producer_thread_ = current_guest_thread_id;
        render_queue_last_producer_tick_ = tick_;
      }
      if (consumer != render_queue_last_consumer_) {
        ++render_queue_consumer_changes_;
        render_queue_last_consumer_thread_ = current_guest_thread_id;
        render_queue_last_consumer_tick_ = tick_;
      }
    }
    render_queue_last_producer_ = producer;
    render_queue_last_consumer_ = consumer;
    const auto pending = (producer - consumer) & 0xFFU;
    render_queue_max_pending_ = std::max(render_queue_max_pending_, pending);
  }

  GuestThread* current = current_guest_thread_id == kPrimaryGuestThreadId
                             ? &primary_thread_
                             : nullptr;
  if (current == nullptr) {
    const auto found = std::find_if(
        threads_.begin(), threads_.end(),
        [](const GuestThread& thread) { return thread.id == current_guest_thread_id; });
    if (found != threads_.end()) {
      current = &*found;
    }
  }
  if (current == nullptr || current->fiber_state == nullptr) {
    return;
  }
  auto* fiber = static_cast<GuestFiber*>(current->fiber_state);
  if (swapcontext(&fiber->context, active_scheduler_context) != 0) {
    throw RuntimeTrap("guest fiber preemption context switch failed", tick_);
  }
}

void GuestBridge::yield_current_guest_thread() {
  if (active_scheduler_context == nullptr ||
      current_guest_thread_id == kSchedulerThreadId) {
    return;
  }
  GuestThread *current = current_guest_thread_id == kPrimaryGuestThreadId
                             ? &primary_thread_
                             : nullptr;
  if (current == nullptr) {
    const auto found = std::find_if(
        threads_.begin(), threads_.end(), [](const GuestThread &thread) {
          return thread.id == current_guest_thread_id;
        });
    if (found != threads_.end()) current = &*found;
  }
  if (current == nullptr || current->fiber_state == nullptr) return;
  auto *fiber = static_cast<GuestFiber *>(current->fiber_state);
  if (swapcontext(&fiber->context, active_scheduler_context) != 0) {
    throw RuntimeTrap("guest fiber handoff context switch failed", tick_);
  }
}

bool GuestBridge::wake_guest_waiters(std::uint8_t wait_kind,
                                     std::uint32_t wait_key) noexcept {
  bool woke = false;
  const auto wake = [wait_kind, wait_key, &woke](GuestThread& thread) {
    if (thread.blocked && thread.wait_kind == wait_kind && thread.wait_key == wait_key) {
      thread.blocked = false;
      thread.wake_timed_out = false;
      woke = true;
    }
  };
  wake(primary_thread_);
  for (auto& thread : threads_) {
    wake(thread);
  }
  return woke;
}

std::uint32_t GuestBridge::wake_one_guest_waiter(
    std::uint8_t wait_kind, std::uint32_t wait_key) noexcept {
  const auto wake = [wait_kind, wait_key](GuestThread& thread) -> std::uint32_t {
    if (!thread.blocked || thread.wait_kind != wait_kind ||
        thread.wait_key != wait_key) {
      return 0U;
    }
    thread.blocked = false;
    thread.wake_timed_out = false;
    return thread.id;
  };
  if (const auto primary = wake(primary_thread_); primary != 0U) {
    return primary;
  }
  for (auto& thread : threads_) {
    if (const auto id = wake(thread); id != 0U) {
      return id;
    }
  }
  return 0U;
}

bool GuestBridge::run_runnable_threads() {
  ucontext_t scheduler_context{};
  auto* previous_scheduler_context = active_scheduler_context;
  const auto previous_thread_id = current_guest_thread_id;
  active_scheduler_context = &scheduler_context;
  current_guest_thread_id = kSchedulerThreadId;
  wake_expired_guest_threads();
  bool ran_thread = false;
  std::size_t activations = 0U;
  bool slice_exhausted = false;

  const auto run = [this, &scheduler_context, &ran_thread, &activations,
                    &slice_exhausted](GuestThread& thread) {
    if (thread.id == kSchedulerThreadId || thread.suspended || thread.blocked ||
        thread.finished) {
      return;
    }
    if (activations >= kMaxGuestActivationsPerSlice) {
      slice_exhausted = true;
      return;
    }
    ac6demo::guest_bridge_detail::trace_event_post_set_schedule(
        thread.id, thread.entry, thread.startup, thread.parameter, tick_);
    ++activations;
    initialize_guest_fiber(thread);
    auto* fiber = static_cast<GuestFiber*>(thread.fiber_state);
    fiber->context.uc_link = &scheduler_context;
    current_guest_thread_id = thread.id;
    if (swapcontext(&scheduler_context, &fiber->context) != 0) {
      throw RuntimeTrap("guest fiber context switch failed", tick_, 0, thread.id);
    }
    current_guest_thread_id = kSchedulerThreadId;
    if (fiber->failure != nullptr) {
      std::rethrow_exception(fiber->failure);
    }
    ran_thread = true;
  };

  bool made_progress = true;
  while (made_progress && !slice_exhausted) {
    made_progress = false;
    wake_expired_guest_threads();
    if (!primary_thread_.blocked && !primary_thread_.finished &&
        !primary_thread_.suspended && primary_thread_.entry != 0U) {
      const auto before = activations;
      run(primary_thread_);
      made_progress = made_progress || activations != before;
    }
    for (auto& thread : threads_) {
      if (slice_exhausted) {
        break;
      }
      const auto before = activations;
      run(thread);
      made_progress = made_progress || activations != before;
    }
  }
  last_slice_activations_ = static_cast<std::uint32_t>(activations);
  if (slice_exhausted) {
    ++scheduler_slice_exhaustions_;
  }
  current_guest_thread_id = previous_thread_id;
  active_scheduler_context = previous_scheduler_context;
  return ran_thread;
}

GuestSchedulerSnapshot GuestBridge::scheduler_snapshot() const noexcept {
  GuestSchedulerSnapshot result;
  result.thread_count = static_cast<std::uint32_t>(threads_.size()) + 1U;
  result.event_count = static_cast<std::uint32_t>(events.size());
  result.mutant_count = static_cast<std::uint32_t>(mutants.size());
  result.semaphore_count = static_cast<std::uint32_t>(
      semaphores.size() + kernel_semaphores.size());
  result.event_set_count = event_set_count;
  result.last_event_set_handle = last_event_set_handle;
  result.last_event_set_thread = last_event_set_thread;
  result.event_publications = event_publications;
  result.event_publication_count = event_publication_count;
  result.slice_exhaustions = scheduler_slice_exhaustions_;
  result.last_slice_activations = last_slice_activations_;
  constexpr std::uint32_t kRenderQueueBase = 0x82386CC0U;
  constexpr std::uint32_t kProducerOffset = 0x60D0U;
  constexpr std::uint32_t kConsumerOffset = 0x60D4U;
  result.render_queue_base = kRenderQueueBase;
  result.render_queue_mapped =
      memory_.mapped(kRenderQueueBase + kProducerOffset, 8U);
  if (result.render_queue_mapped) {
    result.render_queue_producer =
        memory_.load_u32(kRenderQueueBase + kProducerOffset);
    result.render_queue_consumer =
        memory_.load_u32(kRenderQueueBase + kConsumerOffset);
  }
  result.render_queue_producer_changes = render_queue_producer_changes_;
  result.render_queue_consumer_changes = render_queue_consumer_changes_;
  result.render_queue_last_producer_thread =
      render_queue_last_producer_thread_;
  result.render_queue_last_consumer_thread =
      render_queue_last_consumer_thread_;
  result.render_queue_last_producer_tick = render_queue_last_producer_tick_;
  result.render_queue_last_consumer_tick = render_queue_last_consumer_tick_;
  result.render_queue_max_pending = render_queue_max_pending_;
  result.primary_blocked = primary_thread_.blocked;
  result.primary_wait_kind = primary_thread_.wait_kind;
  result.primary_wait_key = primary_thread_.wait_key;
  const auto account = [this, &result](const GuestThread& thread) {
    if (thread.finished) {
      ++result.finished;
    } else if (thread.blocked) {
      ++result.blocked;
    } else if (!thread.suspended && thread.started) {
      ++result.runnable;
    }
    if (result.wait_count < result.waits.size()) {
      std::uint32_t callback = 0U;
      std::uint32_t callback_parameter = 0U;
      if (thread.entry == 0x822EE158U && memory_.mapped(thread.parameter, 28U)) {
        callback = memory_.load_u32(thread.parameter + 20U);
        callback_parameter = memory_.load_u32(thread.parameter + 24U);
      }
      const auto indirect = indirect_calls.find(thread.id);
      const GuestIndirectCall last_call =
          indirect == indirect_calls.end() ? GuestIndirectCall{} : indirect->second;
      result.waits[result.wait_count++] =
          GuestThreadWaitSnapshot{thread.id, thread.startup, thread.entry,
                                  thread.parameter, callback, callback_parameter,
                                  last_call.target, last_call.lr, last_call.count,
                                  last_call.tick,
                                  thread.wait_key, thread.wait_lr,
                                  thread.wake_tick,
                                  thread.wait_kind, thread.blocked,
                                  thread.suspended, thread.finished};
    }
  };
  account(primary_thread_);
  for (const auto& thread : threads_) {
    account(thread);
  }
  return result;
}

bool GuestBridge::reference_guest_thread(std::uint32_t handle,
                                         std::uint32_t* object) const noexcept {
  if (object == nullptr) {
    return false;
  }
  const auto found = std::find_if(
      threads_.begin(), threads_.end(),
      [handle](const GuestThread& thread) { return thread.handle == handle; });
  if (found == threads_.end() || !found->handle_open) {
    return false;
  }
  *object = found->object;
  return true;
}

bool GuestBridge::is_guest_thread_object(std::uint32_t object) const noexcept {
  return std::find_if(
             threads_.begin(), threads_.end(),
             [object](const GuestThread& thread) { return thread.object == object; }) !=
         threads_.end();
}

bool GuestBridge::resume_guest_thread(std::uint32_t handle,
                                      std::uint32_t previous_count_pointer,
                                      std::uint32_t* previous_count) noexcept {
  (void)previous_count_pointer;
  if (previous_count == nullptr) {
    return false;
  }
  const auto found = std::find_if(
      threads_.begin(), threads_.end(),
      [handle](const GuestThread& thread) { return thread.handle == handle; });
  if (found == threads_.end() || !found->handle_open) {
    return false;
  }
  *previous_count = found->suspended ? 1U : 0U;
  found->suspended = false;
  return true;
}

bool GuestBridge::close_guest_thread(std::uint32_t handle) noexcept {
  const auto found = std::find_if(
      threads_.begin(), threads_.end(),
      [handle](const GuestThread& thread) { return thread.handle == handle; });
  if (found == threads_.end() || !found->handle_open) {
    return false;
  }
  found->handle_open = false;
  return true;
}

bool GuestBridge::is_guest_thread_handle(std::uint32_t handle) const noexcept {
  return std::find_if(
             threads_.begin(), threads_.end(),
             [handle](const GuestThread& thread) { return thread.handle == handle; }) !=
         threads_.end();
}

bool GuestBridge::guest_thread_finished(std::uint32_t handle) const noexcept {
  const auto found = std::find_if(
      threads_.begin(), threads_.end(),
      [handle](const GuestThread& thread) { return thread.handle == handle; });
  return found != threads_.end() && found->finished;
}
