# =============================================================================
# Header bundles.
# =============================================================================
set(ALL_HEADER_BUNDLES)
function(generate_header_bundle module)
    set(bundle "${CMAKE_CURRENT_SOURCE_DIR}/${module}.h")
    set(generate_header_bundle "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate-header-bundle.js")
    file(GLOB module_sources "${CMAKE_CURRENT_SOURCE_DIR}/src/${module}/*")
    file(GLOB util_sources "${CMAKE_CURRENT_SOURCE_DIR}/src/util/*")
    add_custom_command(
        OUTPUT ${bundle}
        COMMAND ${generate_header_bundle} ARGS ${module}
        DEPENDS ${generate_header_bundle} ${module_sources} ${util_sources}
        COMMENT "Generating header bundle for module ${module} => ${bundle}"
    )
    set(${module}_BUNDLE "${bundle}" PARENT_SCOPE)
    list(APPEND ALL_HEADER_BUNDLES ${bundle})
    set(ALL_HEADER_BUNDLES ${ALL_HEADER_BUNDLES} PARENT_SCOPE)
endfunction()

generate_header_bundle("bios")
generate_header_bundle("cpu")
generate_header_bundle("dma")
generate_header_bundle("fdc")
generate_header_bundle("hdc")
generate_header_bundle("keyboard")
generate_header_bundle("pic")
generate_header_bundle("pit")
generate_header_bundle("platform")
generate_header_bundle("ppi")
generate_header_bundle("video")

# =============================================================================
# All-in-one header bundle
# =============================================================================
set(YAX86_CORE_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/yax86_core.h")
# Use space separation for shell command
string(REPLACE ";" " " HEADER_BUNDLES_SPACE_SEPARATED "${ALL_HEADER_BUNDLES}")
add_custom_command(
    OUTPUT ${YAX86_CORE_HEADER}
    COMMAND sh -c "cat ${HEADER_BUNDLES_SPACE_SEPARATED} > ${YAX86_CORE_HEADER}"
    DEPENDS ${ALL_HEADER_BUNDLES}
    COMMENT "Generating monolithic header ${YAX86_CORE_HEADER}"
    VERBATIM
)

# =============================================================================
# ROM data file generation
# =============================================================================
# ROMs are compiled into the library rather than read from a file at run time,
# since targets like the Raspberry Pi Pico have no file system to read from.
set(ALL_ROM_DATA_SOURCES)
set(generate_rom_data_files "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate-rom-data-files.js")
function(generate_rom_data module rom_file_name output_name symbol)
    set(rom_file "${CMAKE_CURRENT_SOURCE_DIR}/src/${module}/${rom_file_name}")
    set(prefix "${CMAKE_CURRENT_SOURCE_DIR}/src/${module}/${output_name}")
    add_custom_command(
        OUTPUT "${prefix}.c" "${prefix}.h"
        COMMAND ${generate_rom_data_files} ARGS ${rom_file} ${prefix} ${symbol}
        DEPENDS ${generate_rom_data_files} ${rom_file}
        COMMENT "Generating ROM data files ${rom_file} => ${prefix}.c and ${prefix}.h"
    )
    list(APPEND ALL_ROM_DATA_SOURCES "${prefix}.c")
    set(ALL_ROM_DATA_SOURCES ${ALL_ROM_DATA_SOURCES} PARENT_SCOPE)
endfunction()

generate_rom_data("bios" "GLABIOS_0.2.5_8E.rom" "bios_rom_data" "BIOSROMData")
generate_rom_data("hdc" "XTIDE_Universal_BIOS_XT_r631.rom" "hdc_option_rom_data" "HDCOptionROMData")

# =============================================================================
# Main library
# =============================================================================
add_library(yax86_core STATIC yax86_core.c ${YAX86_CORE_HEADER} ${ALL_ROM_DATA_SOURCES})


if(NOT EMSCRIPTEN)

  # =============================================================================
  # Tests
  # =============================================================================
  if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
      enable_testing()
      include_directories(${CMAKE_CURRENT_SOURCE_DIR})
      add_subdirectory(tests)
  endif()

  # =============================================================================
  # Demo programs
  # =============================================================================
  if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
      include_directories(${CMAKE_CURRENT_SOURCE_DIR})
      add_subdirectory(tools/cpu_demo)
  endif()

endif()
