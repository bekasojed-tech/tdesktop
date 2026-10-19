# Запуск локального Telegram в Windows

Для ежедневной работы используется Release-сборка `out\Release\Telegram.exe`:
Debug-сборка (`/Od /RTC1`) заметно тормозит при раскладке ленты, например
при переключении каналов. Оба EXE запускаются с одним профилем:
`-workdir "<checkout>\out\local-profile" -noupdate`.
Профиль аккаунта хранится в `out\local-profile`; `-noupdate` отключает
проверку обновлений при запуске, чтобы использовать собранную здесь версию.
Одновременно может работать только один EXE с этим профилем.
Debug-сборку по-прежнему запускает `run-debug.cmd`, пересобирает `rebuild-debug.cmd`
(см. [rebuild-debug.md](rebuild-debug.md)).

## Сборка Release

Зависимости подготовлены без `skip-release`, а в `out` добавлена
конфигурация Release (`CMAKE_CONFIGURATION_TYPES=Debug;Release`).
Пересборка и перезапуск Release-клиента из корня checkout:

```bat
.\rebuild-release.cmd
```

Это тот же помощник, что и `rebuild-debug.cmd`, с `-Configuration Release`;
см. [rebuild-debug.md](rebuild-debug.md). Сборка с нуля занимает около
полутора часов, полная линковка требует около 40 ГБ памяти; после небольших
изменений пересборка заметно быстрее. Помощник одной конфигурации завершится
ошибкой и ничего не закроет, если профиль занят клиентом другой конфигурации:
перед переходом между Debug и Release закройте работающий клиент.

Если `prepare` останавливается на `libwebp` с ошибкой
`Unable to auto-detect toolchain architecture`, он запущен из Git Bash:
в `PATH` первым стоит `find` из MSYS. Запускать из cmd.exe или PowerShell.
Release-этап `breakpad` собирает `dump_syms`, которому нужен ATL. Для MSVC
14.44 ATL не установлен; этап собран с заголовками ATL из 14.51 через
`UseEnv=true` и дополненные `INCLUDE`/`LIB`. Сам Telegram `dump_syms`
не использует.

## Автозагрузка

Пользовательский ярлык `Telegram.lnk` в папке `shell:startup` указывает
на `out\Release\Telegram.exe`.
Аргументы: `-workdir "<checkout>\out\local-profile" -noupdate -startintray`.
При входе в Windows клиент запускается в трей. Запись разрешена
в `StartupApproved\StartupFolder`.

Здесь намеренно используется `-startintray`: запуск с `-autostart` дополнительно
проверяет внутреннюю настройку Telegram и может сразу завершиться, если она
выключена. Автозагрузка настроена через Windows. Переключение встроенной
настройки автозапуска в любой установленной версии Telegram может удалить
или перезаписать общий `Startup\Telegram.lnk`; после этого проверьте его цель
и аргументы. Текущая реализация `ManageAppLink()` не сохраняет `-workdir`
и `-noupdate` при создании ярлыка.

## Панель задач

Закреплён ярлык `Telegram Local`; он и одноимённый ярлык в меню «Пуск»
указывают непосредственно на `out\Release\Telegram.exe` с аргументами выше.
Созданный клиентом ярлык `Telegram.lnk` в корне пользовательского меню «Пуск»
указывает туда же, с явными `-workdir` и `-noupdate`.

У ярлыка и процесса должен совпадать `System.AppUserModel.ID`: это связывает
окно с кнопкой панели задач, как описано в
[документации Microsoft](https://learn.microsoft.com/en-us/windows/win32/shell/appids).
У этой сборки идентификатор зависит только от явно заданного пути профиля,
поэтому он одинаков для Debug и Release:
`Telegram.TelegramDesktop.36faeced4a62702927a39eadbd66d912`.
Перенос checkout или смена профиля требуют обновить ярлыки и закрепление.
Для перехода между Debug и Release достаточно поменять в ярлыках цель
и рабочую папку; `System.AppUserModel.ID` при этом сохраняется.

## Восстановление

Ярлыки, указывавшие на Debug-сборку, сохранены в
`out/windows-integration-backup-20261020-022427/`.
Оригинальные ярлыки официальной установки сохранены в
`out/windows-integration-backup-20261017-214748/`:
`Startup-Telegram.lnk` и `StartMenu-Telegram.lnk`.
Для возврата официальной автозагрузки скопируйте `Startup-Telegram.lnk`
в `shell:startup` под именем `Telegram.lnk`.
