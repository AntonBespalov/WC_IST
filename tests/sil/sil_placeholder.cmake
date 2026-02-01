if (NOT DEFINED OUTPUT_FILE)
  message(FATAL_ERROR "OUTPUT_FILE не задан")
endif()

file(WRITE "${OUTPUT_FILE}" "TODO: SIL summary\n")
