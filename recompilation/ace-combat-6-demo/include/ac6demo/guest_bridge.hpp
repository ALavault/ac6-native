#pragma once

#include "ac6demo/guest_memory.hpp"
#include "ac6demo/input.hpp"
#include "ac6demo/xenos_command_processor.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace ac6demo {

class VfsMount;

// Build-mode query used by the command layer to reject play/replay before it
// touches a store or trace when no generated guest object is linked.
[[nodiscard]] bool generated_guest_available() noexcept;

// Initialize the big-endian 32-bit Xbox ANSI_STRING layout used by
// xboxkrnl!RtlInitAnsiString. Returns false for an unmapped descriptor or an
// unterminated/unmapped source rather than exposing host memory.
[[nodiscard]] bool initialize_guest_ansi_string(GuestMemory &memory,
                                                std::uint32_t destination,
                                                std::uint32_t source);
[[nodiscard]] bool write_guest_file_network_open_information(
    GuestMemory &memory, std::uint32_t destination, std::uint32_t length,
    std::uint64_t file_size);
[[nodiscard]] bool read_guest_object_attributes_path(
    GuestMemory &memory, std::uint32_t object_attributes, std::string *path);
// Validate the one XGI user-context message reached by the PAL demo. This is
// deliberately narrower than the generic XAM app-manager contract: it has no
// host side effect and rejects every field not observed in guest memory.
[[nodiscard]] bool validate_xgi_user_context_request(
    GuestMemory &memory, std::uint32_t caller_lr, std::uint32_t app,
    std::uint32_t message, std::uint32_t overlapped, std::uint32_t buffer,
    std::uint32_t length);

struct GuestRegisterSnapshot final {
  std::uint32_t r1{};
  std::uint32_t r3{};
  std::uint32_t r4{};
  std::uint32_t r5{};
  std::uint32_t r6{};
  std::uint32_t r7{};
  std::uint32_t r8{};
  std::uint32_t r9{};
  std::uint32_t r10{};
  std::uint32_t r11{};
  std::uint32_t r12{};
  std::uint32_t r13{};
  std::uint32_t r26{};
  std::uint32_t r27{};
  std::uint32_t r28{};
  std::uint32_t r29{};
  std::uint32_t r30{};
  std::uint32_t r31{};

  bool operator==(const GuestRegisterSnapshot &) const = default;
};

enum class GuestControlFlowKind : std::uint8_t { Indirect, Import };

struct GuestVirtualDispatchSnapshot final {
  std::uint32_t object{};
  std::uint32_t vtable{};
  std::uint32_t slot{};

  bool operator==(const GuestVirtualDispatchSnapshot &) const = default;
};

// Aggregated dynamic edges. Direct guest calls remain the responsibility of
// the qualified static Ghidra call graph; this hook covers the two runtime
// seams that cannot be recovered completely from direct-call analysis.
struct GuestControlFlowEdge final {
  GuestControlFlowKind kind{GuestControlFlowKind::Indirect};
  std::uint32_t thread{};
  std::uint32_t lr{};
  std::uint32_t target{};
  std::string module;
  std::string name;
  std::uint16_t ordinal{};
  std::uint64_t first_tick{};
  std::uint64_t last_tick{};
  std::uint64_t count{};
  std::optional<GuestVirtualDispatchSnapshot> virtual_dispatch;
  GuestRegisterSnapshot registers{};

  bool operator==(const GuestControlFlowEdge &) const = default;
};

struct GuestFunctionReachability final {
  std::uint32_t address{};
  std::uint64_t first_tick{};
  std::uint64_t last_tick{};
  std::uint64_t count{};

  bool operator==(const GuestFunctionReachability &) const = default;
};

struct GuestThreadWaitSnapshot final {
  std::uint32_t id{};
  std::uint32_t startup{};
  std::uint32_t entry{};
  std::uint32_t parameter{};
  std::uint32_t callback{};
  std::uint32_t callback_parameter{};
  std::uint32_t last_indirect_target{};
  std::uint32_t last_indirect_lr{};
  std::uint64_t indirect_call_count{};
  std::uint64_t last_indirect_tick{};
  std::uint32_t wait_key{};
  std::uint32_t wait_lr{};
  std::uint64_t wake_tick{};
  std::uint8_t wait_kind{};
  bool blocked{};
  bool suspended{};
  bool finished{};

