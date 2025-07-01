# =============================================================================
# Executable dependencies
# =============================================================================
find_program(FASM_EXECUTABLE fasm)
if(NOT FASM_EXECUTABLE)
  message(WARNING "fasm not found, tests will fail to run!")
endif()

find_program(OBJDUMP_EXECUTABLE objdump)
if(NOT OBJDUMP_EXECUTABLE)
  message(WARNING "objdump not found, tests will fail to run!")
endif()

# =============================================================================
# Demo runner
# =============================================================================
add_executable(cpu_demo cpu_demo.cpp)
