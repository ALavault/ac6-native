cmake_minimum_required(VERSION 3.20)
project(ac6_native LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(ac6_product_core STATIC src/product_runtime.cpp src/asset_runtime.cpp
  src/render_manifest.cpp src/frontend_runtime.cpp src/combat_scenario_frontend.cpp
  src/native_geometry_raster.cpp
  src/native_geometry_raster_target.cpp
  src/native_renderer.cpp
  src/native_hud.cpp
  src/mission01_compare.cpp src/campaign_progression.cpp src/session_save.cpp)
target_include_directories(ac6_product_core PUBLIC include)
target_compile_features(ac6_product_core PUBLIC cxx_std_20)

find_package(PkgConfig REQUIRED)
find_package(Vulkan REQUIRED)
pkg_check_modules(SDL3 REQUIRED IMPORTED_TARGET sdl3)
add_library(ac6_platform STATIC src/sdl_input.cpp)
target_include_directories(ac6_platform PUBLIC include)
target_link_libraries(ac6_platform PUBLIC ac6_product_core PkgConfig::SDL3 Vulkan::Vulkan)
target_compile_features(ac6_platform PUBLIC cxx_std_20)

add_executable(ac6-native src/main.cpp src/commands.cpp src/cli.cpp src/interactive.cpp)
target_link_libraries(ac6-native PRIVATE ac6_platform)

enable_testing()
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
  add_test(NAME ac6-cpp-complexity
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_SOURCE_DIR}/../../tools/audit_cpp_complexity.py
      --root ${CMAKE_SOURCE_DIR}/../..
      --baseline ${CMAKE_SOURCE_DIR}/../../analysis/source_complexity_baseline.json
      --check)
endif()
add_executable(ac6-product-runtime-tests tests/product_runtime_tests.cpp)
target_link_libraries(ac6-product-runtime-tests PRIVATE ac6_platform)
add_test(NAME ac6-product-runtime-tests COMMAND ac6-product-runtime-tests)

add_executable(ac6-mission-input-functional-tests tests/mission_input_functional_tests.cpp)
target_link_libraries(ac6-mission-input-functional-tests PRIVATE ac6_product_core)
add_test(NAME ac6-mission-input-functional-tests COMMAND ac6-mission-input-functional-tests)

add_executable(ac6-render-contract-functional-tests tests/render_contract_functional_tests.cpp)
target_link_libraries(ac6-render-contract-functional-tests PRIVATE ac6_product_core)
add_test(NAME ac6-render-contract-functional-tests COMMAND ac6-render-contract-functional-tests)

add_executable(ac6-combat-runtime-tests tests/combat_runtime_tests.cpp)
target_link_libraries(ac6-combat-runtime-tests PRIVATE ac6_product_core)
add_test(NAME ac6-combat-runtime-tests COMMAND ac6-combat-runtime-tests)

add_executable(ac6-frontend-runtime-tests tests/frontend_runtime_tests.cpp)
target_link_libraries(ac6-frontend-runtime-tests PRIVATE ac6_product_core)
add_test(NAME ac6-frontend-runtime-tests COMMAND ac6-frontend-runtime-tests)

add_executable(ac6-native-geometry-tests tests/native_geometry_tests.cpp)
target_link_libraries(ac6-native-geometry-tests PRIVATE ac6_product_core)
add_test(NAME ac6-native-geometry-tests COMMAND ac6-native-geometry-tests)

add_executable(ac6-native-raster-tests tests/native_raster_tests.cpp)
target_link_libraries(ac6-native-raster-tests PRIVATE ac6_product_core)
add_test(NAME ac6-native-raster-tests COMMAND ac6-native-raster-tests)

add_executable(ac6-manifest-loader-transaction-tests
  tests/manifest_loader_transaction_tests.cpp)
target_link_libraries(ac6-manifest-loader-transaction-tests PRIVATE ac6_product_core)
add_test(NAME ac6-manifest-loader-transaction-tests
  COMMAND ac6-manifest-loader-transaction-tests)

add_executable(ac6-native-hud-acceptance-tests tests/native_hud_acceptance_tests.cpp)
target_link_libraries(ac6-native-hud-acceptance-tests PRIVATE ac6_product_core)
add_test(NAME ac6-native-hud-acceptance-tests COMMAND ac6-native-hud-acceptance-tests)
add_test(NAME ac6-native-headless COMMAND ac6-native)

add_executable(ac6-mission01-compare-tests tests/mission01_compare_tests.cpp)
target_link_libraries(ac6-mission01-compare-tests PRIVATE ac6_product_core)
add_test(NAME ac6-mission01-compare-tests COMMAND ac6-mission01-compare-tests)

add_executable(ac6-campaign-progression-tests tests/campaign_progression_tests.cpp)
target_link_libraries(ac6-campaign-progression-tests PRIVATE ac6_product_core)
add_test(NAME ac6-campaign-progression-tests COMMAND ac6-campaign-progression-tests)

add_executable(ac6-session-save-tests tests/session_save_tests.cpp)
target_link_libraries(ac6-session-save-tests PRIVATE ac6_product_core)
add_test(NAME ac6-session-save-tests COMMAND ac6-session-save-tests)

add_executable(ac6-vulkan-surface-smoke tests/vulkan_surface_smoke.cpp)
target_link_libraries(ac6-vulkan-surface-smoke PRIVATE ac6_platform)

include(GNUInstallDirs)
install(TARGETS ac6-native RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES README.md DESTINATION .)
install(PROGRAMS
  ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/extract_ndxr_native_slices.py
  ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/extract_ntxr_native_slices.py
  ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/make_mission01_native_manifest.py
  ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/seal_mission01_reference.py
  ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/audit_native_package.py
  ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/audit_campaign_catalog.py
  ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/generate_campaign_manifest.py
  ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/audit_code_reachability.py
  ${CMAKE_CURRENT_SOURCE_DIR}/../../scripts/probe_ntxr_bc.py
  DESTINATION share/ac6-native/tools)

set(CPACK_GENERATOR "TGZ")
set(CPACK_PACKAGE_NAME "ac6-native")
set(CPACK_PACKAGE_VERSION "0.1.0")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Native Linux runtime boundary for Ace Combat 6")
include(CPack)
