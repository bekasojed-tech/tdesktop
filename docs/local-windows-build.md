# Локальная Debug-сборка Windows x64

Инструкция для клона `C:\Users\Home\source\repos\Home\tdesktop`.
Команды выполнять в **cmd.exe**, по одной, с остановкой при ошибке.

## Проверенное состояние на 17 октября 2026 года

Настроены Visual Studio 2026, MSVC 14.44, CMake 4.3.1-msvc1, Windows SDK
10.0.26100.0 и Qt 5.15.19. Подмодули загружены, внешние библиотеки подготовлены,
проект сгенерирован в `out` генератором `Visual Studio 18 2026` для x64.

При первой Debug-сборке два файла тестового стенда использовали API Qt 6,
отсутствующие в Qt 5. В `test_queued_drag.cpp` и `test_chat_menu.cpp` вызовы
заменены совместимыми с обеими версиями Qt; повторная компиляция этих файлов
прошла успешно. Полная Debug-сборка Telegram завершилась с кодом 0; результат —
`out\Debug\Telegram.exe`.

## Подготовка Visual Studio

В Visual Studio Installer нажать **Modify / Изменить** у **Visual Studio
Community 2026** и установить **Desktop development with C++ / Разработка
классических приложений на C++**. Во вкладке **Individual components /
Отдельные компоненты** выбрать:

- **MSVC v143 - VS 2022 C++ x64/x86 build tools (v14.44-17.14)**;
- Windows SDK 10.0.26100.0;
- C++ CMake tools for Windows.

Компонент MSVC 14.44 доступен в каталоге установщика VS 2026 под указанным
именем. Он устанавливается внутрь VS 2026; отдельная IDE Visual Studio 2022
для этого пути не нужна. Имя «VS 2022» обозначает поколение компилятора.
Набор по умолчанию в VS 2026 — v145 / MSVC 14.51; для этой сборки выбираем
v143 / MSVC 14.44, как требует [инструкция проекта](building-win.md).

Открыть cmd.exe и инициализировать окружение:

```bat
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" -vcvars_ver=14.44
where cl
where cmake
where ninja
cmake --version
```

Все команды должны завершиться успешно. Нужен CMake 4.2 или новее для
генератора VS 2026; установленная копия внутри VS 2026 имеет версию
4.3.1-msvc1. Пути из `where cmake` и `where ninja` должны вести в VS 2026.
Последующие команды выполнять в этом же окне.

## Первоначальная подготовка библиотек

```bat
cd /d C:\Users\Home\source\repos\Home\tdesktop
git submodule update --init --recursive --jobs 8
call Telegram\build\prepare\win.bat skip-release
```

Скрипт скачивает инструменты и исходники библиотек, затем собирает зависимости,
включая Qt и WebRTC. Этот тяжёлый этап может занять часы. `skip-release`
отключает дополнительные Release-варианты там, где скрипт предусматривает
такой выбор. Часть зависимостей скрипт собирает только в оптимизированном
варианте; само приложение ниже собирается в Debug.

Фактическую структуру задают `prepare.py` и `cmake/variables.cmake`:

```text
Home\
  tdesktop\
  Libraries\win64\
  ThirdParty\
```

Для x64 по умолчанию выбирается Qt 5.15.19. Не добавлять `qt6` только к одному
из этапов: подготовка и настройка должны использовать одинаковую версию Qt.
Библиотеки не нужно готовить заново при каждом изменении исходников Telegram.

## Настройка и первая сборка

Получить собственные `api_id` и `api_hash` по
[инструкции проекта](api_credentials.md). Вместо `YOUR_API_ID` и
`YOUR_API_HASH` подставить свои значения. Они сохраняются локально
в `out/CMakeCache.txt`.

```bat
set "CMAKE_GENERATOR=Visual Studio 18 2026"
cd /d C:\Users\Home\source\repos\Home\tdesktop\Telegram
call configure.bat x64 -D TDESKTOP_API_ID=YOUR_API_ID -D TDESKTOP_API_HASH=YOUR_API_HASH -D CMAKE_CONFIGURATION_TYPES=Debug -D CMAKE_SYSTEM_VERSION=10.0.26100.0
cd ..
cmake --build out --config Debug --target Telegram --parallel 6
```

Результат: `out\Debug\Telegram.exe`. Для отладки открыть сгенерированное
решение Telegram из каталога `out` в Visual Studio 2026, выбрать
**Debug / x64** и проект **Telegram**. `configure.bat x64` явно выбирает
набор v143; он должен быть установлен именно в VS 2026.

EXE готов только после успешного завершения всей команды сборки. При ошибке
компиляции он может отсутствовать, а во время линковки ещё быть неполным.
Журнал диагностического запуска сохранён в `out\build-debug.log`.

## Обычная работа

Для автоматического цикла «скомпилировать → закрыть перед линковкой → собрать EXE → запустить»
выполнить из обычного cmd.exe или PowerShell:

```bat
.\rebuild-release.cmd
```

Это ежедневный Release-клиент (см. [local-client-launch.md](local-client-launch.md)).
Для отладки тот же цикл для Debug выполняет `.\rebuild-debug.cmd`.

Скрипт сам инициализирует окружение Visual Studio. Состояние профиля
`out\local-profile` сохраняется; установленные клиенты Telegram не закрываются.
При ошибке компиляции клиент остаётся работать. Закрытие выполняется
на стадии `PRE_LINK`; MSBuild может вызывать её и без изменений исходников.
Клиент запускается только после успешной сборки.
Подробности, проверка без сборки и разрешение для следующих сессий — в [rebuild-debug.md](rebuild-debug.md).

Для ручной пересборки открыть окружение VS 2026, как выше, и выполнить
из корня клона:

```bat
cmake --build out --config Debug --target Telegram --parallel 6
```

При ручной пересборке предварительно закрыть локальный Debug-клиент
и завершить его отладку, чтобы EXE и PDB не были заблокированы.

Для запуска дважды щёлкнуть `run-debug.cmd` в корне клона. Можно создать
ярлык на этот файл и положить его на рабочий стол. Скрипт запускает
`out\Debug\Telegram.exe` с отдельной папкой данных `out\local-profile`
и параметром `-noupdate`. Первый запуск потребует входа, следующие сохранят
этот профиль. Локальный клиент может работать рядом с установленным Telegram.
Visual Studio и окружение компилятора для самого запуска не нужны.

Для F5 в свойствах проекта Telegram, **Debugging / Отладка**, задать
**Command Arguments / Аргументы команды**:

```text
-workdir "C:\Users\Home\source\repos\Home\tdesktop\out\local-profile" -noupdate
```

Создать эту папку заранее либо один раз запустить `run-debug.cmd` после
сборки. При одинаковом `-workdir` ярлык и F5 используют один профиль;
перед F5 закрыть уже работающий клиент.

Профиль содержит вход в аккаунт. Сохранить `out\local-profile` при удалении
промежуточных результатов сборки, если нужно сохранить авторизацию.
