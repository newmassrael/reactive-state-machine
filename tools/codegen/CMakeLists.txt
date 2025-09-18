# RSM Code Generation Tool
# Command-line tool for generating C++ code from SCXML files

add_executable(scxml-codegen
    main.cpp
)

target_include_directories(scxml-codegen
    PRIVATE
        ${LIBXMLPP_INCLUDE_DIRS}
        ${Boost_INCLUDE_DIRS}
)

target_link_libraries(scxml-codegen
    PRIVATE
        rsm_unified                  # For domain models and parsing (now integrated)
        ${LIBXMLPP_LIBRARIES}
        ${Boost_LIBRARIES}
)