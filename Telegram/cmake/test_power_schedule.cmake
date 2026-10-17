add_executable(test_power_schedule)
init_target(test_power_schedule "(tests)")

target_include_directories(test_power_schedule PRIVATE ${src_loc})
nice_target_sources(test_power_schedule ${src_loc}
PRIVATE
    history/view/history_view_power_schedule_parse.cpp
    history/view/history_view_power_schedule_parse.h
    tests/test_power_schedule.cpp
)

target_link_libraries(test_power_schedule
PRIVATE
    desktop-app::lib_base
    desktop-app::lib_crl
    desktop-app::lib_ui
    desktop-app::external_qt
)

set_target_properties(test_power_schedule PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
