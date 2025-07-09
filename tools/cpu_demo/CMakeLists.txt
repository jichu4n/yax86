# =============================================================================
# Executable dependencies
# =============================================================================
find_program(NASM_EXECUTABLE nasm)
if(NOT NASM_EXECUTABLE)
  message(WARNING "nasm not found, tests will fail to run!")
endif()

find_program(NDISASM_EXECUTABLE ndisasm)
if(NOT NDISASM_EXECUTABLE)
  message(WARNING "ndisasm not found, tests will fail to run!")
endif()

# =============================================================================
# Demo runner
# =============================================================================
add_executable(cpu_demo cpu_demo.cpp)
