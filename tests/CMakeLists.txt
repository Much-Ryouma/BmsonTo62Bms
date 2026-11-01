include(GoogleTest)

function(b2b_add_test_suite name)
    cmake_parse_arguments(ARG "" "" "SOURCES;LIBRARIES" ${ARGN})
    add_executable(${name} ${ARG_SOURCES})
    target_link_libraries(${name} PRIVATE ${ARG_LIBRARIES} GTest::gtest_main b2b::compiler_options)
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/support)
    target_compile_definitions(${name} PRIVATE B2B_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    gtest_discover_tests(${name} DISCOVERY_MODE PRE_TEST)
endfunction()

b2b_add_test_suite(b2b_core_tests
    SOURCES
        unit/core/BmsWriterTest.cpp
        unit/core/FadeTest.cpp
        unit/core/FileNamesTest.cpp
        unit/core/HeaderValuesTest.cpp
        unit/core/KeyModeTest.cpp
        unit/core/LaunchCommandTest.cpp
        unit/core/MeasureLayoutTest.cpp
        unit/core/MixTest.cpp
        unit/core/NamePatternTest.cpp
        unit/core/ObjectIdTest.cpp
        unit/core/PcmTest.cpp
        unit/core/SliceMergeTest.cpp
        unit/core/SlicePlanTest.cpp
        unit/core/TimingMapTest.cpp
    LIBRARIES b2b::core)

b2b_add_test_suite(b2b_infra_tests
    SOURCES
        unit/infra/AudioFilesTest.cpp
        unit/infra/AudioProbeTest.cpp
        unit/infra/BmsonJsonTest.cpp
        unit/infra/TextEncodingTest.cpp
    LIBRARIES b2b::infra)

b2b_add_test_suite(b2b_app_tests
    SOURCES
        unit/app/ConvertServiceTest.cpp
        unit/app/ConvertSetTest.cpp
        unit/app/ConverterSessionTest.cpp
        unit/app/PreviewServiceTest.cpp
        unit/app/RoundTripTest.cpp
        unit/app/SoundExportTest.cpp
        unit/app/SoundNamingTest.cpp
    LIBRARIES b2b::app)

if(B2B_BUILD_GUI)
    find_package(Qt6 6.4 REQUIRED COMPONENTS Test)
    add_executable(b2b_gui_tests gui/GuiTestMain.cpp gui/MainWindowGuiTest.cpp)
    target_link_libraries(b2b_gui_tests PRIVATE b2b::gui GTest::gtest b2b::compiler_options)
    target_include_directories(b2b_gui_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/support)
    # Why not gtest_discover_tests で 1 件ずつ登録する: 一覧を取り出すときにも Qt の DLL が必要で、
    # Windows では PATH を通す前に実行されて失敗するため、まとめて 1 件として登録し、環境変数を設定して実行する（IgusaBmsonStudio と同じ）
    add_test(NAME MainWindowGui COMMAND b2b_gui_tests)
    set(b2b_gui_test_environment "QT_QPA_PLATFORM=set:offscreen")
    if(WIN32)
        get_target_property(b2b_qt_core_location Qt6::Core IMPORTED_LOCATION_RELEASE)
        if(NOT b2b_qt_core_location)
            get_target_property(b2b_qt_core_location Qt6::Core IMPORTED_LOCATION)
        endif()
        get_filename_component(b2b_qt_bin_dir "${b2b_qt_core_location}" DIRECTORY)
        list(APPEND b2b_gui_test_environment "PATH=path_list_prepend:${b2b_qt_bin_dir}")
    endif()
    set_tests_properties(MainWindowGui PROPERTIES ENVIRONMENT_MODIFICATION "${b2b_gui_test_environment}")
endif()
