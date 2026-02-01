cmake_minimum_required(VERSION 3.20)

# Host/SIL контур (CMake+CTest). Прошивка (STM32CubeIDE/CubeMX) живёт отдельно.
project(WC_IST_HOST LANGUAGES C CXX)

option(WC_IST_BUILD_TESTS "Собирать host unit (L1) и SIL (L2)" ON)

if (WC_IST_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
