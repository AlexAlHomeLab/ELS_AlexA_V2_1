# LiquidCrystalSafeAsync v2 — план работ

Ориентир: skill [lcd-smart-write](../.cursor/skills/lcd-smart-write/SKILL.md), [reference](../.cursor/skills/lcd-smart-write/reference.md), [examples](../.cursor/skills/lcd-smart-write/examples.md).

**Цель v2:** smart write per-char (read/compare/write/verify), полностью неблокирующий runtime для CNC, удаление устаревшего `_buffer` / `SYNC_CELL`.

**Целевая платформа:** ATmega2560 @ 16 MHz, HD44780 4-bit (основной), опционально 8-bit и RW.

---

## Фаза 0 — подготовка

- [ ] Зафиксировать baseline v1: размер Flash/RAM (`test.ino`, Mega 2560, blind и RW)
- [ ] Обновить `library.properties`: `version=2.0.0`, описание под smart write
- [ ] Синхронизировать копии: `src/`, `wokwi/LiquidCrystalSafeAsync.*`, убрать дубль `scr/` если не нужен
- [ ] Добавить `LCD_HAS_RW` в `lcd_config.h` (Arduino) и `platformio.ini` (build_flags)

---

## Фаза 1 — удалить устаревшее (breaking)

- [ ] Удалить `_buffer[LCD_MAX_ROWS * LCD_MAX_COLS]` из `.h` / `.cpp`
- [ ] Удалить `LCD_OP_SYNC_CELL` (0x06)
- [ ] Удалить `syncAll()`, `queuePushSyncCell()`
- [ ] Удалить `LCD_STATE_WAIT_VERIFY` в текущем виде (заменить на `_smartPhase`)
- [ ] Убрать инициализацию `_buffer` из конструкторов
- [ ] Обновить `keywords.txt` — убрать `syncAll`, добавить `setSmartWrite`

---

## Фаза 2 — Smart Write API

- [ ] Добавить `LCD_OP_DATA_SMART` (0x08)
- [ ] `write()`: при `_hasRW` → `LCD_OP_DATA_SMART`, иначе → `LCD_OP_DATA`
- [ ] Реализовать `advanceCursor()` — только RAM (`_cursorCol`, `_cursorRow` по `_displaymode`)
- [ ] `setCursor()` — обновляет RAM-курсор + `LCD_OP_SETCURSOR` в очередь
- [ ] `clear()` / `home()` — слепая команда + сброс `_cursorCol=0`, `_cursorRow=0`
- [ ] `setSmartWrite(bool)` / `isSmartWrite()` — runtime переключение smart ↔ blind (marquee)
- [ ] Документировать: smart по умолчанию при RW; blind forced при >50% изменений за кадр

---

## Фаза 3 — фазовая state machine (`_smartPhase`)

Enum и volatile-поля:

- [ ] `_smartPhase` (enum, см. reference)
- [ ] `_smartAddr` — DDRAM-адрес текущего символа
- [ ] `_currentData` — эталонный символ из `write()`
- [ ] `_retryCount` — попытки записи
- [ ] Сохранить `_cursorCol`, `_cursorRow` для `advanceCursor()`

Фазы (один шаг за вызов `processQueue()`):

- [ ] `SMART_SEND_ADDR` — `sendByte(SETDDRAMADDR|addr)` → `WAIT_EXECUTION` 40 µs
- [ ] `SMART_VERIFY_ADDR` — `verifyCursor(addr)`; fail → retry/fail
- [ ] `SMART_READ_COMPARE` — `readByte(false)`; eq → `advanceCursor`, done; ne → write
- [ ] `SMART_WRITE` — `sendByte(ch)` → `WAIT_EXECUTION` 40 µs
- [ ] `SMART_READDR` — `sendByte(SETDDRAMADDR|addr)` → `WAIT_EXECUTION` 40 µs
- [ ] `SMART_VERIFY_WRITE` — `verifyCursor` + `readByte(false)`; eq → done; ne → retry
- [ ] `processSmartPhase()` — диспетчер фаз, вызывается из `executeCommand` / после `WAIT_EXECUTION`

Правила AC (обязательно):

- [ ] `SETDDRAMADDR` + `verifyCursor` перед **каждой** read/write
- [ ] После `sendByte(ch)` — **не** `verifyCursor(addr)` сразу; сначала `SMART_READDR`
- [ ] После `verifyCursor()` — **не** лишний `readByte(true)` перед `readByte(false)`
- [ ] Не полагаться на AC++ между ячейками (ISR между вызовами `processQueue()`)

Retry и ошибки:

- [ ] `smartWriteFailedOrRetry()` — до `_maxRetries`, иначе `_errorCount++`, `advanceCursor()`, next op
- [ ] `getErrorCount()` / `resetErrorCount()` — без изменений API

---

## Фаза 4 — неблокирующий CPU (CNC)

- [ ] Убрать **все** `delaySafe(40)` из runtime (`executeCommand`, smart-фазы, retry)
- [ ] Все паузы 40 µs / 1520 µs — только `_waitUntil = micros() + N` + `LCD_STATE_WAIT_EXECUTION`
- [ ] После `WAIT_EXECUTION` — возврат в нужную `_smartPhase`, не в `IDLE` преждевременно
- [ ] Один шаг шины (или одна логическая фаза) на вызов `processQueue()` → `return`
- [ ] `flush()` — оставить для setup/отладки; задокументировать «не вызывать в CNC loop»
- [ ] `processQueue()` return value: оставить остаток µs до `_waitUntil` (уже есть)

