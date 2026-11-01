add_library(b2b_compiler_options INTERFACE)
add_library(b2b::compiler_options ALIAS b2b_compiler_options)

if(MSVC)
    # Why not default code page: 日本語を含むソースを CP932 として解釈されると文字化けするため /utf-8 を明示する
    target_compile_options(b2b_compiler_options INTERFACE /utf-8 /W4 /permissive- /Zc:__cplusplus)
    target_compile_definitions(b2b_compiler_options INTERFACE NOMINMAX WIN32_LEAN_AND_MEAN)
else()
    target_compile_options(b2b_compiler_options INTERFACE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()
