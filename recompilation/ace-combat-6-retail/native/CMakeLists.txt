cmake_minimum_required(VERSION 3.20)
project(ac6_native_xenos LANGUAGES CXX)

find_package(OpenSSL REQUIRED COMPONENTS Crypto)
find_package(SDL2 REQUIRED)
find_package(Vulkan REQUIRED)
find_library(XCB_LIBRARY NAMES xcb REQUIRED)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Pinned US shader registry: the fixture is the single source of truth; the
# generated wrapper only carries its bytes into the build tree (ignored
# working copy). Never commit or install generated output.
set(AC6_PINNED_SHADER_FIXTURE
    "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/pinned-shader-registry.v1.bin"
    CACHE FILEPATH "pinned US shader registry capsule fixture")
set(AC6_PINNED_SHADER_DATA
    "${CMAKE_CURRENT_BINARY_DIR}/native_pinned_shaders_data.cpp")
add_custom_command(
  OUTPUT "${AC6_PINNED_SHADER_DATA}"
  COMMAND "${Python3_EXECUTABLE}"
          "${CMAKE_CURRENT_SOURCE_DIR}/tools/materialize_pinned_shader_data.py"
          --capsule "${AC6_PINNED_SHADER_FIXTURE}"
          --output "${AC6_PINNED_SHADER_DATA}"
  DEPENDS "${AC6_PINNED_SHADER_FIXTURE}"
          "${CMAKE_CURRENT_SOURCE_DIR}/tools/materialize_pinned_shader_data.py"
  VERBATIM)

add_library(ac6_native_xenos STATIC
  src/native_xenos.cpp
  src/native_vulkan_backend.cpp
  src/native_vulkan_device.cpp
  src/native_shader_translator.cpp
  src/native_pinned_shaders.cpp
  "${AC6_PINNED_SHADER_DATA}"
  src/native_services.cpp
  src/native_ppc_abi.cpp
  src/native_frontend.cpp
  src/native_guest_memory.cpp
  src/native_guest_vd.cpp
  src/native_guest_media.cpp
  src/native_guest_input.cpp
  src/native_guest_threads.cpp
  src/native_xex.cpp
  src/native_xdvdfs.cpp
  src/native_runtime.cpp
)
target_include_directories(ac6_native_xenos PUBLIC include)
target_link_libraries(ac6_native_xenos PUBLIC OpenSSL::Crypto SDL2::SDL2 Vulkan::Vulkan ${XCB_LIBRARY})
target_compile_options(ac6_native_xenos PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -Wconversion>
)

# Product entry point.  It currently owns media validation and the native
# runtime lifecycle; guest execution is enabled only by the qualified guest
# lane below and remains a separate Gate 2 responsibility.
add_executable(ac6recomp src/ac6recomp_main.cpp)
target_link_libraries(ac6recomp PRIVATE ac6_native_xenos)
target_compile_options(ac6recomp PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -Wconversion>
)
install(TARGETS ac6recomp RUNTIME DESTINATION bin)

