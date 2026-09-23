cmake_minimum_required(VERSION 3.16)
project(CMorghIDE)

set(CMAKE_CXX_STANDARD 17)

add_executable(CMorghIDE src/TextEditor.cpp)

# SDL2 and SDL2_ttf have to be installed in the MinGW folder (include + lib)
target_link_libraries(${PROJECT_NAME} -lmingw32 -lSDL2main -lSDL2 -lSDL2_ttf)

# copy the icon next to the exe
file(COPY ${CMAKE_SOURCE_DIR}/assets DESTINATION ${CMAKE_BINARY_DIR})
