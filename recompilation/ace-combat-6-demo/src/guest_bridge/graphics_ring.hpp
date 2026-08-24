// Internal GuestBridge domain fragment; included only by guest_bridge.cpp.
inline void trace_xenos_ib_read(
    const char* region, std::uint32_t address, std::uint32_t offset_dword,
    std::uint32_t value, std::uint64_t tick, std::uint32_t thread,
    std::uint32_t lr, const char* generated_name,
    std::uint32_t generated_line) noexcept {
  static const bool enabled =
      std::getenv("AC6_DEMO_WATCH_IB_READERS") != nullptr;
  static std::uint32_t record_count = 0U;
  if (!enabled || record_count >= 16384U || region == nullptr) {
    return;
  }
  ++record_count;
  std::fprintf(
      stderr,
      "AC6_IB_READ region=%s address=0x%08X offset_dword=%u value=0x%08X "
      "tick=%llu thread=%u lr=0x%08X function=%s generated_line=%u\n",
      region, address, offset_dword, value,
      static_cast<unsigned long long>(tick), thread, lr,
      generated_name == nullptr ? "" : generated_name, generated_line);
}

inline void trace_observed_main_ib_reads(
    std::uint32_t address, std::span<const std::uint32_t> dwords,
    std::uint64_t tick, std::uint32_t thread, std::uint32_t lr,
    const char* generated_name, std::uint32_t generated_line) noexcept {
  if (address != 0x1274A000U) {
    return;
  }
  constexpr std::array<std::uint32_t, 6U> kObservedOffsets{
      0U, 239U, 387U, 408U, 415U, 3028U};
  for (const auto offset : kObservedOffsets) {
    if (offset < dwords.size()) {
      trace_xenos_ib_read("main_ib", address + offset * 4U, offset,
                          dwords[offset], tick, thread, lr, generated_name,
                          generated_line);
    }
  }
}

inline void record_xenos_packet(XenosPacketCensusSnapshot& census,
                                std::uint64_t tick, std::uint32_t header,
                                std::uint32_t payload_count) {
  const auto packet_type = header >> 30U;
  ++census.packet_count;
  census.decoded_dword_count += payload_count + 1U;
  ++census.type_counts[packet_type];
  if (packet_type == 0U || packet_type == 2U) {
    return;
  }
  if (packet_type == 1U) {
    throw RuntimeTrap("unqualified Xenos type-1 packet", tick, 0, header);
  }
  const auto opcode = (header >> 8U) & 0x7FU;
  ++census.type3_opcode_counts[opcode];
  const auto exact_count = [payload_count](std::uint32_t expected) {
    return payload_count == expected;
  };
  const bool qualified = (opcode == 0x21U && exact_count(3U)) ||
                         (opcode == 0x22U && exact_count(2U)) ||
                         (opcode == 0x27U && exact_count(2U)) ||
                         (opcode == 0x2BU && payload_count >= 2U) ||
                         (opcode == 0x2FU && exact_count(3U)) ||
                         (opcode == 0x36U && exact_count(1U)) ||
                         (opcode == 0x3BU && exact_count(1U)) ||
                         (opcode == 0x3CU && exact_count(5U)) ||
                         (opcode == 0x3FU && exact_count(2U)) ||
                         (opcode == 0x45U && exact_count(6U)) ||
                         (opcode == 0x46U && exact_count(1U)) ||
                         (opcode == 0x48U && exact_count(18U)) ||
                         (opcode == 0x54U && exact_count(1U)) ||
                         (opcode == 0x58U && exact_count(3U)) ||
                         (opcode == 0x60U && exact_count(1U)) ||
                         (opcode == 0x61U && exact_count(1U)) ||
                         (opcode == 0x62U && exact_count(1U)) ||
                         (opcode == 0x63U && exact_count(1U)) ||
                         (opcode == 0x64U && exact_count(4U));
  if (!qualified) {
    throw RuntimeTrap("unqualified Xenos type-3 packet", tick, 0, header);
  }
}

[[nodiscard]] bool is_xenos_semantic_packet(std::uint32_t header) noexcept {
  if ((header >> 30U) == 0U) {
    return true;
  }
  const auto opcode = (header >> 8U) & 0x7FU;
  return (header >> 30U) == 3U &&
         (opcode == 0x21U || opcode == 0x22U || opcode == 0x27U ||
          opcode == 0x2BU ||
          opcode == 0x2FU || opcode == 0x36U ||
          opcode == 0x3BU || opcode == 0x3CU || opcode == 0x45U ||
          opcode == 0x46U || opcode == 0x48U || opcode == 0x54U ||
          opcode == 0x58U || opcode == 0x60U || opcode == 0x61U ||
          opcode == 0x62U || opcode == 0x63U || opcode == 0x64U);
}

