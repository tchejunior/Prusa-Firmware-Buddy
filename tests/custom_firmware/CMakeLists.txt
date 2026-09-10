cmake_minimum_required(VERSION 3.20)
project(custom_firmware_tests LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
get_filename_component(BUDDY_ROOT "../.." ABSOLUTE)
add_subdirectory("${BUDDY_ROOT}/lib/Catch2" catch2 EXCLUDE_FROM_ALL)
enable_testing()
add_executable(
  custom_firmware_tests
  "${BUDDY_ROOT}/src/feature/xbuddy_extension/cooling.cpp"
  ../unit/common/filament_sensor/mmu_runout_tests.cpp
  ../unit/feature/xbuddy_extension/chamber_filtration_stub.cpp
  ../unit/feature/xbuddy_extension/cooling_tests.cpp
  )
target_include_directories(
  custom_firmware_tests PRIVATE stubs "${BUDDY_ROOT}/src" "${BUDDY_ROOT}/src/common"
  )
target_compile_options(
  custom_firmware_tests PRIVATE -include "${CMAKE_CURRENT_SOURCE_DIR}/stubs/test_options.hpp"
  )
target_link_libraries(custom_firmware_tests PRIVATE Catch2::Catch2WithMain)
include("${BUDDY_ROOT}/lib/Catch2/extras/Catch.cmake")
catch_discover_tests(custom_firmware_tests)
