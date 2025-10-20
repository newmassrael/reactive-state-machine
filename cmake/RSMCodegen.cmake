# RSMCodegen.cmake
# Provides rsm_add_state_machine() function for automatic SCXML code generation

function(rsm_add_state_machine)
    # Parse arguments: TARGET target_name SCXML_FILE file.scxml [OUTPUT_DIR dir]
    cmake_parse_arguments(RSM "" "TARGET;SCXML_FILE;OUTPUT_DIR" "" ${ARGN})

    # Validate required arguments
    if(NOT RSM_TARGET)
        message(FATAL_ERROR "rsm_add_state_machine: TARGET is required")
    endif()

    if(NOT RSM_SCXML_FILE)
        message(FATAL_ERROR "rsm_add_state_machine: SCXML_FILE is required")
    endif()

    # Set default output directory if not specified
    if(NOT RSM_OUTPUT_DIR)
        set(RSM_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
    endif()

    # Get absolute path to SCXML file
    get_filename_component(SCXML_ABS_PATH "${RSM_SCXML_FILE}" ABSOLUTE)

    # Extract base name from SCXML file
    get_filename_component(SCXML_NAME "${RSM_SCXML_FILE}" NAME_WE)

    # Generated file path
    set(GENERATED_HEADER "${RSM_OUTPUT_DIR}/${SCXML_NAME}_sm.h")

    # Create output directory
    file(MAKE_DIRECTORY "${RSM_OUTPUT_DIR}")

    # Add custom command to generate code
    add_custom_command(
        OUTPUT "${GENERATED_HEADER}"
        COMMAND python3 "${CMAKE_SOURCE_DIR}/tools/codegen/codegen.py" "${SCXML_ABS_PATH}" -o "${RSM_OUTPUT_DIR}"
        DEPENDS "${SCXML_ABS_PATH}"
        COMMENT "Generating state machine code from ${SCXML_NAME}.scxml"
        VERBATIM
    )

    # Add generated file to target sources
    target_sources(${RSM_TARGET} PRIVATE "${GENERATED_HEADER}")

    # Add include directory for generated files
    target_include_directories(${RSM_TARGET} PRIVATE "${RSM_OUTPUT_DIR}")

    # Mark generated file as GENERATED
    set_source_files_properties("${GENERATED_HEADER}" PROPERTIES GENERATED TRUE)

    message(STATUS "RSM: Configured state machine generation for ${RSM_TARGET}")
    message(STATUS "  SCXML: ${SCXML_ABS_PATH}")
    message(STATUS "  Generated: ${GENERATED_HEADER}")
endfunction()
