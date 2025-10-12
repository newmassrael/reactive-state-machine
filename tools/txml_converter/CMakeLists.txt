# TXML to SCXML Converter Tool
# Command-line tool for converting W3C SCXML Test Suite TXML files to standard SCXML

add_executable(txml-converter
    main.cpp
    ${CMAKE_SOURCE_DIR}/tests/w3c/impl/TXMLConverter.cpp
)

target_include_directories(txml-converter
    PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/rsm/include
)

target_link_libraries(txml-converter
    PRIVATE
        stdc++fs
)