void trace_ib_capture(const GuestMemory &memory, std::uint32_t address,
                      std::uint32_t dword_count, std::uint64_t tick) {
  if (std::getenv("AC6_DEMO_WATCH_IB_WRITERS") == nullptr) {
    return;
  }
  std::fprintf(stderr,
               "AC6_IB_CAPTURE address=0x%08X dwords=%u tick=%llu "
               "first_generation=%llu last_generation=%llu\n",
               address, dword_count, static_cast<unsigned long long>(tick),
               static_cast<unsigned long long>(memory.write_generation(address)),
               static_cast<unsigned long long>(memory.write_generation(
                   address + dword_count * 4U - 4U)));

  const char* lo_str = std::getenv("AC6_DEMO_WATCH_ADDR_LO");
  const char* hi_str = std::getenv("AC6_DEMO_WATCH_ADDR_HI");
  if (lo_str == nullptr || hi_str == nullptr) {
    return;
  }
  const auto lo = static_cast<std::uint32_t>(std::strtoul(lo_str, nullptr, 0));
  const auto hi = static_cast<std::uint32_t>(std::strtoul(hi_str, nullptr, 0));
  const std::uint64_t end = static_cast<std::uint64_t>(address) +
                            static_cast<std::uint64_t>(dword_count) * 4U;
  if (lo >= hi || address < lo || end > hi) {
    return;
  }
  std::fprintf(stderr, "AC6_IB_PAYLOAD address=0x%08X dwords=%u values=",
               address, dword_count);
  for (std::uint32_t index = 0U; index < dword_count; ++index) {
    std::fprintf(stderr, "%s%08X", index == 0U ? "" : ",",
                 memory.load_u32(address + index * 4U));
  }
  std::fputc('\n', stderr);
}

void trace_qualified_title_shader_capture(
    const GuestMemory &memory, std::uint32_t address,
    std::span<const std::uint32_t> dwords, std::uint64_t tick) {
  if (std::getenv("AC6_DEMO_WATCH_IB_WRITERS") == nullptr ||
      address != 0x129CA640U || dwords.size() != 2765U) {
    return;
  }
  static constexpr std::array pixel_packet{
      0xC0012700U, 0x155FAB81U, 0x0000000FU};
  static constexpr std::array constant_packet{
      0xC0022F00U, 0x155FAA00U, 0x000003F0U, 0x00000010U};
  static constexpr std::array vertex_packet{
      0xC0012700U, 0x155FAA40U, 0x00000042U};
  const auto contains = [&](const auto &packet) {
    return std::ranges::search(dwords, packet).begin() != dwords.end();
  };
  if (!contains(pixel_packet) || !contains(constant_packet) ||
      !contains(vertex_packet)) {
    return;
  }
  struct Range final {
    const char *tag;
    std::uint32_t address;
    std::uint32_t dwords;
  };
  static constexpr std::array ranges{
      Range{"constants", 0x155FAA00U, 16U},
      Range{"vs", 0x155FAA40U, 66U},
      Range{"ps", 0x155FAB80U, 15U},
  };
  for (const auto &range : ranges) {
    if (!memory.mapped(range.address,
                       static_cast<std::size_t>(range.dwords) * 4U)) {
      return;
    }
  }
  for (const auto &range : ranges) {
    std::fprintf(stderr,
                 "AC6_TITLE_SHADER_CAPTURE tag=%s ib=0x%08X tick=%llu "
                 "address=0x%08X dwords=%u values=",
                 range.tag, address, static_cast<unsigned long long>(tick),
                 range.address, range.dwords);
    for (std::uint32_t index = 0U; index < range.dwords; ++index) {
      std::fprintf(stderr, "%s%08X", index == 0U ? "" : ",",
                   memory.load_u32(range.address + index * 4U));
    }
    std::fputc('\n', stderr);
  }
}

void GuestBridge::configure_xenos_ring(std::uint32_t base,
                                       std::uint32_t size_log2) noexcept {
  if (xenos_ring_base_ != base || xenos_ring_size_log2_ != size_log2) {
    xenos_ring_rptr_ = 0U;
    xenos_mmio_wptr_ = 0U;
    xenos_ring_last_wptr_ = 0U;
    xenos_ring_owner_endpoint_ = 0U;
    xenos_ring_submissions_ = 0U;
    xenos_ring_pointer_mismatches_ = 0U;
    xenos_ring_submitted_dwords_ = 0U;
    xenos_ring_max_submission_dwords_ = 0U;
    xenos_ring_recent_submission_count_ = 0U;
    xenos_ring_recent_submissions_ = {};
    xenos_indirect_buffer_count_ = 0U;
    xenos_indirect_buffers_ = {};
    xenos_packet_census_ = {};
  }
  xenos_ring_base_ = base;
  xenos_ring_size_log2_ = size_log2;
}

XenosRingSnapshot GuestBridge::xenos_ring_snapshot() const noexcept {
  XenosRingSnapshot result;
  result.initialized = xenos_ring_base_ != 0U && xenos_ring_size_log2_ < 31U;
  result.base = xenos_ring_base_;
  result.size_log2 = xenos_ring_size_log2_;
  if (result.initialized) {
    result.capacity_dwords = static_cast<std::uint32_t>(
        std::uint64_t{1} << (xenos_ring_size_log2_ + 1U));
  }
  result.read_pointer = xenos_ring_rptr_;
  result.write_pointer = xenos_mmio_wptr_;
  result.owner_endpoint = xenos_ring_owner_endpoint_;
  result.submissions = xenos_ring_submissions_;
  result.pointer_mismatches = xenos_ring_pointer_mismatches_;
  result.submitted_dwords = xenos_ring_submitted_dwords_;
  result.max_submission_dwords = xenos_ring_max_submission_dwords_;
  result.recent_submission_count = xenos_ring_recent_submission_count_;
  result.recent_submissions = xenos_ring_recent_submissions_;
  result.indirect_buffer_count = xenos_indirect_buffer_count_;
  result.indirect_buffers = xenos_indirect_buffers_;
  result.packet_census = xenos_packet_census_;
  result.typed_commands = xenos_typed_commands_;
  result.effects = xenos_effects_;
  return result;
}

std::vector<XenosRendererBatch> GuestBridge::consume_xenos_renderer_batches() {
  std::vector<XenosRendererBatch> result;
  result.swap(xenos_renderer_batches_);
  return result;
}