  bool operator==(const GuestThreadWaitSnapshot &) const = default;
};

struct GuestEventPublicationSnapshot final {
  std::uint32_t key{};
  std::uint32_t thread{};
  std::uint32_t lr{};
  std::uint8_t kind{};

  bool operator==(const GuestEventPublicationSnapshot &) const = default;
};

struct GuestSchedulerSnapshot final {
  std::uint32_t thread_count{};
  std::uint32_t runnable{};
  std::uint32_t blocked{};
  std::uint32_t finished{};
  std::uint32_t event_count{};
  std::uint32_t mutant_count{};
  std::uint32_t semaphore_count{};
  std::uint64_t event_set_count{};
  std::uint32_t last_event_set_handle{};
  std::uint32_t last_event_set_thread{};
  std::array<GuestEventPublicationSnapshot, 32U> event_publications{};
  std::uint32_t event_publication_count{};
  std::uint32_t primary_wait_key{};
  std::uint8_t primary_wait_kind{};
  bool primary_blocked{};
  std::array<GuestThreadWaitSnapshot, 64U> waits{};
  std::uint32_t wait_count{};
  std::uint64_t slice_exhaustions{};
  std::uint32_t last_slice_activations{};
  bool render_queue_mapped{};
  std::uint32_t render_queue_base{};
  std::uint32_t render_queue_producer{};
  std::uint32_t render_queue_consumer{};
  std::uint64_t render_queue_producer_changes{};
  std::uint64_t render_queue_consumer_changes{};
  std::uint32_t render_queue_last_producer_thread{};
  std::uint32_t render_queue_last_consumer_thread{};
  std::uint64_t render_queue_last_producer_tick{};
  std::uint64_t render_queue_last_consumer_tick{};
  std::uint32_t render_queue_max_pending{};

  bool operator==(const GuestSchedulerSnapshot &) const = default;
};

struct XenosRingSubmissionSnapshot final {
  std::uint32_t start_pointer{};
  std::uint32_t end_pointer{};
  std::uint32_t dword_count{};
  std::uint32_t captured_dword_count{};
  bool truncated{};
  std::array<std::uint32_t, 32U> dwords{};
};

struct XenosIndirectBufferSnapshot final {
  std::uint32_t address{};
  std::uint32_t dword_count{};
  std::uint32_t captured_dword_count{};
  bool truncated{};
  std::array<std::uint32_t, 4096U> dwords{};
  std::array<char, 65U> byte_sha256{};
};

struct XenosPacketCensusSnapshot final {
  std::uint64_t packet_count{};
  std::uint64_t decoded_dword_count{};
  std::array<std::uint64_t, 4U> type_counts{};
  std::array<std::uint64_t, 128U> type3_opcode_counts{};
  bool reached_corpus_qualified{};
};

struct XenosTypedDrawSnapshot final {
  std::uint16_t index_count{};
  bool predicated{};
  std::string vertex_shader_sha256;
  std::string pixel_shader_sha256;
};

struct XenosTypedCommandSnapshot final {
  std::uint32_t shader_load_count{};
  std::array<std::string, 16U> shader_sha256{};
  std::uint32_t draw_count{};
  std::array<XenosTypedDrawSnapshot, 64U> draws{};
  std::uint32_t present_count{};
  std::string present_resource_id;
  std::uint8_t present_format{};
  bool present_tiled{};
  std::uint32_t present_width{};
  std::uint32_t present_height{};
  bool renderer_relevant_semantics_qualified{};
};

