# LiquidCrystalSafeAsync

[![GitHub license](https://img.shields.io/github/license/yourusername/LiquidCrystalSafeAsync)](LICENSE)
[![GitHub release](https://img.shields.io/github/v/release/yourusername/LiquidCrystalSafeAsync)](https://github.com/yourusername/LiquidCrystalSafeAsync/releases)
[![PlatformIO Registry](https://badges.registry.platformio.org/packages/yourusername/library/LiquidCrystalSafeAsync.svg)](https://registry.platformio.org/libraries/yourusername/LiquidCrystalSafeAsync)
[![Arduino Library](https://img.shields.io/badge/Arduino-library-blue)](https://www.arduino.cc/reference/en/libraries/liquidcrystalsafeasync/)

**Надёжная асинхронная библиотека для управления символьными дисплеями HD44780 (LCD1602, LCD2004) с верификацией записи и автоматическим восстановлением.**

---

## 📌 Особенности

- ✅ **8-битный и 4-битный режимы** — автоматический выбор по количеству пинов
- ✅ **Асинхронная работа** — отправка данных через очередь, не блокирует `loop()`
- ✅ **Верификация записи** — каждый байт проверяется чтением из DDRAM
- ✅ **Автоматическое восстановление** — повреждённые ячейки перезаписываются
- ✅ **Настраиваемое число попыток** — количество повторных попыток восстановления
- ✅ **Быстрый доступ к портам** — прямая работа с регистрами (без `digitalWrite`)
- ✅ **Прерывания НЕ отключаются** — безопасно для систем реального времени
- ✅ **Экономия пинов** — поддержка режима без вывода RW
- ✅ **Совместимость с API LiquidCrystal** — легко заменить стандартную библиотеку
- ✅ **Буфер экрана** — хранит копию отображаемых данных

---

## 📦 Установка

### Через Arduino IDE
1. Откройте **Скетч → Подключить библиотеку → Управлять библиотеками...**
2. В строке поиска введите `LiquidCrystalSafeAsync`
3. Нажмите **Установить**

### Через PlatformIO
В файле `platformio.ini` добавьте:
```ini
lib_deps =
    yourusername/LiquidCrystalSafeAsync
Ручная установка
Скачайте ZIP-архив с GitHub

Распакуйте в папку libraries вашего Arduino проекта

Переименуйте папку в LiquidCrystalSafeAsync

🔌 Подключение
8-битный режим (максимальная скорость)
LCD Pin	Arduino Pin	Описание
RS	30	Выбор регистра
RW	31	Чтение/Запись
EN	32	Строб Enable
D0	22	Данные, бит 0
D1	23	Данные, бит 1
D2	24	Данные, бит 2
D3	25	Данные, бит 3
D4	26	Данные, бит 4
D5	27	Данные, бит 5
D6	28	Данные, бит 6
D7	29	Данные, бит 7
4-битный режим (экономия пинов)
LCD Pin	Arduino Pin	Описание
RS	30	Выбор регистра
RW	31	Чтение/Запись (можно не подключать)
EN	32	Строб Enable
D4	33	Данные, бит 4
D5	34	Данные, бит 5
D6	35	Данные, бит 6
D7	36	Данные, бит 7
Важно: Для работы верификации рекомендуется подключать пин RW. Без него библиотека использует фиксированные задержки.

🚀 Быстрый старт
cpp
#include "LiquidCrystalSafeAsync.h"

// 8-битный режим: RS, RW, EN, D0..D7, макс. попыток = 3
LiquidCrystalSafeAsync lcd(30, 31, 32,
                           22, 23, 24, 25,
                           26, 27, 28, 29,
                           3);

void setup() {
    lcd.begin(20, 4);  // 20 колонок, 4 строки
    lcd.setCursor(0, 0);
    lcd.print("Hello, World!");
}

void loop() {
    // Обработка очереди в кванте 10 мкс
    unsigned long start = micros();
    while ((micros() - start) < 10) {
        if (lcd.processQueue() < 0) break;
    }

    // Ваша основная логика (энкодер, STEP, Брезенхем...)
    // ...

    // Обновление данных (20 Гц)
    static uint32_t lastUpdate = 0;
    if (millis() - lastUpdate > 50) {
        lastUpdate = millis();
        lcd.setCursor(0, 1);
        lcd.print("RPM: ");
        lcd.print(random(0, 3000));
    }
}
📖 API
Конструкторы
cpp
// 8-битный режим (RS, RW, EN, D0..D7, maxRetries)
LiquidCrystalSafeAsync(uint8_t rs, uint8_t rw, uint8_t enable,
                       uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3,
                       uint8_t d4, uint8_t d5, uint8_t d6, uint8_t d7,
                       uint8_t maxRetries = 3);

// 4-битный режим с RW (RS, RW, EN, D4..D7, maxRetries)
LiquidCrystalSafeAsync(uint8_t rs, uint8_t rw, uint8_t enable,
                       uint8_t d4, uint8_t d5, uint8_t d6, uint8_t d7,
                       uint8_t maxRetries = 3);

// 4-битный режим без RW (RS, EN, D4..D7, maxRetries)
LiquidCrystalSafeAsync(uint8_t rs, uint8_t enable,
                       uint8_t d4, uint8_t d5, uint8_t d6, uint8_t d7,
                       uint8_t maxRetries = 3);
Основные методы
Метод	Описание
begin(cols, rows, charsize)	Инициализация дисплея
clear()	Очистка экрана
home()	Возврат курсора в начало
setCursor(col, row)	Установка позиции курсора
print() / write()	Вывод текста или символа
createChar(location, charmap[])	Создание пользовательского символа
processQueue()	Обработка очереди (вызывать в loop())
flush()	Принудительная отправка всей очереди
syncAll()	Полная синхронизация буфера с экраном
Управление дисплеем
Метод	Описание
display() / noDisplay()	Включение/выключение дисплея
cursor() / noCursor()	Показать/скрыть курсор
blink() / noBlink()	Включить/выключить мигание курсора
scrollDisplayLeft() / scrollDisplayRight()	Сдвиг экрана
leftToRight() / rightToLeft()	Направление текста
autoscroll() / noAutoscroll()	Автоматический сдвиг при выводе
Статистика и настройки
Метод	Описание
getErrorCount()	Получить количество ошибок восстановления
resetErrorCount()	Сбросить счётчик ошибок
setMaxRetries(retries)	Установить число попыток восстановления
isBusy()	Проверить, занята ли очередь
is4Bit()	Проверить, используется ли 4-битный режим
⏱️ Временные характеристики
Операция	8-битный режим	4-битный режим
Отправка 1 байта	~2 мкс	~4 мкс
Чтение 1 байта	~2 мкс	~4 мкс
Ожидание Busy Flag	37–40 мкс	37–40 мкс
Восстановление 1 ячейки	~138 мкс	~150 мкс
Полное сканирование (80 ячеек)	~2 мс	~2.5 мс
🔧 Настройка количества попыток
cpp
LiquidCrystalSafeAsync lcd(30, 31, 32, 22, 23, 24, 25, 26, 27, 28, 29, 5);
// 5 попыток восстановления на ячейку

// Или позже:
lcd.setMaxRetries(5);
🧠 Как это работает
Буфер экрана хранит копию данных.

Все команды (print, setCursor, clear) не отправляются сразу, а помещаются в очередь.

Функция processQueue() вызывается из loop() и обрабатывает 1 команду за вызов.

Каждая операция записи верифицируется чтением из DDRAM.

Если данные не совпали — запускается восстановление (повторная запись с проверкой, до maxRetries раз).

Метод syncAll() сканирует весь экран и восстанавливает повреждённые ячейки.

🔬 Пример с синхронизацией
cpp
void loop() {
    // 1. Обработка очереди (квант 10 мкс)
    unsigned long start = micros();
    while ((micros() - start) < 10) {
        if (lcd.processQueue() < 0) break;
    }

    // 2. Основная логика
    // ...

    // 3. Синхронизация экрана (раз в 100 мс)
    static uint32_t lastSync = 0;
    if (millis() - lastSync > 100) {
        lastSync = millis();
        lcd.syncAll();  // Сканирование и восстановление
    }
}
📊 Сравнение с аналогами
Библиотека	Асинхронная	Верификация	Автовосстановление	Прерывания не отключаются	4/8 бит
LiquidCrystalSafeAsync	✅	✅	✅	✅	✅
LiquidCrystal (стандартная)	❌	❌	❌	❌	✅
LiquidCrystal_I2C	❌	❌	❌	❌	❌
AsyncLiquidCrystal	✅	❌	❌	⚠️ (отключает на чтение)	✅
🤝 Вклад в проект
Форкните репозиторий

Создайте ветку для вашей фичи (git checkout -b feature/amazing)

Закоммитьте изменения (git commit -m 'Add amazing feature')

Запушьте в ветку (git push origin feature/amazing)

Откройте Pull Request

📄 Лицензия
MIT License. Подробнее в файле LICENSE.

🙏 Благодарности
Hitachi HD44780U Datasheet — за точную спецификацию.

Сообществу Arduino — за вдохновение.

Всем тестерам и контрибьюторам.