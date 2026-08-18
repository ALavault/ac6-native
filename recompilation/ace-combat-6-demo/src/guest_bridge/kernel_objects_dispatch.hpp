// Internal GuestBridge domain fragment; included only by guest_bridge.cpp.
  if (std::string_view{name} == "ExCreateThread") {
    auto& bridge = require_bridge();
    const auto handle_pointer = context.r3.u32;
    const auto thread_id_pointer = context.r5.u32;
    if ((handle_pointer != 0U && !bridge.memory().mapped(handle_pointer, 4U)) ||
        (thread_id_pointer != 0U && !bridge.memory().mapped(thread_id_pointer, 4U))) {
      return false;
    }
    std::uint32_t thread_id{};
    if (!bridge.create_guest_thread(context.r4.u32, thread_id_pointer, context.r6.u32,
                                    context.r7.u32, context.r8.u32, context.r9.u32,
                                    handle_pointer, &thread_id)) {
      context.r3.s64 = -1;
      return true;
    }
    if (thread_id_pointer != 0U) {
      bridge.memory().store_u32(thread_id_pointer, thread_id);
    }
    context.r3.s64 = 0;
    return true;
  }
  if (std::string_view{name} == "ExTerminateThread") {
    // The Xbox contract does not return to the terminated guest thread. Use a
    // private control transfer caught by the deterministic guest scheduler;
    // it never crosses into the host process as a synthetic status.
    throw GuestThreadExit{};
  }
  if (std::string_view{name} == "RtlNtStatusToDosError") {
    context.r3.u32 = context.r3.u32 == 0U ? 0U : 31U;  // ERROR_GEN_FAILURE
    return true;
  }
  if (std::string_view{name} == "ObReferenceObjectByHandle") {
    auto& bridge = require_bridge();
    const auto object_pointer = context.r5.u32;
    std::uint32_t object{};
    if (object_pointer == 0U || !bridge.memory().mapped(object_pointer, 4U) ||
        !bridge.reference_guest_thread(context.r3.u32, &object)) {
      context.r3.s64 = -1;
      return true;
    }
    bridge.memory().store_u32(object_pointer, object);
    context.r3.s64 = 0;
    return true;
  }
  if (std::string_view{name} == "KeSetBasePriorityThread") {
    context.r3.s64 = require_bridge().is_guest_thread_object(context.r3.u32) ? 0 : -1;
    return true;
  }
  if (std::string_view{name} == "KeSetAffinityThread") {
    auto& bridge = require_bridge();
    const auto object = context.r3.u32;
    const auto previous_affinity_pointer = context.r5.u32;
    const bool previous_mapped =
        previous_affinity_pointer != 0U &&
        bridge.memory().mapped(previous_affinity_pointer, 4U);
    const auto previous_value =
        previous_mapped ? bridge.memory().load_u32(previous_affinity_pointer)
                        : 0U;
    if (context.r4.u32 == 0U ||
        !bridge.is_guest_thread_object(context.r3.u32) ||
        (previous_affinity_pointer != 0U && !previous_mapped)) {
      context.r3.s64 = -1;
      trace_affinity_call(context, object, previous_value, previous_mapped,
                          0xFFFFFFFFU);
      return true;
    }
    if (previous_affinity_pointer != 0U) {
      bridge.memory().store_u32(previous_affinity_pointer, 1U);
    }
    // The mask carries the guest's processor identity; a mask this bridge
    // cannot read as one-hot leaves the previous identity in place rather than
    // inventing one, and the call still succeeds as the XDK specifies.
    (void)bridge.pin_guest_thread_processor(context.r3.u32, context.r4.u32);
    context.r3.s64 = 0;
    trace_affinity_call(context, object, previous_value, previous_mapped, 0U);
    return true;
  }
  if (std::string_view{name} == "ObDereferenceObject") {
    context.r3.s64 = require_bridge().is_guest_thread_object(context.r3.u32) ? 0 : -1;
    return true;
  }
  if (std::string_view{name} == "NtCreateTimer") {
    auto& memory = memory_for(context);
    const auto handle_pointer = context.r3.u32;
    const auto attributes = context.r4.u32;
    const auto timer_type = context.r5.u32;
    if (handle_pointer == 0U || !memory.mapped(handle_pointer, 4U) ||
        timer_type > 1U ||
        (attributes != 0U && !memory.mapped(attributes, 12U)) ||
        next_timer_handle > 0xFFFFFFFCU) {
      return false;
    }
    const auto handle = next_timer_handle;
    next_timer_handle += 4U;
    timers.emplace(handle, GuestTimer{timer_type});
    memory.store_u32(handle_pointer, handle);
    context.r3.u32 = 0U;  // STATUS_SUCCESS
    return true;
  }
  if (std::string_view{name} == "NtSetTimerEx") {
    auto& bridge = require_bridge();
    const auto found = timers.find(context.r3.u32);
    const auto due_time_pointer = context.r4.u32;
    if (found == timers.end() || context.r6.u32 != 1U || context.r10.u32 != 0U ||
        context.r5.u32 != 0U || context.r7.u32 != 0U || context.r8.u32 > 1U) {
      return false;
    }
    if (context.r8.u32 != 0U) {
      context.r3.u32 = 0x40000025U;  // STATUS_TIMER_RESUME_IGNORED
      return true;
    }
    found->second.due_tick = decode_timer_due_tick(bridge, due_time_pointer);
    found->second.period_ticks = timer_period_to_ticks(context.r9.u32);
    found->second.signaled = false;
    found->second.active = found->second.due_tick != kNoWakeTick;
    context.r3.u32 = 0U;  // STATUS_SUCCESS
    return true;
  }
  if (std::string_view{name} == "NtCancelTimer") {
    auto& memory = memory_for(context);
    const auto found = timers.find(context.r3.u32);
    const auto current_state_pointer = context.r4.u32;
    if (found == timers.end() ||
        (current_state_pointer != 0U && !memory.mapped(current_state_pointer, 4U))) {
      return false;
    }
    found->second.active = false;
    found->second.signaled = false;
    found->second.due_tick = kNoWakeTick;
    found->second.period_ticks = 0U;
    if (current_state_pointer != 0U) {
      memory.store_u32(current_state_pointer, 0U);
    }
    context.r3.u32 = 0U;  // STATUS_SUCCESS
    return true;
  }
  if (std::string_view{name} == "NtCreateEvent") {
    const auto handle_pointer = context.r3.u32;
    if (!memory_for(context).mapped(handle_pointer, 4U)) {
      return false;
    }
    const auto handle = next_event_handle;
    next_event_handle += 4U;
    // The AC6/Xenon import uses the four-argument title ABI
    // NtCreateEvent(PHandle, Access, EventType, InitialState): r5 is the
    // event type (0 = notification/manual-reset, 1 = synchronization/auto)
    // and r6 is the initial state.  r7 is not an argument and may retain a
    // previous guest value.
    events.emplace(handle, GuestEvent{context.r6.u32 != 0U, context.r5.u32 == 0U});
    memory_for(context).store_u32(handle_pointer, handle);
    context.r3.s64 = 0;
    return true;
  }
  if (std::string_view{name} == "NtCreateMutant") {
    const auto handle_pointer = context.r3.u32;
    if (!memory_for(context).mapped(handle_pointer, 4U)) {
      return false;
    }
    const auto handle = next_event_handle;
    next_event_handle += 4U;
    const bool initial_owner = context.r5.u32 != 0U;
    mutants.emplace(handle,
                    GuestMutant{initial_owner ? current_guest_thread_id : 0U,
                                initial_owner ? 1U : 0U});
    memory_for(context).store_u32(handle_pointer, handle);
    context.r3.s64 = 0;
    return true;
  }
  if (std::string_view{name} == "NtCreateSemaphore") {
    const auto handle_pointer = context.r3.u32;
    if (!memory_for(context).mapped(handle_pointer, 4U) ||
        context.r6.u32 == 0U || context.r5.u32 > context.r6.u32) {
      return false;
    }
    const auto handle = next_event_handle;
    next_event_handle += 4U;
    semaphores.emplace(handle, GuestSemaphore{context.r5.u32, context.r6.u32});
    memory_for(context).store_u32(handle_pointer, handle);
    context.r3.s64 = 0;
    return true;
  }
  if (std::string_view{name} == "KeDelayExecutionThread") {
    // KeDelayExecutionThread(mode, alertable, interval) uses a signed
    // 100-ns interval.  The reached bootstrap passes a mapped relative
    // interval (including INT64_MIN for an indefinite yield), so hand the
    // wait to the deterministic guest scheduler rather than sleeping a host
    // thread.  Positive absolute deadlines and malformed pointers remain
    // outside the qualified contract.
    auto& bridge = require_bridge();
    const auto interval_pointer = context.r5.u32;
    if (context.r3.u32 > 1U || context.r4.u32 > 1U || interval_pointer == 0U ||
        !bridge.memory().mapped(interval_pointer, 8U)) {
      return false;
    }
    const auto deadline = decode_wait_deadline(bridge, interval_pointer);
    const auto timed_out = bridge.block_current_guest_thread(
        kWaitThread, current_guest_thread_id, deadline);
    context.r3.u32 = timed_out ? 0x102U : 0U;  // STATUS_TIMEOUT / SUCCESS
    return true;
  }
  if (std::string_view{name} == "NtSignalAndWaitForSingleObjectEx") {
    auto& bridge = require_bridge();
    const auto signal_handle = context.r3.u32;
    const auto wait_handle = context.r4.u32;
    ac6demo::guest_bridge_detail::trace_event_handoff(
        "signal_wait_enter", signal_handle, wait_handle, 0U,
        current_guest_thread_id, bridge.tick(),
        static_cast<std::uint32_t>(context.lr), 0U, 0U, 0U);
    const auto signal_event = events.find(signal_handle);
    const auto signal_mutant = mutants.find(signal_handle);
    if (signal_event != events.end()) {
      publish_guest_event(bridge, signal_handle, signal_event->second,
                          static_cast<std::uint32_t>(context.lr));
    } else if (signal_mutant != mutants.end() &&
               signal_mutant->second.owner == current_guest_thread_id &&
               signal_mutant->second.recursion != 0U) {
      if (--signal_mutant->second.recursion == 0U) {
        signal_mutant->second.owner = 0U;
        bridge.wake_guest_waiters(kWaitMutant, signal_handle);
      }
    } else {
      context.r3.s64 = -1;
      ac6demo::guest_bridge_detail::trace_event_handoff(
          "signal_wait_invalid_signal", signal_handle, wait_handle, 0U,
          current_guest_thread_id, bridge.tick(),
          static_cast<std::uint32_t>(context.lr), 0U, 0U, 0xFFFFFFFFU);
      return true;
    }
    const auto deadline = decode_wait_deadline(bridge, context.r7.u32);
    const auto semaphore = semaphores.find(wait_handle);
    if (semaphore != semaphores.end()) {
      while (semaphore->second.count == 0U) {
        if (bridge.block_current_guest_thread(kWaitSemaphore, wait_handle, deadline)) {
          context.r3.s64 = 258;  // STATUS_TIMEOUT
          return true;
        }
      }
      {
        --semaphore->second.count;
        context.r3.s64 = 0;
      }
      return true;
    }
    const auto wait = events.find(wait_handle);
    const auto mutant = mutants.find(wait_handle);
    if (wait == events.end() && mutant == mutants.end()) {
      context.r3.s64 = -1;
      ac6demo::guest_bridge_detail::trace_event_handoff(
          "signal_wait_invalid_wait", signal_handle, wait_handle, 0U,
          current_guest_thread_id, bridge.tick(),
          static_cast<std::uint32_t>(context.lr), 0U, 0U, 0xFFFFFFFFU);
      return true;
    }
    if (mutant != mutants.end()) {
      for (;;) {
        const auto resumed = mutants.find(wait_handle);
        if (resumed == mutants.end()) {
          context.r3.s64 = -1;
          return true;
        }
        if (resumed->second.owner == 0U ||
            resumed->second.owner == current_guest_thread_id) {
          resumed->second.owner = current_guest_thread_id;
          ++resumed->second.recursion;
          context.r3.s64 = 0;
          return true;
        }
        if (bridge.block_current_guest_thread(kWaitMutant, wait_handle, deadline)) {
          context.r3.s64 = 258;
          return true;
        }
      }
    }
    if (consume_guest_event(wait->second)) {
      // This is the only qualifying handoff: arm immediately after the
      // successful wait consume, while the import LR still identifies the
      // resumed guest PC.  The probe itself applies the exact handle/thread/LR
      // filter and is inert unless AC6_DEMO_WATCH_POST_RESUME_ACCESS=1.
      ac6demo::guest_bridge_detail::arm_post_resume_access(
          wait_handle, signal_handle, current_guest_thread_id,
          static_cast<std::uint32_t>(context.lr), bridge.tick());
      context.r3.s64 = 0;
    } else {
      for (;;) {
        ac6demo::guest_bridge_detail::trace_event_handoff(
            "signal_wait_block", signal_handle, wait_handle, kWaitEvent,
            current_guest_thread_id, bridge.tick(),
            static_cast<std::uint32_t>(context.lr),
            wait->second.signaled ? 1U : 0U, wait->second.granted_thread, 0U);
        if (bridge.block_current_guest_thread(kWaitEvent, wait_handle, deadline)) {
          context.r3.s64 = 258;  // STATUS_TIMEOUT
          ac6demo::guest_bridge_detail::trace_event_handoff(
              "signal_wait_timeout", signal_handle, wait_handle, kWaitEvent,
              current_guest_thread_id, bridge.tick(),
              static_cast<std::uint32_t>(context.lr), 0U, 0U, 258U);
          return true;
        }
        const auto resumed = events.find(wait_handle);
        if (resumed == events.end()) {
          context.r3.s64 = -1;
          ac6demo::guest_bridge_detail::trace_event_handoff(
              "signal_wait_missing_after_resume", signal_handle, wait_handle,
              kWaitEvent, current_guest_thread_id, bridge.tick(),
              static_cast<std::uint32_t>(context.lr), 0U, 0U, 0xFFFFFFFFU);
          return true;
        }
        ac6demo::guest_bridge_detail::trace_event_handoff(
            "signal_wait_resume", signal_handle, wait_handle, kWaitEvent,
            current_guest_thread_id, bridge.tick(),
            static_cast<std::uint32_t>(context.lr),
            resumed->second.signaled ? 1U : 0U, resumed->second.granted_thread,
            0U);
        if (consume_guest_event(resumed->second)) {
          ac6demo::guest_bridge_detail::arm_post_resume_access(
              wait_handle, signal_handle, current_guest_thread_id,
              static_cast<std::uint32_t>(context.lr), bridge.tick());
          context.r3.s64 = 0;
          break;
        }
      }
    }
    return true;
  }
  if (std::string_view{name} == "KeWaitForMultipleObjects") {
    // The reached helper passes eight kernel event objects, wait-any, and a
    // null relative timeout.  Keep the general bounded ABI here so the
    // helper can also express wait-all without introducing host waits.
    auto& bridge = require_bridge();
    auto& memory = bridge.memory();
    const auto count = context.r3.u32;
    const auto objects_pointer = context.r4.u32;
    const auto wait_type = context.r5.u32;
    const auto processor_mode = context.r7.u32;
    const auto alertable = context.r8.u32;
    if (count == 0U || count > 64U || wait_type > 1U || processor_mode > 1U ||
        alertable > 1U || objects_pointer == 0U ||
        !memory.mapped(objects_pointer, static_cast<std::size_t>(count) * 4U)) {
      return false;
    }
    const auto deadline = decode_wait_deadline(bridge, context.r9.u32);
    std::array<std::uint32_t, 64U> objects{};
    for (std::uint32_t index = 0U; index < count; ++index) {
      const auto object = memory.load_u32(objects_pointer + index * 4U);
      if (object == 0U || !memory.mapped(object, 8U)) {
        return false;
      }
      objects[index] = object;
    }
    // Same opt-in journal as the imports: the wait registers name a pointer,
    // not a wait. Without the resolved list, "thread 22 waits on eight
    // objects" cannot be turned into which eight.
    if (std::getenv("AC6_DEMO_WATCH_IMPORTS") != nullptr) {
      std::fprintf(stderr, "AC6_WAIT_OBJECTS tick=%llu thread=%u lr=0x%08X "
                           "count=%u type=%u",
                   static_cast<unsigned long long>(bridge.tick()),
                   current_guest_thread_id,
                   static_cast<std::uint32_t>(context.lr), count, wait_type);
      for (std::uint32_t index = 0U; index < count; ++index) {
        std::fprintf(stderr, " obj%u=0x%08X:type=0x%02X", index,
                     objects[index],
                     static_cast<unsigned>(memory.load_u8(objects[index])));
      }
      std::fprintf(stderr, "\n");
    }
    for (;;) {
      std::uint32_t signaled_count = 0U;
      std::uint32_t first_signaled = 0U;
      for (std::uint32_t index = 0U; index < count; ++index) {
        if (memory.load_u32(objects[index] + 4U) != 0U) {
          if (signaled_count == 0U) {
            first_signaled = index;
          }
          ++signaled_count;
        }
      }
      if ((wait_type != 0U && signaled_count != 0U) ||
          (wait_type == 0U && signaled_count == count)) {
        context.r3.u32 = wait_type != 0U ? first_signaled : 0U;
        return true;
      }
      if (bridge.block_current_guest_thread(kWaitKernelEvent, kWaitMultipleKey,
                                            deadline)) {
        context.r3.u32 = 0x102U;  // STATUS_TIMEOUT
        return true;
      }
    }
  }
  if (std::string_view{name} == "NtWaitForSingleObjectEx") {
    auto& bridge = require_bridge();
    const auto semaphore_handle = context.r3.u32;
    ac6demo::guest_bridge_detail::trace_event_handoff(
        "wait_single_enter", semaphore_handle, 0U, 0U,
        current_guest_thread_id, bridge.tick(),
        static_cast<std::uint32_t>(context.lr), 0U, 0U, 0U);
    const auto deadline = decode_wait_deadline(bridge, context.r6.u32);
    update_guest_timers(bridge);
    const auto semaphore = semaphores.find(semaphore_handle);
    if (semaphore != semaphores.end()) {
      while (semaphore->second.count == 0U) {
        if (bridge.block_current_guest_thread(kWaitSemaphore, semaphore_handle,
                                              deadline)) {
          context.r3.s64 = 258;  // STATUS_TIMEOUT
          return true;
        }
      }
      {
        --semaphore->second.count;
        context.r3.s64 = 0;
      }
      return true;
    }
    const auto mutant_handle = context.r3.u32;
    const auto mutant = mutants.find(mutant_handle);
    if (mutant != mutants.end()) {
      for (;;) {
        const auto resumed = mutants.find(mutant_handle);
        if (resumed == mutants.end()) {
          context.r3.s64 = -1;
          return true;
        }
        if (resumed->second.owner == 0U ||
            resumed->second.owner == current_guest_thread_id) {
          resumed->second.owner = current_guest_thread_id;
          ++resumed->second.recursion;
          context.r3.s64 = 0;
          return true;
        }
        if (bridge.block_current_guest_thread(kWaitMutant, mutant_handle, deadline)) {
          context.r3.s64 = 258;
          return true;
        }
      }
    }
    const auto timer_handle = context.r3.u32;
    const auto timer = timers.find(timer_handle);
    if (timer != timers.end()) {
      for (;;) {
        update_guest_timers(bridge);
        const auto resumed = timers.find(timer_handle);
        if (resumed == timers.end()) {
          context.r3.s64 = -1;
          return true;
        }
        if (resumed->second.signaled) {
          if (resumed->second.timer_type == 1U) {
            resumed->second.signaled = false;
          }
          context.r3.u32 = 0U;
          return true;
        }
        if (bridge.block_current_guest_thread(kWaitTimer, timer_handle, deadline)) {
          context.r3.u32 = 0x102U;  // STATUS_TIMEOUT
          return true;
        }
      }
    }
    const auto event_handle = context.r3.u32;
    const auto found = events.find(event_handle);
    if (found == events.end()) {
      if (bridge.is_guest_thread_handle(event_handle)) {
        while (!bridge.guest_thread_finished(event_handle)) {
          if (bridge.block_current_guest_thread(kWaitThread, event_handle, deadline)) {
            context.r3.s64 = 258;  // STATUS_TIMEOUT
            return true;
          }
        }
        context.r3.s64 = 0;
        return true;
      }
      context.r3.s64 = -1;
      return true;
    }
    if (consume_guest_event(found->second)) {
      context.r3.s64 = 0;
    } else {
      for (;;) {
        ac6demo::guest_bridge_detail::trace_event_handoff(
            "wait_single_block", 0U, event_handle, kWaitEvent,
            current_guest_thread_id, bridge.tick(),
            static_cast<std::uint32_t>(context.lr),
            found->second.signaled ? 1U : 0U, found->second.granted_thread, 0U);
        if (bridge.block_current_guest_thread(kWaitEvent, event_handle, deadline)) {
          context.r3.s64 = 258;  // STATUS_TIMEOUT
          ac6demo::guest_bridge_detail::trace_event_handoff(
              "wait_single_timeout", 0U, event_handle, kWaitEvent,
              current_guest_thread_id, bridge.tick(),
              static_cast<std::uint32_t>(context.lr), 0U, 0U, 258U);
          return true;
        }
        const auto resumed = events.find(event_handle);
        if (resumed == events.end()) {
          context.r3.s64 = -1;
          ac6demo::guest_bridge_detail::trace_event_handoff(
              "wait_single_missing_after_resume", 0U, event_handle,
              kWaitEvent, current_guest_thread_id, bridge.tick(),
              static_cast<std::uint32_t>(context.lr), 0U, 0U, 0xFFFFFFFFU);
          return true;
        }
        ac6demo::guest_bridge_detail::trace_event_handoff(
            "wait_single_resume", 0U, event_handle, kWaitEvent,
            current_guest_thread_id, bridge.tick(),
            static_cast<std::uint32_t>(context.lr),
            resumed->second.signaled ? 1U : 0U, resumed->second.granted_thread,
            0U);
        if (consume_guest_event(resumed->second)) {
          context.r3.s64 = 0;
          break;
        }
      }
    }
    return true;
  }
  if (std::string_view{name} == "KeWaitForSingleObject") {
    auto& bridge = require_bridge();
    auto& memory = bridge.memory();
    const auto object = context.r3.u32;
    if (!memory.mapped(object, 8U)) {
      return false;
    }
    const auto deadline = decode_wait_deadline(bridge, context.r7.u32);
    const auto semaphore = kernel_semaphores.find(object);
    if (semaphore != kernel_semaphores.end()) {
      while (semaphore->second.count == 0U) {
        if (bridge.block_current_guest_thread(kWaitKernelSemaphore, object,
                                              deadline)) {
          context.r3.s64 = 258;
          return true;
        }
      }
      --semaphore->second.count;
      memory.store_u32(object + 4U, semaphore->second.count);
      context.r3.s64 = 0;
      return true;
    }
    while (memory.load_u32(object + 4U) == 0U) {
      if (bridge.block_current_guest_thread(kWaitKernelEvent, object, deadline)) {
        context.r3.s64 = 258;  // STATUS_TIMEOUT
        return true;
      }
    }
    context.r3.s64 = 0;
    return true;
  }
  if (std::string_view{name} == "KeInitializeSemaphore") {
    auto& memory = memory_for(context);
    const auto object = context.r3.u32;
    const auto count = context.r4.u32;
    const auto limit = context.r5.u32;
    if (!memory.mapped(object, 20U) || limit == 0U || count > limit) {
      return false;
    }
    memory.store_u8(object, 5U);
    memory.store_u8(object + 1U, 0U);
    memory.store_u16(object + 2U, 0U);
    memory.store_u32(object + 4U, count);
    memory.store_u32(object + 8U, object + 8U);
    memory.store_u32(object + 12U, object + 8U);
    memory.store_u32(object + 16U, limit);
    kernel_semaphores[object] = GuestSemaphore{count, limit};
    return true;
  }
  if (std::string_view{name} == "KeReleaseSemaphore") {
    auto& bridge = require_bridge();
    auto& memory = bridge.memory();
    const auto object = context.r3.u32;
    const auto adjustment = context.r5.u32;
    const auto found = kernel_semaphores.find(object);
    if (found == kernel_semaphores.end() || adjustment == 0U ||
        adjustment > found->second.maximum ||
        found->second.count > found->second.maximum - adjustment) {
      return false;
    }
    const auto previous = found->second.count;
    found->second.count += adjustment;
    memory.store_u32(object + 4U, found->second.count);
    for (std::uint32_t index = 0U; index < adjustment; ++index) {
      if (bridge.wake_one_guest_waiter(kWaitKernelSemaphore, object) == 0U) {
        break;
      }
    }
    context.r3.u32 = previous;
    return true;
  }
  if (std::string_view{name} == "KeAcquireSpinLockAtRaisedIrql") {
    auto& bridge = require_bridge();
    const auto lock = context.r3.u32;
    if (!bridge.memory().mapped(lock, 4U)) {
      return false;
    }
    const auto owner = spinlock_owners.find(lock);
    if (bridge.memory().load_u32(lock) != 0U || owner != spinlock_owners.end()) {
      throw ac6demo::RuntimeTrap("contended guest spin lock", bridge.tick(),
                                 static_cast<std::uint32_t>(context.lr), lock);
    }
    bridge.memory().store_u32(lock, 1U);
    spinlock_owners.emplace(lock, current_guest_thread_id);
    return true;
  }
  if (std::string_view{name} == "KeRaiseIrqlToDpcLevel") {
    const auto previous = guest_irql[current_guest_thread_id];
    if (previous > 2U) {
      return false;
    }
    guest_irql[current_guest_thread_id] = 2U;
    context.r3.u32 = previous;
    return true;
  }
  if (std::string_view{name} == "KfLowerIrql") {
    const auto current = guest_irql[current_guest_thread_id];
    if (context.r3.u32 > current || context.r3.u32 > 2U) {
      return false;
    }
    guest_irql[current_guest_thread_id] =
        static_cast<std::uint8_t>(context.r3.u32);
    return true;
  }
  if (std::string_view{name} == "KeReleaseSpinLockFromRaisedIrql") {
    auto& bridge = require_bridge();
    const auto lock = context.r3.u32;
    const auto owner = spinlock_owners.find(lock);
    if (!bridge.memory().mapped(lock, 4U) || owner == spinlock_owners.end() ||
        owner->second != current_guest_thread_id) {
      return false;
    }
    spinlock_owners.erase(owner);
    bridge.memory().store_u32(lock, 0U);
    return true;
  }
  if (std::string_view{name} == "KeTryToAcquireSpinLockAtRaisedIrql") {
    auto& bridge = require_bridge();
    const auto lock = context.r3.u32;
    if (!bridge.memory().mapped(lock, 4U)) {
      return false;
    }
    if (bridge.memory().load_u32(lock) != 0U || spinlock_owners.contains(lock)) {
      context.r3.u32 = 0U;
      return true;
    }
    bridge.memory().store_u32(lock, 1U);
    spinlock_owners.emplace(lock, current_guest_thread_id);
    context.r3.u32 = 1U;
    return true;
  }
  if (std::string_view{name} == "KfAcquireSpinLock") {
    auto& bridge = require_bridge();
    const auto lock = context.r3.u32;
    if (!bridge.memory().mapped(lock, 4U)) {
      return false;
    }
    if (bridge.memory().load_u32(lock) != 0U || spinlock_owners.contains(lock)) {
      throw ac6demo::RuntimeTrap("contended guest Kf spin lock", bridge.tick(),
                                 static_cast<std::uint32_t>(context.lr), lock);
    }
    const auto previous_irql = guest_irql[current_guest_thread_id];
    guest_irql[current_guest_thread_id] = 2U;
    bridge.memory().store_u32(lock, 1U);
    spinlock_owners.emplace(lock, current_guest_thread_id);
    context.r3.u32 = previous_irql;
    return true;
  }
  if (std::string_view{name} == "KfReleaseSpinLock") {
    auto& bridge = require_bridge();
    const auto lock = context.r3.u32;
    const auto owner = spinlock_owners.find(lock);
    if (!bridge.memory().mapped(lock, 4U) || owner == spinlock_owners.end() ||
        owner->second != current_guest_thread_id || context.r4.u32 > 2U) {
      return false;
    }
    spinlock_owners.erase(owner);
    bridge.memory().store_u32(lock, 0U);
    guest_irql[current_guest_thread_id] = static_cast<std::uint8_t>(context.r4.u32);
    return true;
  }
  if (std::string_view{name} == "NtPulseEvent") {
    auto& bridge = require_bridge();
    const auto event_handle = context.r3.u32;
    const auto found = events.find(event_handle);
    if (found == events.end()) {
      context.r3.s64 = -1;
      return true;
    }
    const auto previous_state = found->second.signaled ? 1U : 0U;
    if (context.r4.u32 != 0U && !bridge.memory().mapped(context.r4.u32, 4U)) {
      return false;
    }
    if (context.r4.u32 != 0U) {
      bridge.memory().store_u32(context.r4.u32, previous_state);
    }
    ac6demo::guest_bridge_detail::trace_event_handoff(
        "pulse_enter", event_handle, 0U, kWaitEvent,
        current_guest_thread_id, bridge.tick(),
        static_cast<std::uint32_t>(context.lr), previous_state, 0U, 0U);
    publish_guest_event(bridge, event_handle, found->second,
                        static_cast<std::uint32_t>(context.lr));
    // NtPulseEvent releases current waiters and then returns the event to its
    // nonsignaled state; it does not leave a synthetic signal latched.
    found->second.signaled = false;
    ac6demo::guest_bridge_detail::trace_event_handoff(
        "pulse_exit", event_handle, 0U, kWaitEvent,
        current_guest_thread_id, bridge.tick(),
        static_cast<std::uint32_t>(context.lr), 0U, 0U, 0U);
    context.r3.s64 = 0;
    return true;
  }
  if (std::string_view{name} == "NtSetEvent") {
    const auto event_handle = context.r3.u32;
    const auto found = events.find(event_handle);
    if (found == events.end()) {
      context.r3.s64 = -1;
      return true;
    }
    context.r3.s64 = found->second.signaled ? 1 : 0;
    ac6demo::guest_bridge_detail::trace_event_handoff(
        "set_enter", event_handle, 0U, kWaitEvent,
        current_guest_thread_id, require_bridge().tick(),
        static_cast<std::uint32_t>(context.lr),
        found->second.signaled ? 1U : 0U, 0U, 0U);
    publish_guest_event(require_bridge(), event_handle, found->second,
                        static_cast<std::uint32_t>(context.lr));
    ac6demo::guest_bridge_detail::trace_event_handoff(
        "set_exit", event_handle, 0U, kWaitEvent,
        current_guest_thread_id, require_bridge().tick(),
        static_cast<std::uint32_t>(context.lr),
        found->second.signaled ? 1U : 0U, found->second.granted_thread, 0U);
    return true;
  }
  if (std::string_view{name} == "NtClearEvent") {
    auto& bridge = require_bridge();
    const auto event_handle = context.r3.u32;
    const auto found = events.find(event_handle);
    if (found == events.end()) {
      context.r3.s64 = -1;
      return true;
    }
    context.r3.s64 = found->second.signaled ? 1 : 0;
    ac6demo::guest_bridge_detail::trace_event_handoff(
        "clear", event_handle, 0U, kWaitEvent, current_guest_thread_id,
        bridge.tick(), static_cast<std::uint32_t>(context.lr),
        found->second.signaled ? 1U : 0U, found->second.granted_thread, 0U);
    found->second.signaled = false;
    return true;
  }
  if (std::string_view{name} == "KeResetEvent") {
    auto& memory = memory_for(context);
    const auto object = context.r3.u32;
    if (!memory.mapped(object, 8U)) {
      return false;
    }
    context.r3.u32 = memory.load_u32(object + 4U);
    memory.store_u32(object + 4U, 0U);
    return true;
  }
  if (std::string_view{name} == "KeSetEvent") {
    auto& bridge = require_bridge();
    auto& memory = bridge.memory();
    const auto object = context.r3.u32;
    if (!memory.mapped(object, 8U)) {
      return false;
    }
    context.r3.u32 = memory.load_u32(object + 4U);
    memory.store_u32(object + 4U, 1U);
    record_event_publication(object, static_cast<std::uint32_t>(context.lr), 2U);
    bridge.wake_guest_waiters(kWaitKernelEvent, object);
    bridge.wake_guest_waiters(kWaitKernelEvent, kWaitMultipleKey);
    return true;
  }
  if (std::string_view{name} == "NtReleaseMutant") {
    const auto handle = context.r3.u32;
    const auto found = mutants.find(handle);
    if (found == mutants.end() || found->second.owner != current_guest_thread_id ||
        found->second.recursion == 0U) {
      context.r3.s64 = -1;
      return true;
    }
    context.r3.s64 = 0;
    if (--found->second.recursion == 0U) {
      found->second.owner = 0U;
      require_bridge().wake_guest_waiters(kWaitMutant, handle);
    }
    return true;
  }
  // The kernel-event side of the same question. The graphics worker threads
  // wait on KEVENTs, which only these three primitives can move, and a run
  // that never calls them is a run whose workers never wake.
  if ((std::string_view{name} == "KeSetEvent" ||
       std::string_view{name} == "KePulseEvent" ||
       std::string_view{name} == "KeResetEvent") &&
      std::getenv("AC6_DEMO_WATCH_SEMAPHORES") != nullptr) {
    std::fprintf(stderr,
                 "AC6_KEVENT tick=%llu thread=%u name=%s object=0x%08X "
                 "lr=0x%08X\n",
                 static_cast<unsigned long long>(require_bridge().tick()),
                 current_guest_thread_id, name, context.r3.u32,
                 static_cast<std::uint32_t>(context.lr));
  }
  // Which semaphore is released, and how often. Cycle 1777 recorded the
  // render queue's consumer index never changing, which reads as "the worker
  // is never woken"; this is the number that decides whether that reading is
  // about waking at all.
  if ((std::string_view{name} == "NtReleaseSemaphore" ||
       std::string_view{name} == "KeReleaseSemaphore") &&
      std::getenv("AC6_DEMO_WATCH_SEMAPHORES") != nullptr) {
    std::fprintf(stderr,
                 "AC6_SEM_RELEASE tick=%llu thread=%u name=%s handle=0x%08X "
                 "count=%u lr=0x%08X\n",
                 static_cast<unsigned long long>(require_bridge().tick()),
                 current_guest_thread_id, name, context.r3.u32, context.r4.u32,
                 static_cast<std::uint32_t>(context.lr));
  }
  if (std::string_view{name} == "NtReleaseSemaphore") {
    const auto handle = context.r3.u32;
    const auto found = semaphores.find(handle);
    if (found == semaphores.end() || context.r4.u32 == 0U ||
        context.r4.u32 > found->second.maximum ||
        found->second.count > found->second.maximum - context.r4.u32) {
      context.r3.s64 = -1;
      return true;
    }
    if (context.r5.u32 != 0U && !memory_for(context).mapped(context.r5.u32, 4U)) {
      return false;
    }
    if (context.r5.u32 != 0U) {
      memory_for(context).store_u32(context.r5.u32, found->second.count);
    }
    found->second.count += context.r4.u32;
    require_bridge().wake_guest_waiters(kWaitSemaphore, handle);
    context.r3.s64 = 0;
    return true;
  }
