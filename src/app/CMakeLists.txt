add_library(b2b_app STATIC
    src/ConvertService.cpp
    src/ConverterSession.cpp
    src/PreviewService.cpp
    src/Ports.cpp
    src/SoundNaming.cpp)
add_library(b2b::app ALIAS b2b_app)

target_include_directories(b2b_app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(b2b_app PUBLIC b2b::core b2b::infra PRIVATE b2b::compiler_options)
