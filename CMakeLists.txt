cmake_minimum_required(VERSION 3.14)
project(waitfreequeue)

set(CMAKE_CXX_STANDARD 17)

if (NOT WAITFREEQUEUE_DISABLE_TESTS)
    # Google test
    include(FetchContent)
    FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG release-1.12.1
    )
    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(BUILD_GTEST ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    enable_testing()

    add_executable(
            test_${PROJECT_NAME}
            WaitFreeMPSCQueue.h
            test/TestWaitFreeMPSCQueue.cpp
            test/TestHelpers.h
            WaitFreeSPSCQueue.h
            test/TestWaitFreeSPSCQueue.cpp
    )

    target_include_directories(
            test_${PROJECT_NAME}
            PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR})

    target_link_libraries(
            test_${PROJECT_NAME}
            GTest::gmock_main)

    include(CTest)
    include(GoogleTest)
    gtest_discover_tests(test_${PROJECT_NAME})
endif ()

install(FILES WaitFreeMPSCQueue.h DESTINATION include/waitfreequeue)
