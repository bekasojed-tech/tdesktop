add_executable(test_channel_filter)
init_target(test_channel_filter "(tests)")

target_include_directories(test_channel_filter PRIVATE ${src_loc})
nice_target_sources(test_channel_filter ${src_loc}
PRIVATE
    history/history_channel_filter.cpp
    history/history_channel_filter.h
    tests/test_channel_filter.cpp
)

target_link_libraries(test_channel_filter
PRIVATE
    desktop-app::lib_base
    desktop-app::external_qt
)

set_target_properties(test_channel_filter PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
