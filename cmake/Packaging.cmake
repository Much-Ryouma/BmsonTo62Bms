# Why not 実行ファイルを bin/ に分ける: 設定の ini を exe の横に置くため、展開したフォルダの直下に exe・DLL・説明をまとめる
install(FILES "${PROJECT_SOURCE_DIR}/LICENSE" DESTINATION . RENAME LICENSE.txt)
install(FILES
    "${PROJECT_SOURCE_DIR}/packaging/README.txt"
    "${PROJECT_SOURCE_DIR}/packaging/THIRD_PARTY_NOTICES.txt"
    DESTINATION .)
install(DIRECTORY "${PROJECT_SOURCE_DIR}/packaging/licenses" DESTINATION .)

if(MSVC)
    # Why not Windows 標準の UCRT も同梱する: Windows 10 以降には入っており、配布物が大きくなるだけのため
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION .)
    set(CMAKE_INSTALL_UCRT_LIBRARIES FALSE)
    include(InstallRequiredSystemLibraries)
endif()

if(WIN32)
    set(b2b_package_platform "win64")
else()
    string(TOLOWER "${CMAKE_SYSTEM_NAME}" b2b_package_platform)
endif()
set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_NAME "bmsonto62bms")
set(CPACK_PACKAGE_VENDOR "bmsonto62bms contributors")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "bmsonto62bms-${PROJECT_VERSION}-${b2b_package_platform}")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_BINARY_DIR}/package")
include(CPack)
