cmake_minimum_required(VERSION 3.20)
project(ac6_native LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# NO FLOATING-POINT CONTRACTION, ANYWHERE IN THIS PROJECT.
#
# Every port here reproduces a retail instruction sequence bit for bit, and it
# does that by calling std::fmaf EXACTLY where retail fuses and using a plain
# `a * b + c` where retail does not. GCC and Clang default to
# -ffp-contract=fast, which fuses the second form as well -- silently changing
# the last ulp of any expression the port deliberately left unfused.
#
# It bit at cycle 1412: retail_scalar_sin_cos rounds each of four lane products
# before summing them, which was ARBITRATED against 412 executed values, and a
# contracted build would fuse each product into the running sum and reproduce a
# reading that lost 114 of them.
#
# Nothing in the product wants implicit contraction, so it is off globally
# rather than per-file. The differentials are what would catch a regression,
# but they need a Ghidra pass; this needs none.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-ffp-contract=off)
elseif(MSVC)
  add_compile_options(/fp:precise)
endif()

option(AC6_BUILD_PLATFORM "Build the SDL3/Vulkan frontend" ON)
find_package(ZLIB REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavformat libavcodec libavutil)

add_library(ac6_product_core STATIC src/product_runtime.cpp src/asset_runtime.cpp
  src/execution_trace.cpp
  src/render_scene.cpp
  src/render_manifest.cpp src/frontend_runtime.cpp src/combat_scenario_frontend.cpp
  src/native_geometry_raster.cpp
  src/native_geometry_raster_target.cpp
  src/native_renderer.cpp
  src/native_hud.cpp
  src/mission01_compare.cpp src/campaign_progression.cpp src/session_save.cpp
  src/retail_scenario.cpp src/retail_mission_state.cpp src/retail_state_machine.cpp
  src/retail_mission_script.cpp src/retail_session.cpp
  src/retail_session_replay.cpp
  src/retail_world_position.cpp
  src/ntxr_texture.cpp
  src/retail_model_directory.cpp
  src/retail_ndxr_container.cpp
  src/retail_bin_readers.cpp
  src/retail_input.cpp
  src/retail_input_record.cpp
  src/retail_transform.cpp
  src/retail_input_binding.cpp
  src/retail_slot_repeat.cpp
  src/retail_slot_gather.cpp
  src/retail_flight_step.cpp
  src/retail_flight_controls.cpp
  src/retail_flight_orientation.cpp
  src/retail_flight_step_driver.cpp
  src/retail_live_flight_ramps.cpp
  src/retail_live_flight_axes.cpp
  src/retail_live_flight_step.cpp
  src/retail_flight_rate_servo.cpp
  src/retail_control_blend.cpp
  src/retail_flight_export.cpp
  src/retail_flight_command.cpp
  src/retail_flight_session.cpp
  src/demo_flight_view.cpp
  src/demo_flight_input.cpp
  src/retail_flight_input_accumulators.cpp
  src/retail_flight_input_apply.cpp
  src/retail_flight_input_router.cpp
  src/retail_scalar_sin_cos.cpp
  src/retail_container_index.cpp
  src/retail_fhm_view.cpp
  src/retail_campaign_bundle.cpp
  src/retail_mission_world_bundle.cpp
  src/retail_mission_bundle.cpp
  src/retail_frontend_resources.cpp
  src/retail_camera_table.cpp
  src/retail_mode2_camera.cpp
  src/retail_mission01_scene_bundle.cpp
  src/retail_mission01_map_render_assets.cpp
  src/retail_mission01_cpu_compositor.cpp
  src/retail_ndxr_geometry.cpp
  src/retail_terrain_field.cpp
  src/retail_map_placement.cpp
  src/retail_map_water.cpp
  src/sha256.cpp
  src/retail_content.cpp
  src/retail_media.cpp
  src/retail_asf_index.cpp
  src/retail_resource_graph.cpp)
target_include_directories(ac6_product_core PUBLIC include)
target_compile_features(ac6_product_core PUBLIC cxx_std_20)
target_link_libraries(ac6_product_core PUBLIC ZLIB::ZLIB PkgConfig::FFMPEG)

if(AC6_BUILD_PLATFORM)
  find_package(Vulkan REQUIRED)
  pkg_check_modules(SDL3 REQUIRED IMPORTED_TARGET sdl3)

  add_library(ac6_vulkan_backend STATIC
    src/vulkan_device.cpp
    src/vulkan_resources.cpp
    src/vulkan_pipelines.cpp
    src/vulkan_commands.cpp
    src/vulkan_readback.cpp)
  target_include_directories(ac6_vulkan_backend PUBLIC include)
  target_link_libraries(ac6_vulkan_backend PUBLIC Vulkan::Vulkan)
  target_compile_features(ac6_vulkan_backend PUBLIC cxx_std_20)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(ac6_vulkan_backend PRIVATE
      -Wall -Wextra -Wpedantic -Wconversion)
  endif()

  add_library(ac6_platform STATIC
    src/sdl_input.cpp
    src/vulkan_surface.cpp
    src/vulkan_swapchain.cpp
    src/vulkan_scene_renderer.cpp
    src/vulkan_scene_resource_cache.cpp
    src/retail_mission01_vulkan_scene.cpp)
  target_include_directories(ac6_platform PUBLIC include)
  target_link_libraries(ac6_platform PUBLIC ac6_product_core ac6_vulkan_backend
    PkgConfig::SDL3 Vulkan::Vulkan)
  target_compile_features(ac6_platform PUBLIC cxx_std_20)

  add_executable(ac6-native src/main.cpp src/commands.cpp src/retail_commands.cpp
    src/cli.cpp src/interactive.cpp)
  target_link_libraries(ac6-native PRIVATE ac6_platform)
endif()

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
target_link_libraries(ac6-product-runtime-tests PRIVATE ac6_product_core)
add_test(NAME ac6-product-runtime-tests COMMAND ac6-product-runtime-tests)

add_executable(ac6-mission-input-functional-tests tests/mission_input_functional_tests.cpp)
target_link_libraries(ac6-mission-input-functional-tests PRIVATE ac6_product_core)
add_test(NAME ac6-mission-input-functional-tests COMMAND ac6-mission-input-functional-tests)

add_executable(ac6-render-contract-functional-tests tests/render_contract_functional_tests.cpp)
target_link_libraries(ac6-render-contract-functional-tests PRIVATE ac6_product_core)
add_test(NAME ac6-render-contract-functional-tests COMMAND ac6-render-contract-functional-tests)

add_executable(ac6-combat-runtime-tests tests/combat_runtime_tests.cpp)
target_link_libraries(ac6-combat-runtime-tests PRIVATE ac6_product_core)
add_test(NAME ac6-combat-runtime-tests COMMAND ac6-combat-runtime-tests
  ${CMAKE_CURRENT_SOURCE_DIR}/../../manifests/mission01-retail)

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

add_executable(ac6-mission01-compare-tests tests/mission01_compare_tests.cpp)
target_link_libraries(ac6-mission01-compare-tests PRIVATE ac6_product_core)
add_test(NAME ac6-mission01-compare-tests COMMAND ac6-mission01-compare-tests)

add_executable(ac6-campaign-progression-tests tests/campaign_progression_tests.cpp)
target_link_libraries(ac6-campaign-progression-tests PRIVATE ac6_product_core)
add_test(NAME ac6-campaign-progression-tests COMMAND ac6-campaign-progression-tests
  ${CMAKE_CURRENT_SOURCE_DIR}/../../manifests/mission01-retail)

add_executable(ac6-session-save-tests tests/session_save_tests.cpp)
target_link_libraries(ac6-session-save-tests PRIVATE ac6_product_core)
add_test(NAME ac6-session-save-tests COMMAND ac6-session-save-tests)

add_executable(ac6-retail-session-replay-tests tests/retail_session_replay_tests.cpp)
target_link_libraries(ac6-retail-session-replay-tests PRIVATE ac6_product_core)
add_test(NAME ac6-retail-session-replay-tests COMMAND ac6-retail-session-replay-tests)

add_executable(ac6-execution-trace-tests tests/execution_trace_tests.cpp)
target_link_libraries(ac6-execution-trace-tests PRIVATE ac6_product_core)
add_test(NAME ac6-execution-trace-tests COMMAND ac6-execution-trace-tests)

add_executable(ac6-render-scene-contract-tests tests/render_scene_contract_tests.cpp)
target_link_libraries(ac6-render-scene-contract-tests PRIVATE ac6_product_core)
add_test(NAME ac6-render-scene-contract-tests COMMAND ac6-render-scene-contract-tests)

# Consumes the committed Mission 01 retail manifests, not a fixture.
add_executable(ac6-retail-scenario-tests tests/retail_scenario_tests.cpp)
target_link_libraries(ac6-retail-scenario-tests PRIVATE ac6_product_core)
add_test(NAME ac6-retail-scenario-tests
  COMMAND ac6-retail-scenario-tests
    ${CMAKE_CURRENT_SOURCE_DIR}/../../manifests/mission01-retail
    ${CMAKE_CURRENT_SOURCE_DIR}/../../reports/mission01-retail/retail-scenario-test.json)

# Reads the retail scenario container natively. The payload is never committed,
# so the test skips its retail half when the file is absent.
add_executable(ac6-retail-scenario-parser-tests tests/retail_scenario_parser_tests.cpp)
target_link_libraries(ac6-retail-scenario-parser-tests PRIVATE ac6_product_core)
add_test(NAME ac6-retail-scenario-parser-tests
  COMMAND ac6-retail-scenario-parser-tests
    ${CMAKE_CURRENT_SOURCE_DIR}/../../manifests/mission01-retail
    ${CMAKE_CURRENT_SOURCE_DIR}/../../reports/logs/cycle-739-pac-mission-gate/fhm/idx_0009/000_00_00_00_10.bin)
set_tests_properties(ac6-retail-scenario-parser-tests PROPERTIES SKIP_RETURN_CODE 77)

# The consumer behaviours: classification, insertion, sub-mission sequencing.
add_executable(ac6-retail-mission-state-tests tests/retail_mission_state_tests.cpp)
target_link_libraries(ac6-retail-mission-state-tests PRIVATE ac6_product_core)
add_test(NAME ac6-retail-mission-state-tests
  COMMAND ac6-retail-mission-state-tests
    ${CMAKE_CURRENT_SOURCE_DIR}/../../reports/logs/cycle-739-pac-mission-gate/fhm/idx_0009/000_00_00_00_10.bin
    ${CMAKE_CURRENT_SOURCE_DIR}/../../reports/mission01-retail/retail-mission-state.json)
set_tests_properties(ac6-retail-mission-state-tests PROPERTIES SKIP_RETURN_CODE 77)

# Replays the committed p-code snapshots through the native *Bin readers.
add_executable(ac6-retail-bin-readers-tests tests/retail_bin_readers_tests.cpp)
target_link_libraries(ac6-retail-bin-readers-tests PRIVATE ac6_product_core)
add_test(NAME ac6-retail-bin-readers-tests
  COMMAND ac6-retail-bin-readers-tests
    ${CMAKE_CURRENT_SOURCE_DIR}/../../analysis/microexec/reader-digests.tsv
    ${CMAKE_CURRENT_SOURCE_DIR}/../../reports/logs/cycle-739-pac-mission-gate/fhm/idx_0009/000_00_00_00_10.bin)
set_tests_properties(ac6-retail-bin-readers-tests PROPERTIES SKIP_RETURN_CODE 77)

# JF: the mission state machine, ported from 0x8219AD20 and the cycle-1112 tree.
add_executable(ac6-retail-state-machine-tests tests/retail_state_machine_tests.cpp)
target_link_libraries(ac6-retail-state-machine-tests PRIVATE ac6_product_core)
add_test(NAME ac6-retail-state-machine
  COMMAND ac6-retail-state-machine-tests
    ${CMAKE_CURRENT_SOURCE_DIR}/../../reports/mission01-retail/retail-state-machine.json)

# J2: Mission 01 driven by the retail payload alone, no manifest.
add_executable(ac6-retail-playable-tests tests/retail_playable_tests.cpp)
target_link_libraries(ac6-retail-playable-tests PRIVATE ac6_product_core)
add_test(NAME ac6-retail-playable
  COMMAND ac6-retail-playable-tests
    ${CMAKE_CURRENT_SOURCE_DIR}/../../reports/logs/cycle-739-pac-mission-gate/fhm/idx_0009/000_00_00_00_10.bin
    ${CMAKE_CURRENT_SOURCE_DIR}/../../manifests/mission01-retail
    ${CMAKE_CURRENT_SOURCE_DIR}/../../reports/mission01-retail/retail-playable.json)
set_tests_properties(ac6-retail-playable PROPERTIES SKIP_RETURN_CODE 77)

# JF: the session loop over the retail-built world, ended by the retail script.
add_executable(ac6-retail-session-tests tests/retail_session_tests.cpp)
target_link_libraries(ac6-retail-session-tests PRIVATE ac6_product_core)
add_test(NAME ac6-retail-session
  COMMAND ac6-retail-session-tests
    ${CMAKE_CURRENT_SOURCE_DIR}/../../reports/logs/cycle-739-pac-mission-gate/fhm/idx_0009/000_00_00_00_10.bin
    ${CMAKE_CURRENT_SOURCE_DIR}/../../reports/mission01-retail/retail-session.json
    ${CMAKE_CURRENT_SOURCE_DIR}/../../reports/mission01-native-captures/jf-retail-session)
set_tests_properties(ac6-retail-session PROPERTIES SKIP_RETURN_CODE 77)

# The model directory of 0x8228E988/0x8228E9A8/0x8228E9B8, over Mission 01's own
# MDLP, plus the join from the scenario's model bytes to it.
add_executable(ac6-retail-model-directory-tests tests/retail_model_directory_tests.cpp)
target_link_libraries(ac6-retail-model-directory-tests PRIVATE ac6_product_core)
add_test(NAME ac6-retail-model-directory
  COMMAND ac6-retail-model-directory-tests
    ${CMAKE_CURRENT_SOURCE_DIR}/../../reports/logs/cycle-739-pac-mission-gate/fhm/idx_0009/001_MDLP.mdlp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../reports/logs/cycle-739-pac-mission-gate/fhm/idx_0009/000_00_00_00_10.bin)
set_tests_properties(ac6-retail-model-directory PROPERTIES SKIP_RETURN_CODE 77)

# Visual inspection of the decoder's output. Not a test - it writes retail art,
# which stays local and is never committed. See tools/ntxr_extract.cpp.
add_executable(ac6-ntxr-extract tools/ntxr_extract.cpp)
target_link_libraries(ac6-ntxr-extract PRIVATE ac6_product_core)

# The NTXR decoder, restricted to single-level block textures. The corpus is
# the control: it must accept exactly the single-level block population and
# refuse the rest with a named cause.
add_executable(ac6-retail-ndxr-container-tests tests/retail_ndxr_container_tests.cpp)
target_link_libraries(ac6-retail-ndxr-container-tests PRIVATE ac6_product_core)
# The corpus is extracted retail content and is never committed; the test exits
# 77 when it is absent, so a clean clone skips rather than fails.
add_test(NAME ac6-retail-ndxr-container
  COMMAND ac6-retail-ndxr-container-tests
    ${CMAKE_CURRENT_SOURCE_DIR}/../../reports/logs
    ${CMAKE_CURRENT_SOURCE_DIR}/../../reports/mission01-retail/ndxr-container.json)
set_tests_properties(ac6-retail-ndxr-container PROPERTIES SKIP_RETURN_CODE 77)

# B1: the descriptor decoder, over every descriptor of the real package.
add_executable(ac6-retail-ndxr-geometry-tests tests/retail_ndxr_geometry_tests.cpp)
target_link_libraries(ac6-retail-ndxr-geometry-tests PRIVATE ac6_product_core)
add_test(NAME ac6-retail-ndxr-geometry
  COMMAND ac6-retail-ndxr-geometry-tests
    ${CMAKE_CURRENT_SOURCE_DIR}/../../reports/logs/cycle-739-pac-mission-gate/fhm/idx_0009/001_MDLP.mdlp)
set_tests_properties(ac6-retail-ndxr-geometry PROPERTIES SKIP_RETURN_CODE 77)

# B1: the terrain heightfield of 0x82102568, over the real map container.
add_executable(ac6-retail-terrain-field-tests tests/retail_terrain_field_tests.cpp)
target_link_libraries(ac6-retail-terrain-field-tests PRIVATE ac6_product_core)
add_test(NAME ac6-retail-terrain-field
  COMMAND ac6-retail-terrain-field-tests
    ${CMAKE_CURRENT_SOURCE_DIR}/../../reports/logs/cycle-739-pac-mission-gate/fhm/idx_0119/021_FHM)
set_tests_properties(ac6-retail-terrain-field PROPERTIES SKIP_RETURN_CODE 77)

# B1: the .pdl placement list of 0x82102148, cross-checked against the heightfield.
add_executable(ac6-retail-map-placement-tests tests/retail_map_placement_tests.cpp)
target_link_libraries(ac6-retail-map-placement-tests PRIVATE ac6_product_core)
add_test(NAME ac6-retail-map-placement
  COMMAND ac6-retail-map-placement-tests
    ${CMAKE_CURRENT_SOURCE_DIR}/../../reports/logs/cycle-739-pac-mission-gate/fhm/idx_0119/021_FHM)
set_tests_properties(ac6-retail-map-placement PROPERTIES SKIP_RETURN_CODE 77)

# B1: the .mca/.mci/.mcd per-position bit of 0x82101EE8.
add_executable(ac6-retail-map-water-tests tests/retail_map_water_tests.cpp)
target_link_libraries(ac6-retail-map-water-tests PRIVATE ac6_product_core)
add_test(NAME ac6-retail-map-water
  COMMAND ac6-retail-map-water-tests
    ${CMAKE_CURRENT_SOURCE_DIR}/../../reports/logs/cycle-739-pac-mission-gate/fhm/idx_0119/021_FHM)
set_tests_properties(ac6-retail-map-water PROPERTIES SKIP_RETURN_CODE 77)

# B3: the flight view over the map's real ground.
add_executable(ac6-demo-terrain-view-tests tests/demo_terrain_view_tests.cpp)
target_link_libraries(ac6-demo-terrain-view-tests PRIVATE ac6_product_core)
add_test(NAME ac6-demo-terrain-view
  COMMAND ac6-demo-terrain-view-tests
    ${CMAKE_CURRENT_SOURCE_DIR}/../../reports/logs/cycle-739-pac-mission-gate/fhm/idx_0119/021_FHM)
set_tests_properties(ac6-demo-terrain-view PROPERTIES SKIP_RETURN_CODE 77)

# A3/B3: the contracted integrator flown over the contracted heightfield.
add_executable(ac6-flight-over-terrain-tests tests/flight_over_terrain_tests.cpp)
target_link_libraries(ac6-flight-over-terrain-tests PRIVATE ac6_product_core)
add_test(NAME ac6-flight-over-terrain
  COMMAND ac6-flight-over-terrain-tests
    ${CMAKE_CURRENT_SOURCE_DIR}/../../reports/logs/cycle-739-pac-mission-gate/fhm/idx_0119/021_FHM)
set_tests_properties(ac6-flight-over-terrain PROPERTIES SKIP_RETURN_CODE 77)

# A3: which stick axis drives which rotation limit, by a control matrix.
add_executable(ac6-flight-turn-mapping-tests tests/flight_turn_mapping_tests.cpp)
target_link_libraries(ac6-flight-turn-mapping-tests PRIVATE ac6_product_core)
add_test(NAME ac6-flight-turn-mapping COMMAND ac6-flight-turn-mapping-tests)

# B3: the map's own post-process, from the mapset XML.
add_executable(ac6-mapset-post-tests tests/mapset_post_tests.cpp)
target_link_libraries(ac6-mapset-post-tests PRIVATE ac6_product_core)
add_test(NAME ac6-mapset-post COMMAND ac6-mapset-post-tests)

# B3: fog as a colour, not a multiplier.
add_executable(ac6-fogged-triangle-tests tests/fogged_triangle_tests.cpp)
target_link_libraries(ac6-fogged-triangle-tests PRIVATE ac6_product_core)
add_test(NAME ac6-fogged-triangle COMMAND ac6-fogged-triangle-tests)

add_executable(ac6-ntxr-texture-tests tests/ntxr_texture_tests.cpp)
target_link_libraries(ac6-ntxr-texture-tests PRIVATE ac6_product_core)
add_test(NAME ac6-ntxr-texture
  COMMAND ac6-ntxr-texture-tests
    ${CMAKE_CURRENT_SOURCE_DIR}/../../reports/logs/cycle-739-pac-mission-gate/fhm
    ${CMAKE_CURRENT_SOURCE_DIR}/../../reports/mission01-retail/ntxr-decode.json)
set_tests_properties(ac6-ntxr-texture PROPERTIES SKIP_RETURN_CODE 77)

add_executable(ac6-ntxr-gidx-tests tests/ntxr_gidx_tests.cpp)
target_link_libraries(ac6-ntxr-gidx-tests PRIVATE ac6_product_core)
add_test(NAME ac6-ntxr-gidx COMMAND ac6-ntxr-gidx-tests)

# The world-position resolver of 0x822953F0, read in the Xenon corpus.
add_executable(ac6-retail-input-tests tests/retail_input_tests.cpp)
target_link_libraries(ac6-retail-input-tests PRIVATE ac6_product_core)
add_test(NAME ac6-retail-input COMMAND ac6-retail-input-tests
  ${CMAKE_CURRENT_SOURCE_DIR}/../../reports/mission01-retail/retail-input.json)

# A7: the per-controller input record 0x821CAA50 builds, differentially checked
# against 321 micro-executions of it.
add_executable(ac6-retail-input-record-tests tests/retail_input_record_tests.cpp)
target_link_libraries(ac6-retail-input-record-tests PRIVATE ac6_product_core)
add_test(NAME ac6-retail-input-record COMMAND ac6-retail-input-record-tests
  ${CMAKE_CURRENT_SOURCE_DIR}/../../analysis/input-path/input-record-vectors.tsv
  ${CMAKE_CURRENT_SOURCE_DIR}/../../reports/mission01-retail/retail-input-record.json)

# A3.1: the one transform kernel, against thirteen micro-executions of
# 0x822A1E80 and its three rotations.
# A3.2: the input binding layer of 0x82211C10.
add_executable(ac6-retail-input-binding-tests tests/retail_input_binding_tests.cpp)
target_link_libraries(ac6-retail-input-binding-tests PRIVATE ac6_product_core)
add_test(NAME ac6-retail-input-binding COMMAND ac6-retail-input-binding-tests)

# A3.2: how the entity's input fields reach the accumulators -- the last link.
add_executable(ac6-retail-flight-input-apply-tests
  tests/retail_flight_input_apply_tests.cpp)
target_link_libraries(ac6-retail-flight-input-apply-tests PRIVATE ac6_product_core)
add_test(NAME ac6-retail-flight-input-apply COMMAND ac6-retail-flight-input-apply-tests)

# B1: retail's container header parser and getter -- the last hop from a model
# byte to an NDXR span.
add_executable(ac6-retail-container-index-tests tests/retail_container_index_tests.cpp)
target_link_libraries(ac6-retail-container-index-tests PRIVATE ac6_product_core)
add_test(NAME ac6-retail-container-index COMMAND ac6-retail-container-index-tests)

add_executable(ac6-retail-fhm-view-tests tests/retail_fhm_view_tests.cpp)
target_link_libraries(ac6-retail-fhm-view-tests PRIVATE ac6_product_core)
add_test(NAME ac6-retail-fhm-view COMMAND ac6-retail-fhm-view-tests)

add_executable(ac6-retail-content-tests tests/retail_content_tests.cpp)
target_link_libraries(ac6-retail-content-tests PRIVATE ac6_product_core ZLIB::ZLIB)
add_test(NAME ac6-retail-content COMMAND ac6-retail-content-tests)

add_executable(ac6-retail-frontend-resources-tests
  tests/retail_frontend_resources_tests.cpp)
target_link_libraries(ac6-retail-frontend-resources-tests PRIVATE ac6_product_core)
add_test(NAME ac6-retail-frontend-resources
  COMMAND ac6-retail-frontend-resources-tests)
set_tests_properties(ac6-retail-frontend-resources PROPERTIES SKIP_RETURN_CODE 77)

add_executable(ac6-retail-camera-table-tests tests/retail_camera_table_tests.cpp)
target_link_libraries(ac6-retail-camera-table-tests PRIVATE ac6_product_core)
add_test(NAME ac6-retail-camera-table COMMAND ac6-retail-camera-table-tests)

add_executable(ac6-retail-mode2-camera-tests
  tests/retail_mode2_camera_tests.cpp)
target_link_libraries(ac6-retail-mode2-camera-tests PRIVATE ac6_product_core)
add_test(NAME ac6-retail-mode2-camera COMMAND ac6-retail-mode2-camera-tests)

add_executable(ac6-retail-mission01-scene-bundle-tests
  tests/retail_mission01_scene_bundle_tests.cpp)
target_link_libraries(ac6-retail-mission01-scene-bundle-tests PRIVATE ac6_product_core)
add_test(NAME ac6-retail-mission01-scene-bundle
  COMMAND ac6-retail-mission01-scene-bundle-tests)

add_executable(ac6-retail-mission-world-bundle-tests
  tests/retail_mission_world_bundle_tests.cpp)
target_link_libraries(ac6-retail-mission-world-bundle-tests PRIVATE ac6_product_core)
add_test(NAME ac6-retail-mission-world-bundle
  COMMAND ac6-retail-mission-world-bundle-tests)

# A3.1/A3.2: XMScalarSinCos, the last unfaithful scalar seam in the chain.
add_executable(ac6-retail-scalar-sin-cos-tests tests/retail_scalar_sin_cos_tests.cpp)
target_link_libraries(ac6-retail-scalar-sin-cos-tests PRIVATE ac6_product_core)
add_test(NAME ac6-retail-scalar-sin-cos COMMAND ac6-retail-scalar-sin-cos-tests)

# A3.2: where the six input fields come from -- the binding layer's outputs.
add_executable(ac6-retail-flight-input-router-tests
  tests/retail_flight_input_router_tests.cpp)
target_link_libraries(ac6-retail-flight-input-router-tests PRIVATE ac6_product_core)
add_test(NAME ac6-retail-flight-input-router COMMAND ac6-retail-flight-input-router-tests)

# A3.2: the flight model's INPUT accumulators -- the player's interface.
add_executable(ac6-retail-flight-input-accumulators-tests
  tests/retail_flight_input_accumulators_tests.cpp)
target_link_libraries(ac6-retail-flight-input-accumulators-tests PRIVATE ac6_product_core)
add_test(NAME ac6-retail-flight-input-accumulators
  COMMAND ac6-retail-flight-input-accumulators-tests)

# The demo input bridge. Contracted path, one invented link, and it says which.
add_executable(ac6-demo-flight-input-tests tests/demo_flight_input_tests.cpp)
target_link_libraries(ac6-demo-flight-input-tests PRIVATE ac6_product_core)
add_test(NAME ac6-demo-flight-input COMMAND ac6-demo-flight-input-tests)

# The demo view. Ports nothing, invents a camera, and says so.
add_executable(ac6-demo-flight-view-tests tests/demo_flight_view_tests.cpp)
target_link_libraries(ac6-demo-flight-view-tests PRIVATE ac6_product_core)
add_test(NAME ac6-demo-flight-view COMMAND ac6-demo-flight-view-tests)

# A3.2: the contracted chain wired together. Ports nothing; composes everything.
add_executable(ac6-retail-flight-session-tests tests/retail_flight_session_tests.cpp)
target_link_libraries(ac6-retail-flight-session-tests PRIVATE ac6_product_core)
add_test(NAME ac6-retail-flight-session COMMAND ac6-retail-flight-session-tests)

# A3.2: the command input API, slots 12, 13 and 14 -- how the game flies.
add_executable(ac6-retail-flight-command-tests tests/retail_flight_command_tests.cpp)
target_link_libraries(ac6-retail-flight-command-tests PRIVATE ac6_product_core)
add_test(NAME ac6-retail-flight-command COMMAND ac6-retail-flight-command-tests)

# A3.2: the export functions, slots 20 and 21.
add_executable(ac6-retail-flight-export-tests tests/retail_flight_export_tests.cpp)
target_link_libraries(ac6-retail-flight-export-tests PRIVATE ac6_product_core)
add_test(NAME ac6-retail-flight-export COMMAND ac6-retail-flight-export-tests)

# A3.2: the blended control accessors, slots 17 and 18.
add_executable(ac6-retail-control-blend-tests tests/retail_control_blend_tests.cpp)
target_link_libraries(ac6-retail-control-blend-tests PRIVATE ac6_product_core)
add_test(NAME ac6-retail-control-blend COMMAND ac6-retail-control-blend-tests)

# A3.2: the rate servo, the first consumer of the control state.
add_executable(ac6-retail-flight-rate-servo-tests tests/retail_flight_rate_servo_tests.cpp)
target_link_libraries(ac6-retail-flight-rate-servo-tests PRIVATE ac6_product_core)
add_test(NAME ac6-retail-flight-rate-servo COMMAND ac6-retail-flight-rate-servo-tests)

# A3.2: the LIVE model's step, slot 15 of 0x8200F310.
add_executable(ac6-retail-live-flight-step-tests tests/retail_live_flight_step_tests.cpp)
target_link_libraries(ac6-retail-live-flight-step-tests PRIVATE ac6_product_core)
add_test(NAME ac6-retail-live-flight-step COMMAND ac6-retail-live-flight-step-tests)

# A3.2: the LIVE model's axis update, the rest of slot 30.
add_executable(ac6-retail-live-flight-axes-tests tests/retail_live_flight_axes_tests.cpp)
target_link_libraries(ac6-retail-live-flight-axes-tests PRIVATE ac6_product_core)
add_test(NAME ac6-retail-live-flight-axes COMMAND ac6-retail-live-flight-axes-tests)

# A3.2: the LIVE model's ramp update, slot 30 of 0x8200F310.
add_executable(ac6-retail-live-flight-ramps-tests tests/retail_live_flight_ramps_tests.cpp)
target_link_libraries(ac6-retail-live-flight-ramps-tests PRIVATE ac6_product_core)
add_test(NAME ac6-retail-live-flight-ramps COMMAND ac6-retail-live-flight-ramps-tests)

# A3.2: the step itself, slot 11 -- the flight model's per-frame entry point.
add_executable(ac6-retail-flight-step-driver-tests tests/retail_flight_step_driver_tests.cpp)
target_link_libraries(ac6-retail-flight-step-driver-tests PRIVATE ac6_product_core)
add_test(NAME ac6-retail-flight-step-driver COMMAND ac6-retail-flight-step-driver-tests)

# A3.2: the orientation angles, slot 32 of 0x8200F270.
add_executable(ac6-retail-flight-orientation-tests tests/retail_flight_orientation_tests.cpp)
target_link_libraries(ac6-retail-flight-orientation-tests PRIVATE ac6_product_core)
add_test(NAME ac6-retail-flight-orientation COMMAND ac6-retail-flight-orientation-tests)

# A3.2: the flight control surfaces, slot 30 of 0x8200F270.
add_executable(ac6-retail-flight-controls-tests tests/retail_flight_controls_tests.cpp)
target_link_libraries(ac6-retail-flight-controls-tests PRIVATE ac6_product_core)
add_test(NAME ac6-retail-flight-controls COMMAND ac6-retail-flight-controls-tests)

# A3.2: the flight position integrator, slot 31 of 0x8200F270.
add_executable(ac6-retail-flight-step-tests tests/retail_flight_step_tests.cpp)
target_link_libraries(ac6-retail-flight-step-tests PRIVATE ac6_product_core)
add_test(NAME ac6-retail-flight-step COMMAND ac6-retail-flight-step-tests)

add_executable(ac6-retail-slot-gather-tests tests/retail_slot_gather_tests.cpp)
target_link_libraries(ac6-retail-slot-gather-tests PRIVATE ac6_product_core)
add_test(NAME ac6-retail-slot-gather COMMAND ac6-retail-slot-gather-tests)

add_executable(ac6-retail-slot-repeat-tests tests/retail_slot_repeat_tests.cpp)
target_link_libraries(ac6-retail-slot-repeat-tests PRIVATE ac6_product_core)
add_test(NAME ac6-retail-slot-repeat COMMAND ac6-retail-slot-repeat-tests)

add_executable(ac6-retail-transform-tests tests/retail_transform_tests.cpp)
target_link_libraries(ac6-retail-transform-tests PRIVATE ac6_product_core)
add_test(NAME ac6-retail-transform COMMAND ac6-retail-transform-tests
  ${CMAKE_CURRENT_SOURCE_DIR}/../../analysis/vmx128/transform-kernel-vectors.tsv
  ${CMAKE_CURRENT_SOURCE_DIR}/../../reports/mission01-retail/retail-transform.json)

add_executable(ac6-retail-world-position-tests tests/retail_world_position_tests.cpp)
target_link_libraries(ac6-retail-world-position-tests PRIVATE ac6_product_core)
add_test(NAME ac6-retail-world-position
  COMMAND ac6-retail-world-position-tests
    ${CMAKE_CURRENT_SOURCE_DIR}/../../reports/logs/cycle-739-pac-mission-gate/fhm/idx_0009/000_00_00_00_10.bin
    ${CMAKE_CURRENT_SOURCE_DIR}/../../reports/mission01-retail/retail-world-position.json)
set_tests_properties(ac6-retail-world-position PROPERTIES SKIP_RETURN_CODE 77)

# J2: every vtable of the binary named by RTTI in a versioned map.
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
  add_test(NAME ac6-retail-classmap
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/audit_ac6_class_map.py
      ${CMAKE_CURRENT_SOURCE_DIR}/../../analysis/class-map.tsv
      --rejects ${CMAKE_CURRENT_SOURCE_DIR}/../../analysis/class-map-rejects.tsv
      --require J2)
  add_test(NAME ac6-oracle-manifest-contract
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/audit_ac6_oracle_manifest.py
      ${CMAKE_CURRENT_SOURCE_DIR}/../../analysis/oracle/ac6-recomp-dcd41b/manifest.json
      --artifact-root ${CMAKE_CURRENT_SOURCE_DIR}/../..)
  add_test(NAME ac6-global-ladder-contract
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/audit_ac6_global_ladder.py)
  add_test(NAME ac6-product-boundary-source
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/audit_ac6_product_boundary.py
      --source-root ${CMAKE_CURRENT_SOURCE_DIR})
  add_test(NAME ac6-vulkan-backend-recovery-manifest
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/audit_ac6_vulkan_backend_recovery.py
      ${CMAKE_CURRENT_SOURCE_DIR}/../../analysis/vulkan/backend-recovery-v1.json
      --artifact-root ${CMAKE_CURRENT_SOURCE_DIR}/../..)
endif()

# Points the same reader at a payload that is not committed - another mission,
# say - and reports what happens. Not wired to ctest: it needs a payload.
add_executable(ac6-retail-scenario-probe tools/retail_scenario_probe.cpp)
target_link_libraries(ac6-retail-scenario-probe PRIVATE ac6_product_core)

if(AC6_BUILD_PLATFORM)
  add_test(NAME ac6-native-headless COMMAND ac6-native)

  add_executable(ac6-vulkan-backend-tests tests/vulkan_backend_tests.cpp)
  target_link_libraries(ac6-vulkan-backend-tests PRIVATE ac6_vulkan_backend)
  add_test(NAME ac6-vulkan-backend COMMAND ac6-vulkan-backend-tests)
  set_tests_properties(ac6-vulkan-backend PROPERTIES SKIP_RETURN_CODE 77)

  add_executable(ac6-vulkan-scene-renderer-tests tests/vulkan_scene_renderer_tests.cpp)
  target_link_libraries(ac6-vulkan-scene-renderer-tests PRIVATE ac6_platform)
  add_test(NAME ac6-vulkan-scene-renderer COMMAND ac6-vulkan-scene-renderer-tests)
  set_tests_properties(ac6-vulkan-scene-renderer PROPERTIES SKIP_RETURN_CODE 77)

  add_executable(ac6-vulkan-scene-resource-cache-tests
    tests/vulkan_scene_resource_cache_tests.cpp)
  target_link_libraries(ac6-vulkan-scene-resource-cache-tests PRIVATE ac6_platform)
  add_test(NAME ac6-vulkan-scene-resource-cache
    COMMAND ac6-vulkan-scene-resource-cache-tests)
  set_tests_properties(ac6-vulkan-scene-resource-cache PROPERTIES SKIP_RETURN_CODE 77)

  add_executable(ac6-retail-mission01-vulkan-scene-tests
    tests/retail_mission01_vulkan_scene_tests.cpp)
  target_link_libraries(ac6-retail-mission01-vulkan-scene-tests PRIVATE ac6_platform)
  add_test(NAME ac6-retail-mission01-vulkan-scene
    COMMAND ac6-retail-mission01-vulkan-scene-tests)
  set_tests_properties(ac6-retail-mission01-vulkan-scene PROPERTIES SKIP_RETURN_CODE 77)

  if(Python3_Interpreter_FOUND)
    add_test(NAME ac6-product-boundary-binary
      COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/audit_ac6_product_boundary.py
        --binary $<TARGET_FILE:ac6-native>)
  endif()

  add_executable(ac6-vulkan-surface-smoke tests/vulkan_surface_smoke.cpp)
  target_link_libraries(ac6-vulkan-surface-smoke PRIVATE ac6_platform)
  add_test(NAME ac6-vulkan-surface-smoke COMMAND ac6-vulkan-surface-smoke)
  set_tests_properties(ac6-vulkan-surface-smoke PROPERTIES SKIP_RETURN_CODE 77)
endif()

include(GNUInstallDirs)
if(AC6_BUILD_PLATFORM)
  install(TARGETS ac6-native RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES README.md DESTINATION .)
install(PROGRAMS
  ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/extract_ndxr_native_slices.py
  ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/extract_ntxr_native_slices.py
  ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/make_mission01_native_manifest.py
  ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/seal_mission01_reference.py
  ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/audit_native_package.py
  ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/audit_ac6_product_boundary.py
  ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/audit_campaign_catalog.py
  ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/audit_ac6_retail_content_cache.py
  ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/generate_campaign_manifest.py
  ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/audit_code_reachability.py
  ${CMAKE_CURRENT_SOURCE_DIR}/../../scripts/probe_ntxr_bc.py
  DESTINATION share/ac6-native/tools)

set(CPACK_GENERATOR "TGZ")
set(CPACK_PACKAGE_NAME "ac6-native")
set(CPACK_PACKAGE_VERSION "0.1.0")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Native Linux runtime boundary for Ace Combat 6")
include(CPack)