struct XenosEffectSnapshot final {
  XenosEffectCounters counts{};
  std::uint32_t memory_write_count{};
  std::uint32_t cpu_interrupt_count{};
  std::uint8_t last_interrupt_cpu{};
  std::uint32_t pending_interrupt_count{};
  bool pending_wait{};
  bool pending_wait_memory{};
  std::uint32_t pending_wait_address{};
  std::uint32_t pending_wait_observed{};
  std::uint32_t pending_wait_reference{};
  std::uint32_t pending_wait_mask{};
  std::uint32_t pending_wait_interval{};
  std::string gamma_lut_sha256;
  bool synchronous_effects_qualified{};
};

struct XenosRingSnapshot final {
  bool initialized{};
  std::uint32_t base{};
  std::uint32_t size_log2{};
  std::uint32_t capacity_dwords{};
  std::uint32_t read_pointer{};
  std::uint32_t write_pointer{};
  std::uint32_t owner_endpoint{};
  std::uint64_t submissions{};
  std::uint64_t pointer_mismatches{};
  std::uint64_t submitted_dwords{};
  std::uint32_t max_submission_dwords{};
  std::uint32_t recent_submission_count{};
  std::array<XenosRingSubmissionSnapshot, 4U> recent_submissions{};
  std::uint32_t indirect_buffer_count{};
  std::array<XenosIndirectBufferSnapshot, 16U> indirect_buffers{};
  XenosPacketCensusSnapshot packet_census{};
  XenosTypedCommandSnapshot typed_commands{};
  XenosEffectSnapshot effects{};
};

struct VdSwapSnapshot final {
  std::uint64_t calls{};
  std::uint64_t tick{};
  std::uint32_t output_buffer{};
  std::uint32_t fetch_address{};
  std::uint32_t swap_state{};
  std::uint32_t system_header{};
  std::array<std::uint32_t, 6> fetch_words{};
  std::uint32_t frontbuffer_address{};
  std::uint32_t texture_format{};
  std::uint32_t color_space{};
  std::uint32_t width{};
  std::uint32_t height{};
};

class GuestBridge final {
public:
  struct ThreadImage final {
    std::uint32_t tls_address{};
    std::uint32_t tls_data_size{};
    std::uint32_t tls_raw_size{};
    std::uint32_t stack_size{};
  };

  explicit GuestBridge(GuestMemory &memory);
  ~GuestBridge();

  GuestBridge(const GuestBridge &) = delete;
  GuestBridge &operator=(const GuestBridge &) = delete;

  [[nodiscard]] bool available() const noexcept;

  // Install the generated indirect-call table and the deterministic guest
  // bootstrap mappings. The image itself must already be mapped at its XEX
  // load address.
  void prepare(const ThreadImage &image);

