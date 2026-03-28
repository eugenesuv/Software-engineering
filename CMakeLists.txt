cmake_minimum_required(VERSION 3.20)

project(car_rental_api VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ENABLE_COVERAGE "Enable coverage flags for GCC/Clang builds" OFF)

include(CTest)

find_package(SQLite3 REQUIRED)
find_package(Poco CONFIG REQUIRED COMPONENTS Foundation JSON Net Util)

if(TARGET SQLite3::SQLite3)
    set(SQLITE_TARGET SQLite3::SQLite3)
elseif(TARGET SQLite::SQLite3)
    set(SQLITE_TARGET SQLite::SQLite3)
else()
    message(FATAL_ERROR "Could not find a usable SQLite CMake target.")
endif()

if(BUILD_TESTING)
    find_package(doctest CONFIG QUIET)
    if(NOT doctest_FOUND)
        include(FetchContent)
        FetchContent_Declare(
            doctest
            URL https://github.com/doctest/doctest/archive/refs/tags/v2.4.12.tar.gz
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        )
        FetchContent_MakeAvailable(doctest)
    endif()
endif()

add_library(car_rental_lib
    src/errors.cpp
    src/database.cpp
    src/security.cpp
    src/services.cpp
    src/server.cpp
    src/utils.cpp
)

target_include_directories(car_rental_lib PUBLIC include)
target_link_libraries(
    car_rental_lib
    PUBLIC
        Poco::Foundation
        Poco::JSON
        Poco::Net
        Poco::Util
        ${SQLITE_TARGET}
)

target_compile_features(car_rental_lib PUBLIC cxx_std_17)

if(ENABLE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(car_rental_lib PUBLIC -O0 -g --coverage)
    target_link_options(car_rental_lib PUBLIC --coverage)
endif()

add_executable(car_rental_api src/main.cpp)
target_link_libraries(car_rental_api PRIVATE car_rental_lib)

if(BUILD_TESTING)
    add_executable(
        car_rental_tests
        tests/test_main.cpp
        tests/test_support.cpp
        tests/auth_tests.cpp
        tests/users_tests.cpp
        tests/fleet_tests.cpp
        tests/rentals_tests.cpp
        tests/smoke_tests.cpp
    )

    target_link_libraries(car_rental_tests PRIVATE car_rental_lib doctest::doctest)

    if(ENABLE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(car_rental_tests PRIVATE -O0 -g --coverage)
        target_link_options(car_rental_tests PRIVATE --coverage)
    endif()

    add_test(NAME car_rental_tests COMMAND car_rental_tests)
endif()
