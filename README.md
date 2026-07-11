# ELS AlexA V2.1 — электронная гитара для токарного станка

[![Platform](https://img.shields.io/badge/platform-Arduino%20Mega%202560-blue)](https://docs.arduino.cc/hardware/mega2560)
[![License](https://img.shields.io/badge/license-MIT-green)](https://opensource.org/licenses/MIT)
[![Version](https://img.shields.io/badge/version-2.1-brightgreen)](01_Исходный_код/ELS_AlexA_V2.1/)
[![Stage](https://img.shields.io/badge/stage-2.2f-orange)](02_Документация/STAGE_2_2.md)

> **Статус:** проект в активной разработке. Ядро движения и ручное управление работают; автоматические циклы и часть режимов — в стадии реализации.

> **Совместимость с существующим железом:** прошивку можно загрузить в **Arduino Mega 2560 на платах с уже работающими электронными гитарами от Олега А.** и их **клонами** — при **совпадении разводки пинов и подключённой периферии** (LCD 2004, джойстик, РГИ, лимиты, энкодер шпинделя и т.д.). Отталкивайтесь от [`hal_pins.h`](01_Исходный_код/ELS_AlexA_V2.1/src/core/hal/hal_pins.h) и схемы [Wokwi](06_Схемы_и_чертежи/wokwi/diagram.json). Если пины или состав железа отличаются — потребуется правка HAL, а не «прошить как есть».

---

## О проекте

**ELS AlexA V2.1** — прошивка Electronic Lead Screw для токарного станка на **Arduino Mega 2560** (ATmega2560).

Система управляет осями **X/Z**, синхронизируется с энкодером шпинделя и поддерживает ручные подачи, программируемые лимиты, меню параметров в EEPROM и набор режимов обработки.

---

## Что уже работает (этап 2.2f)

| Подсистема | Статус | Кратко |
|------------|--------|--------|
| HAL, таймеры, ISR шагов | ✅ | Пины из `hal_pins.h`, DDS ~30 кГц |
| Джойстик подач + Rapid | ✅ | Непрерывная подача, диагональ X+Z, pot-speed |
| РГИ (MPG) | ✅ | Накопитель `hand_pos`, масштаб 1:1 / 0.01 мм, оси X/Z |
| Planner → stepper_gen | ✅ | Абс./отн. перемещения, профиль разгона |
| Компенсация люфта | ✅ | Startup backlash, `BlX`/`BlZ` в меню |
| Программируемые лимиты | ✅ | LimL/F/R/Re, LED, GO_LIM, latch, обнуление оси |
| LCD 2004 + меню EEPROM | ✅ | Параметры осей, подач, Spdl, Buzzer, CrdU |
| Потенциометр подачи | ✅ | Async мм/мин и Sync мм/об — отдельные диапазоны |
| Энкодер шпинделя | ✅ | `spindle_get_count/delta/dir/rpm` |
| E-Stop | ✅ | FSM → ERROR, сброс через меню |
| Serial CLI (GRBL `$`) | ✅ | `$$`, `$n`, `$n=val`, `$I`, `?` |
| Делительная головка | ✅ | A/B/R/COU, сброс угла, бипер при R→0 |
| FSM в главном цикле | ✅ | Jog только в `STATE_MANUAL` |

Подробности этапов — в [STAGE_2_2.md](02_Документация/STAGE_2_2.md).

---

## В разработке / заглушки

| Компонент | Статус | Примечание |
|-----------|--------|------------|
| Синхронная подача (Sync) | 🟡 | LCD и API старта; полный цикл — нет |
| Нарезание резьбы (Thread) | 🟡 | Таблица шагов, выбор pitch; цикл — частично |
| Фаска, конус, сфера | 🟡 | Enter/exit + API; `*_process()` пустые |
| Шлифовка (Grind) | 🟡 | Заготовка цикла; проблемы переходов FSM |
| Подрежимы EXT / INT | 🔴 | Только метка на LCD; joy→cycle не подключён |
| `fsm_cycle_process()` | 🔴 | Не вызывается из `loop()` |
| G-код, веб-интерфейс | 📋 | В планах |

Анализ конфликтов ввода и известных дыр — [statustatealarm.md](statustatealarm.md).

---

## Режимы (селектор Pos 1…8)

| Pos | Режим | Подача pot | Статус |
|-----|-------|------------|--------|
| 1 | Асинхронная подача | мм/мин | ✅ ручной jog |
| 2 | Синхронная подача | мм/об | 🟡 |
| 3 | Нарезание резьбы | — (`F:---`) | 🟡 |
| 4 | Фаска | мм/мин | 🟡 |
| 5 | Конус | мм/мин | 🟡 |
| 6 | Сфера | мм/мин | 🟡 |
| 7 | Делительная головка | мм/мин | ✅ |
| 8 | Шлифовка | мм/мин | 🟡 |

Подрежимы **MAN / EXT / INT** (A13–A15): сейчас влияют только на отображение LCD.

---

## Аппаратная платформа

| Компонент | Назначение |
|-----------|------------|
| Arduino Mega 2560 | 16 МГц, 256 КБ Flash, 8 КБ RAM, 4 КБ EEPROM |
| LCD 2004 | Параллельный интерфейс (без I2C) |
| Шаговые X/Z + A4988 | STEP 48/49, DIR 43/44 |
| Энкодер шпинделя | D20/D21, квадратурный (PPR в меню) |
| РГИ | D18/D19 (INT), ось D2/D3, масштаб D14/D15 |
| Джойстик | A8–A11 + Rapid A12 |
| Потенциометр | A7 |
| Лимиты + LED | D22/24/26/28, LED D23/25/27/29 |
| Селектор режима | D30–D37 (8 позиций) |
| E-Stop | D40 |
| Бипер | D16 |

<details>
<summary><strong>Таблица пинов (из hal_pins.h)</strong></summary>

```
Шаговые двигатели:
  STEP_X → 48    DIR_X → 44
  STEP_Z → 49    DIR_Z → 43

LCD 2004:
  RS → 8   EN → 9   D4–D7 → 10–13

Энкодер шпинделя:  D20 (A), D21 (B)
РГИ:               D18 (A), D19 (B)
Ось РГИ:           Z → D2, X → D3
Масштаб РГИ:       1:1 → D15, 0.01 мм → D14

Меню LCD:          A0–A4 (Down/Up/Right/Left/Select)
Джойстик:          A8–A12
Подрежимы:         A13 (INT), A14 (MAN), A15 (EXT)
Потенциометр:      A7

Селектор режима:   D30–D37
Лимиты:            LimL 22, LimF 24, LimR 26, LimRe 28
LED лимитов:       29, 25, 27, 23

E-Stop → 40    Бипер → 16    Spindle PWM → 6    Coolant → 17
```

Схема Wokwi: [06_Схемы_и_чертежи/wokwi/diagram.json](06_Схемы_и_чертежи/wokwi/diagram.json)  
Дубликат в исходниках: `01_Исходный_код/ELS_AlexA_V2.1/diagram.json`

</details>

---

## Структура репозитория

```
ELS_AlexA_V2_1/
├── 01_Исходный_код/ELS_AlexA_V2.1/   # Прошивка (.ino + src/)
│   ├── ELS_AlexA_V2.1.ino
│   ├── src/
│   │   ├── config/                   # config.h, EEPROM, backlash, feed
│   │   ├── core/
│   │   │   ├── hal/                  # Пины, таймеры, прерывания
│   │   │   ├── motion/               # planner, stepper_gen, jog, limits
│   │   │   ├── process/              # spindle, estop, coolant
│   │   │   ├── ui/                   # LCD, кнопки, энкодеры, меню, pot
│   │   │   ├── fsm/                  # FSM manager, cycles
│   │   │   └── debug/                # Serial, telemetry, $ CLI
│   │   └── modes/                    # async, sync, thread, …, divider, grind
│   └── libraries/                    # EncButton, GyverIO
├── 02_Документация/                  # ARCHITECTURE, API, HARDWARE, MODES, DEBUG
├── 03_Правила_Cursor/
├── 04_Промты_для_Cursor/
├── 05_Навыки_Cursor/
├── 06_Схемы_и_чертежи/
├── .cursor/                          # rules/ + skills/ для Cursor AI
└── README.md
```

---

## Быстрый старт

### 1. Клонирование

```bash
git clone https://github.com/AlexAlHomeLab/ELS_AlexA_V2_1.git
cd ELS_AlexA_V2_1/01_Исходный_код/ELS_AlexA_V2.1
```

### 2. Библиотеки

Установить через Arduino Library Manager или положить в `libraries/`:

| Библиотека | Назначение |
|------------|------------|
| **EncButton** ≥ 3.7.3 | Кнопки и энкодеры (GyverLibs) |
| **GyverIO** | Зависимость EncButton |
| **LiquidCrystal** | LCD 2004 (встроена в Arduino AVR) |

EncButton и GyverIO уже лежат в `libraries/` проекта.

### 3. Сборка

1. Открыть `ELS_AlexA_V2.1.ino` в Arduino IDE
2. Плата: **Arduino Mega 2560**
3. Загрузить прошивку

### 4. Настройка

Параметры станка — в **меню EEPROM** (долгое нажатие Select) или через Serial `$`.

Главный конфиг: `src/config/config.h` — тайминги, уровень отладки, `FIRMWARE_STAGE`.

```cpp
#define FIRMWARE_NAME   "ELS AlexA V2.1"
#define FIRMWARE_STAGE  "Stage 2.2f"
#define SERIAL_BAUD     115200
#define DEBUG_ENABLED   1
```

Шаги/мм, max/rapid/accel, PPR шпинделя — в EEPROM (`config_machine`), не в `#define`.

---

## Управление

### Органы ввода

| Элемент | Действие |
|---------|----------|
| Селектор Pos 1–8 | Выбор режима |
| Sub MAN / EXT / INT | Подрежим (пока только LCD) |
| Джойстик L/R/U/D | Ручная подача по осям |
| Rapid (A12) | Ускоренная подача, без clamp soft limits |
| РГИ | Точное позиционирование выбранной оси |
| Pot (A7) | Скорость подачи (async или sync — по режиму) |
| LimL / LimF / LimR / LimRe | Лимит / сброс / обнуление оси (hold) |
| Rapid + Lim click | GO_LIM — быстрый подвод к лимиту |
| LCD Up/Down/Left/Right | Меню / divider A/B |
| Select short | Подтверждение / сброс угла (divider) |
| Select long (~600 ms) | Вход в меню EEPROM |
| E-Stop (D40) | Аварийная остановка |

### Лимиты

- **Клик** — установить или сбросить программный лимит (LED)
- **Hold** — обнулить координату оси и пересчитать лимиты
- **Joy к лимиту + отпускание** — latch: движение к лимиту на pot-speed

---

## Архитектура

```
┌─────────────────────────────────┐
│  Modes (async, sync, divider…)  │
├─────────────────────────────────┤
│  FSM (MANUAL, CYCLE, ERROR…)    │
├─────────────────────────────────┤
│  UI (LCD, joy, MPG, menu, pot)  │
├─────────────────────────────────┤
│  Motion (planner, DDS, backlash)│
├─────────────────────────────────┤
│  Process (spindle, estop)       │
├─────────────────────────────────┤
│  HAL (pins, timers, ISR)        │
└─────────────────────────────────┘
```

Принципы: модульность, FSM вместо длинных `if-else`, HAL для переносимости, минимум работы в ISR, отладка через `DBG_*` с условной компиляцией.

Подробнее: [ARCHITECTURE.md](02_Документация/ARCHITECTURE.md).

---

## Документация

| Документ | Описание |
|----------|----------|
| [ARCHITECTURE.md](02_Документация/ARCHITECTURE.md) | Слои и потоки данных |
| [API.md](02_Документация/API.md) | Справочник функций |
| [HARDWARE.md](02_Документация/HARDWARE.md) | Аппаратура (⚠️ часть пинов устарела — см. `hal_pins.h`) |
| [MODES.md](02_Документация/MODES.md) | Режимы и циклы (целевое ТЗ) |
| [DEBUG.md](02_Документация/DEBUG.md) | Serial, `$` CLI, телеметрия |
| [STAGE_2_2.md](02_Документация/STAGE_2_2.md) | Этапы 2.2a–f |
| [statustatealarm.md](statustatealarm.md) | Матрица состояний ввода |

---

## Разработка с Cursor AI

В `.cursor/` — правила (`rules/`) и скилы (`skills/`) для отладки LCD, jog, РГИ, лимитов, backlash, divider и др.

Дополнительно в репозитории:

- `03_Правила_Cursor/` — архитектура, безопасность, отладка
- `04_Промты_для_Cursor/` — поэтапные промты разработки
- `05_Навыки_Cursor/` — code review, performance, debug

---

## Источники и вдохновение

GRBL (planner, look-ahead), Marlin (дуги, backlash), LinuxCNC (шпиндель, резьба), FastAccelStepper (DDS), EncButton, NanoELS.

---

## Планируемые улучшения

- Полные циклы EXT/INT и wiring `fsm_cycle_process`
- Доработка Sync, Thread, Chamfer, Cone, Sphere, Grind
- Дюймовые резьбы, G-код, Serial-управление
- ESP32 / STM32, веб-интерфейс

---

## Вклад

1. Fork репозитория
2. Ветка: `git checkout -b feature/my-feature`
3. Commit и Pull Request

---

## Лицензия

MIT License

---

<p align="center">
  <sub>Built with Cursor AI · © 2026 ELS AlexA Project</sub>
</p>