Оптимизация коротких задержек (опционально, низкий приоритет):

- [ ] `fastPulseEnable()`: `delaySafe(1)` → `__delay_cycles(N)` / NOP (~450 ns EN pulse)
- [ ] `readByte()`: минимизировать `delaySafe(1)` после pulse
- [ ] **Не** опрашивать BF на каждый символ (4-bit: read status ~50 µs > blind 40 µs)
- [ ] BF или редкий poll — только для `clear`/`home` (1520 µs), если понадобится позже

---

## Фаза 5 — blind-режим и `#if LCD_HAS_RW`

- [ ] `#if defined(LCD_HAS_RW)` — исключить из blind-сборки: `readByte`, `verifyCursor`, smart-фазы, `_rw_*`
- [ ] Blind `LCD_OP_DATA` — путь как сейчас: `sendByte` + `_waitUntil` 40 µs (уже async)
- [ ] Конструктор без RW — без полей/кода RW (~500 Flash / ~90 RAM экономии, см. reference)
- [ ] PlatformIO env: `megaatmega2560_blind` (-ULCD_HAS_RW) и `megaatmega2560_smart` (-DLCD_HAS_RW)

---

## Фаза 6 — исправление известных багов v1

- [ ] Удалить баг `SYNC_CELL` / `WAIT_VERIFY`: verify после write без `SETDDRAMADDR` (устраняется заменой на smart phases)
- [ ] Убрать лишний `readByte(true)` в старом `WAIT_VERIFY` (строка ~516 в v1 `.cpp`)
- [ ] Проверить `queuePush`: tail/count при переполнении очереди
- [ ] `begin()`: blocking delays только в init — допустимо

---

## Фаза 7 — примеры и тесты

### `examples/test/test.ino`

- [ ] Один `lcd.processQueue()` за итерацию `loop()` — без `flush()` в runtime
- [ ] Вариант сборки с RW (Wokwi: RS=30, RW=31, EN=32, D4–D7=33–36)
- [ ] Статусная строка (smart): редкие изменения цифр — замер времени кадра
- [ ] Marquee (blind forced): `setSmartWrite(false)` на строке 1
- [ ] Счётчик `getErrorCount()` — вывод на LCD или Serial
- [ ] Нагрузка CNC (Timer1/3/5) — без регрессии шаговиков при активном LCD

### Wokwi

- [ ] Обновить `wokwi/sketch.ino` / `diagram.json` — RW pin при smart-тесте
- [ ] Синхронизировать `wokwi/LiquidCrystalSafeAsync.cpp` с `src/`

### Критерии приёмки v2

- [ ] Blind: полный кадр 20×4 не блокирует loop дольше ~50 µs за вызов `processQueue()`
- [ ] Smart: compare-match пропускает write; verify после write с `SETDDRAMADDR`
- [ ] Искусственный ISR на время nibble → retry срабатывает, экран восстанавливается
- [ ] Flash/RAM: blind-сборка меньше v1 на ~90 RAM (без `_buffer`) + `#if LCD_HAS_RW`
- [ ] Регрессия: `USE_LCD_STANDARD` в `test.ino` по-прежнему собирается для сравнения

---

## Фаза 8 — документация

- [ ] `README.md` — v2: smart write, убрать «буфер экрана», CNC loop pattern
- [ ] README: таблица режимов Smart / Blind / Blind forced
- [ ] README: `setSmartWrite()`, `flush()` только в setup
- [ ] CHANGELOG.md — breaking changes v1 → v2
- [ ] Комментарии в `.h` — публичный API

---

## Фаза 9 — опционально (v2.1+)

Низкий приоритет, не блокирует релиз v2:

- [ ] Ассемблерные вставки: `fastPulseEnable`, `fastGetData` при фиксированных пинах (`LCD_FAST_ASM`)
- [ ] Шаблон `template<bool HasRW>` вместо `#if LCD_HAS_RW`
- [ ] Очередь: приоритет CMD над DATA при переполнении
- [ ] Метрики: счётчик skipped writes (compare match), среднее время символа

---

## Чеклист готовности к релизу 2.0.0

```
[ ] Все пункты фаз 1–4 выполнены
[ ] test.ino проходит на Wokwi (blind + smart)
[ ] Нет delaySafe(40) в runtime
[ ] _buffer / SYNC_CELL / syncAll удалены
[ ] library.properties version=2.0.0
[ ] README и CHANGELOG обновлены
[ ] Размеры Flash/RAM задокументированы
```

---

## Порядок реализации (рекомендуемый)

1. Фаза 1 (удаление) + Фаза 4 для blind-пути (убрать blocking delays где остались)
2. Фаза 3 (smart phases) + Фаза 2 (API)
3. Фаза 5 (`#if LCD_HAS_RW`)
4. Фаза 7 (тесты) → Фаза 8 (доки) → релиз

---

## Ссылки

| Документ | Содержание |
|----------|------------|
| `.cursor/skills/lcd-smart-write/SKILL.md` | Алгоритм, чеклист, антипаттерны |
| `.cursor/skills/lcd-smart-write/reference.md` | AC, тайминги, неблокирующий CPU, память |
| `.cursor/skills/lcd-smart-write/examples.md` | Фрагменты .cpp, CNC loop |
| `docs/datasheet.md` | HD44780 тайминги |
