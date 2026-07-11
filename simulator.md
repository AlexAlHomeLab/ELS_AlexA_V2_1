# Симуляция ELS AlexA V2.1 в Wokwi (Cursor)

Пошаговая инструкция для запуска прошивки в симуляторе [Wokwi](https://wokwi.com/) прямо из редактора **Cursor** (или VS Code). Схема повторяет аппаратуру станка: Arduino Mega 2560, LCD 2004, джойстик, РГИ, энкодер шпинделя, лимиты, селектор режимов Pos 1–8, шаговые двигатели X/Z.

![Симулятор Wokwi — ELS AlexA V2.1](06_Схемы_и_чертежи/Screen%20Администратор%2010_07_26%2017_05_57.jpg)

На скриншоте: работающая симуляция с LCD (`Async F:350 Man`, координаты X/Z), кнопками меню, джойстиком, лимитами, селектором режимов и шаговыми моторами.

---

## Что понадобится

| Компонент | Назначение | Ссылка |
|-----------|------------|--------|
| **Cursor** | Редактор (форк VS Code) | [cursor.com/download](https://cursor.com/download) |
| **Wokwi Simulator** | Расширение симулятора | [Visual Studio Marketplace](https://marketplace.visualstudio.com/items?itemName=wokwi.wokwi-vscode) |
| **Arduino IDE 2.x** | Сборка прошивки (встроенный `arduino-cli`) | [arduino.cc/en/software](https://www.arduino.cc/en/software) |
| **Аккаунт Wokwi** | Бесплатная лицензия для расширения | [wokwi.com](https://wokwi.com/) |

> Wokwi for VS Code — коммерческий продукт с **бесплатным пробным периодом**. Для постоянной работы нужна лицензия (есть бесплатный тариф для личного использования).

---

## Шаг 1. Клонировать репозиторий

```bash
git clone https://github.com/AlexAlHomeLab/ELS_AlexA_V2_1.git
cd ELS_AlexA_V2_1
```

---

## Шаг 2. Установить Cursor и расширение Wokwi

1. Скачайте и установите **Cursor** с [cursor.com/download](https://cursor.com/download).
2. Откройте Cursor.
3. Нажмите `Ctrl+Shift+X` — панель **Extensions**.
4. Найдите **Wokwi Simulator** (`wokwi.wokwi-vscode`) и нажмите **Install**.

   Прямая ссылка: [marketplace.visualstudio.com/items?itemName=wokwi.wokwi-vscode](https://marketplace.visualstudio.com/items?itemName=wokwi.wokwi-vscode)

5. Активируйте лицензию:
   - `F1` → **Wokwi: Request a new License**
   - В браузере войдите в аккаунт Wokwi (или создайте новый)
   - Нажмите **GET YOUR LICENSE** и подтвердите передачу лицензии в Cursor

   Подробнее: [docs.wokwi.com/vscode/getting-started](https://docs.wokwi.com/vscode/getting-started)

---

## Шаг 3. Установить Arduino IDE

1. Скачайте **Arduino IDE 2.x**: [arduino.cc/en/software](https://www.arduino.cc/en/software)
2. Установите IDE (путь по умолчанию на Windows):
   ```
   %LOCALAPPDATA%\Programs\Arduino IDE\
   ```
3. При первом запуске IDE установит платформу **Arduino AVR Boards** (Mega 2560).

Библиотеки **EncButton** и **GyverIO** уже лежат в проекте — отдельная установка не нужна:

```
01_Исходный_код/ELS_AlexA_V2.1/libraries/
```

---

## Шаг 4. Открыть папку прошивки в Cursor

Wokwi ищет `diagram.json` и `wokwi.toml` в **корне рабочей области**.

**Рекомендуемый вариант** — открыть папку прошивки:

```
File → Open Folder → ELS_AlexA_V2_1/01_Исходный_код/ELS_AlexA_V2.1
```

**Альтернатива** — открыть весь репозиторий `ELS_AlexA_V2_1` (в корне уже есть `.vscode/tasks.json`, настроенный на сборку прошивки).

---

## Шаг 5. Файлы конфигурации Wokwi (уже в репозитории)

### `diagram.json`

Описание схемы: Mega 2560, LCD 2004, кнопки, энкодеры, потенциометры, A4988, NEMA17, LED лимитов, бипер.

| Расположение | Назначение |
|--------------|------------|
| `01_Исходный_код/ELS_AlexA_V2.1/diagram.json` | Рабочая копия для симулятора |
| `06_Схемы_и_чертежи/wokwi/diagram.json` | Архивная/референсная схема |

### `wokwi.toml`

Указывает симулятору, какой скомпилированный файл загружать:

```toml
[wokwi]
version = 1
firmware = "build/ELS_AlexA_V2.1.ino.hex"
elf = "build/ELS_AlexA_V2.1.ino.elf"
gdbServerPort = 3333
```

Документация Wokwi: [docs.wokwi.com/vscode/project-config](https://docs.wokwi.com/vscode/project-config)

---

## Шаг 6. Собрать прошивку

Перед запуском симуляции **обязательно** скомпилируйте код — Wokwi загружает готовый `.hex`, а не исходники.

### Способ A — через Cursor (рекомендуется)

1. `Ctrl+Shift+B` — **Run Build Task**
2. Выберите **Arduino: Compile Mega (Wokwi)**

Задача вызывает `arduino-cli` из Arduino IDE и кладёт результат в:

```
01_Исходный_код/ELS_AlexA_V2.1/build/
├── ELS_AlexA_V2.1.ino.hex
└── ELS_AlexA_V2.1.ino.elf
```

### Способ B — через Arduino IDE

1. Откройте `01_Исходный_код/ELS_AlexA_V2.1/ELS_AlexA_V2.1.ino`
2. Плата: **Arduino Mega 2560**
3. **Sketch → Verify/Compile**
4. Скопируйте `.hex` из временной папки сборки в `build/` (или настройте вывод в `build/` через arduino-cli).

### Проверка сборки

В терминале должно быть `Sketch uses ... bytes` без ошибок. Если `arduino-cli.exe` не найден — проверьте путь установки Arduino IDE или установите [Arduino CLI](https://arduino.github.io/arduino-cli/) отдельно и поправьте путь в `.vscode/tasks.json`.

---

## Шаг 7. Запустить симулятор

1. Убедитесь, что папка `build/` содержит актуальный `.hex`
2. `F1` → **Wokwi: Start Simulator**
3. Откроется вкладка с интерактивной схемой

Управление симуляцией:

| Элемент | Действие |
|---------|----------|
| Зелёный ▶ | Запуск / перезапуск |
| Красный ■ | Останов |
| Кнопки на схеме | Клик мышью (удержание = hold) |
| Потенциометры | Перетаскивание ручки |
| Энкодеры | Колёсико мыши над энкодером |

После изменения кода: **пересоберите** (`Ctrl+Shift+B`) — Wokwi подхватит новый `.hex` и перезапустит симуляцию.

---

## Шаг 8. Проверка работы

1. **LCD** — строка режима, Feed, координаты X/Z (как на скриншоте выше)
2. **Pos 1–8** — переключение режимов (Async, Sync, Thread, …)
3. **Джойстик** (A8–A11) + **Rapid** (A12) — ручная подача, моторы крутятся
4. **РГИ** (энкодер слева) — точное позиционирование
5. **LimL / LimF / LimR / LimRe** — лимиты и LED
6. **Select long** (~600 ms) — вход в меню EEPROM
7. **Serial** — откройте Serial Monitor в Cursor (`115200` baud) или через RFC2217 (см. ниже)

---

## Serial Monitor

Прошивка использует `SERIAL_BAUD 115200` (`src/config/config.h`).

- Встроенный Serial Monitor Cursor/VS Code — при активной вкладке симулятора
- **RFC2217** (опционально): добавьте в `wokwi.toml` строку `rfc2217ServerPort = 4000`, подключайтесь к `rfc2217://localhost:4000`

Команды GRBL-style: `?`, `$$`, `$I` — см. [DEBUG.md](02_Документация/DEBUG.md).

---

## Отладка через GDB (опционально)

В репозитории есть конфигурация **Wokwi GDB** (`.vscode/launch.json`):

1. Запустите симулятор (**Wokwi: Start Simulator**)
2. `F5` → конфигурация **Wokwi GDB**
3. Отладчик подключится к порту `3333` (`gdbServerPort` в `wokwi.toml`)

Требуется `avr-gdb` (в `.gitignore`, не входит в репозиторий). Для обычной разработки достаточно Serial и визуальной проверки на схеме.

---

## Типичные проблемы

| Симптом | Решение |
|---------|---------|
| «Firmware not found» | Выполните сборку (`Ctrl+Shift+B`), проверьте `build/*.hex` |
| Пустой LCD | Крутите **LCD V0 contrast** на схеме |
| `arduino-cli` не найден | Установите Arduino IDE 2.x или поправьте путь в `tasks.json` |
| Лицензия Wokwi | `F1` → **Wokwi: Request a new License** |
| Схема не открывается | Откройте папку `01_Исходный_код/ELS_AlexA_V2.1` как workspace |
| Симуляция «зависла» | Вкладка Wokwi должна быть видимой; при сворачивании симуляция может паузиться |

---

## Полезные ссылки

| Ресурс | URL |
|--------|-----|
| Wokwi — главная | [wokwi.com](https://wokwi.com/) |
| Wokwi Docs (VS Code) | [docs.wokwi.com/vscode/getting-started](https://docs.wokwi.com/vscode/getting-started) |
| Конфигурация проекта | [docs.wokwi.com/vscode/project-config](https://docs.wokwi.com/vscode/project-config) |
| Расширение Wokwi | [marketplace.visualstudio.com/items?itemName=wokwi.wokwi-vscode](https://marketplace.visualstudio.com/items?itemName=wokwi.wokwi-vscode) |
| Cursor | [cursor.com/download](https://cursor.com/download) |
| Arduino IDE | [arduino.cc/en/software](https://www.arduino.cc/en/software) |
| Arduino CLI | [arduino.github.io/arduino-cli](https://arduino.github.io/arduino-cli/) |
| Схема (diagram.json) | [06_Схемы_и_чертежи/wokwi/diagram.json](06_Схемы_и_чертежи/wokwi/diagram.json) |
| Скрин симулятора | [06_Схемы_и_чертежи/Screen Администратор 10_07_26 17_05_57.jpg](06_Схемы_и_чертежи/Screen%20Администратор%2010_07_26%2017_05_57.jpg) |
| Документация проекта | [README.md](README.md) |

---

<p align="center"><sub>ELS AlexA V2.1 · Wokwi simulation guide</sub></p>
