# Unit Tests for ReXGlue core components

add_executable(unit_tests
    memory/heap_allocation_test.cpp
    kernel/object_table_test.cpp
    core/cvar_test.cpp
    core/fiber_test.cpp
    core/filesystem_posix_test.cpp
    core/memory_posix_test.cpp
    core/threading_test.cpp
    core/sha256_test.cpp
    core/stream_test.cpp
    core/byte_order_test.cpp
    core/chrono_test.cpp
    codegen/config_test.cpp
    codegen/function_graph_test.cpp
    codegen/template_registry_test.cpp
)

target_include_directories(unit_tests PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

# rexsystem and rexkernel form a static-library cycle: kernel_state.cpp and
# xthread.cpp in rexsystem call xeKeKfReleaseSpinLock, which rexkernel defines,
# and rexkernel reaches back into rexsystem. A single pass over the group leaves
# the spinlock symbol undefined and the whole suite fails to link -- which is
# why no unit test in this repository currently runs. RESCAN is the same
# treatment the codegen-only profile applies to its own dependency group.
target_link_libraries(unit_tests PRIVATE
    rexcore
    "$<LINK_GROUP:RESCAN,rexcodegen,rexui,rexgraphics,rexkernel,rexsystem>"
    Catch2::Catch2WithMain
)

set_target_properties(unit_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/bin
)

# Discover tests
include(Catch)
catch_discover_tests(unit_tests
    TEST_PREFIX "unit."
    PROPERTIES
        LABELS "unit"
        TIMEOUT 60
)
