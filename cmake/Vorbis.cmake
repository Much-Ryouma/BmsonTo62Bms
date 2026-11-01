# Why not libvorbis 付属の CMake を使う: libvorbis は find_package(Ogg) でインストール済みの libogg を探すため、
# 同じビルドの中で取得した libogg を渡せない。エンコードに必要なソースだけを自分たちで静的ライブラリにする
set(B2B_XIPH_DIR "${CMAKE_BINARY_DIR}/_deps/xiph")

function(b2b_fetch_archive url sha256 name)
    set(archive "${B2B_XIPH_DIR}/${name}.tar.gz")
    b2b_download_file("${url}" "${sha256}" "${archive}")
    if(NOT EXISTS "${B2B_XIPH_DIR}/${name}")
        file(ARCHIVE_EXTRACT INPUT "${archive}" DESTINATION "${B2B_XIPH_DIR}")
    endif()
endfunction()

b2b_fetch_archive(
    https://github.com/xiph/ogg/releases/download/v1.3.5/libogg-1.3.5.tar.gz
    0eb4b4b9420a0f51db142ba3f9c64b333f826532dc0f48c6410ae51f4799b664
    libogg-1.3.5)
b2b_fetch_archive(
    https://github.com/xiph/vorbis/releases/download/v1.3.7/libvorbis-1.3.7.tar.gz
    0e982409a9c3fc82ee06e08205b1355e5c6aa4c36bca58146ef399621b0ce5ab
    libvorbis-1.3.7)

set(B2B_OGG_DIR "${B2B_XIPH_DIR}/libogg-1.3.5")
set(B2B_VORBIS_DIR "${B2B_XIPH_DIR}/libvorbis-1.3.7")
set(B2B_OGG_GENERATED_DIR "${B2B_XIPH_DIR}/generated")
file(WRITE "${B2B_OGG_GENERATED_DIR}/ogg/config_types.h.tmp" [=[
#ifndef __CONFIG_TYPES_H__
#define __CONFIG_TYPES_H__
#include <stdint.h>
typedef int16_t ogg_int16_t;
typedef uint16_t ogg_uint16_t;
typedef int32_t ogg_int32_t;
typedef uint32_t ogg_uint32_t;
typedef int64_t ogg_int64_t;
typedef uint64_t ogg_uint64_t;
#endif
]=])
file(COPY_FILE "${B2B_OGG_GENERATED_DIR}/ogg/config_types.h.tmp" "${B2B_OGG_GENERATED_DIR}/ogg/config_types.h" ONLY_IF_DIFFERENT)

add_library(b2b_ogg STATIC
    "${B2B_OGG_DIR}/src/bitwise.c"
    "${B2B_OGG_DIR}/src/framing.c")
target_include_directories(b2b_ogg SYSTEM PUBLIC "${B2B_OGG_DIR}/include" "${B2B_OGG_GENERATED_DIR}")

set(b2b_vorbis_sources
    mdct.c smallft.c block.c envelope.c window.c lsp.c lpc.c analysis.c synthesis.c psy.c info.c
    floor1.c floor0.c res0.c mapping0.c registry.c codebook.c sharedbook.c lookup.c bitrate.c vorbisenc.c)
list(TRANSFORM b2b_vorbis_sources PREPEND "${B2B_VORBIS_DIR}/lib/")
add_library(b2b_vorbis STATIC ${b2b_vorbis_sources})
target_include_directories(b2b_vorbis SYSTEM PUBLIC "${B2B_VORBIS_DIR}/include" PRIVATE "${B2B_VORBIS_DIR}/lib")
target_link_libraries(b2b_vorbis PUBLIC b2b_ogg)

foreach(target IN ITEMS b2b_ogg b2b_vorbis)
    # Why not b2b::compiler_options を適用する: 外部ライブラリの実装に自分たちの厳しい警告設定をかけると、直せない警告で埋もれるため
    if(MSVC)
        target_compile_options(${target} PRIVATE /w)
        target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_options(${target} PRIVATE -w)
    endif()
endforeach()
if(NOT MSVC)
    target_link_libraries(b2b_vorbis PUBLIC m)
endif()