std::vector<XenosRendererBatch> GuestBridge::capture_xenos_renderer_batches(
    std::span<const XenosCommand> commands,
    std::span<const std::size_t> write_counts,
    std::span<const XenosGuestMemoryWrite> writes) const {
  if (commands.size() != write_counts.size()) {
    throw RuntimeTrap("Xenos renderer command/write boundary mismatch", tick_);
  }
  std::vector<XenosRendererBatch> result;
  std::size_t first = 0U;
  for (std::size_t index = 0U; index < commands.size(); ++index) {
    const auto *draw = std::get_if<XenosDrawCommand>(&commands[index]);
    const bool renderer_draw = draw != nullptr &&
                               draw->primitive != XenosPrimitive::PointList;
    const bool present =
        std::holds_alternative<XenosPresentCommand>(commands[index]);
    if (renderer_draw || present) {
      result.push_back(capture_xenos_renderer_batch(
          commands.subspan(first, index + 1U - first),
          write_counts.subspan(first, index + 1U - first), writes));
      first = index + 1U;
    }
  }
  if (first < commands.size()) {
    result.push_back(capture_xenos_renderer_batch(
        commands.subspan(first), write_counts.subspan(first), writes));
  }
  return result;
}

XenosRendererBatch GuestBridge::capture_xenos_renderer_batch(
    std::span<const XenosCommand> commands,
    std::span<const std::size_t> write_counts,
    std::span<const XenosGuestMemoryWrite> writes) const {
  if (commands.size() != write_counts.size()) {
    throw RuntimeTrap("Xenos renderer event/write boundary mismatch", tick_);
  }
  XenosRendererBatch renderer_batch;
  renderer_batch.commands.assign(commands.begin(), commands.end());
  const auto snapshot = [this, &renderer_batch, writes](
                            std::uint32_t address, std::size_t size,
                            bool physical, std::size_t visible_writes) {
    if (visible_writes > writes.size()) {
      throw RuntimeTrap("Xenos renderer write boundary exceeds batch", tick_,
                        0, address);
    }
    const auto alias = physical ? resolve_physical_alias(address, size)
                                : std::optional<std::uint32_t>{address};
    if (!alias.has_value() || !memory_.mapped(*alias, size)) {
      throw RuntimeTrap("Xenos renderer payload has no unique guest source",
                        tick_, 0, address);
    }
    const auto duplicate = std::ranges::find_if(
        renderer_batch.payloads, [address](const auto &payload) {
          return payload.address == address;
        });
    if (duplicate != renderer_batch.payloads.end()) {
      if (duplicate->bytes.size() != size) {
        throw RuntimeTrap("Xenos renderer payload extent changed", tick_, 0,
                          address);
      }
      return;
    }
    auto bytes = memory_.load_bytes(*alias, size);
    for (std::size_t index = 0U; index < visible_writes; ++index) {
      const auto &write = writes[index];
      const auto write_end = static_cast<std::uint64_t>(write.address) +
                             write.guest_bytes.size();
      const auto range_end = static_cast<std::uint64_t>(*alias) + size;
      if (write.address >= range_end || write_end <= *alias) {
        continue;
      }
      const auto begin = std::max(write.address, *alias);
      const auto end = static_cast<std::uint32_t>(
          std::min(write_end, range_end));
      std::copy_n(write.guest_bytes.begin() + (begin - write.address),
                  end - begin, bytes.begin() + (begin - *alias));
    }
    renderer_batch.payloads.push_back({address, std::move(bytes)});
  };
  static constexpr std::string_view kTitleVertex =
      "84e2d87ca4c7e6b6463cd007778e3e76f2d33d5b3a2bdf71bdabedf5e2949e6b";
  for (std::size_t command_index = 0U;
       command_index < renderer_batch.commands.size(); ++command_index) {
    const auto &command = renderer_batch.commands[command_index];
    const auto *draw = std::get_if<XenosDrawCommand>(&command);
    if (draw == nullptr || !draw->registers ||
        draw->primitive == XenosPrimitive::PointList) {
      continue;
    }
    if (draw->primitive == XenosPrimitive::RectangleList) {
      const auto fetch0 = draw->registers->value(kXenosTextureFetch00);
      const auto fetch1 = draw->registers->value(kXenosTextureFetch00 + 1U);
      const auto address = (fetch0 >> 2U) * 4U;
      const auto dwords = (fetch1 >> 2U) & 0x00FFFFFFU;
      if ((fetch0 & 3U) != 3U || dwords == 0U || dwords > 64U) {
        throw RuntimeTrap("unqualified renderer vertex snapshot", tick_, 0,
                          address);
      }
      snapshot(address, static_cast<std::size_t>(dwords) * 4U, false,
               write_counts[command_index]);
      continue;
    }
    if (draw->primitive != XenosPrimitive::QuadList ||
        draw->vertex_shader_sha256 != kTitleVertex) {
      throw RuntimeTrap("unqualified renderer payload draw", tick_);
    }
    // Xenos' disassembler names the first title fetch `vf0`, whose source
    // mapping is fetch constant 95 (Xenia's `vf{95 - storage_index}`).  The
    // draw therefore consumes the P allocation in slot 95; Q is retained as
    // the adjacent coherency/profile guard.
    constexpr std::uint16_t kTitleVertexFetch =
        kXenosTextureFetch00 + 95U * 2U;
    constexpr std::uint16_t kTitleAdjacentFetch = kTitleVertexFetch - 2U;
    const auto title_fetch0 = draw->registers->value(kTitleVertexFetch);
    const auto title_fetch1 = draw->registers->value(kTitleVertexFetch + 1U);
    const auto adjacent_fetch0 =
        draw->registers->value(kTitleAdjacentFetch);
    const auto adjacent_fetch1 =
        draw->registers->value(kTitleAdjacentFetch + 1U);
    const bool p1_q1 =
        title_fetch0 == 0x103FB893U && title_fetch1 == 0x100A9002U &&
        adjacent_fetch0 == 0x104A4893U && adjacent_fetch1 == 0x100087F2U;
    const bool p2_q2 =
        title_fetch0 == 0x104AF123U && title_fetch1 == 0x100A9002U &&
        adjacent_fetch0 == 0x10558123U && adjacent_fetch1 == 0x100087F2U;
    const bool p0_q0 =
        title_fetch0 == 0x10348003U && title_fetch1 == 0x100A9002U &&
        adjacent_fetch0 == 0x103F1003U && adjacent_fetch1 == 0x100087F2U;
    if (!p1_q1 && !p2_q2 && !p0_q0) {
      std::ostringstream values;
      values << "qualified title vertex fetch profile changed: p="
             << std::hex << title_fetch0 << "," << title_fetch1
             << " q=" << adjacent_fetch0 << "," << adjacent_fetch1;
      throw RuntimeTrap(values.str(), tick_);
    }
    const auto title_texture =
        qualified_title_texture_profile(*draw->registers);
    if (!title_texture.has_value()) {
        std::ostringstream values;
        values << "qualified title texture fetch changed: f0=0x" << std::hex
               << draw->registers->value(kXenosTextureFetch00 + 0U)
               << " f1=0x" << draw->registers->value(kXenosTextureFetch00 + 1U)
               << " f2=0x" << draw->registers->value(kXenosTextureFetch00 + 2U)
               << " f3=0x" << draw->registers->value(kXenosTextureFetch00 + 3U)
               << " f4=0x" << draw->registers->value(kXenosTextureFetch00 + 4U)
               << " f5=0x" << draw->registers->value(kXenosTextureFetch00 + 5U);
        throw RuntimeTrap(values.str(), tick_);
    }
    const auto title_vertex_size = qualified_title_vertex_snapshot_size(
        draw->registers->value(0x2102U), draw->index_count);
    if (!title_vertex_size.has_value()) {
      std::ostringstream values;
      values << "qualified title vertex window changed: base="
             << draw->registers->value(0x2102U)
             << " count=" << draw->index_count;
      throw RuntimeTrap(values.str(), tick_);
    }
    snapshot(title_fetch0 & ~3U, *title_vertex_size, true,
             write_counts[command_index]);
    snapshot(title_texture->address, title_texture->payload_size, true,
             write_counts[command_index]);
  }
  return renderer_batch;
}

