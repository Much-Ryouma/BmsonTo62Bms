find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets Concurrent)
qt_standard_project_setup()

add_library(b2b_gui STATIC
    AboutDialog.cpp
    AboutDialog.hpp
    AppIcon.cpp
    AppIcon.hpp
    MainWindow.cpp
    MainWindow.hpp
    PlayerSettingsDialog.cpp
    PlayerSettingsDialog.hpp
    QtProcessLauncher.hpp
    QtText.hpp
    QtTranslations.cpp
    QtTranslations.hpp
    Settings.cpp
    Settings.hpp)
add_library(b2b::gui ALIAS b2b_gui)
set_target_properties(b2b_gui PROPERTIES AUTOMOC ON)
target_include_directories(b2b_gui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(b2b_gui
    PUBLIC b2b::app Qt6::Widgets Qt6::Concurrent
    PRIVATE b2b::compiler_options)

set(b2b_gui_resource_files "${PROJECT_SOURCE_DIR}/packaging/THIRD_PARTY_NOTICES.txt")
if(EXISTS "${PROJECT_SOURCE_DIR}/resources/icons/app.png")
    list(APPEND b2b_gui_resource_files "${PROJECT_SOURCE_DIR}/resources/icons/app.png")
endif()
qt_add_resources(b2b_gui b2b_gui_resources
    PREFIX "/"
    BASE "${PROJECT_SOURCE_DIR}"
    FILES ${b2b_gui_resource_files})

qt_add_executable(b2b_studio main.cpp)
set_target_properties(b2b_studio PROPERTIES
    OUTPUT_NAME "bmsonto62bms"
    WIN32_EXECUTABLE ON)
target_link_libraries(b2b_studio PRIVATE b2b::gui b2b::compiler_options)
b2b_add_windows_resources(b2b_studio "bmsonto62bms" "bmsonto62bms - bmson to extended BMS converter")

install(TARGETS b2b_studio RUNTIME DESTINATION .)
if(WIN32 AND COMMAND qt_generate_deploy_script)
    # Why not windeployqt の既定のまま: MSVC では DLL ではなく vc_redist のインストーラーを置くため、
    # ランタイムは InstallRequiredSystemLibraries で exe の横に置く。訳は Qt 標準のボタンを日本語にするためだけに使う（IgusaBmsonStudio と同じ）
    qt_generate_deploy_script(
        TARGET b2b_studio
        OUTPUT_SCRIPT b2b_deploy_script
        CONTENT "
qt_deploy_runtime_dependencies(
    EXECUTABLE \"$<TARGET_FILE_NAME:b2b_studio>\"
    BIN_DIR .
    PLUGINS_DIR .
    DEPLOY_TOOL_OPTIONS --translations ja
    NO_COMPILER_RUNTIME)
")
    install(SCRIPT "${b2b_deploy_script}")
endif()
