# Таблица соответствия пинов ELS AlexA V2.1 и Digital_Feed_7e2

Источники:
- ELS: `01_Исходный_код/ELS_AlexA_V2.1/src/core/hal/hal_pins.h`, `config_board.h`
- 7e2: `Digital_Feed_7e2/Digital_Feed_7e2.ino` (макросы PORTx)

Платформа: **Arduino Mega 2560** (ATmega2560).

**Общее:** входы — активный **LOW** (подтяжка к +5 В), кроме выходов моторов, LCD, LED и PWM.

---

## Шаговые двигатели и тахо

| Пин | AVR | Функция ELS | Функция 7e2 | Совпадение |
|-----|-----|-------------|-------------|------------|
| D43 | PL6 | DIR Z | Motor Z DIR | ✓ |
| D44 | PL5 | DIR X | Motor X DIR | ✓ |
| D45 | PL4 | — | Motor Z **Enable** | только 7e2 |
| D46 | PL3 | — | Motor X **Enable** | только 7e2 |
| D47 | PL2 | — | (вход, не используется) | только 7e2 |
| D48 | PL1 | STEP X | Motor X STEP | ✓ |
| D49 | PL0 | STEP Z | Motor Z STEP | ✓ |
| D42 | PL7 | LED тахометра | Tacho LED | ✓ |

---

## LCD, звук, шпиндель, СОЖ

| Пин | AVR | Функция ELS | Функция 7e2 | Совпадение |
|-----|-----|-------------|-------------|------------|
| D8 | PH5 | LCD RS | LCD RS (закомм., I2C) | пин ✓, обвязка разная |
| D9 | PH6 | LCD EN | LCD EN (закомм., I2C) | пин ✓, обвязка разная |
| D10 | PB4 | LCD D4 | LCD D4 (закомм.) | пин ✓ |
| D11 | PB5 | LCD D5 | LCD D5 (закомм.) | пин ✓ |
| D12 | PB6 | LCD D6 | LCD D6 (закомм.) | пин ✓ |
| D13 | PB7 | LCD D7 | LCD D7 (закомм.) | пин ✓ |
| D16 | PH1 | Бипер (активный LOW) | Beeper | ✓ |
| D6 | PH3 | PWM шпинделя | — | только ELS |
| D17 | PH0 | СОЖ (coolant) | — | только ELS |
| D40 | PG1 | E-STOP (кнопка) | — | только ELS |

> В 7e2 дисплей реально на **I2C 0x27**; параллельные D8–D13 в коде закомментированы.

---

## Энкодеры и РГИ (MPG)

| Пин | AVR | Функция ELS | Функция 7e2 | Совпадение |
|-----|-----|-------------|-------------|------------|
| D20 | PD1 | Энкодер шпинделя, канал A | Enc Ch B | пин ✓, A/B в коде зеркально |
| D21 | PD0 | Энкодер шпинделя, канал B | Enc Ch A | пин ✓, A/B в коде зеркально |
| D18 | PD3 | РГИ, канал A | Hand Ch B | пин ✓, A/B в коде зеркально |
| D19 | PD2 | РГИ, канал B | Hand Ch A | пин ✓, A/B в коде зеркально |
| D2 | PE4 | Селектор оси РГИ: **Z** (`HAND_AXIS_Z_PIN`) | Селектор оси РГИ: **X** | пин ✓, **Z/X перепутаны** |
| D3 | PE5 | Селектор оси РГИ: **X** (`HAND_AXIS_X_PIN`) | Селектор оси РГИ: **Z** | пин ✓, **Z/X перепутаны** |
| D14 | PJ0 | Масштаб РГИ x1 / 1 шаг | Scale x1 | ✓ |
| D15 | PJ1 | Масштаб РГИ x10 / 0.01 мм | Scale x10 | ✓ |
| A7 | PF7 (ADC) | Потенциометр подачи | ADC Feed (A7) | ✓ |

---

## Кнопки меню (PORT F)

| Пин | AVR | Функция ELS | Функция 7e2 | Совпадение |
|-----|-----|-------------|-------------|------------|
| A0 | PF0 | Меню: Down | Key Down | ✓ |
| A1 | PF1 | Меню: Up | Key Up | ✓ |
| A2 | PF2 | Меню: Right | Key Right | ✓ |
| A3 | PF3 | Меню: Left | Key Left | ✓ |
| A4 | PF4 | Меню: Select | Key Select | ✓ |

---

## Джойстик и Rapid (PORT K, младшие биты)

| Пин | AVR | Функция ELS | Функция 7e2 | Совпадение |
|-----|-----|-------------|-------------|------------|
| A8 | PK0 | Джойстик Left | Joy Left | ✓ |
| A9 | PK1 | Джойстик Right | Joy Right | ✓ |
| A10 | PK2 | Джойстик Up (вперёд) | Joy Up | ✓ |
| A11 | PK3 | Джойстик Down (назад) | Joy Down | ✓ |
| A12 | PK4 | Rapid (ускоренная подача) | Button Rapid | ✓ |

---

## Подрежим Int / Man / Ext (PORT K, старшие биты)

