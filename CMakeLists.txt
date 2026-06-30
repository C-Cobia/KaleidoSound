cmake_minimum_required(VERSION 3.16)
project(KaleidoSound LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_PREFIX_PATH "C:/Qt/6.9.3/msvc2022_64")
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets OpenGLWidgets Multimedia)

# LibTorch (PyTorch C++ API)
set(LIBTORCH_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../Third-Parties/libtorch")
set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH};${LIBTORCH_DIR}/share/cmake/Torch")
find_package(Torch REQUIRED)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${TORCH_CXX_FLAGS}")

option(KALEIDOSOUND_RENDERER_SHARED "Build renderer module as DLL" ON)

# Aubio library (pre-built)
set(AUBIO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../Third-Parties/aubio-install")

set(KALEIDOSOUND_RENDERER_SOURCES
    # core
    core/MainWindow.h
    core/MainWindow.cpp
    core/MainWindow.ui
    # asset
    asset/AssetManager.h
    asset/AssetManager.cpp
    asset/AssetBrowser.h
    asset/AssetBrowser.cpp
    asset/AudioAsset.h
    asset/STLAsset.h
    asset/SelectionModel.h
    asset/SelectionModel.cpp
    # audio
    audio/AudioAnalyzer.h
    audio/AudioAnalyzer.cpp
    audio/AudioLoadWorker.h
    audio/AudioLoadWorker.cpp
    audio/BeatAnalysisResult.h
    audio/IBeatDetector.h
    audio/AubioBeatDetector.h
    audio/AubioBeatDetector.cpp
    audio/BeatNetDetector.h
    audio/BeatNetDetector.cpp
    audio/BeatDetector.h
    audio/BeatDetector.cpp
    audio/AnalysisManager.h
    audio/AnalysisManager.cpp
    audio/TransportController.h
    audio/TransportController.cpp
    # timeline
    timeline/TimelineTrack.h
    timeline/TimelineWidget.h
    timeline/TimelineWidget.cpp
    timeline/TimelineViewState.h
    timeline/TimelineViewState.cpp
    timeline/WaveformTrack.h
    timeline/WaveformTrack.cpp
    timeline/BeatTrack.h
    timeline/BeatTrack.cpp
    timeline/WaveformWidget.h
    timeline/WaveformWidget.cpp
    # viewer
    viewer/RenderWidget.h
    viewer/RenderWidget.cpp
    viewer/Mesh.h
    viewer/Mesh.cpp
    viewer/StlLoader.h
    viewer/StlLoader.cpp
    # ui
    ui/InspectorWidget.h
    ui/InspectorWidget.cpp
    ui/PanelManager.h
    ui/PanelManager.cpp
)

if(KALEIDOSOUND_RENDERER_SHARED)
    add_library(KaleidoSoundRenderer SHARED ${KALEIDOSOUND_RENDERER_SOURCES})
    if(WIN32)
        set_target_properties(KaleidoSoundRenderer PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
    endif()
else()
    add_library(KaleidoSoundRenderer STATIC ${KALEIDOSOUND_RENDERER_SOURCES})
endif()

target_include_directories(KaleidoSoundRenderer
    PRIVATE
        "${AUBIO_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(KaleidoSoundRenderer
    PUBLIC
        Qt6::Widgets
        Qt6::OpenGLWidgets
        Qt6::Multimedia
        ${TORCH_LIBRARIES}
    PRIVATE
        "${AUBIO_DIR}/lib/aubio.lib"
)

add_executable(KaleidoSound core/main.cpp)
target_sources(KaleidoSound PRIVATE resources.qrc)
target_include_directories(KaleidoSound PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(KaleidoSound
    PRIVATE
        KaleidoSoundRenderer
)