void GuestBridge::set_xenos_ring_owner(std::uint32_t address) noexcept {
  xenos_ring_owner_ = address;
}

void GuestBridge::enable_xenos_read_pointer_writeback(
    std::uint32_t address) noexcept {
  xenos_ring_rptr_writeback_ = address;
}

std::size_t
GuestBridge::apply_xenos_typed_batch(std::span<const std::uint32_t> stream) {
  auto next_processor = xenos_command_processor_;
  auto next_commands = xenos_typed_commands_;
  auto next_effects = xenos_effects_;
  auto next_renderer_batches = xenos_pending_renderer_batches_;
  const auto read_memory = [this](std::uint32_t address)
      -> std::optional<XenosCommandProcessor::GuestBytes> {
    if (!memory_.mapped(address, 4U)) {
      return std::nullopt;
    }
    const auto bytes = memory_.load_bytes(address, 4U);
    XenosCommandProcessor::GuestBytes result{};
    std::copy(bytes.begin(), bytes.end(), result.begin());
    return result;
  };
  const auto batch = next_processor.process_batch(stream, read_memory);
  if (batch.renderer_commands.size() !=
      batch.renderer_write_counts.size()) {
    throw RuntimeTrap("Xenos renderer command/write count mismatch", tick_);
  }
  for (const auto &write : batch.memory_writes) {
    if (!memory_.mapped(write.address, write.guest_bytes.size())) {
      throw RuntimeTrap("Xenos staged write became unmapped", tick_, 0,
                        write.address);
    }
  }
  for (const auto cpu : batch.cpu_interrupts) {
    if (cpu != 2U) {
      throw RuntimeTrap("unqualified Xenos interrupt CPU", tick_, 0, cpu);
    }
  }
  for (const auto &command : batch) {
    if (const auto *shader = std::get_if<XenosShaderLoadCommand>(&command)) {
      if (next_commands.shader_load_count <
          next_commands.shader_sha256.size()) {
        next_commands.shader_sha256[next_commands.shader_load_count++] =
            shader->guest_big_endian_sha256;
      } else {
        std::move(next_commands.shader_sha256.begin() + 1,
                  next_commands.shader_sha256.end(),
                  next_commands.shader_sha256.begin());
        next_commands.shader_sha256.back() = shader->guest_big_endian_sha256;
      }
      continue;
    }
    if (const auto *draw = std::get_if<XenosDrawCommand>(&command)) {
#ifdef AC6_DEMO_GENERATED_GUEST
      trace_xenos_point_draw(*draw, memory_, tick_, current_guest_thread_id);
#else
      trace_xenos_point_draw(*draw, memory_, tick_, 0U);
#endif
      XenosTypedDrawSnapshot *snapshot{};
      if (next_commands.draw_count < next_commands.draws.size()) {
        snapshot = &next_commands.draws[next_commands.draw_count++];
      } else {
        std::move(next_commands.draws.begin() + 1,
                  next_commands.draws.end(), next_commands.draws.begin());
        snapshot = &next_commands.draws.back();
      }
      snapshot->index_count = draw->index_count;
      snapshot->predicated = draw->predicated;
      snapshot->vertex_shader_sha256 = draw->vertex_shader_sha256;
      snapshot->pixel_shader_sha256 = draw->pixel_shader_sha256;
      if (std::getenv("AC6_DEMO_WATCH_RESOLVE") != nullptr &&
          draw->primitive == XenosPrimitive::RectangleList) {
        const auto fetch0 = draw->registers->value(kXenosTextureFetch00);
        const auto fetch1 = draw->registers->value(kXenosTextureFetch00 + 1U);
        std::fprintf(stderr,
                     "AC6_RECT_DRAW mode=0x%08X fetch0=0x%08X "
                     "fetch1=0x%08X tick=%llu\n",
                     draw->registers->value(0x2208U),
                     fetch0, fetch1,
                     static_cast<unsigned long long>(tick_));
        // Raw state snapshot only: the generic Xenos register names are not
        // promoted to PAL semantics here.  Keep the complete bounded set so
        // an offline decoder can join RT dimensions, viewport and copy state
        // without inventing values from a partial packet preview.
        const auto &regs = *draw->registers;
        std::fprintf(
            stderr,
            "AC6_RECT_STATE surface=0x%08X color0=0x%08X color1=0x%08X "
            "color2=0x%08X color3=0x%08X depth=0x%08X scissor_tl=0x%08X "
            "scissor_br=0x%08X vport_xscale=0x%08X vport_xoffset=0x%08X "
            "vport_yscale=0x%08X vport_yoffset=0x%08X color_mask=0x%08X "
            "depth_control=0x%08X clip_control=0x%08X vte_control=0x%08X "
            "copy_control=0x%08X copy_base=0x%08X copy_pitch=0x%08X "
            "copy_info=0x%08X tick=%llu\n",
            regs.value(0x2000U), regs.value(0x2001U), regs.value(0x2003U),
            regs.value(0x2004U), regs.value(0x2005U), regs.value(0x2002U),
            regs.value(0x200EU), regs.value(0x200FU), regs.value(0x210FU),
            regs.value(0x2110U), regs.value(0x2111U), regs.value(0x2112U),
            regs.value(0x2104U), regs.value(0x2200U), regs.value(0x2204U),
            regs.value(0x2206U), regs.value(0x2318U), regs.value(0x2319U),
            regs.value(0x231AU), regs.value(0x231BU),
            static_cast<unsigned long long>(tick_));
        const auto address = (fetch0 >> 2U) * 4U;
        const auto size = (fetch1 >> 2U) & 0x00FFFFFFU;
        const auto endian = fetch1 & 3U;
        if ((fetch0 & 3U) != 3U || size == 0U || size > 64U ||
            !memory_.mapped(address, size * 4U)) {
          throw RuntimeTrap("unqualified Xenos rectangle vertex fetch", tick_,
                            0, address);
        }
        const auto bytes = memory_.load_bytes(address, size * 4U);
        std::fprintf(stderr,
                     "AC6_RECT_VERTEX address=0x%08X dwords=%u endian=%u "
                     "bytes=",
                     address, size, endian);
        for (const auto byte : bytes) {
          std::fprintf(stderr, "%02X",
                       static_cast<unsigned>(std::to_integer<std::uint8_t>(byte)));
        }
        std::fprintf(stderr, " tick=%llu\n",
                     static_cast<unsigned long long>(tick_));
      }
      if (std::getenv("AC6_DEMO_WATCH_RESOLVE") != nullptr &&
          draw->registers->value(0x2208U) == 6U) {
        const auto fetch0 = draw->registers->value(kXenosTextureFetch00);
        const auto fetch1 = draw->registers->value(kXenosTextureFetch00 + 1U);
        const auto address = (fetch0 >> 2U) * 4U;
        const auto size = (fetch1 >> 2U) & 0x00FFFFFFU;
        const auto endian = fetch1 & 3U;
        if ((fetch0 & 3U) != 3U || size != 6U || endian != 2U ||
            !memory_.mapped(address, size * 4U)) {
          throw RuntimeTrap("unqualified Xenos resolve vertex fetch", tick_, 0,
                            address);
        }
        const auto bytes = memory_.load_bytes(address, size * 4U);
        std::fprintf(stderr,
                     "AC6_RESOLVE_VERTEX address=0x%08X dwords=%u endian=%u "
                     "bytes=",
                     address, size, endian);
        for (const auto byte : bytes) {
          std::fprintf(stderr, "%02X",
                       static_cast<unsigned>(std::to_integer<std::uint8_t>(byte)));
        }
        std::fprintf(stderr, " tick=%llu\n",
                     static_cast<unsigned long long>(tick_));
      }
      continue;
    }
    const auto &present = std::get<XenosPresentCommand>(command);
    ++next_commands.present_count;
    next_commands.present_resource_id = present.resource_id;
    next_commands.present_format = present.format;
    next_commands.present_tiled = present.tiled;
    next_commands.present_width = present.width;
    next_commands.present_height = present.height;
  }
  constexpr std::size_t kRendererCommandLimit = 4096U;
  const auto published_commands = std::accumulate(
      xenos_renderer_batches_.begin(), xenos_renderer_batches_.end(),
      std::size_t{}, [](std::size_t count, const XenosRendererBatch &item) {
        return count + item.commands.size();
      });
  const auto pending_commands = std::accumulate(
      next_renderer_batches.begin(), next_renderer_batches.end(),
      std::size_t{}, [](std::size_t count, const XenosRendererBatch &item) {
        return count + item.commands.size();
      });
  if (published_commands > kRendererCommandLimit ||
      pending_commands > kRendererCommandLimit - published_commands ||
      batch.renderer_commands.size() >
          kRendererCommandLimit - published_commands - pending_commands) {
    throw RuntimeTrap("Xenos renderer command mailbox limit reached", tick_);
  }
  next_effects.counts.register_rmw += batch.effects.register_rmw;
  next_effects.counts.scratch_writeback += batch.effects.scratch_writeback;
  next_effects.counts.wait_reg_mem += batch.effects.wait_reg_mem;
  next_effects.counts.conditional_write += batch.effects.conditional_write;
  next_effects.counts.event_write += batch.effects.event_write;
  next_effects.counts.interrupt += batch.effects.interrupt;
  next_effects.counts.event_write_shader_done +=
      batch.effects.event_write_shader_done;
  next_effects.counts.invalidate_state += batch.effects.invalidate_state;
  next_effects.counts.micro_engine_init += batch.effects.micro_engine_init;
  next_effects.memory_write_count +=
      static_cast<std::uint32_t>(batch.memory_writes.size());
  next_effects.cpu_interrupt_count +=
      static_cast<std::uint32_t>(batch.cpu_interrupts.size());
  if (!batch.cpu_interrupts.empty()) {
    next_effects.last_interrupt_cpu = batch.cpu_interrupts.back();
  }
  std::array<std::byte, 256U * 4U> gamma_bytes{};
  for (std::size_t index = 0U; index < 256U; ++index) {
    const auto value =
        next_processor.gamma_lut_value(static_cast<std::uint8_t>(index));
    gamma_bytes[index * 4U] = static_cast<std::byte>(value >> 24U);
    gamma_bytes[index * 4U + 1U] = static_cast<std::byte>(value >> 16U);
    gamma_bytes[index * 4U + 2U] = static_cast<std::byte>(value >> 8U);
    gamma_bytes[index * 4U + 3U] = static_cast<std::byte>(value);
  }
  next_effects.gamma_lut_sha256 = Sha256::bytes(gamma_bytes);
  next_effects.synchronous_effects_qualified = true;
  if (!batch.renderer_commands.empty()) {
    auto renderer_batches = capture_xenos_renderer_batches(
        batch.renderer_commands, batch.renderer_write_counts,
        batch.memory_writes);
    next_renderer_batches.insert(
        next_renderer_batches.end(),
        std::make_move_iterator(renderer_batches.begin()),
        std::make_move_iterator(renderer_batches.end()));
  }
  for (const auto &write : batch.memory_writes) {
    memory_.store_bytes(write.address, write.guest_bytes);
  }
  for (const auto cpu : batch.cpu_interrupts) {
    ac6demo::guest_bridge_detail::trace_graphics_interrupt_pm4(cpu, tick_);
  }
  xenos_cp_interrupts_pending_ +=
      static_cast<std::uint32_t>(batch.cpu_interrupts.size());
  next_effects.pending_interrupt_count = xenos_cp_interrupts_pending_;
  next_effects.pending_wait = batch.pending_wait;
  next_effects.pending_wait_memory = batch.pending_wait_memory;
  next_effects.pending_wait_address = batch.pending_wait_address;
  next_effects.pending_wait_observed = batch.pending_wait_observed;
  next_effects.pending_wait_reference = batch.pending_wait_reference;
  next_effects.pending_wait_mask = batch.pending_wait_mask;
  next_effects.pending_wait_interval = batch.pending_wait_interval;
  next_commands.renderer_relevant_semantics_qualified = true;
  xenos_command_processor_ = std::move(next_processor);
  xenos_typed_commands_ = std::move(next_commands);
  xenos_effects_ = std::move(next_effects);
  xenos_pending_renderer_batches_ = std::move(next_renderer_batches);
  return batch.consumed_dwords;
}