| Пин | AVR | Функция ELS | Функция 7e2 | Совпадение |
|-----|-----|-------------|-------------|------------|
| A13 | PK5 | **Int** (`SUB_INT_PIN`) | **Man** | пин ✓, **Int/Man перепутаны** |
| A14 | PK6 | **Man** (`SUB_MAN_PIN`) | **Int** | пин ✓, **Int/Man перепутаны** |
| A15 | PK7 | Ext (`SUB_EXT_PIN`) | Ext | ✓ |

---

## Селектор режима 8 позиций (PORT C, D30–D37)

| Пин | AVR | ELS (`MODE_PIN_n`) | Режим в 7e2 (сырой) | Режим ELS (после remap) |
|-----|-----|--------------------|---------------------|-------------------------|
| D30 | PC0 | MODE_PIN_1 | Divider | Divider |
| D31 | PC1 | MODE_PIN_2 | Sphere | Grind |
| D32 | PC2 | MODE_PIN_3 | Reserve | Cone |
| D33 | PC3 | MODE_PIN_4 | Cone R | Sphere |
| D34 | PC4 | MODE_PIN_5 | Cone L | Thread |
| D35 | PC5 | MODE_PIN_6 | aFeed (мм/мин) | Chamfer |
| D36 | PC6 | MODE_PIN_7 | Feed (мм/об) | Async |
| D37 | PC7 | MODE_PIN_8 | Thread | Sync |

Пины совпадают с 7e2. Логика режимов в ELS переназначена таблицей `BOARD_MODE_PIN_REMAP` в `config_board.h` (набор режимов другой: Chamfer, Grind вместо Cone R, Reserve).

---

## Лимиты — кнопки (PORT A, входы)

| Пин | AVR | Функция ELS | Функция 7e2 | Совпадение |
|-----|-----|-------------|-------------|------------|
| D22 | PA0 | Лимит **Rear** (задний) | Limit Rear | ✓ |
| D24 | PA2 | Лимит **Front** (передний) | Limit Front | ✓ |
| D26 | PA4 | Лимит **Right** (правый) | Limit Right | ✓ |
| D28 | PA6 | Лимит **Left** (левый) | Limit Left | ✓ |

---

## Лимиты — LED (PORT A, выходы)

| Пин | AVR | Функция ELS | Функция 7e2 | Совпадение |
|-----|-----|-------------|-------------|------------|
| D23 | PA1 | LED Rear | Limit Rear LED | ✓ |
| D25 | PA3 | LED Front | Limit Front LED | ✓ |
| D27 | PA5 | LED Right | Limit Right LED | ✓ |
| D29 | PA7 | LED Left | Limit Left LED | ✓ |

---

## Сводка расхождений

| Тип | Пины | Суть |
|-----|------|------|
| Только ELS | D6, D17, D40 | PWM шпинделя, СОЖ, E-STOP |
| Только 7e2 | D45, D46, D47 | Enable Z/X, резерв PL2 |
| Те же пины, разная логика | D2, D3 | Ось РГИ: Z/X названы наоборот |
| Те же пины, разная логика | A13, A14 | Int/Man названы наоборот |
| Те же пины, разные метки в коде | D18–D21 | Каналы A/B энкодеров зеркально |
| Те же пины, разная реализация | D8–D13 | ELS: параллельный LCD; 7e2: I2C |
| Те же пины, другая карта режимов | D30–D37 | Remap режимов в ELS |

---

## Макросы ELS (`hal_pins.h`) — быстрый справочник

| Макрос | Пин |
|--------|-----|
| `STEP_X_PIN` | 48 |
| `DIR_X_PIN` | 44 |
| `STEP_Z_PIN` | 49 |
| `DIR_Z_PIN` | 43 |
| `LCD_RS_PIN` … `LCD_D7_PIN` | 8–13 |
| `BUZZER_PIN` | 16 |
| `POT_FEED_PIN` | A7 |
| `ENC_SPINDLE_A_PIN` / `ENC_SPINDLE_B_PIN` | 20 / 21 |
| `ENC_HAND_A_PIN` / `ENC_HAND_B_PIN` | 18 / 19 |
| `HAND_AXIS_Z_PIN` / `HAND_AXIS_X_PIN` | 2 / 3 |
| `HAND_SCALE_X1_PIN` / `HAND_SCALE_X10_PIN` | 14 / 15 |
| `BTN_DOWN_PIN` … `BTN_SELECT_PIN` | A0–A4 |
| `JOY_LEFT_PIN` … `JOY_RAPID_PIN` | A8–A12 |
| `SUB_INT_PIN` / `SUB_MAN_PIN` / `SUB_EXT_PIN` | A13–A15 |
| `MODE_PIN_1` … `MODE_PIN_8` | 30–37 |
| `LIMIT_LEFT_PIN` / `LIMIT_FRONT_PIN` / `LIMIT_RIGHT_PIN` / `LIMIT_REAR_PIN` | 28 / 24 / 26 / 22 |
| `LED_LIMIT_LEFT_PIN` … `LED_LIMIT_REAR_PIN` | 29 / 25 / 27 / 23 |
| `LED_TACHO_PIN` | 42 |
| `ESTOP_BTN_PIN` | 40 |
| `SPINDLE_PWM_PIN` | 6 |
| `COOLANT_PIN` | 17 |
