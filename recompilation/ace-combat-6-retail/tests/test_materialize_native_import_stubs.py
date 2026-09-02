from __future__ import annotations

import importlib.util
from pathlib import Path

import pytest


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


def test_ke_query_system_time_fills_a_real_changing_filetime(
    tmp_path: Path,
) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text("PPC_EXTERN_FUNC(__imp__KeQuerySystemTime);\n")
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 1
    text = output.read_text()
    # r179: real contract is VOID KeQuerySystemTime(PLARGE_INTEGER) -- a
    # struct-fill through r3 (a FILETIME tick count), not a status. All
    # four of this XEX's real call sites need a real, changing wall-clock
    # value (calendar-field conversion, an elapsed-time delta, a
    # session/seed value) -- a fixed constant would break every one of
    # them differently.
    body = text.split("void __imp__KeQuerySystemTime")[1]
    assert "kOfflineStatus" not in body
    assert "std::chrono::system_clock::now()" in body
    assert "116444736000000000LL" in body
    assert "PPC_STORE_U64(ctx.r3.u32," in body


def test_xam_input_get_state_fills_the_real_xinput_struct(
    tmp_path: Path,
) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text("PPC_EXTERN_FUNC(__imp__XamInputGetState);\n")
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 1
    text = output.read_text()
    # r180: real contract is DWORD XamInputGetState(DWORD, XINPUT_STATE*)
    # -- struct-fill through r4. This XEX's own real call site
    # (0x8234cedc) confirms the (index, &struct) argument shape and the
    # 0x48F ERROR_DEVICE_NOT_CONNECTED error contract.
    body = text.split("void __imp__XamInputGetState")[1]
    assert "kOfflineStatus" not in body
    assert "native_guest_input_service().get_state(user_index, pad)" in body
    assert "0x48fu" in body
    assert "PPC_STORE_U32(ctx.r4.u32 + 0x0, pad.packet_number)" in body
    assert "PPC_STORE_U16(ctx.r4.u32 + 0x4, pad.buttons)" in body
    assert "PPC_STORE_U8(ctx.r4.u32 + 0x6, pad.left_trigger)" in body
    assert "PPC_STORE_U8(ctx.r4.u32 + 0x7, pad.right_trigger)" in body
    assert "ctx.r3.u64 = 0u;" in body


def test_xam_input_set_state_reads_the_real_xinput_vibration_struct(
    tmp_path: Path,
) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text("PPC_EXTERN_FUNC(__imp__XamInputSetState);\n")
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 1
    text = output.read_text()
    # r180: real contract is DWORD XamInputSetState(DWORD,
    # XINPUT_VIBRATION*) -- struct-read through r4, same 0x48F error
    # contract as XamInputGetState.
    body = text.split("void __imp__XamInputSetState")[1]
    assert "kOfflineStatus" not in body
    assert "PPC_LOAD_U16(ctx.r4.u32 + 0x0)" in body
    assert "PPC_LOAD_U16(ctx.r4.u32 + 0x2)" in body
    assert "native_guest_input_service().set_vibration(" in body
    assert "0x48fu" in body


def test_xam_input_get_capabilities_matches_confirmed_offsets(
    tmp_path: Path,
) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text("PPC_EXTERN_FUNC(__imp__XamInputGetCapabilities);\n")
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 1
    text = output.read_text()
    # r180: this XEX's own real call site (0x82390d48) reads struct+0x1
    # (SubType) and struct+0x2 (Flags) -- exactly Microsoft's own
    # XINPUT_CAPABILITIES offsets, confirmed byte-for-byte.
    body = text.split("void __imp__XamInputGetCapabilities")[1]
    assert "kOfflineStatus" not in body
    assert "PPC_STORE_U8(ctx.r5.u32 + 0x0, 1u)" in body
    assert "PPC_STORE_U8(ctx.r5.u32 + 0x1, 1u)" in body
    assert "PPC_STORE_U16(ctx.r5.u32 + 0x2, 0u)" in body
    assert "0x48fu" in body


def test_xam_input_get_keystroke_ex_returns_error_empty(
    tmp_path: Path,
) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text("PPC_EXTERN_FUNC(__imp__XamInputGetKeystrokeEx);\n")
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 1
    text = output.read_text()
    # r181: real contract's normal, steady-state answer is ERROR_EMPTY
    # (0x4306, no new menu-navigation keystroke queued) -- a different
    # shape than GetState/SetState/GetCapabilities (r180), since no
    # press/release edge-tracking queue exists yet in this project.
    body = text.split("void __imp__XamInputGetKeystrokeEx")[1]
    assert "kOfflineStatus" not in body
    assert "ctx.r3.u64 = 0x4306u;" in body


def test_xam_user_check_privilege_grants_and_succeeds(
    tmp_path: Path,
) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text("PPC_EXTERN_FUNC(__imp__XamUserCheckPrivilege);\n")
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 1
    text = output.read_text()
    # r182: real contract is DWORD XamUserCheckPrivilege(DWORD, DWORD,
    # LPBOOL) -- both the return (checked == 0 for ERROR_SUCCESS) and the
    # output bool (checked == 1) are consumed by this XEX's own real call
    # sites (0x82206bcc/0x821f44ac).
    body = text.split("void __imp__XamUserCheckPrivilege")[1]
    assert "kOfflineStatus" not in body
    assert "PPC_STORE_U32(ctx.r5.u32, 1u)" in body
    assert "ctx.r3.u64 = 0u;" in body


def test_rtl_image_xex_header_field_reports_not_present(
    tmp_path: Path,
) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text("PPC_EXTERN_FUNC(__imp__RtlImageXexHeaderField);\n")
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 1
    text = output.read_text()
    # r183: real contract is PVOID RtlImageXexHeaderField(PVOID, DWORD) --
    # the return value itself is the field pointer (0 = not present), not
    # a status. Both of this XEX's real call sites dereference a nonzero
    # return, so kOfflineStatus (nonzero) was a real crash risk, not a
    # cosmetic gap.
    body = text.split("void __imp__RtlImageXexHeaderField")[1]
    assert "kOfflineStatus" not in body
    assert "ctx.r3.u64 = 0u;" in body


def test_xe_crypt_sha_computes_a_real_digest(tmp_path: Path) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text("PPC_EXTERN_FUNC(__imp__XeCryptSha);\n")
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 1
    text = output.read_text()
    # r184: real signature is VOID XeCryptSha(pbInput1, cbInput1,
    # pbInput2, cbInput2, pbInput3, cbInput3, pbDigest, cbDigestSize) --
    # this XEX's own real call site populates all 8 arg registers with
    # cbDigestSize=0x14 (SHA-1). Computes the actual digest (OpenSSL EVP,
    # same pattern as native_xex.cpp's AES-CBC use) over real guest
    # bytes -- a garbage/absent digest would fail a real reference-hash
    # comparison downstream.
    body = text.split("void __imp__XeCryptSha")[1]
    assert "kOfflineStatus" not in body
    assert "EVP_sha1()" in body
    assert "EVP_DigestUpdate(sha1_ctx, base + ctx.r3.u32, ctx.r4.u32)" in body
    assert "EVP_DigestUpdate(sha1_ctx, base + ctx.r5.u32, ctx.r6.u32)" in body
    assert "EVP_DigestUpdate(sha1_ctx, base + ctx.r7.u32, ctx.r8.u32)" in body
    assert "PPC_STORE_U8(ctx.r9.u32 + i, digest[i])" in body


