cmake_minimum_required(VERSION 3.20)

# Платформо-независимая библиотека логирования (host/SIL).
# Важно: этот код не должен тянуть HAL/CMSIS/FreeRTOS.

add_library(mfdc_logging_core STATIC
  ${CMAKE_CURRENT_LIST_DIR}/logging_capture_sram.c
  ${CMAKE_CURRENT_LIST_DIR}/logging_core.c
  ${CMAKE_CURRENT_LIST_DIR}/logging_crc32.c
  ${CMAKE_CURRENT_LIST_DIR}/logging_packer.c
  ${CMAKE_CURRENT_LIST_DIR}/logging_record.c
  ${CMAKE_CURRENT_LIST_DIR}/logging_spsc.c
  ${CMAKE_CURRENT_LIST_DIR}/logging_tx_scheduler.c
)

target_include_directories(mfdc_logging_core PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}
)

target_compile_options(mfdc_logging_core PRIVATE
  $<$<COMPILE_LANG_AND_ID:C,GNU,Clang>:-std=gnu11>
)