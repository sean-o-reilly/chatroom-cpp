FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)

FetchContent_MakeAvailable(googletest)

set(SOURCE_FILES server_tests.cpp)
add_executable(ServerTests ${SOURCE_FILES})

target_link_libraries(ServerTests GTest::gtest_main)
