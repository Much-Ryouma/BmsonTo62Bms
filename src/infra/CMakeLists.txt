# Why not b2b::compiler_options を適用する: 外部ライブラリの実装に自分たちの厳しい警告設定をかけると、直せない警告で埋もれるため
add_library(b2b_audio_codecs STATIC thirdparty/AudioCodecs.c)
target_include_directories(b2b_audio_codecs SYSTEM PUBLIC "${B2B_AUDIO_DEPS_DIR}")
if(MSVC)
    target_compile_options(b2b_audio_codecs PRIVATE /w)
else()
    target_compile_options(b2b_audio_codecs PRIVATE -w)
    find_package(Threads REQUIRED)
    target_link_libraries(b2b_audio_codecs PUBLIC Threads::Threads ${CMAKE_DL_LIBS} m)
endif()

add_library(b2b_infra STATIC
    src/audio/AudioFiles.cpp
    src/audio/AudioProbe.cpp
    src/audio/MiniaudioPlayer.cpp
    src/bmson/BmsonJson.cpp
    src/fs/Utf8Path.cpp
    src/text/TextEncoding.cpp)
add_library(b2b::infra ALIAS b2b_infra)

target_include_directories(b2b_infra PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(b2b_infra
    PUBLIC b2b::core
    PRIVATE b2b_audio_codecs b2b_vorbis nlohmann_json::nlohmann_json b2b::compiler_options)
