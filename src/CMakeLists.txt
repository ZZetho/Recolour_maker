cmake_minimum_required(VERSION 3.11)

set(CMAKE_BUILD_TYPE Debug)
project(RecolourMaker LANGUAGES C)

include(FetchContent)

FetchContent_Declare(
	raylib
	GIT_REPOSITORY https://github.com/raysan5/raylib.git
	GIT_TAG master
)

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(raylib)

add_executable(RecolourMaker RecolourMaker.c)

target_link_libraries(RecolourMaker PRIVATE raylib)
target_compile_options(RecolourMaker PRIVATE -Wall -Wextra -Werror -g)