def test_rtl_time_to_time_fields_fills_a_real_calendar_struct(
    tmp_path: Path,
) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text("PPC_EXTERN_FUNC(__imp__RtlTimeToTimeFields);\n")
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 1
    text = output.read_text()
    # r185: real contract is VOID RtlTimeToTimeFields(PLARGE_INTEGER,
    # PTIME_FIELDS) -- the real companion r179's KeQuerySystemTime fix
    # feeds into. This XEX's own real call site confirms the standard
    # Win32 TIME_FIELDS field offsets. Uses C++20 <chrono>'s own real
    # Gregorian calendar conversion, not a hand-rolled reimplementation.
    body = text.split("void __imp__RtlTimeToTimeFields")[1]
    assert "kOfflineStatus" not in body
    assert "std::chrono::year_month_day" in body
    assert "PPC_STORE_U16(ctx.r4.u32 + 0x0," in body
    assert "PPC_STORE_U16(ctx.r4.u32 + 0xe," in body


def test_rtl_time_fields_to_time_is_the_real_inverse(
    tmp_path: Path,
) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text("PPC_EXTERN_FUNC(__imp__RtlTimeFieldsToTime);\n")
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 1
    text = output.read_text()
    # r185: real contract is BOOLEAN RtlTimeFieldsToTime(PTIME_FIELDS,
    # PLARGE_INTEGER) -- this XEX's own real call site confirms both the
    # same TIME_FIELDS offsets and the BOOLEAN return contract (low byte
    # of r3, nonzero = valid fields).
    body = text.split("void __imp__RtlTimeFieldsToTime")[1]
    assert "kOfflineStatus" not in body
    assert "ymd.ok()" in body
    assert "PPC_STORE_U64(ctx.r4.u32," in body
    assert "ctx.r3.u64 = 1u;" in body


def test_rtl_fill_memory_ulong_fills_with_the_real_pattern(
    tmp_path: Path,
) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text("PPC_EXTERN_FUNC(__imp__RtlFillMemoryUlong);\n")
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 1
    text = output.read_text()
    # r186: real signature is VOID RtlFillMemoryUlong(PVOID, ULONG,
    # ULONG) -- a standard, fixed-algorithm NT RTL primitive (fill
    # Length/4 ULONGs with Pattern), not a guessed value.
    body = text.split("void __imp__RtlFillMemoryUlong")[1]
    assert "kOfflineStatus" not in body
    assert "ctx.r4.u32 / 4u" in body
    assert "PPC_STORE_U32(ctx.r3.u32 + i * 4u, ctx.r5.u32)" in body


def test_rtl_compare_memory_ulong_returns_the_matched_prefix_length(
    tmp_path: Path,
) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text("PPC_EXTERN_FUNC(__imp__RtlCompareMemoryUlong);\n")
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 1
    text = output.read_text()
    # r186: real signature is ULONG RtlCompareMemoryUlong(PVOID, ULONG,
    # ULONG) -- returns the byte count of the longest matching ULONG
    # prefix, the standard NT RTL companion to RtlFillMemoryUlong.
    body = text.split("void __imp__RtlCompareMemoryUlong")[1]
    assert "kOfflineStatus" not in body
    assert "PPC_LOAD_U32(ctx.r3.u32 + matched_words * 4u) != ctx.r5.u32" in body
    assert "ctx.r3.u64 = matched_words * 4u;" in body


def test_rtl_unicode_to_multi_byte_n_converts_and_succeeds(
    tmp_path: Path,
) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text("PPC_EXTERN_FUNC(__imp__RtlUnicodeToMultiByteN);\n")
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 1
    text = output.read_text()
    # r187: real contract is NTSTATUS RtlUnicodeToMultiByteN(PCHAR, ULONG,
    # PULONG, PCWCH, ULONG). This XEX's own real call site confirms the
    # arg shape and the real NTSTATUS success contract (>= 0 is success).
    body = text.split("void __imp__RtlUnicodeToMultiByteN")[1]
    assert "kOfflineStatus" not in body
    assert "PPC_LOAD_U16(ctx.r6.u32 + written * 2u)" in body
    assert "0x3fu" in body
    assert "ctx.r3.u64 = 0u;  // STATUS_SUCCESS" in body


def test_rtl_unicode_string_to_ansi_string_allocates_and_converts(
    tmp_path: Path,
) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text("PPC_EXTERN_FUNC(__imp__RtlUnicodeStringToAnsiString);\n")
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 1
    text = output.read_text()
    # r188: real contract is NTSTATUS
    # RtlUnicodeStringToAnsiString(PANSI_STRING, PCUNICODE_STRING,
    # BOOLEAN). This XEX's own real call site confirms
    # AllocateDestinationString=1 and the real NTSTATUS success contract
    # (matching r187's own confirmed RtlUnicodeToMultiByteN contract);
    # RtlFreeAnsiString is called on success, confirming a real
    # allocation is expected.
    body = text.split("void __imp__RtlUnicodeStringToAnsiString")[1]
    assert "kOfflineStatus" not in body
    assert "allocate_guest(base, needed_bytes)" in body
    assert "PPC_STORE_U16(ctx.r3.u32 + 0x0, static_cast<std::uint16_t>(written))" in body
    assert "PPC_STORE_U32(ctx.r3.u32 + 0x4, dest_buffer)" in body
    assert "ctx.r3.u64 = 0u;  // STATUS_SUCCESS" in body


def test_rtl_free_ansi_string_clears_the_struct(tmp_path: Path) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text("PPC_EXTERN_FUNC(__imp__RtlFreeAnsiString);\n")
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 1
    text = output.read_text()
    # r188: matches this file's own ExFreePool precedent -- guest pool
    # pages are never individually reclaimed, so this clears the
    # ANSI_STRING fields rather than inventing a per-allocation free.
    body = text.split("void __imp__RtlFreeAnsiString")[1]
    assert "kOfflineStatus" not in body
    assert "PPC_STORE_U32(ctx.r3.u32 + 0x4, 0u)" in body


def test_nt_query_full_attributes_file_fills_the_real_struct(
    tmp_path: Path,
) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text("PPC_EXTERN_FUNC(__imp__NtQueryFullAttributesFile);\n")
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 1
    text = output.read_text()
    # r189: real contract is NTSTATUS
    # NtQueryFullAttributesFile(POBJECT_ATTRIBUTES,
    # PFILE_NETWORK_OPEN_INFORMATION) -- reuses NtCreateFile's own
    # confirmed ObjectAttributes/ANSI_STRING path shape (r122/r123). Both
    # of this XEX's real call sites confirm the real
    # FILE_NETWORK_OPEN_INFORMATION layout (FileAttributes at +0x30).
    body = text.split("void __imp__NtQueryFullAttributesFile")[1]
    assert "kOfflineStatus" not in body
    assert "guest_path_to_relative(raw)" in body
    assert "native_guest_media_service().open_file(relative)" in body
    assert "native_guest_media_service().file_size(*opened)" in body
    assert "PPC_STORE_U64(ctx.r4.u32 + 0x20, real_size)" in body
    assert "PPC_STORE_U64(ctx.r4.u32 + 0x28, real_size)" in body
    assert "PPC_STORE_U32(ctx.r4.u32 + 0x30, 0x80u)" in body
    assert "0xC0000034u" in body  # STATUS_OBJECT_NAME_NOT_FOUND on a real miss


