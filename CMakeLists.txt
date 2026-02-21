cmake_minimum_required(VERSION 3.20)
project(MyScratch LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- SDL paths ---
set(SDL2_INCLUDE_DIR     "MinGW/include/SDL2")
set(SDL2_LIB_DIR         "MinGW/lib")

set(SDL2_TTF_INCLUDE_DIR "SDL_ttf/MinGW/include/SDL2")
set(SDL2_TTF_LIB_DIR     "SDL_ttf/MinGW/lib")

set(SDL2_IMAGE_INCLUDE_DIR "SDL_image/MinGW/include/SDL2")
set(SDL2_IMAGE_LIB_DIR     "SDL_image/MinGW/lib")

set(SDL2_MIXER_INCLUDE_DIR "SDL_mixer/MinGW/include/SDL2")
set(SDL2_MIXER_LIB_DIR     "SDL_mixer/MinGW/lib")

# --- Sources ---
set(SOURCES
        src/main.cpp
        src/ui.cpp
        src/block.cpp
        src/block_manager.cpp
        src/sprite.cpp
        src/app_state.cpp
        src/logger.cpp
        src/engine.cpp
        src/project_io.cpp
)

# --- Create executable ---
add_executable(${PROJECT_NAME} ${SOURCES})

# --- Include dirs ---
target_include_directories(${PROJECT_NAME} PRIVATE
        ${SDL2_INCLUDE_DIR}
        ${SDL2_TTF_INCLUDE_DIR}
        ${SDL2_IMAGE_INCLUDE_DIR}
        ${SDL2_MIXER_INCLUDE_DIR}
)

# --- Link dirs ---
target_link_directories(${PROJECT_NAME} PRIVATE
        ${SDL2_LIB_DIR}
        ${SDL2_TTF_LIB_DIR}
        ${SDL2_IMAGE_LIB_DIR}
        ${SDL2_MIXER_LIB_DIR}
)

# --- Link libraries ---
target_link_libraries(${PROJECT_NAME} PRIVATE
        mingw32
        SDL2main
        SDL2
        SDL2_ttf
        SDL2_image
        SDL2_mixer
)

# --- Copy DLLs ---
file(GLOB SDL2_DLLS      "MinGW/bin/*.dll")
file(GLOB SDL2_TTF_DLLS  "SDL_ttf/MinGW/bin/*.dll")
file(GLOB SDL2_IMG_DLLS  "SDL_image/MinGW/bin/*.dll")
file(GLOB SDL2_MIX_DLLS  "SDL_mixer/MinGW/bin/*.dll")

foreach(DLL_FILE
        ${SDL2_DLLS}
        ${SDL2_TTF_DLLS}
        ${SDL2_IMG_DLLS}
        ${SDL2_MIX_DLLS})

    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${DLL_FILE}" "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    )
endforeach()

# --- Copy assets ---
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
)