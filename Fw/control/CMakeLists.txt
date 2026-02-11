cmake_minimum_required(VERSION 3.20)

# Платформо-независимая библиотека (host/SIL).
# Важно: этот код не должен тянуть HAL/CMSIS/FreeRTOS.

add_library(mfdc_control_core STATIC
  ${CMAKE_CURRENT_LIST_DIR}/control_core.c
)

target_include_directories(mfdc_control_core PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}
)

target_compile_options(mfdc_control_core PRIVATE
  $<$<COMPILE_LANG_AND_ID:C,GNU,Clang>:-std=gnu11>
)

if (UNIX)
  # math.h (isfinite и др.) может требовать линковки libm.
  target_link_libraries(mfdc_control_core PUBLIC m)
endif()

