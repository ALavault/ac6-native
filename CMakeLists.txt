# ac6recomp - ReXGlue Recompiled Project
#
# This file is yours to edit. 'rexglue migrate' will NOT overwrite it.
# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(ac6recomp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CheckIPOSupported)
check_ipo_supported(RESULT AC6RECOMP_IPO_SUPPORTED OUTPUT AC6RECOMP_IPO_ERROR)

# Keep reconfigure passes pinned to the vendored SDK when the cache is empty.
if(NOT DEFINED REXSDK_DIR OR REXSDK_DIR STREQUAL "")
    set(_ac6_vendored_rexsdk "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/rexglue-sdk")
    if(EXISTS "${_ac6_vendored_rexsdk}/CMakeLists.txt")
        set(REXSDK_DIR "${_ac6_vendored_rexsdk}" CACHE PATH "Path to rexglue-sdk source tree" FORCE)
    endif()
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/generated/rexglue.cmake")
    include(generated/rexglue.cmake)
else()
    include(cmake/rexglue_bootstrap.cmake)
endif()

# Sources
set(AC6RECOMP_SOURCES
    src/main.cpp
    src/ac6_pac_decode_dump.cpp
    src/ac6_pac_decoder_probe.cpp
    src/ac6_pac_index.cpp
    src/ac6_recomp_overrides.cpp
    src/ac6_guest_call_trace.cpp
    src/ac6_ppc_store_watch.cpp
    src/d3d_hooks.cpp
    src/render_hooks.cpp
    src/ac6_native_graphics.cpp
    src/ac6_native_graphics_overlay.cpp
    src/ac6_backend_fixes/ac6_backend_capture_bridge.cpp
    src/ac6_backend_fixes/ac6_backend_hooks.cpp
    src/ac6_backend_fixes/ac6_backend_pass_classifier.cpp
    src/ac6_backend_fixes/ac6_fps_physics_fix.cpp
    src/ac6_backend_fixes/ac6_ui_input_dispatch_probe.cpp
)

# Texture dump/replacement is currently integrated only by the D3D12 texture
# cache. Do not pull Windows SDK types into the Vulkan-only Linux runtime.
if(REXGLUE_USE_D3D12)
    list(APPEND AC6RECOMP_SOURCES src/ac6_texture_overrides.cpp)
endif()

if(REXGLUE_CODEGEN_ONLY)
    # Keep the generated corpus compilable in the headless tool profile
    # without pretending that the UI-free stubs form a runnable game. Normal
    # builds below retain the real executable and runtime source set.
    add_library(ac6recomp OBJECT ${AC6RECOMP_SOURCES})
elseif(WIN32)
    add_executable(ac6recomp WIN32 ${AC6RECOMP_SOURCES})
else()
    add_executable(ac6recomp ${AC6RECOMP_SOURCES})
endif()

rexglue_setup_target(ac6recomp)

# Link libmspack for AC6 PAC LZX decompression in the runtime dump path.
target_link_libraries(ac6recomp PRIVATE mspack)

# P2.2: whole-program control-flow trace of the recompiled guest.
#
# -finstrument-functions is applied to the GENERATED sources only, so the
# callback fires at the entry of the 23 058 guest functions and nowhere else --
# instrumenting the runtime as well would bury the guest's execution path in
# host noise, and would recurse through the callback itself.
#
# LTO is forced off in this configuration. Cross-module inlining erases exactly
# the function boundaries the trace is trying to observe, and the plan's warning
# applies: 100 MB of generated C++ under LTO turns a five-minute turnaround into
# a forty-minute one, which is how a project ends up preferring twelve cheap
# useless measurements to one expensive decisive one. Build this in a separate
# directory and keep the shipping build untouched.
option(AC6RECOMP_TRACE_GUEST_CALLS "Instrument generated guest functions for control-flow tracing" OFF)
if(AC6RECOMP_TRACE_GUEST_CALLS)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        message(FATAL_ERROR "AC6RECOMP_TRACE_GUEST_CALLS needs -finstrument-functions (Clang or GCC)")
    endif()
    target_compile_definitions(ac6recomp PRIVATE AC6RECOMP_TRACE_GUEST_CALLS)

    # GENERATED_SOURCES is set by generated/sources.cmake, which the SDK
    # includes from its own scope -- so it is NOT visible here, and referencing
    # it without this include silently expands to nothing. The property call
    # then succeeds, applies to zero files, and produces a binary that looks
    # instrumented (it defines the callbacks and grows by 85 MB of debug info)
    # while no guest function ever calls them. Verified by disassembling a
    # generated function and finding no call to __cyg_profile_func_enter.
    include(generated/sources.cmake)
    list(LENGTH GENERATED_SOURCES _ac6_generated_count)
    if(_ac6_generated_count EQUAL 0)
        message(FATAL_ERROR "AC6RECOMP_TRACE_GUEST_CALLS: GENERATED_SOURCES is empty, nothing would be instrumented")
    endif()

    # Instrumenting all 52 translation units costs about a 7x slowdown -- the
    # runtime drops from 60 FPS to roughly 9 -- which is enough that the game
    # cannot reach the dialog inside a practical run length. AC6RECOMP_TRACE_FILTER
    # restricts instrumentation to the translation units that matter.
    #
    # The UI region (sub_821Bxxxx-sub_821Exxxx and sub_8234xxxx) lives in six
    # files, so "recomp\\.(13|14|15|16|37|38)\\." keeps the interesting code and
    # drops seven eighths of the cost. The trade is real coverage: a consumer
    # outside those files is invisible to the trace. The count is printed so the
    # reduction is never silent -- a narrowed trace that reads as a complete one
    # is how a null result gets believed.
    set(AC6RECOMP_TRACE_FILTER "" CACHE STRING
        "Regex selecting which generated sources to instrument (empty = all)")
    if(AC6RECOMP_TRACE_FILTER STREQUAL "")
        set(_ac6_trace_sources ${GENERATED_SOURCES})
    else()
        set(_ac6_trace_sources "")
        foreach(_src IN LISTS GENERATED_SOURCES)
            if(_src MATCHES "${AC6RECOMP_TRACE_FILTER}")
                list(APPEND _ac6_trace_sources "${_src}")
            endif()
        endforeach()
        if(_ac6_trace_sources STREQUAL "")
            message(FATAL_ERROR "AC6RECOMP_TRACE_FILTER '${AC6RECOMP_TRACE_FILTER}' matched no generated sources")
        endif()
    endif()
    list(LENGTH _ac6_trace_sources _ac6_trace_count)

    set_source_files_properties(${_ac6_trace_sources}
        TARGET_DIRECTORY ac6recomp
        PROPERTIES COMPILE_OPTIONS "-finstrument-functions"
    )
    message(STATUS "AC6: guest call tracing ON (${_ac6_trace_count} of ${_ac6_generated_count} generated sources instrumented, LTO forced off)")
