option(TDESKTOP_FORK_DISABLE_ADS "Disable advertisements in the bekasojed-tech fork." ON)
option(TDESKTOP_BUILD_POWER_SCHEDULE_TESTS "Build the local power schedule filter test." OFF)
option(TDESKTOP_BUILD_CHANNEL_FILTER_TESTS "Build the local channel post filter test." OFF)

target_compile_definitions(Telegram PRIVATE
    TDESKTOP_FORK_DISABLE_ADS=$<BOOL:${TDESKTOP_FORK_DISABLE_ADS}>
)

if (WIN32 AND CMAKE_GENERATOR MATCHES "^Visual Studio ")
    # rebuild_debug.ps1 passes /p:TdesktopRebuildStage=-stage, so Telegram links
    # into out/<Config>-stage while the client keeps running from out/<Config>.
    # MSBuild expands the empty default, which keeps manual builds unchanged.
    foreach (config ${CMAKE_CONFIGURATION_TYPES})
        string(TOUPPER ${config} config_upper)
        set_target_properties(Telegram PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY_${config_upper}
                "${CMAKE_BINARY_DIR}/${config}$(TdesktopRebuildStage)"
        )
    endforeach()
endif()