def test_nt_query_volume_information_file_fills_real_fs_size_info(
    tmp_path: Path,
) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text("PPC_EXTERN_FUNC(__imp__NtQueryVolumeInformationFile);\n")
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 1
    text = output.read_text()
    # r190: real contract is NTSTATUS
    # NtQueryVolumeInformationFile(HANDLE, PIO_STATUS_BLOCK, PVOID,
    # ULONG, FS_INFORMATION_CLASS) -- all three of this XEX's real call
    # sites request FileFsSizeInformation (class 3, 0x18 bytes); one
    # computes real free/total byte counts from the queried fields.
    body = text.split("void __imp__NtQueryVolumeInformationFile")[1]
    assert "kOfflineStatus" not in body
    assert "ctx.r7.u32 != 3u" in body
    assert "0xc0000003u" in body  # STATUS_INVALID_INFO_CLASS
    assert "kSectorsPerAllocationUnit = 0x20u" in body
    assert "kBytesPerSector = 0x200u" in body
    assert "PPC_STORE_U64(ctx.r5.u32 + 0x0, kTotalAllocationUnits)" in body
    assert "PPC_STORE_U64(ctx.r5.u32 + 0x8, kTotalAllocationUnits)" in body
    assert "ctx.r3.u64 = 0u;  // STATUS_SUCCESS" in body


def test_query_statistics_fills_pages_the_caller_reads(tmp_path: Path) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text("PPC_EXTERN_FUNC(__imp__MmQueryStatistics);\n")
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 1
    text = output.read_text()
    assert "PPC_STORE_U32(buffer + 4, kTotalPhysicalPages)" in text
    assert "PPC_STORE_U32(buffer + 12, kAvailablePhysicalPages)" in text
    assert "kAvailablePhysicalPages = 0x18000u" in text
    assert "ctx.r3.u64 = 0u" in text


def test_thread_create_writes_guest_handle_without_host_thread(tmp_path: Path) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text("PPC_EXTERN_FUNC(__imp__ExCreateThread);\n")
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 1
    text = output.read_text()
    assert "PPC_STORE_U32(output_handle" in text
    assert "g_next_handle.fetch_add(1u)" in text
    assert "ctx.r3.u64 = 0u" in text


def test_create_file_reads_object_attributes_and_opens_via_media_service(
    tmp_path: Path,
) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text("PPC_EXTERN_FUNC(__imp__NtCreateFile);\n")
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 1
    text = output.read_text()
    # r122/r123: ObjectAttributes {RootDirectory, ObjectName, Attributes} at
    # +0/+4/+8; ANSI_STRING {Length, MaximumLength, Buffer} at +0/+2/+4.
    assert "PPC_LOAD_U32(object_attributes + 4u)" in text
    assert "PPC_LOAD_U16(object_name + 0u)" in text
    assert "PPC_LOAD_U32(object_name + 4u)" in text
    assert "native_guest_media_service().open_file(relative)" in text
    assert "guest_path_to_relative(raw)" in text
    assert "[NtCreateFile]" in text
    assert "0xC0000034u" in text  # STATUS_OBJECT_NAME_NOT_FOUND on a real miss
    assert "PPC_STORE_U32(ctx.r3.u32, handle)" in text
    assert "kOfflineStatus" not in text.split("void __imp__NtCreateFile(")[1].split(
        "\n}\n"
    )[0]


def test_read_file_completes_synchronously_not_pending_forever(
    tmp_path: Path,
) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text("PPC_EXTERN_FUNC(__imp__NtReadFile);\n")
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 1
    text = output.read_text()
    body = text.split("void __imp__NtReadFile(")[1].split("\n}\n")[0]
    # r125's own design constraint: a stub that always claims pending would
    # hang the caller's retry loop rather than crash it -- must not appear.
    assert "0x103" not in body  # STATUS_PENDING
    assert "native_guest_media_service().read_file(" in body
    assert "0xC0000011u" in body  # STATUS_END_OF_FILE, real completion
    assert "PPC_STORE_U32(ctx.r7.u32 + 0u, status)" in body
    assert "PPC_STORE_U32(ctx.r7.u32 + 4u, bytes_read)" in body
    assert "kOfflineStatus" not in body


def test_guest_path_to_relative_strips_drive_prefix(tmp_path: Path) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text("PPC_EXTERN_FUNC(__imp__NtCreateFile);\n")
    output = tmp_path / "stubs.cpp"
    MODULE.render(mapping, output)
    text = output.read_text()
    helper = text.split("std::string guest_path_to_relative(")[1].split("\n}\n")[0]
    assert "path.find(':')" in helper
    assert "c = '/'" in helper


def test_thread_create_honors_creation_flags_suspended_bit(
    tmp_path: Path,
) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text("PPC_EXTERN_FUNC(__imp__ExCreateThread);\n")
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 1
    text = output.read_text()
    assert "const std::uint32_t creation_flags = ctx.r9.u32" in text
    assert "kCreateSuspended = 0x00000004u" in text
    assert "(creation_flags & kCreateSuspended) != 0u" in text
    assert "if (start_suspended) create_event(handle" in text
    assert "/*manual_reset=*/true," in text
    assert "/*signaled=*/false);" in text
    assert "if (start_suspended) park_until_resumed(handle)" in text
    assert '"[ExCreateThread] handle=%u routine=0x%08x flags=0x%08x "' in text
    assert "AC6_NATIVE_IMPORT_TRACE" in text


def test_park_until_resumed_blocks_indefinitely_not_bounded(
    tmp_path: Path,
) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text("PPC_EXTERN_FUNC(__imp__ExCreateThread);\n")
    output = tmp_path / "stubs.cpp"
    MODULE.render(mapping, output)
    text = output.read_text()
    # park_until_resumed() must not reuse wait_event()'s bounded 2ms retry
    # contract (that would let a suspended thread's guest code run before
    # an actual resume, reintroducing r114's crash) -- it uses an
    # unbounded g_event_cv.wait(), not wait_for().
    park_body = text.split("void park_until_resumed")[1].split("\n}\n")[0]
    assert "g_event_cv.wait(lock" in park_body
    assert "wait_for" not in park_body


def test_resume_thread_releases_a_parked_thread(tmp_path: Path) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text(
        "PPC_EXTERN_FUNC(__imp__NtResumeThread);\n"
        "PPC_EXTERN_FUNC(__imp__KeResumeThread);\n"
    )
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 2
    text = output.read_text()
    for symbol in ("__imp__NtResumeThread", "__imp__KeResumeThread"):
        body = text.split(f"void {symbol}(")[1].split("\n}\n")[0]
        assert "set_event(ctx.r3.u32)" in body
        assert "was_already_running ? 0u : 1u" in body
        assert "ctx.r3.u64 = 0u" in body


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


