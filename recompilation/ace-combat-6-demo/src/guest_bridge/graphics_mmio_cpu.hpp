} // namespace ac6demo

extern "C" void AC6_PPC_EIEIO(PPCContext &) noexcept {}
extern "C" void AC6_PPC_SYNC(PPCContext &) noexcept {}
extern "C" void AC6_PPC_LWSYNC(PPCContext &) noexcept {}

extern "C" std::uint64_t AC6_PPC_READ_TIMEBASE(PPCContext &) noexcept {
  return (require_bridge().tick() * 50'000'000ULL) / 60ULL;
}

extern "C" std::uint32_t AC6_PPC_LWARX(PPCContext &context,
                                       std::uint32_t address) {
  if ((address & 3U) != 0U) {
    throw ac6demo::RuntimeTrap("unaligned guest lwarx", require_bridge().tick(),
                               static_cast<std::uint32_t>(context.lr), address);
  }
  auto &memory = memory_for(context);
  const auto value = memory.load_u32(address);
  reservations[&context] =
      Reservation{address, memory.write_generation(address), 0U, 4U, true};
  ac6demo::guest_bridge_detail::refuse_post_resume_atomic(
      "lwarx", address, 4U, require_bridge().tick(), current_guest_thread_id,
      static_cast<std::uint32_t>(context.lr));
  ac6demo::guest_bridge_detail::record_xam_return_chain_atomic(
      "lwarx", address, 4U, value, true, require_bridge().tick(),
      current_guest_thread_id, static_cast<std::uint32_t>(context.lr),
      current_load_generated_name, current_load_generated_line);
  return value;
}

extern "C" bool AC6_PPC_STWCX(PPCContext &context, std::uint32_t address,
                              std::uint32_t value) {
  if ((address & 3U) != 0U) {
    throw ac6demo::RuntimeTrap("unaligned guest stwcx", require_bridge().tick(),
                               static_cast<std::uint32_t>(context.lr), address);
  }
  auto &memory = memory_for(context);
  const auto found = reservations.find(&context);
  const bool success =
      found != reservations.end() && found->second.valid &&
      found->second.width == 4U &&
      found->second.address == address &&
      found->second.generation == memory.write_generation(address);
  if (found != reservations.end()) {
    found->second.valid = false;
  }
  if (success) {
    memory.store_u32(address, value);
  }
  ac6demo::guest_bridge_detail::refuse_post_resume_atomic(
      "stwcx", address, 4U, require_bridge().tick(), current_guest_thread_id,
      static_cast<std::uint32_t>(context.lr),
      success ? "atomic_success" : "atomic_failed");
  ac6demo::guest_bridge_detail::record_xam_return_chain_atomic(
      "stwcx", address, 4U, value, success, require_bridge().tick(),
      current_guest_thread_id, static_cast<std::uint32_t>(context.lr),
      current_load_generated_name, current_load_generated_line);
  return success;
}

extern "C" __attribute__((noinline)) std::uint64_t
AC6_PPC_LDARX(PPCContext &context, std::uint32_t address) {
  if ((address & 7U) != 0U) {
    throw ac6demo::RuntimeTrap("unaligned guest ldarx", require_bridge().tick(),
                               static_cast<std::uint32_t>(context.lr), address);
  }
  auto &memory = memory_for(context);
  // Do not route an atomic load through AC6_PPC_LOAD_U64: that would present
  // a reservation instruction as an ordinary scalar access to the probe.
  const auto value = memory.load_u64(address);
  reservations[&context] = Reservation{
      address, memory.write_generation(address),
      memory.write_generation(address + 4U), 8U, true};
  ac6demo::guest_bridge_detail::refuse_post_resume_atomic(
      "ldarx", address, 8U, require_bridge().tick(), current_guest_thread_id,
      static_cast<std::uint32_t>(context.lr));
  ac6demo::guest_bridge_detail::record_xam_return_chain_atomic(
      "ldarx", address, 8U, value, true, require_bridge().tick(),
      current_guest_thread_id, static_cast<std::uint32_t>(context.lr),
      current_load_generated_name, current_load_generated_line);
  return value;
}