void GuestBridge::complete_xenos_ring_submission() {
  if (!xenos_pending_renderer_batches_.empty()) {
    for (auto &renderer_batch : xenos_pending_renderer_batches_) {
      renderer_batch.sequence = ++xenos_renderer_sequence_;
      xenos_renderer_batches_.push_back(std::move(renderer_batch));
    }
    xenos_pending_renderer_batches_.clear();
  }
  ++xenos_ring_submissions_;
  xenos_ring_submitted_dwords_ += xenos_pending_submitted_dwords_;
  xenos_ring_max_submission_dwords_ = std::max(
      xenos_ring_max_submission_dwords_, xenos_pending_submitted_dwords_);
  xenos_packet_census_.reached_corpus_qualified = true;
  xenos_ring_rptr_ = xenos_pending_wptr_;
  memory_.store_u32(xenos_ring_rptr_writeback_, xenos_pending_wptr_);
  xenos_pending_stream_.clear();
  xenos_pending_wptr_ = 0U;
  xenos_pending_endpoint_ = 0U;
  xenos_pending_submitted_dwords_ = 0U;
}

void GuestBridge::resume_xenos_pending_batch() {
  if (xenos_pending_stream_.empty()) {
    return;
  }
  const auto consumed = apply_xenos_typed_batch(xenos_pending_stream_);
  if (consumed > xenos_pending_stream_.size()) {
    throw RuntimeTrap("Xenos pending batch consumed beyond its bound", tick_);
  }
  xenos_pending_stream_.erase(xenos_pending_stream_.begin(),
                              xenos_pending_stream_.begin() +
                                  static_cast<std::ptrdiff_t>(consumed));
  if (xenos_pending_stream_.empty()) {
    complete_xenos_ring_submission();
    if (xenos_ring_last_wptr_ != xenos_mmio_wptr_) {
      apply_xenos_mmio_write(0x7FC80714U, xenos_mmio_wptr_);
    }
  }
}

