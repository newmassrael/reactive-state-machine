# RSM Static W3C Test Code Generation
# Generates C++ state machine code from W3C SCXML test suite

# rsm_generate_static_w3c_test: Generate C++ code for a single W3C test
#
# This does NOT create executable - just generates C++ header from TXML
# Automatically discovers and processes sub SCXML files (e.g., test226sub1.txml)
#
function(rsm_generate_static_w3c_test TEST_NUM OUTPUT_DIR)
    set(RESOURCE_DIR "${CMAKE_SOURCE_DIR}/resources/${TEST_NUM}")
    set(TXML_FILE "${RESOURCE_DIR}/test${TEST_NUM}.txml")
    set(SCXML_FILE "${OUTPUT_DIR}/test${TEST_NUM}.scxml")
    set(GENERATED_HEADER "${OUTPUT_DIR}/test${TEST_NUM}_sm.h")

    # Collect all code generator dependencies for proper incremental builds
    # When any template or Python script changes, all generated code must be rebuilt
    file(GLOB CODEGEN_TEMPLATES "${CMAKE_SOURCE_DIR}/tools/codegen/templates/*.jinja2")
    file(GLOB CODEGEN_ACTION_TEMPLATES "${CMAKE_SOURCE_DIR}/tools/codegen/templates/actions/*.jinja2")
    set(CODEGEN_DEPENDENCIES
        "${CMAKE_SOURCE_DIR}/tools/codegen/codegen.py"
        "${CMAKE_SOURCE_DIR}/tools/codegen/scxml_parser.py"
        ${CODEGEN_TEMPLATES}
        ${CODEGEN_ACTION_TEMPLATES}
    )

    # Check if main TXML file exists
    if(NOT EXISTS "${TXML_FILE}")
        message(WARNING "TXML file not found: ${TXML_FILE} - Skipping test ${TEST_NUM}")
        return()
    endif()

    # Auto-discover sub SCXML files (e.g., test226sub1.txml, test226sub2.txml)
    # W3C SCXML 6.2/6.4: Sub SCXML files are child state machines invoked by parent
    file(GLOB SUB_TXML_FILES "${RESOURCE_DIR}/test${TEST_NUM}sub*.txml")
    set(SUB_SCXML_DEPENDENCIES "")
    set(SUB_HEADER_DEPENDENCIES "")

    foreach(SUB_TXML_FILE ${SUB_TXML_FILES})
        get_filename_component(SUB_TXML_NAME "${SUB_TXML_FILE}" NAME_WE)
        set(SUB_SCXML_FILE "${OUTPUT_DIR}/${SUB_TXML_NAME}.scxml")
        set(SUB_HEADER_FILE "${OUTPUT_DIR}/${SUB_TXML_NAME}_sm.h")

        # Convert sub TXML to SCXML (without pass/fail validation via filename detection)
        add_custom_command(
            OUTPUT "${SUB_SCXML_FILE}"
            COMMAND txml-converter "${SUB_TXML_FILE}" "${SUB_SCXML_FILE}"
            DEPENDS txml-converter "${SUB_TXML_FILE}"
            COMMENT "Converting ${SUB_TXML_NAME}.txml to SCXML (sub state machine)"
            VERBATIM
        )

        # Generate C++ code for sub SCXML (as invoked child)
        add_custom_command(
            OUTPUT "${SUB_HEADER_FILE}"
            COMMAND python3 "${CMAKE_SOURCE_DIR}/tools/codegen/codegen.py" "${SUB_SCXML_FILE}" -o "${OUTPUT_DIR}" --as-child
            DEPENDS "${SUB_SCXML_FILE}" ${CODEGEN_DEPENDENCIES}
            COMMENT "Generating C++ code: ${SUB_TXML_NAME}_sm.h"
            VERBATIM
        )

        # Add sub SCXML to dependencies and headers
        list(APPEND SUB_SCXML_DEPENDENCIES "${SUB_SCXML_FILE}")
        list(APPEND SUB_HEADER_DEPENDENCIES "${SUB_HEADER_FILE}")
        set(GENERATED_W3C_HEADERS ${GENERATED_W3C_HEADERS} "${SUB_HEADER_FILE}" PARENT_SCOPE)
    endforeach()

    # Step 1: TXML -> SCXML conversion with name attribute
    # ARCHITECTURE.MD: CMake portability - Use ${CMAKE_COMMAND} instead of bash
    add_custom_command(
        OUTPUT "${SCXML_FILE}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${OUTPUT_DIR}"
        COMMAND txml-converter "${TXML_FILE}" "${SCXML_FILE}"
        COMMAND python3 "${CMAKE_SOURCE_DIR}/tools/fix_scxml_name.py" "${SCXML_FILE}" "test${TEST_NUM}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${RESOURCE_DIR}/test${TEST_NUM}.txt" "${OUTPUT_DIR}/" || ${CMAKE_COMMAND} -E true
        DEPENDS txml-converter "${TXML_FILE}" ${SUB_SCXML_DEPENDENCIES}
        COMMENT "Converting TXML to SCXML: test${TEST_NUM}.txml"
        VERBATIM
    )

    # Step 2: SCXML -> C++ code generation (parent + inline children)
    # W3C SCXML 6.2/6.4: Parent header must depend on child headers (template detection)
    # Uses Python helper script to generate parent and process inline content children
    # ARCHITECTURE.MD: CMake portability - Use CMake scripting instead of bash
    set(CHILDREN_METADATA "${OUTPUT_DIR}/test${TEST_NUM}_children.txt")
    set(PROCESS_CHILDREN_SCRIPT "${OUTPUT_DIR}/process_children_${TEST_NUM}.cmake")

    # Generate CMake script to process inline children
    file(WRITE "${PROCESS_CHILDREN_SCRIPT}" "
        if(EXISTS \"${CHILDREN_METADATA}\")
            file(STRINGS \"${CHILDREN_METADATA}\" CHILDREN)
            foreach(child \${CHILDREN})
                if(child)
                    execute_process(
                        COMMAND python3 \"${CMAKE_SOURCE_DIR}/tools/codegen/codegen.py\"
                                \"${OUTPUT_DIR}/\${child}.scxml\" -o \"${OUTPUT_DIR}\" --as-child
                        RESULT_VARIABLE result
                    )
                    if(NOT result EQUAL 0)
                        message(WARNING \"Failed to generate child: \${child}\")
                    endif()
                endif()
            endforeach()
        endif()
    ")

    add_custom_command(
        OUTPUT "${GENERATED_HEADER}"
        COMMAND python3 "${CMAKE_SOURCE_DIR}/tools/codegen/codegen.py" "${SCXML_FILE}" -o "${OUTPUT_DIR}"
        COMMAND ${CMAKE_COMMAND} -P "${PROCESS_CHILDREN_SCRIPT}"
        DEPENDS "${SCXML_FILE}" ${SUB_HEADER_DEPENDENCIES} ${CODEGEN_DEPENDENCIES}
        COMMENT "Generating C++ code: test${TEST_NUM}_sm.h (with inline children)"
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