  void set_tick(std::uint64_t tick) noexcept;
  void set_input_frame(InputFrame frame) noexcept { input_.set_frame(frame); }
  [[nodiscard]] InputPollStats consume_input_poll_stats() {
    return input_.consume_poll_stats(memory_);
  }
  [[nodiscard]] XamInputDevice &input_device() noexcept { return input_; }
  void attach_vfs(const VfsMount &vfs) noexcept { vfs_ = &vfs; }
  [[nodiscard]] std::uint32_t open_guest_file(std::string_view xbox_path,
                                              std::uint32_t desired_access,
                                              std::uint32_t disposition,
                                              std::uint32_t *handle);
  [[nodiscard]] bool close_guest_file(std::uint32_t handle) noexcept;
  [[nodiscard]] bool guest_file_size(std::uint32_t handle,
                                     std::uint64_t *size) const noexcept;
  [[nodiscard]] bool set_guest_file_position(std::uint32_t handle,
                                             std::uint64_t position) noexcept;
  [[nodiscard]] std::uint32_t
  read_guest_file(std::uint32_t handle, std::optional<std::uint64_t> offset,
                  std::span<std::byte> output, std::uint32_t *bytes_read);
  [[nodiscard]] bool register_xaudio_client(std::uint32_t callback,
                                            std::uint32_t callback_context,
                                            std::uint32_t *handle) noexcept;
  [[nodiscard]] bool unregister_xaudio_client(std::uint32_t handle) noexcept;
  // The reached XNotifyPositionUI call is a void XAM state update. Keep the
  // observed value in the bridge so the import is not silently accepted and
  // reject positions outside the qualified demo corridor.
  [[nodiscard]] bool set_notify_ui_position(std::uint32_t position) noexcept;
  [[nodiscard]] std::optional<std::uint32_t>
  notify_ui_position() const noexcept {
    return notify_ui_position_;
  }
  [[nodiscard]] std::optional<std::uint32_t>
  xam_user_signin_state(std::uint32_t user_index) const noexcept;
  [[nodiscard]] bool write_xam_user_name(std::uint32_t user_index,
                                         std::uint32_t destination,
                                         std::uint32_t length);
  [[nodiscard]] GuestSchedulerSnapshot scheduler_snapshot() const noexcept;
  [[nodiscard]] std::vector<GuestControlFlowEdge> control_flow_snapshot() const;
  void enable_function_reachability(bool enabled) noexcept;
  void record_function_entry(std::string_view generated_name) noexcept;
  [[nodiscard]] std::vector<GuestFunctionReachability>
  function_reachability_snapshot() const;
  void record_indirect_edge(std::uint32_t thread, std::uint32_t lr,
                            std::uint32_t target,
                            GuestRegisterSnapshot registers,
                            std::optional<GuestVirtualDispatchSnapshot>
                                virtual_dispatch = std::nullopt);
  void record_import_edge(std::uint32_t thread, std::uint32_t lr,
                          std::string_view module, std::string_view name,
                          std::uint16_t ordinal,
                          GuestRegisterSnapshot registers);
  [[nodiscard]] std::uint64_t graphics_present_count() const noexcept {
    return graphics_present_count_;
  }
  [[nodiscard]] XenosRingSnapshot xenos_ring_snapshot() const noexcept;
  [[nodiscard]] std::vector<XenosCommand> consume_xenos_renderer_commands();
  [[nodiscard]] const VdSwapSnapshot &vd_swap_snapshot() const noexcept {
    return vd_swap_snapshot_;
  }
  void record_graphics_present(VdSwapSnapshot snapshot) noexcept {
    ++graphics_present_count_;
    snapshot.calls = graphics_present_count_;
    snapshot.tick = tick_;
    vd_swap_snapshot_ = snapshot;
  }
  void run_entry(std::uint32_t entry_point);

  [[nodiscard]] GuestMemory &memory() noexcept { return memory_; }
  [[nodiscard]] std::uint64_t tick() const noexcept { return tick_; }

