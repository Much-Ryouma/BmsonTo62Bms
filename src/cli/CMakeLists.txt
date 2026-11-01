add_executable(b2b_cli
    main.cpp
    Commands.cpp
    Commands.hpp)

set_target_properties(b2b_cli PROPERTIES OUTPUT_NAME "bmsonto62bms-cli")
target_link_libraries(b2b_cli PRIVATE b2b::app CLI11::CLI11 nlohmann_json::nlohmann_json b2b::compiler_options)
b2b_add_windows_resources(b2b_cli "bmsonto62bms-cli" "bmsonto62bms command line tool")
install(TARGETS b2b_cli RUNTIME DESTINATION .)
