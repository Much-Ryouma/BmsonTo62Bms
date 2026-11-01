configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/include/b2b/core/BuildInfo.hpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/b2b/core/BuildInfo.hpp
    @ONLY)

add_library(b2b_core STATIC
    src/audio/Fade.cpp
    src/audio/Mix.cpp
    src/audio/Pcm.cpp
    src/bmson/Bmson.cpp
    src/bms/BmsWriter.cpp
    src/bms/HeaderValues.cpp
    src/bms/KeyMode.cpp
    src/bms/ObjectId.cpp
    src/naming/FileNames.cpp
    src/naming/NamePattern.cpp
    src/player/LaunchCommand.cpp
    src/slicing/SliceMerge.cpp
    src/slicing/SlicePlan.cpp
    src/timing/MeasureLayout.cpp
    src/timing/TimingMap.cpp)
add_library(b2b::core ALIAS b2b_core)

target_include_directories(b2b_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_BINARY_DIR}/generated)

# Why not JSON や音声のライブラリをリンクする: 変換ロジックをファイル形式や画面から独立させ、CLI・GUI・テストから同じように使うため
target_link_libraries(b2b_core PRIVATE b2b::compiler_options)