def test_critical_sections_use_real_mutual_exclusion(tmp_path: Path) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text(
        "PPC_EXTERN_FUNC(__imp__RtlEnterCriticalSection);\n"
        "PPC_EXTERN_FUNC(__imp__RtlLeaveCriticalSection);\n"
        "PPC_EXTERN_FUNC(__imp__RtlInitializeCriticalSection);\n"
        "PPC_EXTERN_FUNC(__imp__RtlInitializeCriticalSectionAndSpinCount);\n"
    )
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 4
    text = output.read_text()
    # r116: must not still be the stale single-guest-thread no-op.
    assert "single guest thread" not in text
    assert "critical_section_for(ctx.r3.u32).lock()" in text
    assert "critical_section_for(ctx.r3.u32).unlock()" in text
    enter_body = text.split("void __imp__RtlEnterCriticalSection(")[1].split(
        "\n}\n"
    )[0]
    assert "lock()" in enter_body
    leave_body = text.split("void __imp__RtlLeaveCriticalSection(")[1].split(
        "\n}\n"
    )[0]
    assert "unlock()" in leave_body
    # Backing storage keyed by the guest critical-section object's own
    # address, same pattern g_events already uses for handles.
    assert "std::recursive_mutex" in text
    assert "g_critical_sections" in text


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
    assert "std::thread([shim, worker, base, handle, start_suspended]" in text
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


def test_dbg_print_dumps_raw_guest_string_without_synthesizing_varargs(
    tmp_path: Path,
) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text("PPC_EXTERN_FUNC(__imp__DbgPrint);\n")
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 1
    text = output.read_text()
    assert "PPC_LOAD_U8(guest_format + length)" in text
    assert "AC6_NATIVE_IMPORT_TRACE" in text
    assert "[DbgPrint] %s" in text
    # No printf-style substitution of guest register args: only the raw
    # format string is dumped, since a caller-side arg is not reliably
    # readable as a specific type from this stub alone.
    assert "ctx.r4" not in text
    assert "ctx.r3.u64 = 0u" in text


def test_wait_event_blocks_briefly_instead_of_busy_spinning(
    tmp_path: Path,
) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text("PPC_EXTERN_FUNC(__imp__NtWaitForSingleObjectEx);\n")
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 1
    text = output.read_text()
    # Still a single-shot, non-blocking-forever contract: an unsignaled wait
    # still resolves to STATUS_TIMEOUT for the caller.
    assert "ctx.r3.u64 = 0x102u" in text
    assert "#include <condition_variable>" in text
    assert "g_event_cv.wait_for(lock, std::chrono::milliseconds(2)" in text
    assert "g_event_cv.notify_all()" in text


def test_mutant_and_semaphore_release_succeed_without_contention_model(
    tmp_path: Path,
) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text(
        "PPC_EXTERN_FUNC(__imp__NtReleaseMutant);\n"
        "PPC_EXTERN_FUNC(__imp__NtReleaseSemaphore);\n"
    )
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 2
    text = output.read_text()
    assert text.count("ctx.r3.u64 = 0u") == 2
    mutant_body = text.split("void __imp__NtReleaseMutant")[1].split(
        "void __imp__NtReleaseSemaphore"
    )[0]
    semaphore_body = text.split("void __imp__NtReleaseSemaphore")[1]
    # NtReleaseMutant(handle, PreviousCount*): r4 is the out pointer.
    assert "PPC_STORE_U32(ctx.r4.u32, 0u)" in mutant_body
    # NtReleaseSemaphore(handle, ReleaseCount, PreviousCount*): r4 is the
    # ReleaseCount integer, not a pointer -- r5 is the real out pointer, and
    # the release must actually signal the semaphore's wait state (r145).
    assert "PPC_STORE_U32(ctx.r5.u32, 0u)" in semaphore_body
    assert "ctx.r4.u32" not in semaphore_body
    assert "set_event(ctx.r3.u32)" in semaphore_body
    assert "kOfflineStatus" not in mutant_body
    assert "kOfflineStatus" not in semaphore_body


def test_create_semaphore_registers_a_waitable_event(tmp_path: Path) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text(
        "PPC_EXTERN_FUNC(__imp__NtCreateSemaphore);\n"
        "PPC_EXTERN_FUNC(__imp__NtCreateTimer);\n"
        "PPC_EXTERN_FUNC(__imp__NtCreateMutant);\n"
    )
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 3
    text = output.read_text()
    semaphore_body = text.split("void __imp__NtCreateSemaphore")[1].split(
        "void __imp__NtCreateTimer"
    )[0]
    # r145: a real NT signature -- r5=InitialCount, r6=MaximumCount --
    # confirmed against this XEX's own NtCreateSemaphore call site. Only
    # NtCreateSemaphore registers with create_event(); NtCreateTimer and
    # NtCreateMutant keep the prior generic handle-only allocation, since
    # nothing in this project's own tracing has shown either needs to
    # participate in the same wait/signal model.
    assert "create_event(handle, /*manual_reset=*/false, /*signaled=*/ctx.r5.s32 > 0)" in (
        semaphore_body
    )
    timer_and_mutant = text.split("void __imp__NtCreateTimer")[1]
    assert "create_event(" not in timer_and_mutant


def test_ob_reference_object_by_handle_writes_the_real_handle_through(
    tmp_path: Path,
) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text("PPC_EXTERN_FUNC(__imp__ObReferenceObjectByHandle);\n")
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 1
    text = output.read_text()
    # r147: (Handle, ObjectType, PVOID* Object) -- every call site this
    # project has traced treats the returned object as an opaque token
    # immediately re-passed to another kernel thread API, never
    # dereferenced directly, so the real handle itself is a safe stand-in.
    # This also fixes a real thread-resume bug: KeResumeThread is called
    # on this output at one call site without checking this call's own
    # status, so an unwritten *Object left a parked ExCreateThread worker
    # unresumable.
    body = text.split("void __imp__ObReferenceObjectByHandle")[1]
    assert "kOfflineStatus" not in body
    assert "PPC_STORE_U32(ctx.r5.u32, ctx.r3.u32)" in body
    assert "ctx.r3.u64 = 0u;" in body


def test_ke_set_affinity_thread_returns_a_real_mask_not_a_status(
    tmp_path: Path,
) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text("PPC_EXTERN_FUNC(__imp__KeSetAffinityThread);\n")
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 1
    text = output.read_text()
    # r148: the real contract returns the previous affinity mask itself in
    # r3, not an NTSTATUS -- the generic offline fallback's negative
    # kOfflineStatus tripped this XEX's own caller's "< 0" sanity guard
    # every time. Both the return value and the *PreviousAffinity output
    # report the same small, in-range mask (core 0) so the caller's own
    # bit-scan over it stays meaningful instead of reading uninitialized
    # stack memory.
    body = text.split("void __imp__KeSetAffinityThread")[1]
    assert "kOfflineStatus" not in body
    assert "PPC_STORE_U32(ctx.r5.u32, 1u)" in body
    assert "ctx.r3.u64 = 1u;" in body