endif()

# P2.3: route every guest store through a hook that records its writer.
#
# The prelude must be included BEFORE <rex/ppc/memory.h> for its #ifndef guards
# to leave our definitions in place, which -include guarantees. It goes on the
# generated sources only: the runtime's own code does not use PPC_STORE_*, and
# handing it the macros would only add host noise to a guest-store capture.
#
# LTO off for the same reason as the call trace, and because a store hook is a
# call on the hottest path in the program -- with LTO the inliner may hoist the
# armed check in ways that make the capture window imprecise.
option(AC6RECOMP_WATCH_GUEST_STORES "Record every guest store with its writer's guest lr" OFF)
if(AC6RECOMP_WATCH_GUEST_STORES)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        message(FATAL_ERROR "AC6RECOMP_WATCH_GUEST_STORES needs statement expressions (Clang or GCC)")
    endif()
    target_compile_definitions(ac6recomp PRIVATE AC6RECOMP_WATCH_GUEST_STORES)

    include(generated/sources.cmake)
    list(LENGTH GENERATED_SOURCES _ac6_store_count)
    if(_ac6_store_count EQUAL 0)
        message(FATAL_ERROR "AC6RECOMP_WATCH_GUEST_STORES: GENERATED_SOURCES is empty, no store would be hooked")
    endif()
    set_source_files_properties(${GENERATED_SOURCES}
        TARGET_DIRECTORY ac6recomp
        PROPERTIES COMPILE_OPTIONS "-include;${CMAKE_CURRENT_SOURCE_DIR}/src/ac6_ppc_store_hook.h"
    )
    message(STATUS "AC6: guest store watch ON (${_ac6_store_count} generated sources hooked, LTO forced off)")
endif()

if(AC6RECOMP_IPO_SUPPORTED AND NOT AC6RECOMP_TRACE_GUEST_CALLS AND NOT AC6RECOMP_WATCH_GUEST_STORES)
    set_property(TARGET ac6recomp PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
    set_property(TARGET ac6recomp PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO TRUE)
elseif(NOT AC6RECOMP_IPO_SUPPORTED)
    message(STATUS "IPO/LTO disabled for ac6recomp: ${AC6RECOMP_IPO_ERROR}")
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(ac6recomp PRIVATE
        $<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>:-fomit-frame-pointer>
    )
endif()

# AC6-specific unit tests.
#
# These cover the parts of the backend diagnostics that need neither the guest
# corpus nor a graphics device -- the draw telemetry counters, and the capture
# bridge's signature builder and material-draw identity latch -- so they build
# and run in seconds without the 100 MB recompiled corpus. Kept out of the
# ac6recomp target so a runtime build is unaffected, and off by default to
# match REXGLUE_BUILD_TESTS.
option(AC6RECOMP_BUILD_TESTS "Build AC6-specific unit tests" OFF)
if(AC6RECOMP_BUILD_TESTS AND NOT REXGLUE_CODEGEN_ONLY)
    enable_testing()

    # Sources each test needs beyond its own translation unit. The telemetry is
    # header-only; the capture bridge test calls BuildRenderEventSignature.
    set(ac6_backend_draw_telemetry_test_EXTRA "")
    set(ac6_backend_capture_bridge_test_EXTRA
        src/ac6_backend_fixes/ac6_backend_capture_bridge.cpp
        src/ac6_backend_fixes/ac6_backend_pass_classifier.cpp
    )

    foreach(ac6_test IN ITEMS ac6_backend_draw_telemetry ac6_backend_capture_bridge)
        add_executable(${ac6_test}_test
            src/ac6_backend_fixes/${ac6_test}_test.cpp
            ${${ac6_test}_test_EXTRA}
        )
        target_include_directories(${ac6_test}_test PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/src/ac6_backend_fixes
            ${REXSDK_DIR}/include
        )
        # Both tests state their expectations with assert(), and the Release and
        # RelWithDebInfo configurations define NDEBUG, which compiles every one
        # of them out. Without this the binaries run, exit 0, and check nothing
        # -- a green result that means the opposite of what it appears to.
        target_compile_options(${ac6_test}_test PRIVATE -UNDEBUG)
        add_test(NAME ${ac6_test} COMMAND ${ac6_test}_test)
    endforeach()
endif()
