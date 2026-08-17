cmake_minimum_required(VERSION 3.20)

project(ac6_demo_recomp VERSION 0.1.0 LANGUAGES C CXX)

option(AC6_DEMO_ENABLE_CODEGEN "Expose the pinned XenonRecomp codegen target" OFF)
option(AC6_DEMO_ENABLE_VECTOR_READ_TRACE
       "Intercept direct generated VMX loads for an opt-in frontbuffer audit"
       OFF)
set(AC6_DEMO_XEX "" CACHE FILEPATH "Qualified demo Default.xex used by the codegen target")
set(AC6_DEMO_XEX1TOOL "" CACHE FILEPATH "xex1tool used to extract .pdata and imports")
set(AC6_DEMO_GHIDRA_MANIFEST "" CACHE FILEPATH "Qualified Ghidra chunk manifest")
set(AC6_DEMO_XENONRECOMP_ROOT "" CACHE PATH
    "Optional source checkout for the pinned XenonRecomp build")
set(AC6_DEMO_REXGLUE_REFERENCE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../../.tools/rexglue-upstream-reference"
    CACHE PATH "Pinned read-only ReXGlue reference checkout")
set(AC6_DEMO_REXGLUE_SDK
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../../.tools/ac6-recomp-oracle-runtime-dcd41b/thirdparty/rexglue-sdk"
    CACHE PATH "Pinned ReXGlue SDK used by the runtime shader translator")
set(AC6_DEMO_SIMDE_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../../.tools/xenonrecomp-source/thirdparty"
    CACHE PATH "Parent of pinned SIMDe headers required by the ReXGlue SDK")
set(AC6_DEMO_BUILD_JOBS 12 CACHE STRING
    "Default Ninja compile parallelism for the AC6 demo product")
if (NOT AC6_DEMO_BUILD_JOBS MATCHES "^[1-9][0-9]*$")
  message(FATAL_ERROR "AC6_DEMO_BUILD_JOBS must be a positive integer")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
find_package(Threads REQUIRED)