# Optional build-time guest lane. The directory must be an ignored output from
# the qualified XenonRecomp run; no generated source can enter the product
# tree or installation. The default remains the dependency-free native tests.
set(AC6_NATIVE_GUEST_DIR "" CACHE PATH "ignored XenonRecomp generated output")
set(AC6_NATIVE_SIMDE_DIR "" CACHE PATH "build-time SIMDe include tree")
if(AC6_NATIVE_GUEST_DIR)
  file(GLOB AC6_NATIVE_GUEST_SOURCES CONFIGURE_DEPENDS
       "${AC6_NATIVE_GUEST_DIR}/ppc_recomp.*.cpp")
  if(EXISTS "${AC6_NATIVE_GUEST_DIR}/ppc_func_mapping.cpp")
    list(APPEND AC6_NATIVE_GUEST_SOURCES
         "${AC6_NATIVE_GUEST_DIR}/ppc_func_mapping.cpp")
  endif()
  if(NOT AC6_NATIVE_GUEST_SOURCES)
    message(FATAL_ERROR "AC6_NATIVE_GUEST_DIR has no generated guest sources")
  endif()
  if(NOT AC6_NATIVE_SIMDE_DIR)
    message(FATAL_ERROR "AC6_NATIVE_SIMDE_DIR is required for guest codegen")
  endif()
  set(AC6_NATIVE_IMPORT_STUBS "" CACHE FILEPATH
      "build-time generated native import definitions")
  if(NOT EXISTS "${AC6_NATIVE_IMPORT_STUBS}")
    message(FATAL_ERROR "AC6_NATIVE_IMPORT_STUBS is required for guest linkage")
  endif()
  list(APPEND AC6_NATIVE_GUEST_SOURCES "${AC6_NATIVE_IMPORT_STUBS}")
  add_library(ac6_native_guest STATIC ${AC6_NATIVE_GUEST_SOURCES})
  # The generated import stubs call into ac6_native_xenos services (Vd,
  # media, input, ...); declaring the dependency lets CMake order the
  # final static-archive link correctly instead of relying on the
  # accidental extraction order that happened to work while nothing in
  # ac6_native_xenos itself referenced native_guest_input_service.
  target_link_libraries(ac6_native_guest PRIVATE ac6_native_xenos)
  target_include_directories(ac6_native_guest PUBLIC
    "${AC6_NATIVE_GUEST_DIR}"
    "${AC6_NATIVE_SIMDE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
  target_compile_options(ac6_native_guest PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wno-unused-parameter -Wno-unused-variable -Wno-sign-compare>)
  target_link_libraries(ac6recomp PRIVATE ac6_native_guest)
  target_include_directories(ac6recomp PRIVATE "${AC6_NATIVE_GUEST_DIR}")
  target_compile_definitions(ac6recomp PRIVATE AC6_NATIVE_GUEST_LINKED=1)
  add_executable(ac6_native_guest_link_test src/native_guest_link_test.cpp)
  target_include_directories(ac6_native_guest_link_test PRIVATE "${AC6_NATIVE_GUEST_DIR}")
  target_link_libraries(ac6_native_guest_link_test PRIVATE ac6_native_guest
    ac6_native_xenos m)
  add_test(NAME ac6_native_guest_link_test COMMAND ac6_native_guest_link_test)
endif()

include(CTest)
if(BUILD_TESTING)
  # The test bodies use assertions as executable checks.  Keep them enabled
  # even when the product build is Release (-DNDEBUG).
  set(AC6_TEST_NO_NDEBUG -UNDEBUG)
  add_executable(ac6_native_xenos_tests tests/native_xenos_tests.cpp)
  target_link_libraries(ac6_native_xenos_tests PRIVATE ac6_native_xenos)
  target_compile_options(ac6_native_xenos_tests PRIVATE ${AC6_TEST_NO_NDEBUG})
  add_test(NAME ac6_native_xenos_tests COMMAND ac6_native_xenos_tests)

  add_executable(ac6_native_services_tests tests/native_services_tests.cpp)
  target_link_libraries(ac6_native_services_tests PRIVATE ac6_native_xenos)
  target_compile_options(ac6_native_services_tests PRIVATE ${AC6_TEST_NO_NDEBUG})
  add_test(NAME ac6_native_services_tests COMMAND ac6_native_services_tests)

  add_executable(ac6_native_ppc_abi_tests tests/native_ppc_abi_tests.cpp)
  target_link_libraries(ac6_native_ppc_abi_tests PRIVATE ac6_native_xenos)
  target_compile_options(ac6_native_ppc_abi_tests PRIVATE ${AC6_TEST_NO_NDEBUG})
  add_test(NAME ac6_native_ppc_abi_tests COMMAND ac6_native_ppc_abi_tests)

  add_executable(ac6_native_frontend_tests tests/native_frontend_tests.cpp)
  target_link_libraries(ac6_native_frontend_tests PRIVATE ac6_native_xenos)
  target_compile_options(ac6_native_frontend_tests PRIVATE ${AC6_TEST_NO_NDEBUG})
  add_test(NAME ac6_native_frontend_tests COMMAND ac6_native_frontend_tests)

  add_executable(ac6_native_runtime_tests tests/native_runtime_tests.cpp)
  target_link_libraries(ac6_native_runtime_tests PRIVATE ac6_native_xenos)
  target_compile_options(ac6_native_runtime_tests PRIVATE ${AC6_TEST_NO_NDEBUG})
  add_test(NAME ac6_native_runtime_tests COMMAND ac6_native_runtime_tests)

  add_executable(ac6_native_xex_tests tests/native_xex_tests.cpp)
  target_link_libraries(ac6_native_xex_tests PRIVATE ac6_native_xenos)
  target_compile_options(ac6_native_xex_tests PRIVATE ${AC6_TEST_NO_NDEBUG})
  add_test(NAME ac6_native_xex_tests COMMAND ac6_native_xex_tests)

  add_executable(ac6_native_xdvdfs_tests tests/native_xdvdfs_tests.cpp)
  target_link_libraries(ac6_native_xdvdfs_tests PRIVATE ac6_native_xenos)
  target_compile_options(ac6_native_xdvdfs_tests PRIVATE ${AC6_TEST_NO_NDEBUG})
  add_test(NAME ac6_native_xdvdfs_tests COMMAND ac6_native_xdvdfs_tests)

  add_executable(ac6_native_guest_memory_tests tests/native_guest_memory_tests.cpp)
  target_link_libraries(ac6_native_guest_memory_tests PRIVATE ac6_native_xenos)
  target_compile_options(ac6_native_guest_memory_tests PRIVATE ${AC6_TEST_NO_NDEBUG})
  add_test(NAME ac6_native_guest_memory_tests COMMAND ac6_native_guest_memory_tests)

  add_executable(ac6_native_guest_input_tests tests/native_guest_input_tests.cpp)
  target_link_libraries(ac6_native_guest_input_tests PRIVATE ac6_native_xenos)
  target_compile_options(ac6_native_guest_input_tests PRIVATE ${AC6_TEST_NO_NDEBUG})
  add_test(NAME ac6_native_guest_input_tests COMMAND ac6_native_guest_input_tests)

  # r424: covers the r421/r422 regression class directly (residual raised
  # IRQL released on simulated thread termination) instead of relying only
  # on the end-to-end standalone-launch verification r422 ran by hand.
  add_executable(ac6_native_guest_threads_tests tests/native_guest_threads_tests.cpp)
  target_link_libraries(ac6_native_guest_threads_tests PRIVATE ac6_native_xenos)
  target_compile_options(ac6_native_guest_threads_tests PRIVATE ${AC6_TEST_NO_NDEBUG})
  add_test(NAME ac6_native_guest_threads_tests COMMAND ac6_native_guest_threads_tests)
  # A regression here (the mutex left orphaned) hangs forever, not fails
  # fast -- bound it so a broken build reports a clear CTest TIMEOUT
  # instead of stalling every suite queued after it in the same run.
  set_tests_properties(ac6_native_guest_threads_tests PROPERTIES TIMEOUT 10)
endif()
