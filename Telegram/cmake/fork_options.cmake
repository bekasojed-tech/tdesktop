option(TDESKTOP_FORK_DISABLE_ADS "Disable advertisements in the bekasojed-tech fork." ON)

target_compile_definitions(Telegram PRIVATE
    TDESKTOP_FORK_DISABLE_ADS=$<BOOL:${TDESKTOP_FORK_DISABLE_ADS}>
)

if (WIN32 AND CMAKE_GENERATOR MATCHES "^Visual Studio ")
    add_custom_command(TARGET Telegram PRE_LINK
        COMMAND powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_LIST_DIR}/../build/rebuild_debug_pre_link.ps1"
            -BuildDirectory "${CMAKE_BINARY_DIR}"
            -Configuration "$<CONFIG>"
            -Executable "$<TARGET_FILE:Telegram>"
        VERBATIM
    )
endif()
