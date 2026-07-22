# Подхват резьбы (COU = ΔZ) — справочник

## Статус

| Элемент | Статус |
|---------|--------|
| COU как обороты:фаза | **реализовано** (`els-divider`) |
| COU как ΔZ | **не реализовано** |
| `mode_thread` интеграция | **заготовка** |
| Latch C_ref + Z_ref | **не реализовано** |

## Формулы

```c
/* Согласовано с INT0 Rising на B (×1 к PPR), как mode_divider */
#define TICKS_PER_REV  ((int32_t)SPINDLE_ENCODER_PPR)

/* Опорная точка (после Select / latch) */
int32_t C_ref;   /* spindle_get_count() */
int32_t Z_ref;   /* motion_get_pos_steps(AXIS_Z) */
float   P_mm;    /* шаг резьбы, мм/об */

/* Текущие */
int32_t C = spindle_get_count();
int32_t Z = motion_get_pos_steps(AXIS_Z);
float   spm = STEPS_PER_MM_Z;

/* Целевая Z по резьбе (мм) */
float dc = (float)(C - C_ref);
float z_target_mm = (float)Z_ref / spm + dc * P_mm / (float)TICKS_PER_REV;

/* Дельта для оператора */
float z_current_mm = (float)Z / spm;
float delta_z_mm   = z_target_mm - z_current_mm;

/* Эквивалент в шагах */
int32_t delta_z_steps = (int32_t)(delta_z_mm * spm + (delta_z_mm >= 0 ? 0.5f : -0.5f));
```

### Упрощённая форма (всё в шагах)

```c
int32_t pitch_steps_per_rev = (int32_t)(P_mm * spm + 0.5f);
int32_t z_target = Z_ref + (int32_t)((int64_t)(C - C_ref) * pitch_steps_per_rev / TICKS_PER_REV);
int32_t delta_z_steps = z_target - Z;
```

Использовать `int64_t` для промежуточного произведения на AVR при больших PPR.

## Порог «ΔZ = 0»

| Единица CrdU | Рекомендуемый порог |
|--------------|---------------------|
| мм | ±0.01 мм |
| дюймы | ±0.0005″ |
| шаги | ±1 шаг |

Согласовать с тиком энкодера:

```
ΔZ_min_mm ≈ P_mm / TICKS_PER_REV   /* одна фаза шпинделя */
```

## LCD — формат строки 2

### Режим divider (сейчас)

```c
snprintf(buf, 21, "COU %c%07ld:%04ld", sign_revs, labs(revs), phase4);
```

### Режим подхвата (целевой)

Мм, без лишних нулей слева (как A/B/R в divider):

```c
/* delta_z_mm > 0 */
snprintf(buf, 21, "COU +%u.%02u", iw, frac);
/* delta_z_mm < 0 */
snprintf(buf, 21, "COU -%u.%02u", iw, frac);
/* |delta| < threshold */
snprintf(buf, 21, "COU +0.00");
```

При необходимости 20 символов — укороченный формат `COU %+6.2f` с пробелами.

## Переключение режима COU

```c
typedef enum {
    DIV_COU_SPINDLE,   /* обороты:фаза — по умолчанию divider */
    DIV_COU_DELTA_Z    /* ΔZ — при thread_align_on && P задан */
} DivCouMode_t;
```

Управление:

- `mode_thread_enter()` + pitch valid → `DIV_COU_DELTA_Z`
- `mode_thread_exit()` или pitch = 0 → `DIV_COU_SPINDLE`

## Бипер (как R в divider)

| Событие | Звук |
|---------|------|
| \|ΔZ\| < порог, ранее было вне порога | 1 длинный (200 ms) |
| \|ΔZ\| ≥ порог, ранее в пороге | 2 коротких (40 ms) |

Сброс состояния при latch (Select), смене P, смене B/A не требуется.

## Сценарий: подхват после остановки

1. Нарезали резьбу, sync остановлен — **C и Z продолжают отслеживаться**.
2. Повернули шпиндель вручную на N оборотов — C изменился, Z нет.
3. COU показывает ΔZ = N×P (плюс фаза).
4. Сдвинули Z на ΔZ → COU → 0.
5. Sync с тем же P → в нитке.

## Сценарий: первая стыковка

1. Select — latch `(C_ref, Z_ref)` в произвольной точке.
2. ΔZ всегда 0 сразу после latch.
3. Повернули шпиндель — COU показывает нужную коррекцию.
4. Подогнали Z — sync.

## Зависимости от mode_thread

При реализации `mode_thread` потребуется:

- глобальный или общий `current_pitch_mm`;
- сигнал «threading active» для divider LCD;
- согласование знака с направлением резьбы (наружная/внутренняя).

## Типичные ошибки (при реализации)

1. **Только фаза шпинделя** — игнор целых оборотов в C.
2. **COU в шагах, координаты в мм** — несогласованные единицы.
3. **Latch только C** — без Z_ref формула бессмысленна.
4. **Автодвижение Z по ΔZ** — по ТЗ оператор двигает вручную; planner не трогать.
5. **ΔZ в ISR** — только poll в `mode_divider_process`.

## Связь с обсуждением (2026)

Концепция «виртуальной гайки»: шпиндель и Z произвольны, но при известных
`(C_ref, Z_ref, P)` и полном счёте энкодера ΔZ однозначна. После обнуления ΔZ
sync-подача с тем же P держит нитку.
