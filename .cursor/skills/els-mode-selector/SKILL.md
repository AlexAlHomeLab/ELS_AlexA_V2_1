---
name: els-mode-selector
description: >-
  Спецификация и реализация селектора режимов (MODE 1–8), MODE_OFF, soft-latch
  EN_X и экрана CNC OFF для ELS AlexA V2.1 на Arduino Mega 2560. Используй при
  разработке, отладке и ревью ui_switches, motor_en, FSM mode, LCD MODE_OFF;
  когда пользователь упоминает селектор режимов, MODE_OFF, CNC OFF, EN_X,
  enable двигателя X или EN_X_SOFT_LATCH.
---

# Селектор режимов и EN_X (ELS AlexA V2.1)

## Спецификация (ТЗ)

## Селектор режимов
Позиции 1–8 (пины D30–D37, remap `BOARD_MODE_PIN_REMAP`) задают режим FSM.
Положение, когда **все пины режима неактивны** (сырой скан `0`) — **MODE_OFF**.
Вход в MODE_OFF — только после удержания `scan==0` в течение `MODE_OFF_DEBOUNCE_MS`
(по умолчанию 150 мс), чтобы краткий разрыв контакта при повороте 1↔8 не давал OFF.
Выход из MODE_OFF — сразу при появлении любого валидного пина режима.
Скан 1–8 / OFF: `digitalRead` MODE_PIN (галетник), не EncButton `pressing()`.
Boot: `mode_off=0`, таймер debounce не стартуем (старт только в poll).
В MODE_OFF блокируются джойстик и РГИ полностью.
На дисплее в MODE_OFF: строка 1 — `CNC OFF` по центру; строка 2 пустая;
строка 3 — обороты шпинделя по шаблону `RPM 1000` по центру;
строка 4 — короткая версия прошивки `V2.1 Stage 2.2f` по центру.
Логический режим для FSM при MODE_OFF остаётся защёлкнутым (последний 1–8);
UI MODE_OFF определяется по сырому скану.

## Soft-latch EN оси X
Настройка дефайн `EN_X_SOFT_LATCH`: при включении станка отключить enable для двигателя оси X;
если enable отключен, то перед включением любого движения по оси X enable включается и защёлкивается;
если селектор режимов в положении когда отключены все пины — enable X отключается.
Полярность EN аналогична STEP: HIGH = мотор под током, LOW = выкл.
Пин EN_X = 46, EN_Z = 45 (объявить на будущее **без** инициализации).
Функционал E-Stop → EN пока без реализации (заготовки закомментированы).
После выхода из MODE_OFF EN остается off до первого движения по X. Z не меняется.

## Быстрый старт для агента

1. Прочитай [reference.md](reference.md) — карта файлов и API.
2. Правь ui_switches / motor_en / HAL EN / LCD MODE_OFF; не трогай Z EN и E-Stop без запроса.
3. Перед правками: кратко опиши **что** и **зачем**; жди подтверждения при нетривиальной логике.
4. После правок пройди чеклист ниже.
5. Joy/РГИ блок — skills `els-joy-feed`, `els-rgi-mpg`; LCD — `els-display-menu`.

## Карта кода (кратко)

| Слой | Файл | Роль |
|------|------|------|
| HAL | `hal_pins.h` | `EN_X_PIN` 46, `EN_Z_PIN` 45, макросы EN_X |
| HAL | `hal_init.cpp` | init EN_X OUTPUT+LOW при `EN_X_SOFT_LATCH` |
| Конфиг | `config_defs.h` | `EN_X_SOFT_LATCH`, `EN_X_SETTLE_MS`, `MODE_OFF_DEBOUNCE_MS` |
| Конфиг | `config.h` | `FIRMWARE_LCD_VER` |
| Логика | `motor_en.cpp` | ensure / release / latch |
| Motion | `stepper_gen.cpp` | `motor_en_x_ensure` перед стартом/retarget с ΔX |
| UI | `ui_switches.cpp` | `mode_raw` / MODE_OFF → release EN |
| Motion | `motion_jog.cpp` | early return joy/MPG при MODE_OFF |
| LCD | `ELS_AlexA_V2.1.ino` | экран CNC OFF |
| FSM | `fsm_manager.cpp` | режим из защёлкнутого `sw.mode` (1–8) |

## MODE_OFF

| Условие | Действие |
|---------|----------|
| `scan==0` непрерывно ≥ `MODE_OFF_DEBOUNCE_MS` | `mode_off=1`, `motor_en_x_release()`, стоп jog |
| `scan==0` короче debounce (переход 1–8) | MODE_OFF **не** включать; latch режима прежний |
| `scan > 0` | `mode_off=0` сразу |
| Joy / РГИ / go_lim | не работают при `mode_off` |
| LCD | экран CNC OFF (см. ниже) |
| FSM `sw.mode` | последний валидный 1–8 (latch), не менять на 0 |
| Boot | `mode_off=0`, `mode_off_raw_ms=0`; debounce только в poll (не на init) |

## Экран MODE_OFF (LCD 2004)

| Строка | Содержимое |
|--------|------------|
| 0 | `CNC OFF` по центру |
| 1 | пустая |
| 2 | `RPM N` по центру (`spindle_get_rpm()`, шаблон `RPM 1000`) |
| 3 | `FIRMWARE_LCD_VER` (`V2.1 Stage 2.2f`) по центру |

Приоритет: меню > MODE_OFF > обычный главный / divider.

## Soft-latch EN_X

| Событие | EN_X |
|---------|------|
| Boot (`EN_X_SOFT_LATCH=1`) | LOW (off), latch=0 |
| Первое движение с ΔX ≠ 0 | HIGH + latch=1 (+ settle) |
| Дальнейшие ходы (в т.ч. только Z) | не гасить |
| MODE_OFF | LOW, latch=0 |
| После MODE_OFF | остаётся off до следующего ΔX |
| E-Stop | не трогать (пока) |

Точка ensure: `dds_motion_start` / `dds_motion_jog_retarget` при движении по X (в т.ч. backlash на AXIS_X).

## Чеклист при правках

```
- [ ] EN_X_PIN 46, EN_Z_PIN 45 без init Z
- [ ] Полярность HIGH=on как STEP
- [ ] EN_X_SOFT_LATCH вырезает код при 0
- [ ] Boot: EN off
- [ ] Ensure только при ΔX / backlash X
- [ ] MODE_OFF: release EN + блок joy и РГИ
- [ ] MODE_OFF: debounce `MODE_OFF_DEBOUNCE_MS` при входе; выход сразу
- [ ] LCD: CNC OFF / RPM / FIRMWARE_LCD_VER
- [ ] E-Stop → EN не активен (закомментирован)
- [ ] Минимальный diff
```

## Типичные ошибки

1. **Гасить EN после каждого jog** — нарушает защёлку; гасить только MODE_OFF (и будущее E-Stop).
2. **FSM mode=0** — ломает индекс режима; MODE_OFF только флаг/`mode_raw`.
3. **Init EN_Z** — не делать без запроса.
4. **Ensure в ISR** — запрещено; только до `cli()` в start/retarget.
5. **Блок только оси X** — в MODE_OFF блок joy и РГИ полностью.
6. **MODE_OFF без debounce** — мигание CNC OFF / EN при повороте селектора между 1–8.