extern "C" __attribute__((noinline)) bool
AC6_PPC_STDCX(PPCContext &context, std::uint32_t address,
              std::uint64_t value) {
  if ((address & 7U) != 0U) {
    throw ac6demo::RuntimeTrap("unaligned guest stdcx", require_bridge().tick(),
                               static_cast<std::uint32_t>(context.lr), address);
  }
  auto &memory = memory_for(context);
  const auto found = reservations.find(&context);
  const bool success =
      found != reservations.end() && found->second.valid &&
      found->second.width == 8U && found->second.address == address &&
      found->second.generation == memory.write_generation(address) &&
      found->second.trailing_generation == memory.write_generation(address + 4U);
  if (found != reservations.end()) {
    found->second.valid = false;
  }
  if (success) {
    // As with LDARX, keep the reservation route out of the ordinary scalar
    // probe path and classify it explicitly as unsupported.
    memory.store_u64(address, value);
  }
  ac6demo::guest_bridge_detail::refuse_post_resume_atomic(
      "stdcx", address, 8U, require_bridge().tick(), current_guest_thread_id,
      static_cast<std::uint32_t>(context.lr),
      success ? "atomic_success" : "atomic_failed");
  ac6demo::guest_bridge_detail::record_xam_return_chain_atomic(
      "stdcx", address, 8U, value, success, require_bridge().tick(),
      current_guest_thread_id, static_cast<std::uint32_t>(context.lr),
      current_load_generated_name, current_load_generated_line);
  return success;
}

extern "C" void AC6_PPC_VPKSWSS(PPCVRegister &destination,
                                const PPCVRegister &left,
                                const PPCVRegister &right) noexcept {
  for (std::size_t lane = 0; lane < 4U; ++lane) {
    destination.s16[lane * 2U] = saturate_s16(left.s32[lane]);
    destination.s16[lane * 2U + 1U] = saturate_s16(right.s32[lane]);
  }
}

extern "C" void AC6_PPC_VCMPBFP(PPCVRegister &destination,
                                const PPCVRegister &left,
                                const PPCVRegister &right,
                                PPCCRRegister &condition,
                                bool record) noexcept {
  bool all_in_bounds = true;
  for (std::size_t lane = 0; lane < 4U; ++lane) {
    const float value = left.f32[lane];
    const float bound = right.f32[lane];
    const bool low = !(value >= -bound);
    const bool high = !(value <= bound);
    all_in_bounds = all_in_bounds && !low && !high;
    destination.u32[lane] =
        (low ? 0x80000000U : 0U) | (high ? 0x40000000U : 0U);
  }
  if (record) {
    condition.lt = 0;
    condition.gt = 0;
    condition.eq = all_in_bounds ? 1U : 0U;
    condition.so = 0;
  }
}

extern "C" void AC6_PPC_VREFP(PPCVRegister &destination,
                              const PPCVRegister &source) noexcept {
  for (std::size_t lane = 0; lane < 4U; ++lane) {
    destination.f32[lane] =
        ac6demo::xenon_reciprocal_estimate(source.f32[lane]);
  }
}

extern "C" void AC6_PPC_VRSQRTEFP(PPCVRegister &destination,
                                  const PPCVRegister &source) noexcept {
  for (std::size_t lane = 0; lane < 4U; ++lane) {
    destination.f32[lane] = ac6demo::xenon_rsqrt_estimate(source.f32[lane]);
  }
}

extern "C" bool AC6_PPC_IMPORT_DISPATCH(PPCContext &context, std::uint8_t *,
                                        const char *module, const char *name,
                                        std::uint16_t ordinal) {
  return dispatch_import(context, module, name, ordinal);
}