  [[nodiscard]] std::uint32_t allocate_address(std::uint32_t size) noexcept;
  // Experimental, opt-in XMA probe only. The production path remains
  // fail-closed until the PAL post-import state is qualified.
  void map_xma_kick_window();
  [[nodiscard]] std::uint32_t ensure_xma_context_array();
  [[nodiscard]] std::uint32_t allocate_xma_context();
  [[nodiscard]] bool release_xma_context(std::uint32_t context);
  [[nodiscard]] std::uint32_t xaudio_client_handle() const noexcept {
    return xaudio_client_handle_;
  }
  [[nodiscard]] std::uint32_t xaudio_frame_bytes() const noexcept {
    return xaudio_frame_bytes_;
  }
  void set_xaudio_frame_bytes(std::uint32_t bytes) noexcept {
    xaudio_frame_bytes_ = bytes;
  }
  void count_xaudio_frame_submission() noexcept { ++xaudio_frames_submitted_; }
  [[nodiscard]] std::uint64_t xaudio_frames_submitted() const noexcept {
    return xaudio_frames_submitted_;
  }
  void observe_xma_physical_context(std::uint32_t context) noexcept {
    xma_last_physical_context_ = context;
  }
  [[nodiscard]] bool owns_allocation(std::uint32_t address,
                                     std::size_t size) const noexcept;
  void record_allocation(std::uint32_t address, std::size_t size);
  [[nodiscard]] bool
  create_guest_thread(std::uint32_t stack_size, std::uint32_t thread_id_pointer,
                      std::uint32_t startup, std::uint32_t entry,
                      std::uint32_t parameter, std::uint32_t creation_flags,
                      std::uint32_t handle_pointer, std::uint32_t *thread_id);
  [[nodiscard]] bool
  reference_guest_thread(std::uint32_t handle,
                         std::uint32_t *object) const noexcept;
  [[nodiscard]] bool
  is_guest_thread_object(std::uint32_t object) const noexcept;
  [[nodiscard]] bool
  resume_guest_thread(std::uint32_t handle,
                      std::uint32_t previous_count_pointer,
                      std::uint32_t *previous_count) noexcept;
  [[nodiscard]] bool close_guest_thread(std::uint32_t handle) noexcept;
  [[nodiscard]] bool
  is_guest_thread_handle(std::uint32_t handle) const noexcept;
  [[nodiscard]] bool guest_thread_finished(std::uint32_t handle) const noexcept;
  [[nodiscard]] bool run_runnable_threads();
  // Return a running guest fiber to the deterministic scheduler after a
  // bounded memory-operation quantum. This is a scheduling boundary only;
  // it does not mark the guest thread signaled, completed, or timed out.
  void yield_guest_thread_if_due();
  void yield_current_guest_thread();
  // Suspend the currently executing guest fiber at a qualified kernel wait.
  // The return value is true only when the scheduler woke it by timeout.
  [[nodiscard]] bool block_current_guest_thread(std::uint8_t wait_kind,
                                                std::uint32_t wait_key,
                                                std::uint64_t wake_tick);
  bool wake_guest_waiters(std::uint8_t wait_kind,
                          std::uint32_t wait_key) noexcept;
  [[nodiscard]] std::uint32_t
  wake_one_guest_waiter(std::uint8_t wait_kind,
                        std::uint32_t wait_key) noexcept;
  void configure_xenos_ring(std::uint32_t base,
                            std::uint32_t size_log2) noexcept;
  void set_xenos_ring_owner(std::uint32_t address) noexcept;
  void enable_xenos_read_pointer_writeback(std::uint32_t address) noexcept;
  void apply_xenos_mmio_write(
      std::uint32_t address, std::uint32_t value,
      std::uint32_t guest_thread = 0U, std::uint32_t guest_lr = 0U,
      const char* generated_name = nullptr,
      std::uint32_t generated_line = 0U);

private:
  [[nodiscard]] std::size_t
  apply_xenos_typed_batch(std::span<const std::uint32_t> stream);
  void resume_xenos_pending_batch();
  void complete_xenos_ring_submission();

  struct Allocation final {
    std::uint32_t address{};
    std::size_t size{};
  };

  struct GuestFile final {
    std::filesystem::path path;
    std::uint64_t offset{};
    std::uint64_t size{};
  };

  struct GuestThread final {
    std::uint32_t handle{};
    std::uint32_t object{};
    std::uint32_t id{};
    std::uint32_t stack_size{};
    std::uint32_t startup{};
    std::uint32_t entry{};
    std::uint32_t parameter{};
    std::uint32_t creation_flags{};
    std::uint32_t stack_top{};
    bool suspended{};
    bool handle_open{true};
    bool started{};
    bool finished{};
    bool blocked{};
    bool wake_timed_out{};
    std::uint8_t wait_kind{};
    std::uint32_t wait_key{};
    std::uint32_t wait_lr{};
    std::uint64_t wake_tick{};
    void *fiber_state{};
  };

  void execute_guest_thread(GuestThread &thread);
  void initialize_guest_fiber(GuestThread &thread);
  void destroy_guest_fiber(GuestThread &thread) noexcept;
  void wake_expired_guest_threads() noexcept;
  static void guest_fiber_trampoline(std::uintptr_t bridge,
                                     std::uintptr_t thread);

