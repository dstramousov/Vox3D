# VoX3D

Первый каркас C++20 + raylib проекта.

Текущая версия: `v0.2.5`.

## Сборка

Debug:

```bash
make debug
```

Release:

```bash
make release
```

Запуск:

```bash
./r --log-level=debug --debug-ui
```

То же самое напрямую:

```bash
make run ARGS="--log-level=debug --debug-ui"
```


## Управление экранами

- Приложение сразу открывает `MainRender`, без предварительного главного меню.
- Map package сразу загружается из `map.path` в конфиге или из `--map=...`.
- `Up/Down/W/S/Left/Right/A/D/Tab` на `MainRender` переключают выбранный раздел справа.
- Клик по разделу правой панели раскрывает/закрывает его подпункты; hover только подсвечивает/показывает hovered-статус и не меняет выбранный раздел.
- В разделе `Layers` можно переключать 2D-слои `Terrain`, `Elevation`, `Collision` и `Grid`, если данные слоя доступны.
- `Esc` на `MainRender` открывает подтверждение выхода из программы.
- `Enter` на `Yes` в confirmation dialog завершает приложение.
- `Esc` или `No` в confirmation dialog отменяет выход и возвращает назад в `MainRender`.
- Кнопка закрытия окна тоже открывает подтверждение выхода; отмена не должна переоткрывать dialog каждый кадр.

## Конфигурация

Основной файл настроек:

```text
config/app.json
```

Из него читаются:

- имя приложения;
- базовый и fallback-размер окна;
- ограничение размера окна относительно монитора;
- resizable/vsync/target FPS;
- минимальный и максимальный UI scale;
- один общий масштаб шрифтов `ui.font_scale`;
- декоративный шрифт заголовка `ui.title_font_path`;
- основной текстовый шрифт интерфейса `ui.text_font_path`;
- уровень логирования приложения;
- уровень логирования raylib;
- цветной лог;
- текущий язык UI `language.current`;
- директория языковых файлов `language.directory`;
- путь к map package `map.path`.

Размеры отдельных элементов UI не настраиваются сотней мелких ручек. Основная схема такая:

```text
ui.font_scale      -> одним числом меняет размер всей типографики
ui.title_font_path -> меняет декоративный заголовочный шрифт
ui.text_font_path  -> меняет меню, dialog, FPS, debug overlay и placeholder-экраны
```

Геометрия меню, dialog, hitbox-ов, перенос текста и отступы рассчитываются через `UiMetrics`, а результат layout кэшируется до изменения окна или UI-состояния.

Можно передать другой конфиг:

```bash
./r --config=config/app.json
```

CLI переопределяет настройки из конфига:

```bash
./r --log-level=debug --raylib-log-level=error --debug-ui --no-color
```

Поддерживаемые аргументы:

```text
--config=<path>
--map=<path>
--log-level=trace|debug|info|warn|error|fatal
--raylib-log-level=trace|debug|info|warning|error|fatal
--debug-ui
--no-color
--language=en|uk
```

## Map package

Путь к map package задаётся в конфиге:

```json
"map": {
  "path": "../TopDownMapGen/output/map_package"
}
```

Его можно переопределить из CLI:

```bash
./r --map=../TopDownMapGen/output/map_package
```

В `v0.2.1` loader делает bounded-инспекцию пакета и первый диагностический overview:

- проверяет, что путь существует и является директорией;
- ищет известные metadata/manifest/runtime-grid файлы;
- пытается вытащить `width/height`, `tile_size` и диапазон уровней;
- читает terrain/tile grid только в пределах безопасного лимита;
- строит 2D overview в центральном viewport, если grid найден;
- показывает summary в правой нижней служебной панели и status bar;
- не строит 3D, чанки, физику и GPU-ресурсы карты.

Это первый безопасный слой перед реальной 3D-визуализацией карты.


## Локализация

Весь пользовательский текст UI вынесен в label-файлы:

```text
res/lang/en.json
res/lang/uk.json
```

Язык по умолчанию — английский:

```json
"language": {
  "current": "en",
  "directory": "res/lang"
}
```

Переключение языка через config:

```json
"current": "uk"
```

Или через CLI:

```bash
./r --language=uk
```

Технические идентификаторы состояний и hover/action в debug overlay пока остаются стабильными английскими enum-именами (`main_menu`, `exit_confirmation`, `placeholder_exit`), чтобы логи и диагностика не плавали от языка.

## Шрифты

Проект хранит только нужный минимум:

```text
res/fonts/Noto_Sans/static/NotoSans-Regular.ttf
res/fonts/Noto_Sans/static/NotoSans-Bold.ttf
res/fonts/Noto_Sans/OFL.txt
res/fonts/Noto_Sans/README.txt
```

`NotoSans-Regular.ttf` используется как основной UI-шрифт. `NotoSans-Bold.ttf` используется для заголовка `VoX3D`.

## Очистка

Обычная очистка:

```bash
./c
# или
make clean
```

Она не удаляет `build/`, `.deps/` и скачанный raylib. Это важно, чтобы не тянуть raylib из GitHub после каждой чистки.

Полная очистка build-директорий:

```bash
./c --dist
# или
make distclean
```

Она удаляет `build/` и `build-release/`, но не трогает `.deps/`.

Явно удалить скачанные зависимости:

```bash
./c --deps
# или
make depsclean
```

Это делать только если реально нужно перекачать зависимости.


## Workspace UI

`v0.2.1` добавляет первый рабочий экран приложения в стиле старого 3D/editor UI:

```text
┌───────────────────────────────┬──────────────┐
│                               │              │
│        Main View               │  Tool panel  │
│                               │              │
├───────────────────────────────┴──────────────┤
│ Status bar                                    │
└───────────────────────────────────────────────┘
```

Цветовая схема зашита как единая editor-theme внутри UI-слоя: серый viewport, сине-бирюзовая боковая панель, светлая нижняя/status-панель, жёлтый active item. Служебная информация и debug overlay больше не висят в левом верхнем углу: они выводятся в правом нижнем служебном блоке. FPS и RSS-память процесса показываются справа в нижнем status bar. В конфиг это пока не вынесено, чтобы не плодить очередные сто ручек.
