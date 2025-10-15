# RSM Static W3C Test Code Generation
# Generates C++ state machine code from W3C SCXML test suite

# rsm_generate_static_w3c_test: Generate C++ code for a single W3C test
#
# This does NOT create executable - just generates C++ header from TXML
# Optional third parameter: RESOURCE_DIR (defaults to TEST_NUM if not specified)
#
function(rsm_generate_static_w3c_test TEST_NUM OUTPUT_DIR)
    # Optional third parameter for resource directory (for sub-tests like test239sub1)
    set(RESOURCE_DIR "${TEST_NUM}")
    if(ARGC GREATER 2)
        set(RESOURCE_DIR "${ARGV2}")
    endif()
    
    set(TXML_FILE "${CMAKE_SOURCE_DIR}/resources/${RESOURCE_DIR}/test${TEST_NUM}.txml")
    set(SCXML_FILE "${OUTPUT_DIR}/test${TEST_NUM}.scxml")
    set(GENERATED_HEADER "${OUTPUT_DIR}/test${TEST_NUM}_sm.h")

    # Check if TXML file exists
    if(NOT EXISTS "${TXML_FILE}")
        message(WARNING "TXML file not found: ${TXML_FILE} - Skipping test ${TEST_NUM}")
        return()
    endif()

    # Step 1: TXML -> SCXML conversion with name attribute
    add_custom_command(
        OUTPUT "${SCXML_FILE}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${OUTPUT_DIR}"
        COMMAND txml-converter "${TXML_FILE}" "${SCXML_FILE}"
        COMMAND sed -i "s/<scxml /<scxml name=\\\"test${TEST_NUM}\\\" /" "${SCXML_FILE}"
        DEPENDS txml-converter "${TXML_FILE}"
        COMMENT "Converting TXML to SCXML: test${TEST_NUM}.txml"
        VERBATIM
    )

    # Step 2: SCXML -> C++ code generation
    add_custom_command(
        OUTPUT "${GENERATED_HEADER}"
        COMMAND scxml-codegen -o "${OUTPUT_DIR}" "${SCXML_FILE}"
        DEPENDS scxml-codegen "${SCXML_FILE}"
        COMMENT "Generating C++ code: test${TEST_NUM}_sm.h"
        VERBATIM
    )

    # Add to parent scope variable
    set(GENERATED_W3C_HEADERS ${GENERATED_W3C_HEADERS} "${GENERATED_HEADER}" PARENT_SCOPE)
endfunction()

# rsm_generate_static_w3c_test_batch: Generate C++ code for multiple W3C tests
#
function(rsm_generate_static_w3c_test_batch OUTPUT_DIR)
    foreach(TEST_NUM ${ARGN})
        rsm_generate_static_w3c_test(${TEST_NUM} ${OUTPUT_DIR})
    endforeach()
endfunction()
