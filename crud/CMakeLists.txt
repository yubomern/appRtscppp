cmake_minimum_required(VERSION 3.10)
project(OxydianDB)

set(CMAKE_CXX_STANDARD 17)

find_package(SQLite3 REQUIRED)

add_executable(oxydian
    main.cpp
    database.cpp
)

target_include_directories(oxydian PRIVATE ${SQLite3_INCLUDE_DIRS})
target_link_libraries(oxydian PRIVATE ${SQLite3_LIBRARIES})