void GuestBridge::apply_xenos_mmio_write(
    std::uint32_t address, std::uint32_t value, std::uint32_t guest_thread,
    std::uint32_t guest_lr, const char* generated_name,
    std::uint32_t generated_line) {
  if (address != 0x7FC80714U || xenos_ring_base_ == 0U ||
      xenos_ring_rptr_writeback_ == 0U ||
      !memory_.mapped(xenos_ring_rptr_writeback_, 4U)) {
    throw RuntimeTrap("unqualified Xenos ring write", tick_, 0, address);
  }
  if (xenos_ring_size_log2_ >= 31U) {
    throw RuntimeTrap("unqualified Xenos ring size", tick_, 0,
                      xenos_ring_size_log2_);
  }
  const auto capacity_dwords = static_cast<std::uint32_t>(
      std::uint64_t{1} << (xenos_ring_size_log2_ + 1U));
  if (value >= capacity_dwords) {
    throw RuntimeTrap("unqualified Xenos ring write pointer", tick_, 0, value);
  }
  if (!xenos_pending_stream_.empty()) {
    // CP_RB_WPTR is a producer pointer.  A memory wait stalls command
    // consumption, not guest publication; retain the latest producer target
    // and drain it only after the pending packet completes.
    xenos_mmio_wptr_ = value;
    resume_xenos_pending_batch();
    return;
  }
  const auto submitted_dwords =
      value >= xenos_ring_last_wptr_
          ? value - xenos_ring_last_wptr_
          : capacity_dwords - xenos_ring_last_wptr_ + value;
  XenosRingSubmissionSnapshot submission;
  submission.start_pointer = xenos_ring_last_wptr_;
  submission.end_pointer = value;
  submission.dword_count = submitted_dwords;
  submission.captured_dword_count = std::min<std::uint32_t>(
      submitted_dwords, static_cast<std::uint32_t>(submission.dwords.size()));
  submission.truncated =
      submission.captured_dword_count != submission.dword_count;
  if (submission.truncated) {
    throw RuntimeTrap("Xenos primary submission exceeds capture bound", tick_,
                      0, submitted_dwords);
  }
  for (std::uint32_t index = 0U; index < submission.captured_dword_count;
       ++index) {
    const auto ring_index =
        (submission.start_pointer + index) % capacity_dwords;
    const auto word_address_wide =
        std::uint64_t{xenos_ring_base_} + std::uint64_t{ring_index} * 4U;
    if (word_address_wide > std::numeric_limits<std::uint32_t>::max()) {
      throw RuntimeTrap("Xenos ring submission address wraps guest memory",
                        tick_, 0, xenos_ring_base_);
    }
    const auto word_address = static_cast<std::uint32_t>(word_address_wide);
    if (!memory_.mapped(word_address, 4U)) {
      throw RuntimeTrap("Xenos ring submission is not guest-backed", tick_, 0,
                        word_address);
    }
    const auto word = memory_.load_u32(word_address);
    submission.dwords[index] = word;
    if (word_address >= 0x126CA058U && word_address < 0x126CA064U) {
      trace_xenos_ib_read("ring_publication", word_address,
                          ring_index - 0x16U, word, tick_, guest_thread,
                          guest_lr, generated_name, generated_line);
    }
  }
  if (xenos_ring_recent_submission_count_ <
      xenos_ring_recent_submissions_.size()) {
    xenos_ring_recent_submissions_[xenos_ring_recent_submission_count_] =
        submission;
    ++xenos_ring_recent_submission_count_;
  } else {
    std::move(xenos_ring_recent_submissions_.begin() + 1,
              xenos_ring_recent_submissions_.end(),
              xenos_ring_recent_submissions_.begin());
    xenos_ring_recent_submissions_.back() = submission;
  }
  std::vector<XenosIndirectBufferSnapshot> indirect_buffers;
  indirect_buffers.reserve(xenos_indirect_buffers_.size());
  const auto capture_indirect =
      [this, &indirect_buffers, guest_thread, guest_lr, generated_name,
       generated_line](
          std::uint32_t address_word, std::uint32_t count_word) {
    if (indirect_buffers.size() >= xenos_indirect_buffers_.size()) {
      throw RuntimeTrap("Xenos per-submission indirect-buffer limit reached",
                        tick_);
    }
    XenosIndirectBufferSnapshot indirect;
    indirect.address = address_word & ~3U;
    indirect.dword_count = count_word & 0xFFFFFU;
    const auto byte_count = std::uint64_t{indirect.dword_count} * 4U;
    if (indirect.address == 0U || indirect.dword_count == 0U ||
        indirect.dword_count > indirect.dwords.size() ||
        byte_count > std::numeric_limits<std::size_t>::max() ||
        std::uint64_t{indirect.address} + byte_count >
            std::uint64_t{std::numeric_limits<std::uint32_t>::max()} + 1U ||
        !memory_.mapped(indirect.address,
                        static_cast<std::size_t>(byte_count))) {
      throw RuntimeTrap("Xenos indirect buffer is not guest-backed", tick_, 0,
                        indirect.address);
    }
    const auto raw = memory_.load_bytes(indirect.address,
                                        static_cast<std::size_t>(byte_count));
    trace_ib_capture(memory_, indirect.address, indirect.dword_count, tick_);
    const auto digest = Sha256::bytes(raw);
    std::copy(digest.begin(), digest.end(), indirect.byte_sha256.begin());
    indirect.captured_dword_count = indirect.dword_count;
    for (std::uint32_t index = 0U; index < indirect.dword_count; ++index) {
      indirect.dwords[index] = memory_.load_u32(indirect.address + index * 4U);
    }
    trace_qualified_title_shader_capture(
        memory_, indirect.address,
        std::span<const std::uint32_t>(indirect.dwords.data(),
                                       indirect.dword_count),
        tick_);
    trace_observed_main_ib_reads(
        indirect.address,
        std::span<const std::uint32_t>(indirect.dwords.data(),
                                       indirect.dword_count),
        tick_, guest_thread, guest_lr, generated_name, generated_line);
    if (xenos_indirect_buffer_count_ < xenos_indirect_buffers_.size()) {
      xenos_indirect_buffers_[xenos_indirect_buffer_count_] = indirect;
      ++xenos_indirect_buffer_count_;
    } else {
      std::move(xenos_indirect_buffers_.begin() + 1,
                xenos_indirect_buffers_.end(),
                xenos_indirect_buffers_.begin());
      xenos_indirect_buffers_.back() = indirect;
    }
    indirect_buffers.push_back(std::move(indirect));
  };
  std::vector<std::uint32_t> renderer_stream;
  const auto append_renderer_packet =
      [&renderer_stream](const std::uint32_t *words,
                         std::uint32_t payload_count) {
        const auto header = words[0];
        if (is_xenos_semantic_packet(header)) {
          renderer_stream.insert(renderer_stream.end(), words,
                                 words + payload_count + 1U);
        }
      };
  std::uint32_t packet_index = 0U;
  while (packet_index < submission.dword_count) {
    const auto header = submission.dwords[packet_index];
    const auto packet_type = header >> 30U;
    const auto payload_count = packet_type == 0U || packet_type == 3U
                                   ? ((header >> 16U) & 0x3FFFU) + 1U
                               : packet_type == 1U ? 2U
                                                   : 0U;
    if (payload_count > submission.dword_count - packet_index - 1U) {
      throw RuntimeTrap("truncated primary Xenos packet", tick_, 0, header);
    }
    record_xenos_packet(xenos_packet_census_, tick_, header, payload_count);
    append_renderer_packet(submission.dwords.data() + packet_index,
                           payload_count);
    if (packet_type == 3U && ((header >> 8U) & 0x7FU) == 0x3FU) {
      capture_indirect(submission.dwords[packet_index + 1U],
                       submission.dwords[packet_index + 2U]);
    }
    packet_index += payload_count + 1U;
  }
  for (std::uint32_t buffer_index = 0U;
       buffer_index < indirect_buffers.size(); ++buffer_index) {
    const auto &indirect = indirect_buffers[buffer_index];
    std::uint32_t indirect_index = 0U;
    while (indirect_index < indirect.dword_count) {
      const auto header = indirect.dwords[indirect_index];
      const auto packet_type = header >> 30U;
      const auto payload_count = packet_type == 0U || packet_type == 3U
                                     ? ((header >> 16U) & 0x3FFFU) + 1U
                                 : packet_type == 1U ? 2U
                                                     : 0U;
      if (payload_count > indirect.dword_count - indirect_index - 1U) {
        throw RuntimeTrap("truncated Xenos indirect packet", tick_, 0, header);
      }
      record_xenos_packet(xenos_packet_census_, tick_, header, payload_count);
      if (std::getenv("AC6_DEMO_WATCH_EDRAM_SOURCE") != nullptr &&
          indirect.address == 0x1274A000U &&
          packet_type == 3U && ((header >> 8U) & 0x7FU) == 0x36U) {
        if (payload_count != 1U) {
          throw RuntimeTrap("unqualified EDRAM source draw packet", tick_, 0,
                            header);
        }
        const auto packet_address_wide =
            std::uint64_t{indirect.address} +
            std::uint64_t{indirect_index} * 4U;
        if (packet_address_wide > std::numeric_limits<std::uint32_t>::max()) {
          throw RuntimeTrap("EDRAM source draw address wraps guest memory",
                            tick_, 0, indirect.address);
        }
        std::fprintf(
            stderr,
            "AC6_EDRAM_SOURCE_COMMAND ib_address=0x%08X "
            "packet_address=0x%08X offset_dword=%u header=0x%08X "
            "initiator=0x%08X tick=%llu thread=%u submit_lr=0x%08X\n",
            indirect.address, static_cast<std::uint32_t>(packet_address_wide),
            indirect_index, header, indirect.dwords[indirect_index + 1U],
            static_cast<unsigned long long>(tick_), guest_thread, guest_lr);
      }
      append_renderer_packet(indirect.dwords.data() + indirect_index,
                             payload_count);
      if (packet_type == 3U && ((header >> 8U) & 0x7FU) == 0x3FU) {
        capture_indirect(indirect.dwords[indirect_index + 1U],
                         indirect.dwords[indirect_index + 2U]);
      }
      indirect_index += payload_count + 1U;
    }
  }
  const auto consumed = apply_xenos_typed_batch(renderer_stream);
  if (consumed > renderer_stream.size()) {
    throw RuntimeTrap("Xenos batch consumed beyond its bound", tick_);
  }
  xenos_mmio_wptr_ = value;
  xenos_ring_last_wptr_ = value;
  xenos_pending_wptr_ = value;
  xenos_pending_submitted_dwords_ = submitted_dwords;
  xenos_pending_stream_.assign(renderer_stream.begin() +
                                   static_cast<std::ptrdiff_t>(consumed),
                               renderer_stream.end());
  if (xenos_pending_stream_.empty()) {
    complete_xenos_ring_submission();
  }
}