  GuestMemory &memory_;
  const VfsMount *vfs_{};
  XamInputDevice input_;
  std::uint64_t tick_{};
  bool function_reachability_enabled_{};
  std::map<std::uint32_t, GuestFunctionReachability> function_reachability_;
  std::uint32_t next_allocation_{0x10000000U};
  std::uint32_t xma_context_next_index_{};
  std::array<bool, 320U> xma_context_active_{};
  // Test-only PAL XMA probe: expected logical bit and last physical context
  // for the six observed slots. No production route consults this state.
  std::uint32_t xma_kick_expected_bit_{1U};
  std::uint32_t xma_last_physical_context_{};
  std::uint32_t next_thread_id_{4U};
  std::uint32_t next_thread_handle_{0xE1000000U};
  std::uint32_t stack_top_{};
  std::uint32_t graphics_interrupt_stack_top_{};
  std::uint64_t last_graphics_interrupt_tick_{~std::uint64_t{0}};
  std::uint32_t xenos_ring_rptr_{};
  std::uint32_t xenos_mmio_wptr_{};
  std::uint32_t xenos_ring_base_{};
  std::uint32_t xenos_ring_owner_{};
  std::uint32_t xenos_ring_state_{};
  std::uint32_t xenos_ring_rptr_writeback_{};
  std::uint32_t xenos_ring_size_log2_{};
  std::uint32_t xenos_ring_last_wptr_{};
  std::uint32_t xenos_ring_owner_endpoint_{};
  std::uint64_t xenos_ring_submissions_{};
  std::uint64_t xenos_ring_pointer_mismatches_{};
  std::uint64_t xenos_ring_submitted_dwords_{};
  std::uint32_t xenos_ring_max_submission_dwords_{};
  std::uint32_t xenos_ring_recent_submission_count_{};
  std::array<XenosRingSubmissionSnapshot, 4U> xenos_ring_recent_submissions_{};
  std::uint32_t xenos_indirect_buffer_count_{};
  std::array<XenosIndirectBufferSnapshot, 16U> xenos_indirect_buffers_{};
  XenosPacketCensusSnapshot xenos_packet_census_{};
  XenosCommandProcessor xenos_command_processor_{};
  XenosTypedCommandSnapshot xenos_typed_commands_{};
  std::vector<XenosCommand> xenos_renderer_commands_;
  XenosEffectSnapshot xenos_effects_{};
  std::uint32_t xenos_cp_interrupts_pending_{};
  std::vector<std::uint32_t> xenos_pending_stream_;
  std::uint32_t xenos_pending_wptr_{};
  std::uint32_t xenos_pending_endpoint_{};
  std::uint32_t xenos_pending_submitted_dwords_{};
  std::uint32_t xma_context_array_address_{};
  std::uint32_t xaudio_client_handle_{};
  std::uint32_t xaudio_callback_{};
  std::uint32_t xaudio_callback_context_{};
  std::uint64_t xaudio_frames_emitted_{};
  std::uint32_t xaudio_frame_bytes_{};
  std::uint64_t xaudio_frames_submitted_{};
  std::optional<std::uint32_t> notify_ui_position_;
  std::uint64_t graphics_present_count_{};
  VdSwapSnapshot vd_swap_snapshot_{};
  std::uint64_t scheduler_slice_exhaustions_{};
  std::uint32_t last_slice_activations_{};
  bool render_queue_observed_{};
  std::uint32_t render_queue_last_producer_{};
  std::uint32_t render_queue_last_consumer_{};
  std::uint64_t render_queue_producer_changes_{};
  std::uint64_t render_queue_consumer_changes_{};
  std::uint32_t render_queue_last_producer_thread_{};
  std::uint32_t render_queue_last_consumer_thread_{};
  std::uint64_t render_queue_last_producer_tick_{};
  std::uint64_t render_queue_last_consumer_tick_{};
  std::uint32_t render_queue_max_pending_{};
  bool prepared_{};
  std::vector<Allocation> allocations_;
  std::unordered_map<std::uint32_t, GuestFile> files_;
  std::uint32_t next_file_handle_{0xE3000000U};
  std::vector<GuestThread> threads_;
  GuestThread primary_thread_{};
  std::map<std::tuple<std::uint32_t, std::uint32_t, std::uint32_t>,
           GuestControlFlowEdge>
      indirect_flow_edges_;
  std::map<std::tuple<std::uint32_t, std::uint32_t, std::string, std::uint16_t>,
           GuestControlFlowEdge>
      import_flow_edges_;
};

} // namespace ac6demo
