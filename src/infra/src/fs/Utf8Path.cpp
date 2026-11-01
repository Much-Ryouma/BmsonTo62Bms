#include "b2b/infra/fs/Utf8Path.hpp"

#include <fstream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace b2b::infra {

std::filesystem::path pathFromUtf8(const std::string& utf8) {
    // Why not std::filesystem::path(utf8) をそのまま使う: Windows ではシステムのコードページ (CP932) として解釈され、日本語のファイル名が壊れるため
    return std::filesystem::path(std::u8string(utf8.begin(), utf8.end()));
}

std::string pathToUtf8(const std::filesystem::path& path) {
    const std::u8string utf8 = path.u8string();
    return std::string(utf8.begin(), utf8.end());
}

std::string readTextFile(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("ファイルを開けません: " + pathToUtf8(path));
    }
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
}

void writeTextFileAtomically(const std::filesystem::path& path, const std::string& content) {
    std::filesystem::path temporary = path;
    temporary += ".tmp";
    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        if (!stream) {
            throw std::runtime_error("ファイルを書き込めません: " + pathToUtf8(temporary));
        }
        stream.write(content.data(), static_cast<std::streamsize>(content.size()));
        if (!stream) {
            throw std::runtime_error("ファイルの書き込みに失敗しました: " + pathToUtf8(temporary));
        }
    }
    // Why not 直接上書きする: 書き込みの途中で失敗したとき、元のファイルが壊れた状態で残るのを避けるため
    std::error_code error;
    std::filesystem::rename(temporary, path, error);
    if (error) {
        std::filesystem::remove(temporary);
        throw std::runtime_error("ファイルを置き換えられません: " + pathToUtf8(path) + " (" + error.message() + ")");
    }
}

std::filesystem::path executableDirectory() {
#if defined(_WIN32)
    std::vector<wchar_t> buffer(MAX_PATH);
    for (;;) {
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            throw std::runtime_error("実行ファイルの場所を取得できません");
        }
        if (length < buffer.size()) {
            return std::filesystem::path(std::wstring(buffer.data(), length)).parent_path();
        }
        buffer.resize(buffer.size() * 2);
    }
#else
    return std::filesystem::canonical("/proc/self/exe").parent_path();
#endif
}

}  // namespace b2b::infra
