# RSM Code Generation Tool
# Command-line tool for generating C++ code from SCXML files

add_executable(scxml-codegen
    main.cpp
    StaticCodeGenerator.cpp
)

target_include_directories(scxml-codegen
    PRIVATE
        ${CMAKE_SOURCE_DIR}/rsm/include
        ${CMAKE_SOURCE_DIR}/tools/codegen
        ${LIBXMLPP_INCLUDE_DIRS}
        ${Boost_INCLUDE_DIRS}
        /usr/include/libxml2
)

target_link_libraries(scxml-codegen
    PRIVATE
        rsm_unified
        ${LIBXMLPP_LIBRARIES}
        ${Boost_LIBRARIES}
        xml2
        pthread
)

add_dependencies(scxml-codegen libxmlxx_external rsm_unified)