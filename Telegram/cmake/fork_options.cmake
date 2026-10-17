option(TDESKTOP_FORK_DISABLE_ADS "Disable advertisements in the bekasojed-tech fork." ON)

target_compile_definitions(Telegram PRIVATE
    TDESKTOP_FORK_DISABLE_ADS=$<BOOL:${TDESKTOP_FORK_DISABLE_ADS}>
)
