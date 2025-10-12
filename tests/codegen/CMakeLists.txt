# 정적 코드 생성기 테스트

# 테스트 실행 파일
add_executable(test_static_codegen
    test_static_codegen.cpp
)

# 테스트 의존성
target_link_libraries(test_static_codegen
    PRIVATE
    GTest::GTest
    GTest::Main
    # 나중에 추가: static_codegen_lib
)

# 테스트 include 경로
target_include_directories(test_static_codegen
    PRIVATE
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/rsm/include
)

# CTest에 등록
add_test(NAME StaticCodegenTests COMMAND test_static_codegen)