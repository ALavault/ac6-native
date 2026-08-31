from __future__ import annotations

import importlib.util
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "materialize_native_import_stubs", ROOT / "tools/materialize_native_import_stubs.py"
)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def test_stub_materialization_is_unique_and_side_effect_free(tmp_path: Path) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text("PPC_EXTERN_FUNC(__imp__NetDll_socket);\nPPC_EXTERN_FUNC(__imp__NetDll_socket);\n")
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 1
    text = output.read_text()
    assert text.count("__imp__NetDll_socket") == 1
    assert 'extern "C"' not in text
    assert "kOfflineStatus" in text


def test_tls_stubs_are_bounded_and_thread_local(tmp_path: Path) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text(
        "PPC_EXTERN_FUNC(__imp__KeTlsAlloc);\n"
        "PPC_EXTERN_FUNC(__imp__KeTlsGetValue);\n"
        "PPC_EXTERN_FUNC(__imp__KeTlsSetValue);\n"
        "PPC_EXTERN_FUNC(__imp__KeTlsFree);\n"
    )
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 4
    text = output.read_text()
    assert "thread_local" in text
    assert "g_tls_values[index] = ctx.r4.u32" in text


def test_timebase_frequency_is_xenon_qualified(tmp_path: Path) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text("PPC_EXTERN_FUNC(__imp__KeQueryPerformanceFrequency);\n")
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 1
    assert "ctx.r3.u64 = 50000000u" in output.read_text()


def test_thread_create_writes_guest_handle_without_host_thread(tmp_path: Path) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text("PPC_EXTERN_FUNC(__imp__ExCreateThread);\n")
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 1
    text = output.read_text()
    assert "PPC_STORE_U32(output_handle" in text
    assert "g_next_handle.fetch_add(1u)" in text
    assert "ctx.r3.u64 = 0u" in text


def test_wait_contract_times_out_without_blocking_host(tmp_path: Path) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text("PPC_EXTERN_FUNC(__imp__NtWaitForSingleObjectEx);\n")
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 1
    assert "ctx.r3.u64 = 0x102u" in output.read_text()


def test_virtual_memory_stub_is_bounded_and_big_endian(tmp_path: Path) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text("PPC_EXTERN_FUNC(__imp__NtAllocateVirtualMemory);\n")
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 1
    text = output.read_text()
    assert "Xbox 360 ABI: r3 = PVOID* base, r4 = SIZE_T* size" in text
    assert "g_next_virtual{0x10000000u}" in text
    assert "next + rounded > 0x7f000000ull" in text
    assert "PPC_STORE_U32(ctx.r3.u32, address)" in text
    assert "PPC_STORE_U32(ctx.r4.u32" in text
    assert "std::memset(base + address" in text


def test_virtual_memory_binding_is_bounded_and_zeroing(tmp_path: Path) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text("PPC_EXTERN_FUNC(__imp__NtAllocateVirtualMemory);\n")
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 1
    text = output.read_text()
    assert "g_next_virtual{0x10000000u}" in text
    assert "PPC_LOAD_U32(ctx.r4.u32)" in text
    assert "PPC_STORE_U32(ctx.r3.u32, address)" in text
    assert "allocate_guest(base, requested)" in text
    assert "std::memset(base + address" in text


def test_pool_and_string_bindings_are_guest_memory_only(tmp_path: Path) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text(
        "PPC_EXTERN_FUNC(__imp__ExAllocatePool);\n"
        "PPC_EXTERN_FUNC(__imp__RtlInitAnsiString);\n"
    )
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 2
    text = output.read_text()
    assert "allocate_guest(base, ctx.r3.u32)" in text
    assert "PPC_STORE_U16(destination + 0u" in text
    assert "PPC_STORE_U32(destination + 4u, source)" in text


def test_crt_bootstrap_bindings_return_success(tmp_path: Path) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text(
        "PPC_EXTERN_FUNC(__imp__RtlEnterCriticalSection);\n"
        "PPC_EXTERN_FUNC(__imp__RtlInitializeCriticalSection);\n"
        "PPC_EXTERN_FUNC(__imp__KeGetCurrentProcessType);\n"
        "PPC_EXTERN_FUNC(__imp__ExGetXConfigSetting);\n"
    )
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 4
    text = output.read_text()
    assert text.count("ctx.r3.u64 = 0u") == 4
    assert "PPC_STORE_U32(ctx.r5.u32, 0x00000300u)" in text


def test_vd_retrain_is_immediate_and_nonblocking(tmp_path: Path) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text(
        "PPC_EXTERN_FUNC(__imp__VdRetrainEDRAM);\n"
        "PPC_EXTERN_FUNC(__imp__VdIsHSIOTrainingSucceeded);\n"
    )
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 2
    text = output.read_text()
    assert "native renderer owns the Vd lifecycle" in text
    assert "host has no Xenon HSIO training phase" in text


def test_thread_binding_dispatches_only_generated_guest_targets(tmp_path: Path) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text("PPC_EXTERN_FUNC(__imp__ExCreateThread);\n")
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 1
    text = output.read_text()
    assert "PPC_LOOKUP_FUNC(base, shim_address)" in text
    assert "std::thread([shim, worker, base]" in text
    assert "worker.r1.u32 = g_next_thread_stack.fetch_sub(0x10000u)" in text
    assert "routine_address < PPC_CODE_BASE" in text


def test_physical_memory_binding_feeds_guest_ring_allocations(tmp_path: Path) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text(
        "PPC_EXTERN_FUNC(__imp__MmAllocatePhysicalMemoryEx);\n"
        "PPC_EXTERN_FUNC(__imp__MmGetPhysicalAddress);\n"
    )
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 2
    text = output.read_text()
    assert "allocate_guest(base, ctx.r4.u32)" in text
    assert "ctx.r3.u64 &= 0x1fffffffu" in text


def test_vd_ring_imports_bind_to_native_guest_service(tmp_path: Path) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text(
        "PPC_EXTERN_FUNC(__imp__VdInitializeRingBuffer);\n"
        "PPC_EXTERN_FUNC(__imp__VdEnableRingBufferRPtrWriteBack);\n"
    )
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 2
    text = output.read_text()
    assert "native_guest_vd_service().initialize_ring" in text
    assert "native_guest_vd_service().enable_readback" in text
    assert "ctx.r3.u64 = 0u" in text