set(AC6_XBOX360_HOST_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/../../reconstruction/ac6-xbox360-host")
add_library(ac6_xbox360_host STATIC
  "${AC6_XBOX360_HOST_ROOT}/src/xam_input_movie.cpp")
target_include_directories(ac6_xbox360_host PUBLIC
  "${AC6_XBOX360_HOST_ROOT}/include")
target_compile_features(ac6_xbox360_host PUBLIC cxx_std_20)

add_library(ac6_demo_runtime STATIC
  src/content.cpp
  src/frontier_report.cpp
  src/guest_bridge.cpp
  src/guest_bridge/vector_read_trace.cpp
  src/guest_bridge_resources.cpp
  src/guest_memory.cpp
  src/graphics.cpp
  src/hash.cpp
  src/input.cpp
  src/imports.cpp
  src/media.cpp
  src/ppc.cpp
  src/session.cpp
  src/trace.cpp
  src/xenos_command_processor.cpp
  src/xenos_shader_translator.cpp
  src/xenos_tiling.cpp
  src/xex_image.cpp
)

target_include_directories(ac6_demo_runtime PUBLIC include)
target_compile_features(ac6_demo_runtime PUBLIC cxx_std_20)

find_package(OpenSSL REQUIRED COMPONENTS Crypto)
find_package(Vulkan QUIET)
find_package(Python3 COMPONENTS Interpreter REQUIRED)
target_link_libraries(ac6_xbox360_host PUBLIC OpenSSL::Crypto)
target_link_libraries(ac6_demo_runtime PUBLIC
  ac6_xbox360_host
  OpenSSL::Crypto)

set(AC6_DEMO_REXGLUE_LIBDIR "${AC6_DEMO_REXGLUE_SDK}/out/linux-amd64")
if (EXISTS "${AC6_DEMO_REXGLUE_LIBDIR}/librexgraphics.a")
  file(SHA256 "${AC6_DEMO_REXGLUE_LIBDIR}/librexgraphics.a"
       AC6_DEMO_REXGRAPHICS_SHA256)
  if (NOT AC6_DEMO_REXGRAPHICS_SHA256 STREQUAL
      "564a728c0f83217d0e6f1c5c4cb8d829cb9d3c6e04c63e4285b1a449a98243a5")
    message(FATAL_ERROR "Pinned ReXGlue graphics archive hash mismatch")
  endif()
  add_library(ac6_demo_rexglue_translator STATIC
    src/rexglue_runtime_shader.cpp)
  target_compile_features(ac6_demo_rexglue_translator PUBLIC cxx_std_23)
  target_include_directories(ac6_demo_rexglue_translator PUBLIC include PRIVATE
    "${AC6_DEMO_REXGLUE_SDK}/include"
    "${AC6_DEMO_REXGLUE_SDK}/thirdparty/renderdoc"
    "${AC6_DEMO_REXGLUE_SDK}/thirdparty/spirv-headers/include"
    "${AC6_DEMO_REXGLUE_SDK}/thirdparty/spirv-tools/include"
    "${AC6_DEMO_REXGLUE_SDK}/thirdparty/vulkan-headers/include"
    "${AC6_DEMO_REXGLUE_SDK}/thirdparty/fmt/include"
    "${AC6_DEMO_REXGLUE_SDK}/thirdparty/spdlog/include"
    "${AC6_DEMO_REXGLUE_SDK}/thirdparty/glslang"
    "${AC6_DEMO_REXGLUE_SDK}/thirdparty/glslang/SPIRV"
    "${AC6_DEMO_REXGLUE_SDK}/thirdparty/volk"
    "${AC6_DEMO_REXGLUE_SDK}/thirdparty/vulkan-memory-allocator/include"
    "${AC6_DEMO_SIMDE_ROOT}")
  target_link_libraries(ac6_demo_rexglue_translator PUBLIC OpenSSL::Crypto
    "-Wl,--start-group"
    "${AC6_DEMO_REXGLUE_LIBDIR}/librexgraphics.a"
    "${AC6_DEMO_REXGLUE_LIBDIR}/librexcore.a"
    "${AC6_DEMO_REXGLUE_LIBDIR}/librexsystem.a"
    "${AC6_DEMO_REXGLUE_LIBDIR}/librexui.a"
    "${AC6_DEMO_REXGLUE_LIBDIR}/libSPIRV.a"
    "${AC6_DEMO_REXGLUE_LIBDIR}/libSPIRV-Tools.a"
    "${AC6_DEMO_REXGLUE_LIBDIR}/libglslang.a"
    "${AC6_DEMO_REXGLUE_LIBDIR}/libMachineIndependent.a"
    "${AC6_DEMO_REXGLUE_LIBDIR}/libGenericCodeGen.a"
    "${AC6_DEMO_REXGLUE_LIBDIR}/libOSDependent.a"
    "${AC6_DEMO_REXGLUE_LIBDIR}/libOGLCompiler.a"
    "${AC6_DEMO_REXGLUE_LIBDIR}/libfmt.a"
    "${AC6_DEMO_REXGLUE_LIBDIR}/libxxhash.a"
    "${AC6_DEMO_REXGLUE_LIBDIR}/libsnappy.a"
    "${AC6_DEMO_REXGLUE_LIBDIR}/libvolk.a"
    "${AC6_DEMO_REXGLUE_LIBDIR}/libspdlog.a"
    "-Wl,--end-group" pthread dl)
  target_compile_definitions(ac6_demo_runtime PUBLIC
    AC6_DEMO_HAVE_REXGLUE_TRANSLATOR=1)
  add_executable(ac6-demo-rexglue-runtime-probe
    tools/rexglue_runtime_shader_probe.cpp)
  target_link_libraries(ac6-demo-rexglue-runtime-probe PRIVATE
    ac6_demo_rexglue_translator)
  add_executable(ac6-demo-rexglue-shader-cli
    tools/rexglue_shader_cli.cpp)
  target_compile_features(ac6-demo-rexglue-shader-cli PRIVATE cxx_std_23)
  target_include_directories(ac6-demo-rexglue-shader-cli PRIVATE
    "${AC6_DEMO_REXGLUE_SDK}/include"
    "${AC6_DEMO_REXGLUE_SDK}/thirdparty/renderdoc"
    "${AC6_DEMO_REXGLUE_SDK}/thirdparty/spirv-headers/include"
    "${AC6_DEMO_REXGLUE_SDK}/thirdparty/spirv-tools/include"
    "${AC6_DEMO_REXGLUE_SDK}/thirdparty/vulkan-headers/include"
    "${AC6_DEMO_REXGLUE_SDK}/thirdparty/fmt/include"
    "${AC6_DEMO_REXGLUE_SDK}/thirdparty/spdlog/include"
    "${AC6_DEMO_REXGLUE_SDK}/thirdparty/glslang"
    "${AC6_DEMO_REXGLUE_SDK}/thirdparty/glslang/SPIRV"
    "${AC6_DEMO_REXGLUE_SDK}/thirdparty/volk"
    "${AC6_DEMO_REXGLUE_SDK}/thirdparty/vulkan-memory-allocator/include"
    "${AC6_DEMO_SIMDE_ROOT}")
  target_link_libraries(ac6-demo-rexglue-shader-cli PRIVATE
    ac6_demo_rexglue_translator)
  add_executable(ac6-demo-rexglue-runtime-tests
    tests/ac6-demo-rexglue-runtime-tests.cpp)
  target_link_libraries(ac6-demo-rexglue-runtime-tests PRIVATE
    ac6_demo_rexglue_translator)
  add_test(NAME ac6-demo-rexglue-runtime-tests
    COMMAND ac6-demo-rexglue-runtime-tests)
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(ac6_xbox360_host PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wdouble-promotion)
  target_compile_options(ac6_demo_runtime PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wdouble-promotion)
endif()

set(AC6_DEMO_GENERATED_SOURCES)
if (Vulkan_FOUND AND TARGET ac6_demo_rexglue_translator)
  set(AC6_DEMO_DXC
      "${CMAKE_CURRENT_SOURCE_DIR}/../../../../.tools/dxc-install/bin/dxc")
  set(AC6_DEMO_SPIRV_VAL
      "${CMAKE_CURRENT_SOURCE_DIR}/../../../../.tools/spirv-tools-install/bin/spirv-val")
  foreach(tool IN ITEMS AC6_DEMO_DXC AC6_DEMO_SPIRV_VAL)
    if (NOT EXISTS "${${tool}}")
      message(FATAL_ERROR "Required pinned shader tool is missing: ${${tool}}")
    endif()
  endforeach()
  file(SHA256 "${AC6_DEMO_DXC}" AC6_DEMO_DXC_SHA256)
  file(SHA256 "${AC6_DEMO_SPIRV_VAL}" AC6_DEMO_SPIRV_VAL_SHA256)
  if (NOT AC6_DEMO_DXC_SHA256 STREQUAL
      "db50584b967fba011f571a6b63e63ae9d14a04418a52d57ff3600750b1c9940d" OR
      NOT AC6_DEMO_SPIRV_VAL_SHA256 STREQUAL
      "2cc19cddc1293518705467f41f55094800b319bd77b1eaf6e30bc7901d6e3406")
    message(FATAL_ERROR "Pinned shader tool hash mismatch")
  endif()
  set(AC6_DEMO_RECTANGLE_SPV
      "${CMAKE_CURRENT_BINARY_DIR}/generated/rexglue_rectangle_list.gs.spv")
  set(AC6_DEMO_RECTANGLE_HEADER
      "${CMAKE_CURRENT_BINARY_DIR}/generated/rexglue_rectangle_list_spirv.hpp")
  add_custom_command(
    OUTPUT "${AC6_DEMO_RECTANGLE_HEADER}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${CMAKE_CURRENT_BINARY_DIR}/generated"
    COMMAND "${AC6_DEMO_DXC}" -spirv -fspv-target-env=vulkan1.1
            -T gs_6_0 -E main
            -Fo "${AC6_DEMO_RECTANGLE_SPV}"
            "${CMAKE_CURRENT_SOURCE_DIR}/shaders/rexglue_rectangle_list.gs.hlsl"
    COMMAND "${AC6_DEMO_SPIRV_VAL}" --target-env vulkan1.1
            "${AC6_DEMO_RECTANGLE_SPV}"
    COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_spirv.py"
            --input "${AC6_DEMO_RECTANGLE_SPV}"
            --output "${AC6_DEMO_RECTANGLE_HEADER}"
            --symbol rexglue_rectangle_list_spirv
    DEPENDS shaders/rexglue_rectangle_list.gs.hlsl tools/embed_spirv.py
    VERBATIM)
  list(APPEND AC6_DEMO_GENERATED_SOURCES "${AC6_DEMO_RECTANGLE_HEADER}")
endif()

add_executable(ac6-demo-recomp src/main.cpp src/cli.cpp src/emu_agent_ipc.cpp
  src/vulkan_normal_draw.cpp src/vulkan_shared_memory.cpp
  ${AC6_DEMO_GENERATED_SOURCES})
target_include_directories(ac6-demo-recomp PRIVATE
  "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(ac6-demo-recomp PRIVATE ac6_demo_runtime)
if (TARGET ac6_demo_rexglue_translator)
  target_link_libraries(ac6-demo-recomp PRIVATE ac6_demo_rexglue_translator)
  if (Vulkan_FOUND)
    target_link_libraries(ac6-demo-recomp PRIVATE Vulkan::Vulkan)
    target_compile_definitions(ac6-demo-recomp PRIVATE
      AC6_DEMO_HAVE_VULKAN_RENDERER_FRONTIER=1)
  endif()
endif()

if (CMAKE_GENERATOR MATCHES "Ninja")
  set(CMAKE_JOB_POOLS ac6_demo_compile=${AC6_DEMO_BUILD_JOBS}
                      ac6_demo_link=1)
  set_property(TARGET ac6_xbox360_host PROPERTY JOB_POOL_COMPILE ac6_demo_compile)
  set_property(TARGET ac6_xbox360_host PROPERTY JOB_POOL_LINK ac6_demo_link)
  set_property(TARGET ac6_demo_runtime PROPERTY JOB_POOL_COMPILE ac6_demo_compile)
  set_property(TARGET ac6-demo-recomp PROPERTY JOB_POOL_COMPILE ac6_demo_compile)
  set_property(TARGET ac6_demo_runtime PROPERTY JOB_POOL_LINK ac6_demo_link)
  set_property(TARGET ac6-demo-recomp PROPERTY JOB_POOL_LINK ac6_demo_link)
endif()

if (AC6_DEMO_ENABLE_CODEGEN)
  if (NOT AC6_DEMO_XEX OR NOT AC6_DEMO_XEX1TOOL OR NOT AC6_DEMO_GHIDRA_MANIFEST)
    message(FATAL_ERROR
      "AC6_DEMO_ENABLE_CODEGEN requires AC6_DEMO_XEX, AC6_DEMO_XEX1TOOL and "
      "AC6_DEMO_GHIDRA_MANIFEST")
  endif()

  set(AC6_DEMO_CODEGEN_ROOT "${CMAKE_CURRENT_BINARY_DIR}/codegen")
  set(AC6_DEMO_CODEGEN_OBJECT "${AC6_DEMO_CODEGEN_ROOT}/generated-guest.o")
  set(AC6_DEMO_CODEGEN_MANIFEST "${AC6_DEMO_CODEGEN_ROOT}/manifest.json")
  set(AC6_DEMO_BUILD_IDENTITY
      "${AC6_DEMO_CODEGEN_ROOT}/ac6_demo_build_identity.hpp")
  set(AC6_DEMO_CODEGEN_COMMAND
      "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/tools/build_demo.py"
      --xex "${AC6_DEMO_XEX}"
      --build-root "${AC6_DEMO_CODEGEN_ROOT}"
      --xex1tool "${AC6_DEMO_XEX1TOOL}"
      --ghidra-manifest "${AC6_DEMO_GHIDRA_MANIFEST}")
  if (AC6_DEMO_XENONRECOMP_ROOT)
    list(APPEND AC6_DEMO_CODEGEN_COMMAND
         --xenonrecomp-root "${AC6_DEMO_XENONRECOMP_ROOT}")
  endif()
  add_custom_command(
    OUTPUT "${AC6_DEMO_CODEGEN_OBJECT}" "${AC6_DEMO_CODEGEN_MANIFEST}"
           "${AC6_DEMO_BUILD_IDENTITY}"
    COMMAND ${AC6_DEMO_CODEGEN_COMMAND}
    COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/tools/write_build_identity.py"
      --manifest "${AC6_DEMO_CODEGEN_MANIFEST}"
      --ghidra-manifest "${AC6_DEMO_GHIDRA_MANIFEST}"
      --boundary-config
        "${CMAKE_CURRENT_SOURCE_DIR}/config/confirmed-chunks.toml"
      --output "${AC6_DEMO_BUILD_IDENTITY}"
    DEPENDS
      "${CMAKE_CURRENT_SOURCE_DIR}/tools/build_demo.py"
      "${CMAKE_CURRENT_SOURCE_DIR}/tools/write_build_identity.py"
      "${CMAKE_CURRENT_SOURCE_DIR}/patches/xenonrecomp-strict-recompiler.patch"
      "${CMAKE_CURRENT_SOURCE_DIR}/patches/xenonrecomp-strict-config.patch"
      "${CMAKE_CURRENT_SOURCE_DIR}/tools/ppc_context_adapter.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/config/demo-identity.json"
      "${CMAKE_CURRENT_SOURCE_DIR}/config/demo-abi.json"
      "${CMAKE_CURRENT_SOURCE_DIR}/config/confirmed-chunks.toml"
      "${AC6_DEMO_XEX}"
      "${AC6_DEMO_XEX1TOOL}"
      "${AC6_DEMO_GHIDRA_MANIFEST}"
    VERBATIM
    COMMENT "Generating the qualified AC6 guest with pinned XenonRecomp")
  add_custom_target(ac6-demo-codegen
                    DEPENDS "${AC6_DEMO_CODEGEN_OBJECT}"
                            "${AC6_DEMO_CODEGEN_MANIFEST}"
                            "${AC6_DEMO_BUILD_IDENTITY}")

  set(AC6_DEMO_LZX_SOURCE
      "${AC6_DEMO_CODEGEN_ROOT}/XenonRecomp/thirdparty/libmspack/libmspack/mspack/lzxd.c")
  set_source_files_properties("${AC6_DEMO_LZX_SOURCE}" PROPERTIES GENERATED TRUE)
  target_sources(ac6_demo_runtime PRIVATE
    "${AC6_DEMO_CODEGEN_OBJECT}"
    "${AC6_DEMO_LZX_SOURCE}")
  add_dependencies(ac6_demo_runtime ac6-demo-codegen)
  target_include_directories(ac6_demo_runtime PRIVATE
    "${AC6_DEMO_CODEGEN_ROOT}"
    "${AC6_DEMO_CODEGEN_ROOT}/generated"
    "${AC6_DEMO_CODEGEN_ROOT}/XenonRecomp/thirdparty/simde"
    "${AC6_DEMO_CODEGEN_ROOT}/XenonRecomp/thirdparty/libmspack/libmspack/mspack")
  target_compile_definitions(ac6_demo_runtime PRIVATE
    AC6_DEMO_GENERATED_GUEST=1
    AC6_DEMO_HAVE_LZX=1)
  if (AC6_DEMO_ENABLE_VECTOR_READ_TRACE)
    target_compile_definitions(ac6_demo_runtime PRIVATE
      AC6_DEMO_ENABLE_VECTOR_READ_TRACE=1)
  endif()
endif()

include(GNUInstallDirs)
install(TARGETS ac6-demo-recomp RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY config DESTINATION ${CMAKE_INSTALL_DATADIR}/ac6-demo-recomp)

enable_testing()
add_executable(ac6-demo-post-resume-header-tests
  tests/post_resume_probe_header_tests.cpp)
target_link_libraries(ac6-demo-post-resume-header-tests PRIVATE Threads::Threads)
add_test(NAME ac6-demo-post-resume-header-tests
  COMMAND ac6-demo-post-resume-header-tests)
foreach(test_name IN ITEMS
    ac6-demo-core-tests
    ac6-demo-content-tests
    ac6-demo-frontier-tests
    ac6-demo-graphics-tests
    ac6-demo-input-tests
    ac6-demo-reached-raster-tests
    ac6-demo-trace-tests
    ac6-demo-xenos-command-tests
    ac6-demo-xenos-shader-tests
    ac6-demo-xenos-tiling-tests)
  add_executable(${test_name} tests/${test_name}.cpp)
  target_link_libraries(${test_name} PRIVATE ac6_demo_runtime)
  if (CMAKE_GENERATOR MATCHES "Ninja")
    set_property(TARGET ${test_name} PROPERTY JOB_POOL_COMPILE ac6_demo_compile)
    set_property(TARGET ${test_name} PROPERTY JOB_POOL_LINK ac6_demo_link)
  endif()
  add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()
add_executable(ac6-demo-emu-agent-ipc-tests
  tests/ac6-demo-emu-agent-ipc-tests.cpp src/emu_agent_ipc.cpp)
target_link_libraries(ac6-demo-emu-agent-ipc-tests PRIVATE ac6_demo_runtime)
add_test(NAME ac6-demo-emu-agent-ipc-tests COMMAND ac6-demo-emu-agent-ipc-tests)
if (Vulkan_FOUND)
  set(AC6_DEMO_REXGLUE_RESOLVE_SHADER
      "${AC6_DEMO_REXGLUE_REFERENCE}/src/graphics/shaders/vulkan_spirv/resolve_fast_32bpp_1x2xmsaa_cs.h")
  if (EXISTS "${AC6_DEMO_REXGLUE_RESOLVE_SHADER}")
    file(SHA256 "${AC6_DEMO_REXGLUE_RESOLVE_SHADER}"
         AC6_DEMO_REXGLUE_RESOLVE_SHADER_SHA256)
    if (NOT AC6_DEMO_REXGLUE_RESOLVE_SHADER_SHA256 STREQUAL
        "de24d6b23367da6b2fa3b5d1d843d920cbdf4a5170cf49544412c4bebcb1eb11")
      message(FATAL_ERROR "Pinned ReXGlue resolve shader header hash mismatch")
    endif()
    target_sources(ac6-demo-recomp PRIVATE src/vulkan_neutral_resolve.cpp)
    target_include_directories(ac6-demo-recomp PRIVATE
      "${AC6_DEMO_REXGLUE_REFERENCE}/src/graphics/shaders/vulkan_spirv")
    add_executable(ac6-demo-vulkan-resolve-tests
      tests/ac6-demo-vulkan-resolve-tests.cpp)
    target_include_directories(ac6-demo-vulkan-resolve-tests PRIVATE
      "${AC6_DEMO_REXGLUE_REFERENCE}/src/graphics/shaders/vulkan_spirv")
    target_link_libraries(ac6-demo-vulkan-resolve-tests PRIVATE
      ac6_demo_runtime Vulkan::Vulkan OpenSSL::Crypto)
    target_compile_definitions(ac6-demo-vulkan-resolve-tests PRIVATE
      AC6_DEMO_REXGLUE_COMMIT="cb58065c793429aa92895d778af58d12e9d26d8f")
    add_test(NAME ac6-demo-vulkan-resolve-tests
      COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tools/run_vulkan_resolve_repeat.py
        $<TARGET_FILE:ac6-demo-vulkan-resolve-tests>
        ${AC6_DEMO_REXGLUE_RESOLVE_SHADER}
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../../.tools/spirv-tools-install/bin/spirv-val)
    set_tests_properties(ac6-demo-vulkan-resolve-tests PROPERTIES
      SKIP_RETURN_CODE 77)
  endif()
endif()
add_executable(ac6-xbox360-host-xam-input-movie-tests
  "${AC6_XBOX360_HOST_ROOT}/tests/xam_input_movie_tests.cpp")
target_link_libraries(ac6-xbox360-host-xam-input-movie-tests PRIVATE
  ac6_xbox360_host)
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(ac6-demo-input-tests PRIVATE -UNDEBUG)
  target_compile_options(ac6-xbox360-host-xam-input-movie-tests PRIVATE
    -UNDEBUG)
endif()
if (CMAKE_GENERATOR MATCHES "Ninja")
  set_property(TARGET ac6-xbox360-host-xam-input-movie-tests
               PROPERTY JOB_POOL_COMPILE ac6_demo_compile)
  set_property(TARGET ac6-xbox360-host-xam-input-movie-tests
               PROPERTY JOB_POOL_LINK ac6_demo_link)
endif()
add_test(NAME ac6-xbox360-host-xam-input-movie-tests
         COMMAND ac6-xbox360-host-xam-input-movie-tests)
add_test(NAME ac6-demo-python-tests
  COMMAND ${Python3_EXECUTABLE} -m unittest discover
    -s ${CMAKE_CURRENT_SOURCE_DIR}/tests -p test_build_demo.py)
add_test(NAME ac6-demo-post-resume-tests
  COMMAND ${Python3_EXECUTABLE} -m unittest discover
    -s ${CMAKE_CURRENT_SOURCE_DIR}/tests -p test_post_resume_probe.py)
add_test(NAME ac6-demo-xam-return-chain-mapper-tests
  COMMAND ${Python3_EXECUTABLE} -m unittest discover
    -s ${CMAKE_CURRENT_SOURCE_DIR}/tests -p test_xam_return_chain.py)
add_executable(ac6-demo-xam-return-chain-header-tests
  tests/xam_return_chain_header_tests.cpp)
target_include_directories(ac6-demo-xam-return-chain-header-tests PRIVATE
  src include)
target_link_libraries(ac6-demo-xam-return-chain-header-tests PRIVATE Threads::Threads)
add_test(NAME ac6-demo-xam-return-chain-header-tests
  COMMAND ac6-demo-xam-return-chain-header-tests)
add_test(NAME ac6-demo-post-resume-xvfb-lifecycle
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/test_post_resume_xvfb_lifecycle.sh)
set_tests_properties(ac6-demo-post-resume-xvfb-lifecycle PROPERTIES TIMEOUT 55)
add_test(NAME ac6-demo-complexity
  COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/audit_cpp_complexity.py
    --root ${CMAKE_CURRENT_SOURCE_DIR}
    --baseline ${CMAKE_CURRENT_SOURCE_DIR}/config/source-complexity-baseline.json
    --check)
add_test(NAME ac6-demo-source-audit
  COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/audit_demo_sources.py)
add_test(NAME ac6-demo-status
  COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/render_status.py --check)
if (NOT AC6_DEMO_ENABLE_CODEGEN)
  add_test(NAME ac6-demo-no-guest-cli
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_no_guest_cli.py
      $<TARGET_FILE:ac6-demo-recomp>)
endif()
