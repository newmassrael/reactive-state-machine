# SCXML 코드 생성기 라이브러리를 위한 CMakeLists.txt

# libxml++ 확인
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBXMLPP REQUIRED libxml++-5.0)

# 생성기 라이브러리 정의
add_library(scxml_generator
    # 모델 구현 클래스
    ${CMAKE_CURRENT_SOURCE_DIR}/src/impl/StateNode.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/impl/TransitionNode.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/impl/GuardNode.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/impl/ActionNode.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/impl/DataModelItem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/impl/InvokeNode.cpp

    # 모델 클래스
    ${CMAKE_CURRENT_SOURCE_DIR}/src/model/SCXMLContext.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/model/SCXMLModel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/model/StateHierarchy.cpp

    # 파싱 클래스
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parsing/SCXMLParser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parsing/StateNodeParser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parsing/TransitionParser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parsing/ActionParser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parsing/GuardParser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parsing/DataModelParser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parsing/XIncludeProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parsing/ParsingCommon.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parsing/InvokeParser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parsing/DoneDataParser.cpp

    # 팩토리 클래스
    ${CMAKE_CURRENT_SOURCE_DIR}/src/factory/NodeFactory.cpp

    # 유틸리티
    ${CMAKE_CURRENT_SOURCE_DIR}/src/GuardUtils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Logger.cpp
)

# 헤더 파일 위치 설정
target_include_directories(scxml_generator
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/include/model
        ${CMAKE_CURRENT_SOURCE_DIR}/include/impl
        ${CMAKE_CURRENT_SOURCE_DIR}/include/parsing
        ${CMAKE_CURRENT_SOURCE_DIR}/include/factory
        ${LIBXMLPP_INCLUDE_DIRS}
)

# 의존성 링크
target_link_libraries(scxml_generator
    PRIVATE
        ${LIBXMLPP_LIBRARIES}
)

# 컴파일 옵션 설정
target_compile_features(scxml_generator PUBLIC cxx_std_17)
target_compile_options(scxml_generator PRIVATE ${LIBXMLPP_CFLAGS})

# 설치 설정
install(TARGETS scxml_generator
    LIBRARY DESTINATION lib
)

# 헤더 파일 설치
install(
    DIRECTORY include/
    DESTINATION include/scxml_generator
    FILES_MATCHING PATTERN "*.h"
)
