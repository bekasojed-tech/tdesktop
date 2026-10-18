# Работа с форком Telegram Desktop

Наш репозиторий: https://github.com/bekasojed-tech/tdesktop.
Оригинал: https://github.com/telegramdesktop/tdesktop.

## Ветки и remotes

- `origin` — наш форк: сюда отправляются наши коммиты и pull requests.
- `upstream` — оригинал: отсюда только получаем обновления.
- `dev` / `origin/dev` — основная ветка нашей версии, включая наши изменения.
- `upstream/dev` — состояние оригинальной ветки разработки.
- `codex/<change>` — необязательная отдельная ветка от нашей `dev`, когда
  доработку нужно сначала проверить через pull request.

Обновления оригинала вливаем в нашу `dev` через merge. Это сохраняет уже
опубликованные коммиты и позволяет обходиться без force push. Наши изменения
не отправляем в оригинал и не создаём туда pull requests.

## Настройка нового клона

Настройки Git локальны: после клонирования их нужно применить на каждом
компьютере. В текущем рабочем клоне они уже применены.

```sh
git clone https://github.com/bekasojed-tech/tdesktop.git
cd tdesktop
git switch dev
git remote add upstream https://github.com/telegramdesktop/tdesktop.git
git config --local remote.upstream.pushurl disabled://upstream
git config --local remote.pushDefault origin
git config --local push.default simple
git config --local pull.ff only
git config --local pull.rebase false
git config --local rerere.enabled true
git config --local rerere.autoupdate false
git config --local fetch.recurseSubmodules false
```

Если используется GitHub CLI, указываем форк как репозиторий по умолчанию:

```sh
gh repo set-default origin
```

`disabled://upstream` — намеренно нерабочий адрес для push. Получение обновлений
продолжает использовать настоящий URL оригинала. Это защита от случайной
отправки через remote `upstream`, а не серверный запрет: явный URL или изменение
настроек могут её обойти. Все PR направляем явно в `bekasojed-tech/tdesktop`.

`fetch.recurseSubmodules false` отключает докачку подмодулей при `git fetch`.
В старой истории оригинала есть ссылка на коммит `tgcalls`, которого больше нет
на GitHub, и после force push в ветки оригинала fetch может завершаться ошибкой
`not our ref`. Подмодули обновляются отдельным шагом после merge, см. ниже.

## Небольшая доработка

Обычно коммитим прямо в `dev`. Начинаем с чистого рабочего дерева. Каждую
команду выполняем только после успешного завершения предыдущей; при ошибке
или конфликте останавливаемся.

```sh
git switch dev
git pull --ff-only origin dev
```

Делаем небольшие коммиты по одной самостоятельной доработке, проверяем поведение
и отправляем `dev` в форк:

```sh
git push origin dev
```

Если доработку нужно сначала посмотреть в pull request, создаём для неё ветку
`git switch -c codex/my-change` от свежей `dev`, отправляем её через
`git push -u origin codex/my-change` и открываем PR, явно задав репозиторий
и базовую ветку:

```sh
gh pr create --repo bekasojed-tech/tdesktop --base dev
```

Избегаем попутного форматирования и ненужных перемещений исходного кода: чем
меньше пересечение с изменениями оригинала, тем проще последующие обновления.

## Обновление из оригинала

Сначала завершаем или отдельно сохраняем незакоммиченные изменения. Выполняем
команды по одной и продолжаем только при успехе:

```sh
git switch dev
git pull --ff-only origin dev
git fetch upstream
git merge --no-edit upstream/dev
```

Если `pull --ff-only` сообщает о расхождении с `origin/dev`, сначала разбираем
коммиты обеих сторон и объединяем их через `git merge origin/dev`. Не сбрасываем
свою ветку на оригинал и не переписываем опубликованную историю через rebase.

Если merge остановился на конфликтах, согласовываем изменения обеих сторон,
добавляем исправленные файлы через `git add <path>` и выполняем
`git merge --continue`. Для отмены незавершённого merge есть `git merge --abort`.
`rerere` запоминает решения повторяющихся конфликтов; проверяем результат через
`git diff` перед добавлением файлов в индекс.

После успешного объединения обновляем подмодули до версий, записанных в проекте:

```sh
git submodule sync --recursive
git submodule update --init --recursive
```

Проверяем diff обновления, собираем Debug по инструкциям платформы в `AGENTS.md`
и проверяем наши доработки. Обновления могут требовать новых зависимостей — в
этом случае следуем соответствующей инструкции сборки в `docs/`. После проверки:

```sh
git push origin dev
```

При пересечении наших изменений с оригиналом конфликты возможны. Обновляемся
небольшими порциями, чтобы их было проще разбирать. Процесс запускается вручную.
