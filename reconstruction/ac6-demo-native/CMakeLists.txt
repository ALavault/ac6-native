cmake_minimum_required(VERSION 3.20)

project(ac6_demo_native VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(AC6DEMO_NATIVE_SOURCES
    src/content_store.cpp
    src/platform.cpp
    src/replay.cpp
    src/posix_fd.cpp
    src/sha256.cpp
    src/vfs.cpp
)
add_library(ac6-demo-native-core STATIC ${AC6DEMO_NATIVE_SOURCES})
target_include_directories(ac6-demo-native-core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_compile_features(ac6-demo-native-core PUBLIC cxx_std_20)
target_compile_options(ac6-demo-native-core PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic;-Wconversion>
)

add_executable(ac6-demo-native src/main.cpp)
target_link_libraries(ac6-demo-native PRIVATE ac6-demo-native-core)
target_compile_features(ac6-demo-native PRIVATE cxx_std_20)

include(CTest)
if(BUILD_TESTING)
    # Fixture/profile hooks are compiled into a test-only copy of the core.
    # The production library and CLI are deliberately built without this macro.
    add_library(ac6-demo-native-test-core STATIC ${AC6DEMO_NATIVE_SOURCES})
    target_include_directories(ac6-demo-native-test-core
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_compile_features(ac6-demo-native-test-core PUBLIC cxx_std_20)
    target_compile_definitions(ac6-demo-native-test-core
        PRIVATE AC6DEMO_NATIVE_ENABLE_TESTING=1
    )
    target_compile_options(ac6-demo-native-test-core PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic;-Wconversion>
    )

    add_executable(ac6-demo-native-content-tests tests/content_store_tests.cpp)
    target_link_libraries(ac6-demo-native-content-tests PRIVATE ac6-demo-native-test-core)
    target_compile_features(ac6-demo-native-content-tests PRIVATE cxx_std_20)
    target_compile_definitions(ac6-demo-native-content-tests
        PRIVATE AC6DEMO_NATIVE_ENABLE_TESTING=1
    )

    add_executable(ac6-demo-native-vfs-tests tests/vfs_boundary_tests.cpp)
    target_link_libraries(ac6-demo-native-vfs-tests PRIVATE ac6-demo-native-test-core)
    target_compile_features(ac6-demo-native-vfs-tests PRIVATE cxx_std_20)
    target_compile_definitions(ac6-demo-native-vfs-tests
        PRIVATE AC6DEMO_NATIVE_ENABLE_TESTING=1
    )

    add_executable(ac6-demo-native-platform-tests tests/platform_tests.cpp)
    target_link_libraries(ac6-demo-native-platform-tests PRIVATE ac6-demo-native-test-core)
    target_compile_features(ac6-demo-native-platform-tests PRIVATE cxx_std_20)

    add_executable(ac6-demo-native-replay-tests tests/replay_tests.cpp)
    target_link_libraries(ac6-demo-native-replay-tests PRIVATE ac6-demo-native-test-core)
    target_compile_features(ac6-demo-native-replay-tests PRIVATE cxx_std_20)

    add_test(NAME ac6-demo-native-content COMMAND ac6-demo-native-content-tests)
    add_test(NAME ac6-demo-native-vfs COMMAND ac6-demo-native-vfs-tests)
    add_test(NAME ac6-demo-native-platform COMMAND ac6-demo-native-platform-tests)
    add_test(NAME ac6-demo-native-replay COMMAND ac6-demo-native-replay-tests)

    find_program(AC6DEMO_NATIVE_NM nm)
    if(AC6DEMO_NATIVE_NM)
        add_test(
            NAME ac6-demo-native-production-surface
            COMMAND ${CMAKE_COMMAND}
                -DAC6DEMO_NM=${AC6DEMO_NATIVE_NM}
                -DAC6DEMO_CORE=$<TARGET_FILE:ac6-demo-native-core>
                -DAC6DEMO_CLI=$<TARGET_FILE:ac6-demo-native>
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/production_surface.cmake
        )
    endif()
endif()

install(TARGETS ac6-demo-native RUNTIME DESTINATION bin)
install(FILES config/demo-native-identity-v1.json
        DESTINATION share/ac6-demo-native/config)
