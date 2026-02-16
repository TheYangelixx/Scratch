cmake_minimum_required(VERSION 4.0)
project(Scratch)

set(CMAKE_CXX_STANDARD 20)

add_executable(Scratch main.cpp
        include/Block.h
        include/Sprite.h
        src/Block.cpp
        include/MoveBlock.h
        src/MoveBlock.cpp
        src/Workspace.h
        src/Workspace.cpp
)

target_link_libraries(${PROJECT_NAME} -lconio)
target_link_libraries(${PROJECT_NAME} -lmingw32 -lSDL2main -lSDL2 -lSDL2_image -lSDL2_ttf -lSDL2_mixer)