def test_ob_dereference_object_returns_a_plain_count_not_a_status(
    tmp_path: Path,
) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text("PPC_EXTERN_FUNC(__imp__ObDereferenceObject);\n")
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 1
    text = output.read_text()
    # r162: the real contract is (PVOID Object) -> LONG new reference
    # count, not an NTSTATUS -- the generic offline fallback's
    # kOfflineStatus is NTSTATUS-shaped and wrong here even though every
    # one of this XEX's own 18 static call sites currently discards the
    # return value.
    body = text.split("void __imp__ObDereferenceObject")[1]
    assert "kOfflineStatus" not in body
    assert "ctx.r3.u64 = 0u;" in body


def test_ke_set_base_priority_thread_returns_a_plain_increment_not_a_status(
    tmp_path: Path,
) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text("PPC_EXTERN_FUNC(__imp__KeSetBasePriorityThread);\n")
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 1
    text = output.read_text()
    # r162: the real contract is (PKTHREAD, LONG) -> LONG previous
    # increment, not an NTSTATUS -- same contract-shape fix as
    # ObDereferenceObject above, for the same reason (all 3 of this
    # XEX's own static call sites discard the return value today).
    body = text.split("void __imp__KeSetBasePriorityThread")[1]
    assert "kOfflineStatus" not in body
    assert "ctx.r3.u64 = 0u;" in body


def test_ke_query_base_priority_thread_returns_an_in_range_value_not_a_status(
    tmp_path: Path,
) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text("PPC_EXTERN_FUNC(__imp__KeQueryBasePriorityThread);\n")
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 1
    text = output.read_text()
    # r163: the real contract is (PKTHREAD) -> LONG current priority
    # increment, not an NTSTATUS. Unlike KeSetBasePriorityThread/
    # ObDereferenceObject, this XEX's own single call site (sub_821F3EA0)
    # actually uses the return value (clamps it to [-16, 15] and returns
    # that as its own result) -- the old kOfflineStatus sentinel always
    # hit the clamp floor. 0 keeps the caller's clamp a no-op.
    body = text.split("void __imp__KeQueryBasePriorityThread")[1]
    assert "kOfflineStatus" not in body
    assert "ctx.r3.u64 = 0u;" in body


@pytest.mark.parametrize(
    "name", ["KeEnterCriticalRegion", "KeLeaveCriticalRegion"]
)
def test_critical_region_enter_leave_are_void_not_a_status(
    tmp_path: Path, name: str
) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text(f"PPC_EXTERN_FUNC(__imp__{name});\n")
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 1
    text = output.read_text()
    # r164: the real contract is VOID -- no return value at all. This
    # XEX's own real call sites discard r3 (overwritten before anything
    # reads it), matching the real contract's own expectation, but the
    # generic offline fallback's kOfflineStatus is still the wrong shape
    # for a VOID-returning kernel call.
    body = text.split(f"void __imp__{name}")[1]
    assert "kOfflineStatus" not in body
    assert "ctx.r3.u64 = 0u;" in body


def test_rtl_try_enter_critical_section_returns_a_real_boolean_not_a_status(
    tmp_path: Path,
) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text("PPC_EXTERN_FUNC(__imp__RtlTryEnterCriticalSection);\n")
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 1
    text = output.read_text()
    # r164: the real contract is BOOLEAN (nonzero = lock acquired). This
    # XEX's own 5 real call sites do check the return value with a
    # zero-vs-nonzero test; kOfflineStatus was already nonzero so this
    # changes no currently-observed control flow, but a canonical 1 is
    # the correct shape for a "lock always available" stub.
    body = text.split("void __imp__RtlTryEnterCriticalSection")[1]
    assert "kOfflineStatus" not in body
    assert "ctx.r3.u64 = 1u;" in body


@pytest.mark.parametrize("name", ["NetDll_XNetStartup", "NetDll_WSAStartup"])
def test_net_startup_reports_success_not_a_status(
    tmp_path: Path, name: str
) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text(f"PPC_EXTERN_FUNC(__imp__{name});\n")
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 1
    text = output.read_text()
    # r167: the real contract is INT (0 = success), the WinSock/XNet
    # convention, not an NTSTATUS. This XEX's own thin wrapper functions
    # (sub_821FCCE0/sub_821FCED0) return this call's value completely
    # unmodified as their own result, so kOfflineStatus (nonzero) would
    # read as failure under the standard "== 0 means success" check --
    # wrong for an offline-only stub with no real network condition to
    # fail on, matching this project's own established offline-boundary
    # convention of succeeding past an absent network.
    body = text.split(f"void __imp__{name}")[1]
    assert "kOfflineStatus" not in body
    assert "ctx.r3.u64 = 0u;" in body


def test_vd_query_video_mode_fills_the_struct_not_a_status(
    tmp_path: Path,
) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text("PPC_EXTERN_FUNC(__imp__VdQueryVideoMode);\n")
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 1
    text = output.read_text()
    # r169: real contract is VOID VdQueryVideoMode(X_VIDEO_MODE*) -- a
    # struct-fill through r3, not a status return (r168 named this gap).
    # struct+0x0/+0x4/+0x8/+0x14 are evidence-confirmed from this XEX's
    # own two real call sites (a duplicate-write pattern for width, a
    # PPC boolean-normalization idiom for the interlaced flag, and a
    # float used in real refresh-rate arithmetic).
    body = text.split("void __imp__VdQueryVideoMode")[1]
    assert "kOfflineStatus" not in body
    assert "PPC_STORE_U32(ctx.r3.u32 + 0, 1280u)" in body
    assert "PPC_STORE_U32(ctx.r3.u32 + 4, 720u)" in body
    assert "PPC_STORE_U32(ctx.r3.u32 + 8, 0u)" in body
    assert "PPC_STORE_U32(ctx.r3.u32 + 0x14, 0x42700000u)" in body


def test_xget_avpack_avoids_the_four_skip_setup_values(
    tmp_path: Path,
) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text("PPC_EXTERN_FUNC(__imp__XGetAVPack);\n")
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 1
    text = output.read_text()
    # r173: this XEX's one real call site only checks equality against
    # {0x3, 0x6, 0x8, 0x4}, all branching to the same "skip setup" target;
    # the value is never stored or read again. 0u avoids all four.
    body = text.split("void __imp__XGetAVPack")[1]
    assert "kOfflineStatus" not in body
    assert "ctx.r3.u64 = 0u;" in body


def test_xget_language_returns_english(
    tmp_path: Path,
) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text("PPC_EXTERN_FUNC(__imp__XGetLanguage);\n")
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 1
    text = output.read_text()
    # r174: this XEX's one real call site bounds-checks the result against
    # 10 and indexes a per-language table with it. 1 is the real Xbox 360
    # XDK's own standardized XC_LANGUAGE_ENGLISH constant.
    body = text.split("void __imp__XGetLanguage")[1]
    assert "kOfflineStatus" not in body
    assert "ctx.r3.u64 = 1u;" in body


