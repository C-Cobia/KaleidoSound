cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_PREFIX_PATH "C:/Qt/6.9.3/msvc2022_64")
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets OpenGLWidgets)

option(KALEIDOSOUND_RENDERER_SHARED "Build renderer module as DLL" OFF)

set(KALEIDOSOUND_RENDERER_SOURCES
    MainWindow.h
    MainWindow.cpp
    RenderWidget.h
    RenderWidget.cpp
    Mesh.h
    Mesh.cpp
    StlLoader.h
    StlLoader.cpp
)

if(KALEIDOSOUND_RENDERER_SHARED)
    add_library(KaleidoSoundRenderer SHARED ${KALEIDOSOUND_RENDERER_SOURCES})
    if(WIN32)
        set_target_properties(KaleidoSoundRenderer PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
    endif()
else()
    add_library(KaleidoSoundRenderer STATIC ${KALEIDOSOUND_RENDERER_SOURCES})
endif()

target_link_libraries(KaleidoSoundRenderer
    PUBLIC
        Qt6::Widgets
        Qt6::OpenGLWidgets
)

add_executable(KaleidoSound main.cpp)

target_link_libraries(KaleidoSound
    PRIVATE
        KaleidoSoundRenderer
)
