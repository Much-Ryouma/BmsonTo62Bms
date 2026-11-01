include(FetchContent)

# Why not vcpkg / conan: 依存がまだ少なく、ビルド手順を「CMake だけ」に保つほうが参加者の敷居が低いため

# Why not FetchContent で取得する: アーカイブの展開と依存先の project() 呼び出しが、日本語を含むパスや
# Qt Creator の自動設定スクリプトと組み合わさると構成ごと失敗することがあるため、ハッシュ付きの単一ヘッダだけを取得する
function(b2b_download_file url sha256 destination)
    if(EXISTS "${destination}")
        return()
    endif()
    file(DOWNLOAD "${url}" "${destination}" EXPECTED_HASH SHA256=${sha256} TLS_VERIFY ON STATUS status)
    list(GET status 0 code)
    if(NOT code EQUAL 0)
        file(REMOVE "${destination}")
        message(FATAL_ERROR "Failed to download ${url}: ${status}")
    endif()
endfunction()

function(b2b_add_single_header_library target alias url sha256 relative_path)
    set(include_root "${CMAKE_BINARY_DIR}/_deps/${target}")
    b2b_download_file("${url}" "${sha256}" "${include_root}/${relative_path}")
    add_library(${target} INTERFACE)
    add_library(${alias} ALIAS ${target})
    target_include_directories(${target} SYSTEM INTERFACE "${include_root}")
endfunction()

b2b_add_single_header_library(b2b_nlohmann_json nlohmann_json::nlohmann_json
    https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp
    9bea4c8066ef4a1c206b2be5a36302f8926f7fdc6087af5d20b417d0cf103ea6
    nlohmann/json.hpp)

if(B2B_BUILD_CLI)
    b2b_add_single_header_library(b2b_cli11 CLI11::CLI11
        https://github.com/CLIUtils/CLI11/releases/download/v2.4.2/CLI11.hpp
        3935283e4c9db7089ed304476dbab2805c9311c41b125a55e92da4659df0d660
        CLI/CLI.hpp)
endif()

set(B2B_AUDIO_DEPS_DIR "${CMAKE_BINARY_DIR}/_deps/b2b_audio")
b2b_download_file(
    https://raw.githubusercontent.com/mackron/miniaudio/0.11.21/miniaudio.h
    6b2029714f8634c4d7c70cc042f45074e0565766113fc064f20cd27c986be9c9
    "${B2B_AUDIO_DEPS_DIR}/miniaudio.h")
b2b_download_file(
    https://raw.githubusercontent.com/nothings/stb/f58f558c120e9b32c217290b80bad1a0729fbb2c/stb_vorbis.c
    4c7cb2ff1f7011e9d67950446b7eb9ca044f2e464d76bfbb0b84dd2e23e65636
    "${B2B_AUDIO_DEPS_DIR}/stb_vorbis.c")

if(B2B_BUILD_TESTS)
    # Why not 静的 CRT: GoogleTest の既定 (/MT) のままだと他ターゲット (/MD) とリンク時に衝突するため
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.15.2
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(googletest)
endif()
