cmake_minimum_required(VERSION 3.11)

project(RecolourMaker)

add_executable(RecolourMaker RecolourMaker.c)

target_link_libraries(RecolourMaker PRIVATE raylib GL m pthread dl X11)
target_compile_options(RecolourMaker PRIVATE -Wall -Wextra -Werror -g)
