cmake_minimum_required(VERSION 3.20)

project(ac6_demo_native VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(ac6-demo-native-core STATIC
    src/content_store.cpp
    src/sha256.cpp
    src/vfs.cpp
)
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
    add_executable(ac6-demo-native-content-tests tests/content_store_tests.cpp)
    target_link_libraries(ac6-demo-native-content-tests PRIVATE ac6-demo-native-core)
    target_compile_features(ac6-demo-native-content-tests PRIVATE cxx_std_20)

    add_executable(ac6-demo-native-vfs-tests tests/vfs_boundary_tests.cpp)
    target_link_libraries(ac6-demo-native-vfs-tests PRIVATE ac6-demo-native-core)
    target_compile_features(ac6-demo-native-vfs-tests PRIVATE cxx_std_20)

    add_test(NAME ac6-demo-native-content COMMAND ac6-demo-native-content-tests)
    add_test(NAME ac6-demo-native-vfs COMMAND ac6-demo-native-vfs-tests)
endif()

install(TARGETS ac6-demo-native RUNTIME DESTINATION bin)
install(FILES config/demo-native-identity-v1.json
        DESTINATION share/ac6-demo-native/config)