def test_xam_user_get_signin_state_reports_index_zero_signed_in_locally(
    tmp_path: Path,
) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text("PPC_EXTERN_FUNC(__imp__XamUserGetSigninState);\n")
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 1
    text = output.read_text()
    # r176: real contract is DWORD XamUserGetSigninState(DWORD
    # dwUserIndex) -- a real enum (0/1/2), not a status. This XEX's real
    # call sites test the result for exact equality against 1 (signed in
    # locally) to pick the active user, and against 0 to gate further
    # per-player updates; kOfflineStatus never equalled 1, so this always
    # fell through to a sign-in-prompt fallback path.
    body = text.split("void __imp__XamUserGetSigninState")[1]
    assert "kOfflineStatus" not in body
    assert "user_index == 0u" in body
    assert "1u : 0u" in body


def test_xam_get_system_version_stays_below_every_observed_threshold(
    tmp_path: Path,
) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text("PPC_EXTERN_FUNC(__imp__XamGetSystemVersion);\n")
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 1
    text = output.read_text()
    # r177: this XEX's own call sites compare the result against version
    # thresholds to gate optional feature probing; every examined branch
    # is safe either way except 0x821f4440 (r176's XamUserGetSigninState
    # fix), where >= 0x20096b00 skips the signin loop entirely. A value
    # below every observed threshold is required for r176 to matter here.
    body = text.split("void __imp__XamGetSystemVersion")[1]
    assert "kOfflineStatus" not in body
    assert "ctx.r3.u64 = 0x20000000u;" in body


def test_xget_game_region_returns_the_privileged_exact_match_code(
    tmp_path: Path,
) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text("PPC_EXTERN_FUNC(__imp__XGetGameRegion);\n")
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 1
    text = output.read_text()
    # r172: real contract is DWORD XGetGameRegion(VOID). 0x101 is the one
    # value this XEX's own code (two of its three real call sites) treats
    # as the privileged, exact-match region code, not merely a naming
    # convention pick.
    body = text.split("void __imp__XGetGameRegion")[1]
    assert "kOfflineStatus" not in body
    assert "ctx.r3.u64 = 0x101u;" in body


def test_xget_video_mode_fills_the_refresh_rate_field(
    tmp_path: Path,
) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text("PPC_EXTERN_FUNC(__imp__XGetVideoMode);\n")
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 1
    text = output.read_text()
    # r171: real contract is VOID XGetVideoMode(XVIDEO_MODE*) -- the same
    # struct type VdQueryVideoMode (r169) fills. This XEX's one real call
    # site reads struct+0x14 as a float and uses it as a division divisor
    # -- left unimplemented, that divisor is uninitialized stack garbage.
    body = text.split("void __imp__XGetVideoMode")[1]
    assert "kOfflineStatus" not in body
    assert "PPC_STORE_U32(ctx.r3.u32 + 0x14, 0x42700000u)" in body


def test_vd_query_video_flags_returns_a_flag_value_not_a_status(
    tmp_path: Path,
) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text("PPC_EXTERN_FUNC(__imp__VdQueryVideoFlags);\n")
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 1
    text = output.read_text()
    # r170: real contract is DWORD VdQueryVideoFlags(VOID) -- a flags
    # bitmask, not a status. The generic offline fallback's kOfflineStatus
    # (0xC00000BB) has bit 0 set, which forced this XEX's one real call
    # site's branch by coincidence, not by real flag semantics.
    body = text.split("void __imp__VdQueryVideoFlags")[1]
    assert "kOfflineStatus" not in body
    assert "ctx.r3.u64 = 0u;" in body


def test_vd_get_current_display_gamma_fills_both_pointer_outputs(
    tmp_path: Path,
) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text("PPC_EXTERN_FUNC(__imp__VdGetCurrentDisplayGamma);\n")
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 1
    text = output.read_text()
    # r170: real contract is VOID VdGetCurrentDisplayGamma(DWORD*, FLOAT*)
    # -- two pointer outputs (evidence-confirmed at this XEX's one real call
    # site, 0x821eb454), not a status return.
    body = text.split("void __imp__VdGetCurrentDisplayGamma")[1]
    assert "kOfflineStatus" not in body
    assert "PPC_STORE_U32(ctx.r3.u32, 0u)" in body
    assert "PPC_STORE_U32(ctx.r4.u32, 0x400ccccdu)" in body


def test_vd_get_current_display_information_fills_width_height_fields(
    tmp_path: Path,
) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text("PPC_EXTERN_FUNC(__imp__VdGetCurrentDisplayInformation);\n")
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 1
    text = output.read_text()
    # r170: struct-fill call, cross-validated against r169's
    # VdQueryVideoMode fix -- this XEX's own call site at 0x821f0764 reads
    # struct+0x48/+0x4a/+0x56 and forwards them into the same downstream
    # fields (0x5414/0x5418/0x541c) VdQueryVideoMode fills.
    body = text.split("void __imp__VdGetCurrentDisplayInformation")[1]
    assert "kOfflineStatus" not in body
    assert "PPC_STORE_U16(ctx.r3.u32 + 0x48, 1280u)" in body
    assert "PPC_STORE_U16(ctx.r3.u32 + 0x4a, 720u)" in body
    assert "PPC_STORE_U16(ctx.r3.u32 + 0x56, 1280u)" in body
    # r175: struct+0x05 traced to a real algorithm choice at both call
    # sites (linear-interpolation vs nearest-neighbor scaler at
    # 0x821ea4d8; a persisted deviation flag at 0x821ea2a4), both agreeing
    # that 1 is the plain/default case -- the physically sensible choice
    # for this project's own HD/widescreen target.
    assert "PPC_STORE_U8(ctx.r3.u32 + 0x5, 1u)" in body


def test_nt_status_to_dos_error_maps_pending_to_io_pending(
    tmp_path: Path,
) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text("PPC_EXTERN_FUNC(__imp__RtlNtStatusToDosError);\n")
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 1
    text = output.read_text()
    # STATUS_SUCCESS -> ERROR_SUCCESS
    assert "case 0x00000000u: dos_error = 0u;" in text
    # STATUS_PENDING -> ERROR_IO_PENDING (r125's traced fix target)
    assert "case 0x00000103u: dos_error = 997u;" in text
    # Real Windows default for an unmapped status: ERROR_MR_MID_NOT_FOUND.
    assert "dos_error = 317u;" in text
    body = text.split("void __imp__RtlNtStatusToDosError(")[1].split("\n}\n")[0]
    assert "kOfflineStatus" not in body


def test_generic_fallback_is_traceable_and_still_returns_offline_status(
    tmp_path: Path,
) -> None:
    mapping = tmp_path / "mapping.cpp"
    # r194: KeDelayExecutionThread got a real fix (actually sleeps); use a
    # still-generic import as the fallback-shape example instead.
    mapping.write_text("PPC_EXTERN_FUNC(__imp__NtCancelTimer);\n")
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 1
    text = output.read_text()
    assert 'trace_offline_import("NtCancelTimer")' in text
    assert "ctx.r3.u64 = kOfflineStatus" in text


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


def test_spinlock_and_irql_primitives_use_real_mutual_exclusion(
    tmp_path: Path,
) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text(
        "PPC_EXTERN_FUNC(__imp__KfAcquireSpinLock);\n"
        "PPC_EXTERN_FUNC(__imp__KfReleaseSpinLock);\n"
        "PPC_EXTERN_FUNC(__imp__KeAcquireSpinLockAtRaisedIrql);\n"
        "PPC_EXTERN_FUNC(__imp__KeReleaseSpinLockFromRaisedIrql);\n"
        "PPC_EXTERN_FUNC(__imp__KeRaiseIrqlToDpcLevel);\n"
        "PPC_EXTERN_FUNC(__imp__KfLowerIrql);\n"
    )
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 6
    text = output.read_text()
    # r191: must not still be the stale kOfflineStatus no-op.
    assert "kOfflineStatus" not in text.split("void __imp__KfAcquireSpinLock(")[
        1
    ].split("\n}\n")[0]
    assert "spin_lock_for(ctx.r3.u32).lock()" in text
    assert "spin_lock_for(ctx.r3.u32).unlock()" in text
    assert "g_dpc_level_mutex.lock()" in text
    assert "g_dpc_level_mutex.unlock()" in text

    acquire_body = text.split("void __imp__KfAcquireSpinLock(")[1].split(
        "\n}\n"
    )[0]
    assert "spin_lock_for(ctx.r3.u32).lock()" in acquire_body

    release_body = text.split("void __imp__KfReleaseSpinLock(")[1].split(
        "\n}\n"
    )[0]
    assert "spin_lock_for(ctx.r3.u32).unlock()" in release_body

    raised_acquire_body = text.split(
        "void __imp__KeAcquireSpinLockAtRaisedIrql("
    )[1].split("\n}\n")[0]
    assert "spin_lock_for(ctx.r3.u32).lock()" in raised_acquire_body

    raised_release_body = text.split(
        "void __imp__KeReleaseSpinLockFromRaisedIrql("
    )[1].split("\n}\n")[0]
    assert "spin_lock_for(ctx.r3.u32).unlock()" in raised_release_body

    raise_body = text.split("void __imp__KeRaiseIrqlToDpcLevel(")[1].split(
        "\n}\n"
    )[0]
    assert "g_dpc_level_mutex.lock()" in raise_body

    lower_body = text.split("void __imp__KfLowerIrql(")[1].split("\n}\n")[0]
    assert "g_dpc_level_mutex.unlock()" in lower_body

    # r191: g_dpc_level_mutex must be recursive -- unlike a spinlock object,
    # real IRQL is per-thread state, so the same thread legitimately nests
    # Raise/Lower pairs without that being a self-reacquisition.
    assert "std::recursive_mutex g_dpc_level_mutex" in text


def test_semaphore_and_try_spinlock_use_real_mutual_exclusion(
    tmp_path: Path,
) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text(
        "PPC_EXTERN_FUNC(__imp__KeTryToAcquireSpinLockAtRaisedIrql);\n"
        "PPC_EXTERN_FUNC(__imp__KeInitializeSemaphore);\n"
        "PPC_EXTERN_FUNC(__imp__KeReleaseSemaphore);\n"
    )
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 3
    text = output.read_text()
    assert "kOfflineStatus" not in text.split(
        "void __imp__KeTryToAcquireSpinLockAtRaisedIrql("
    )[1].split("\n}\n")[0]

    try_body = text.split(
        "void __imp__KeTryToAcquireSpinLockAtRaisedIrql("
    )[1].split("\n}\n")[0]
    assert "spin_lock_for(ctx.r3.u32).try_lock()" in try_body

    init_body = text.split("void __imp__KeInitializeSemaphore(")[1].split(
        "\n}\n"
    )[0]
    assert "create_event(ctx.r3.u32" in init_body
    assert "ctx.r4.s32 > 0" in init_body

    release_body = text.split("void __imp__KeReleaseSemaphore(")[1].split(
        "\n}\n"
    )[0]
    assert "set_event(ctx.r3.u32)" in release_body


def test_kebugcheck_family_aborts_instead_of_returning(tmp_path: Path) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text(
        "PPC_EXTERN_FUNC(__imp__KeBugCheck);\n"
        "PPC_EXTERN_FUNC(__imp__KeBugCheckEx);\n"
    )
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 2
    text = output.read_text()
    assert "kOfflineStatus" not in text.split("void __imp__KeBugCheck(")[
        1
    ].split("\n}\n")[0]
    assert "kOfflineStatus" not in text.split("void __imp__KeBugCheckEx(")[
        1
    ].split("\n}\n")[0]

    bugcheck_body = text.split("void __imp__KeBugCheck(")[1].split("\n}\n")[0]
    assert "std::abort()" in bugcheck_body
    assert "ctx.r3.u32" in bugcheck_body

    bugcheckex_body = text.split("void __imp__KeBugCheckEx(")[1].split(
        "\n}\n"
    )[0]
    assert "std::abort()" in bugcheckex_body
    assert "ctx.r4.u32" in bugcheckex_body
    assert "ctx.r7.u32" in bugcheckex_body


def test_ke_delay_execution_thread_actually_sleeps(tmp_path: Path) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text("PPC_EXTERN_FUNC(__imp__KeDelayExecutionThread);\n")
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 1
    text = output.read_text()
    body = text.split("void __imp__KeDelayExecutionThread(")[1].split(
        "\n}\n"
    )[0]
    assert "kOfflineStatus" not in body
    assert "PPC_LOAD_U64(ctx.r5.u32)" in body
    assert "std::this_thread::sleep_for" in body
    assert "interval < 0" in body


def test_xam_alloc_and_free_use_the_guest_bump_allocator(tmp_path: Path) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text(
        "PPC_EXTERN_FUNC(__imp__XamAlloc);\n"
        "PPC_EXTERN_FUNC(__imp__XamFree);\n"
    )
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 2
    text = output.read_text()

    alloc_body = text.split("void __imp__XamAlloc(")[1].split("\n}\n")[0]
    assert "kOfflineStatus" not in alloc_body
    assert "allocate_guest(base, ctx.r4.u32)" in alloc_body
    assert "PPC_STORE_U32(ctx.r5.u32, address)" in alloc_body

    free_body = text.split("void __imp__XamFree(")[1].split("\n}\n")[0]
    assert "kOfflineStatus" not in free_body
    assert "ctx.r3.u64 = 0u" in free_body


def test_ob_symbolic_link_registration_always_succeeds(tmp_path: Path) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text(
        "PPC_EXTERN_FUNC(__imp__ObCreateSymbolicLink);\n"
        "PPC_EXTERN_FUNC(__imp__ObDeleteSymbolicLink);\n"
    )
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 2
    text = output.read_text()

    create_body = text.split("void __imp__ObCreateSymbolicLink(")[1].split(
        "\n}\n"
    )[0]
    assert "kOfflineStatus" not in create_body
    assert "ctx.r3.u64 = 0u" in create_body

    delete_body = text.split("void __imp__ObDeleteSymbolicLink(")[1].split(
        "\n}\n"
    )[0]
    assert "kOfflineStatus" not in delete_body
    assert "ctx.r3.u64 = 0u" in delete_body


def test_l2_lock_and_apc_nop_always_succeed(tmp_path: Path) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text(
        "PPC_EXTERN_FUNC(__imp__KeLockL2);\n"
        "PPC_EXTERN_FUNC(__imp__KeUnlockL2);\n"
        "PPC_EXTERN_FUNC(__imp__KiApcNormalRoutineNop);\n"
    )
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 3
    text = output.read_text()
    for name in ("KeLockL2", "KeUnlockL2", "KiApcNormalRoutineNop"):
        body = text.split(f"void __imp__{name}(")[1].split("\n}\n")[0]
        assert "kOfflineStatus" not in body
        assert "ctx.r3.u64 = 0u" in body


def test_xam_task_should_exit_defaults_to_keep_working(tmp_path: Path) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text("PPC_EXTERN_FUNC(__imp__XamTaskShouldExit);\n")
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 1
    text = output.read_text()
    body = text.split("void __imp__XamTaskShouldExit(")[1].split("\n}\n")[0]
    assert "kOfflineStatus" not in body
    assert "ctx.r3.u64 = 0u" in body


def test_nt_flush_buffers_file_always_succeeds(tmp_path: Path) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text("PPC_EXTERN_FUNC(__imp__NtFlushBuffersFile);\n")
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 1
    text = output.read_text()
    body = text.split("void __imp__NtFlushBuffersFile(")[1].split("\n}\n")[0]
    assert "kOfflineStatus" not in body
    assert "ctx.r3.u64 = 0u" in body


def test_xnotify_family_reports_no_notification_pending(tmp_path: Path) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text(
        "PPC_EXTERN_FUNC(__imp__XNotifyGetNext);\n"
        "PPC_EXTERN_FUNC(__imp__XNotifyPositionUI);\n"
    )
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 2
    text = output.read_text()
    for name in ("XNotifyGetNext", "XNotifyPositionUI"):
        body = text.split(f"void __imp__{name}(")[1].split("\n}\n")[0]
        assert "kOfflineStatus" not in body
        assert "ctx.r3.u64 = 0u" in body


def test_nt_open_file_reuses_the_create_file_media_service_path(
    tmp_path: Path,
) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text("PPC_EXTERN_FUNC(__imp__NtOpenFile);\n")
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 1
    text = output.read_text()
    assert "native_guest_media_service().open_file(relative)" in text
    assert "guest_path_to_relative(raw)" in text
    assert "[NtOpenFile]" in text
    assert "[NtCreateFile]" not in text
    assert "0xC0000034u" in text  # STATUS_OBJECT_NAME_NOT_FOUND on a real miss
    body = text.split("void __imp__NtOpenFile(")[1].split("\n}\n")[0]
    assert "kOfflineStatus" not in body


def test_xaudio_voice_category_volume_reports_no_change_full_volume(
    tmp_path: Path,
) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text(
        "PPC_EXTERN_FUNC(__imp__XAudioGetVoiceCategoryVolumeChangeMask);\n"
        "PPC_EXTERN_FUNC(__imp__XAudioGetVoiceCategoryVolume);\n"
    )
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 2
    text = output.read_text()

    mask_body = text.split(
        "void __imp__XAudioGetVoiceCategoryVolumeChangeMask("
    )[1].split("\n}\n")[0]
    assert "kOfflineStatus" not in mask_body
    assert "PPC_STORE_U32(ctx.r4.u32, 0u)" in mask_body

    volume_body = text.split("void __imp__XAudioGetVoiceCategoryVolume(")[
        1
    ].split("\n}\n")[0]
    assert "kOfflineStatus" not in volume_body
    assert "kFullVolume = 1.0f" in volume_body
    assert "PPC_STORE_U32(ctx.r4.u32, bits)" in volume_body


def test_io_dismount_volume_family_always_succeeds(tmp_path: Path) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text(
        "PPC_EXTERN_FUNC(__imp__IoDismountVolume);\n"
        "PPC_EXTERN_FUNC(__imp__IoDismountVolumeByFileHandle);\n"
    )
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 2
    text = output.read_text()
    for name in ("IoDismountVolume", "IoDismountVolumeByFileHandle"):
        body = text.split(f"void __imp__{name}(")[1].split("\n}\n")[0]
        assert "kOfflineStatus" not in body
        assert "ctx.r3.u64 = 0u" in body


def test_xam_notify_create_listener_returns_a_real_handle(tmp_path: Path) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text("PPC_EXTERN_FUNC(__imp__XamNotifyCreateListener);\n")
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 1
    text = output.read_text()
    body = text.split("void __imp__XamNotifyCreateListener(")[1].split(
        "\n}\n"
    )[0]
    assert "kOfflineStatus" not in body
    assert "g_next_handle.fetch_add(1u)" in body


def test_xaudio_render_driver_client_family(tmp_path: Path) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text(
        "PPC_EXTERN_FUNC(__imp__XAudioRegisterRenderDriverClient);\n"
        "PPC_EXTERN_FUNC(__imp__XAudioUnregisterRenderDriverClient);\n"
        "PPC_EXTERN_FUNC(__imp__XAudioSubmitRenderDriverFrame);\n"
    )
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 3
    text = output.read_text()

    register_body = text.split(
        "void __imp__XAudioRegisterRenderDriverClient("
    )[1].split("\n}\n")[0]
    assert "kOfflineStatus" not in register_body
    assert "g_next_handle.fetch_add(1u)" in register_body
    assert "PPC_STORE_U32(ctx.r4.u32" in register_body

    unregister_body = text.split(
        "void __imp__XAudioUnregisterRenderDriverClient("
    )[1].split("\n}\n")[0]
    assert "kOfflineStatus" not in unregister_body
    assert "ctx.r3.u64 = 0u" in unregister_body

    submit_body = text.split("void __imp__XAudioSubmitRenderDriverFrame(")[
        1
    ].split("\n}\n")[0]
    assert "kOfflineStatus" not in submit_body
    assert "ctx.r3.u64 = 0u" in submit_body


def test_xam_voice_headset_present_reports_absent(tmp_path: Path) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text("PPC_EXTERN_FUNC(__imp__XamVoiceHeadsetPresent);\n")
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 1
    text = output.read_text()
    body = text.split("void __imp__XamVoiceHeadsetPresent(")[1].split(
        "\n}\n"
    )[0]
    assert "kOfflineStatus" not in body
    assert "ctx.r3.u64 = 0u" in body


def test_xam_voice_close_always_succeeds(tmp_path: Path) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text("PPC_EXTERN_FUNC(__imp__XamVoiceClose);\n")
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 1
    text = output.read_text()
    body = text.split("void __imp__XamVoiceClose(")[1].split("\n}\n")[0]
    assert "kOfflineStatus" not in body
    assert "ctx.r3.u64 = 0u" in body


def test_xam_loader_terminate_title_exits_cleanly(tmp_path: Path) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text("PPC_EXTERN_FUNC(__imp__XamLoaderTerminateTitle);\n")
    output = tmp_path / "stubs.cpp"
    assert MODULE.render(mapping, output) == 1
    text = output.read_text()
    body = text.split("void __imp__XamLoaderTerminateTitle(")[1].split(
        "\n}\n"
    )[0]
    assert "kOfflineStatus" not in body
    assert "std::exit(0)" in body
