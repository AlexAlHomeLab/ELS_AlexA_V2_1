#include "LiquidCrystalSafeAsync.h"
#include <Arduino.h>
#include <avr/io.h>
#include <util/delay.h>

// ============================================
// КОНСТРУКТОРЫ
// ============================================

// 8-битный режим: RS, RW, EN, D0, D1, D2, D3, D4, D5, D6, D7
LiquidCrystalSafeAsync::LiquidCrystalSafeAsync(uint8_t rs, uint8_t rw, uint8_t enable,
                                               uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3,
                                               uint8_t d4, uint8_t d5, uint8_t d6, uint8_t d7,
                                               uint8_t maxRetries)
    : _rs_pin(rs), _rw_pin(rw), _enable_pin(enable), _maxRetries(maxRetries)
{
    _data_pins[0] = d0;
    _data_pins[1] = d1;
    _data_pins[2] = d2;
    _data_pins[3] = d3;
    _data_pins[4] = d4;
    _data_pins[5] = d5;
    _data_pins[6] = d6;
    _data_pins[7] = d7;
    _numDataPins = 8;
    _fourBitMode = false;
    _hasRW = true;
    _displayfunction = LCD_8BITMODE | LCD_1LINE | LCD_5x8DOTS;
    _data_shift = 0;
    _initialized = false;
    _errorCount = 0;
    _state = LCD_STATE_IDLE;
    _waitUntil = 0;
    _retryCount = 0;
    _cursorCol = 0;
    _cursorRow = 0;
    _cols = 20;
    _rows = 4;

    for (int i = 0; i < 8; i++) {
        pinMode(_data_pins[i], OUTPUT);
    }
    pinMode(_rs_pin, OUTPUT);
    pinMode(_rw_pin, OUTPUT);
    pinMode(_enable_pin, OUTPUT);

    initPorts();
    initQueue();

    for (int i = 0; i < LCD_MAX_ROWS * LCD_MAX_COLS; i++) {
        _buffer[i] = ' ';
    }
}

// 4-битный режим с RW: RS, RW, EN, D4, D5, D6, D7
LiquidCrystalSafeAsync::LiquidCrystalSafeAsync(uint8_t rs, uint8_t rw, uint8_t enable,
                                               uint8_t d4, uint8_t d5, uint8_t d6, uint8_t d7,
                                               uint8_t maxRetries)
    : _rs_pin(rs), _rw_pin(rw), _enable_pin(enable), _maxRetries(maxRetries)
{
    _data_pins[0] = d4;
    _data_pins[1] = d5;
    _data_pins[2] = d6;
    _data_pins[3] = d7;
    _numDataPins = 4;
    _fourBitMode = true;
    _hasRW = true;
    _displayfunction = LCD_4BITMODE | LCD_1LINE | LCD_5x8DOTS;
    _data_shift = 4;
    _initialized = false;
    _errorCount = 0;
    _state = LCD_STATE_IDLE;
    _waitUntil = 0;
    _retryCount = 0;
    _cursorCol = 0;
    _cursorRow = 0;
    _cols = 20;
    _rows = 4;

    for (int i = 0; i < 4; i++) {
        pinMode(_data_pins[i], OUTPUT);
    }
    pinMode(_rs_pin, OUTPUT);
    pinMode(_rw_pin, OUTPUT);
    pinMode(_enable_pin, OUTPUT);

    initPorts();
    initQueue();

    for (int i = 0; i < LCD_MAX_ROWS * LCD_MAX_COLS; i++) {
        _buffer[i] = ' ';
    }
}

// 4-битный режим без RW: RS, EN, D4, D5, D6, D7
LiquidCrystalSafeAsync::LiquidCrystalSafeAsync(uint8_t rs, uint8_t enable,
                                               uint8_t d4, uint8_t d5, uint8_t d6, uint8_t d7,
                                               uint8_t maxRetries)
    : _rs_pin(rs), _rw_pin(255), _enable_pin(enable), _maxRetries(maxRetries)
{
    _data_pins[0] = d4;
    _data_pins[1] = d5;
    _data_pins[2] = d6;
    _data_pins[3] = d7;
    _numDataPins = 4;
    _fourBitMode = true;
    _hasRW = false;
    _displayfunction = LCD_4BITMODE | LCD_1LINE | LCD_5x8DOTS;
    _data_shift = 4;
    _initialized = false;
    _errorCount = 0;
    _state = LCD_STATE_IDLE;
    _waitUntil = 0;
    _retryCount = 0;
    _cursorCol = 0;
    _cursorRow = 0;
    _cols = 20;
    _rows = 4;

    for (int i = 0; i < 4; i++) {
        pinMode(_data_pins[i], OUTPUT);
    }
    pinMode(_rs_pin, OUTPUT);
    pinMode(_enable_pin, OUTPUT);

    initPorts();
    initQueue();

    for (int i = 0; i < LCD_MAX_ROWS * LCD_MAX_COLS; i++) {
        _buffer[i] = ' ';
    }
}

// ============================================
// ИНИЦИАЛИЗАЦИЯ ПОРТОВ (БЫСТРЫЙ ДОСТУП)
// ============================================
void LiquidCrystalSafeAsync::initPorts()
{
    #define GET_PORT(pin) (portOutputRegister(digitalPinToPort(pin)))
    #define GET_PIN(pin) (portInputRegister(digitalPinToPort(pin)))
    #define GET_MASK(pin) (digitalPinToBitMask(pin))

    _rs_port = GET_PORT(_rs_pin);
    _rs_mask = GET_MASK(_rs_pin);
    _en_port = GET_PORT(_enable_pin);
    _en_mask = GET_MASK(_enable_pin);

    if (_hasRW) {
        _rw_port = GET_PORT(_rw_pin);
        _rw_mask = GET_MASK(_rw_pin);
    } else {
        _rw_port = nullptr;
        _rw_mask = 0;
    }

    _data_out_port = GET_PORT(_data_pins[0]);
    _data_in_pin = GET_PIN(_data_pins[0]);
    _data_mask = 0;
    for (int i = 0; i < _numDataPins; i++) {
        _data_bit_masks[i] = GET_MASK(_data_pins[i]);
        _data_mask |= _data_bit_masks[i];
    }

    #undef GET_PORT
    #undef GET_PIN
    #undef GET_MASK
}

// ============================================
// ИНИЦИАЛИЗАЦИЯ ОЧЕРЕДИ
// ============================================
void LiquidCrystalSafeAsync::initQueue()
{
    _queueHead = 0;
    _queueTail = 0;
    _queueCount = 0;
}

bool LiquidCrystalSafeAsync::queuePush(uint8_t op, uint8_t data)
{
    if (_queueCount >= LCD_QUEUE_SIZE - 1) return false;
    _queue[_queueTail] = op;
    _queueTail = (_queueTail + 1) % LCD_QUEUE_SIZE;
    _queue[_queueTail] = data;
    _queueTail = (_queueTail + 1) % LCD_QUEUE_SIZE;
    _queueCount++;
    return true;
}

// ============================================
// БЫСТРЫЕ ФИЗИЧЕСКИЕ ОПЕРАЦИИ
// ============================================
inline void LiquidCrystalSafeAsync::fastSetRS(bool state)
{
    if (state) *_rs_port |= _rs_mask;
    else *_rs_port &= ~_rs_mask;
}

inline void LiquidCrystalSafeAsync::fastSetRW(bool state)
{
    if (!_hasRW) return;
    if (state) *_rw_port |= _rw_mask;
    else *_rw_port &= ~_rw_mask;
}

inline void LiquidCrystalSafeAsync::fastSetEN(bool state)
{
    if (state) *_en_port |= _en_mask;
    else *_en_port &= ~_en_mask;
}

inline void LiquidCrystalSafeAsync::fastSetData(uint8_t value)
{
    uint8_t mask = _data_mask;
    *_data_out_port = (*_data_out_port & ~mask) | (value & mask);
}

inline uint8_t LiquidCrystalSafeAsync::fastGetData()
{
    uint8_t pinVal = *_data_in_pin;
    uint8_t value = 0;
    for (int i = 0; i < _numDataPins; i++) {
        if (pinVal & _data_bit_masks[i]) {
            value |= (1 << i);
        }
    }
    return value;
}

inline void LiquidCrystalSafeAsync::fastPulseEnable()
{
    fastSetEN(true);
    delaySafe(1);
    fastSetEN(false);
    delaySafe(1);
}

// ============================================
// ОТПРАВКА НИББЛА (4 БИТА)
// ============================================
void LiquidCrystalSafeAsync::sendNibble(uint8_t value, bool isHigh)
{
    uint8_t portVal = *_data_out_port & ~_data_mask;
    for (int i = 0; i < _numDataPins; i++) {
        if (value & (1 << i)) {
            portVal |= _data_bit_masks[i];
        }
    }
    *_data_out_port = portVal;
    fastPulseEnable();
}

// ============================================
// ОТПРАВКА БАЙТА (С УЧЁТОМ РЕЖИМА 4/8 БИТ)
// ============================================
void LiquidCrystalSafeAsync::sendByte(uint8_t value, bool isData)
{
    fastSetRS(isData);
    if (_hasRW) fastSetRW(false);

    if (_fourBitMode) {
        sendNibble(value >> 4, true);
        sendNibble(value & 0x0F, false);
    } else {
        fastSetData(value);
        fastPulseEnable();
    }

    if (_hasRW) fastSetRW(false);
}

// ============================================
// ЧТЕНИЕ БАЙТА (С УЧЁТОМ РЕЖИМА 4/8 БИТ)
// ============================================
uint8_t LiquidCrystalSafeAsync::readByte(bool isStatus)
{
    // Переключаем пины данных на ВХОД
    volatile uint8_t* ddr = _data_out_port - 1;
    *ddr &= ~_data_mask;

    fastSetRS(!isStatus);
    if (_hasRW) fastSetRW(true);

    uint8_t value = 0;

    if (_fourBitMode) {
        // 4-битный режим: читаем старшую тетраду, затем младшую
        fastPulseEnable();
        delaySafe(1);
        uint8_t high = fastGetData() & 0x0F;

        fastPulseEnable();
        delaySafe(1);
        uint8_t low = fastGetData() & 0x0F;

        value = (high << 4) | low;
    } else {
        // 8-битный режим: читаем все 8 бит
        fastPulseEnable();
        delaySafe(1);
        value = fastGetData();
    }

    // Возвращаем пины в режим ВЫХОДА
    *ddr |= _data_mask;
    if (_hasRW) fastSetRW(false);

    return value;
}

// ============================================
// ИНИЦИАЛИЗАЦИЯ ДИСПЛЕЯ
// ============================================
void LiquidCrystalSafeAsync::begin(uint8_t cols, uint8_t rows, uint8_t charsize)
{
    _cols = cols;
    _rows = rows;
    _numlines = rows;

    _row_offsets[0] = 0x00;
    _row_offsets[1] = 0x40;
    _row_offsets[2] = 0x14;
    _row_offsets[3] = 0x54;

    delay(50);

    if (_fourBitMode) {
        // Специальная последовательность для 4-битного режима
        fastSetRS(false);
        if (_hasRW) fastSetRW(false);

        // 0x30 три раза (8-битные команды)
        sendNibble(0x03, true);
        delayMicroseconds(4500);
        sendNibble(0x03, true);
        delayMicroseconds(150);
        sendNibble(0x03, true);
        delayMicroseconds(150);

        // Переключение в 4-битный режим
        sendNibble(0x02, true);
        delayMicroseconds(150);

        // Теперь работаем в 4-битном режиме
        _displayfunction = LCD_4BITMODE | LCD_1LINE | LCD_5x8DOTS;
        if (rows > 1) _displayfunction |= LCD_2LINE;
        if (charsize == LCD_5x10DOTS && rows == 1) _displayfunction |= LCD_5x10DOTS;

        sendByte(LCD_FUNCTIONSET | _displayfunction, false);
        delayMicroseconds(100);
    } else {
        // 8-битный режим: стандартная инициализация
        _displayfunction = LCD_8BITMODE | LCD_1LINE | LCD_5x8DOTS;
        if (rows > 1) _displayfunction |= LCD_2LINE;
        if (charsize == LCD_5x10DOTS && rows == 1) _displayfunction |= LCD_5x10DOTS;

        sendByte(LCD_FUNCTIONSET | _displayfunction, false);
        delayMicroseconds(100);
    }

    _displaycontrol = LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF;
    sendByte(LCD_DISPLAYCONTROL | _displaycontrol, false);
    delayMicroseconds(100);

    sendByte(LCD_CLEARDISPLAY, false);
    delayMicroseconds(1520);

    _displaymode = LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT;
    sendByte(LCD_ENTRYMODESET | _displaymode, false);
    delayMicroseconds(100);

    _initialized = true;
    _state = LCD_STATE_IDLE;
}

// ============================================
// ПУБЛИЧНЫЕ МЕТОДЫ (ДОБАВЛЕНИЕ В ОЧЕРЕДЬ)
// ============================================
void LiquidCrystalSafeAsync::clear()
{
    if (!_initialized) return;
    queuePush(LCD_OP_CLEAR, 0);
}

void LiquidCrystalSafeAsync::home()
{
    if (!_initialized) return;
    queuePush(LCD_OP_HOME, 0);
}

void LiquidCrystalSafeAsync::setCursor(uint8_t col, uint8_t row)
{
    if (!_initialized) return;
    if (row >= _rows) row = _rows - 1;
    if (col >= _cols) col = _cols - 1;
    _cursorCol = col;
    _cursorRow = row;
    queuePushSetCursor(col, row);
}

bool LiquidCrystalSafeAsync::queuePushSetCursor(uint8_t col, uint8_t row)
{
    uint8_t addr = getAddr(row, col);
    return queuePush(LCD_OP_SETCURSOR, addr);
}

size_t LiquidCrystalSafeAsync::write(uint8_t value)
{
    if (!_initialized) return 0;
    if (queuePush(LCD_OP_DATA, value)) {
        return 1;
    }
    return 0;
}

void LiquidCrystalSafeAsync::noDisplay() {
    _displaycontrol &= ~LCD_DISPLAYON;
    queuePushCmd(LCD_DISPLAYCONTROL | _displaycontrol);
}
void LiquidCrystalSafeAsync::display() {
    _displaycontrol |= LCD_DISPLAYON;
    queuePushCmd(LCD_DISPLAYCONTROL | _displaycontrol);
}
void LiquidCrystalSafeAsync::noBlink() {
    _displaycontrol &= ~LCD_BLINKON;
    queuePushCmd(LCD_DISPLAYCONTROL | _displaycontrol);
}
void LiquidCrystalSafeAsync::blink() {
    _displaycontrol |= LCD_BLINKON;
    queuePushCmd(LCD_DISPLAYCONTROL | _displaycontrol);
}
void LiquidCrystalSafeAsync::noCursor() {
    _displaycontrol &= ~LCD_CURSORON;
    queuePushCmd(LCD_DISPLAYCONTROL | _displaycontrol);
}
void LiquidCrystalSafeAsync::cursor() {
    _displaycontrol |= LCD_CURSORON;
    queuePushCmd(LCD_DISPLAYCONTROL | _displaycontrol);
}
void LiquidCrystalSafeAsync::scrollDisplayLeft() {
    queuePushCmd(LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVELEFT);
}
void LiquidCrystalSafeAsync::scrollDisplayRight() {
    queuePushCmd(LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVERIGHT);
}
void LiquidCrystalSafeAsync::leftToRight() {
    _displaymode |= LCD_ENTRYLEFT;
    queuePushCmd(LCD_ENTRYMODESET | _displaymode);
}
void LiquidCrystalSafeAsync::rightToLeft() {
    _displaymode &= ~LCD_ENTRYLEFT;
    queuePushCmd(LCD_ENTRYMODESET | _displaymode);
}
void LiquidCrystalSafeAsync::autoscroll() {
    _displaymode |= LCD_ENTRYSHIFTINCREMENT;
    queuePushCmd(LCD_ENTRYMODESET | _displaymode);
}
void LiquidCrystalSafeAsync::noAutoscroll() {
    _displaymode &= ~LCD_ENTRYSHIFTINCREMENT;
    queuePushCmd(LCD_ENTRYMODESET | _displaymode);
}

bool LiquidCrystalSafeAsync::queuePushCmd(uint8_t cmd)
{
    return queuePush(LCD_OP_CMD, cmd);
}

void LiquidCrystalSafeAsync::createChar(uint8_t location, uint8_t charmap[])
{
    if (!_initialized) return;
    location &= 0x7;
    queuePush(LCD_OP_CMD, LCD_SETCGRAMADDR | (location << 3));
    for (int i = 0; i < 8; i++) {
        queuePush(LCD_OP_DATA, charmap[i]);
    }
}

// ============================================
// СИНХРОНИЗАЦИЯ ВСЕГО ЭКРАНА
// ============================================
void LiquidCrystalSafeAsync::syncAll()
{
    if (!_initialized) return;
    for (uint8_t row = 0; row < _rows; row++) {
        for (uint8_t col = 0; col < _cols; col++) {
            queuePushSyncCell(row, col);
        }
    }
}

bool LiquidCrystalSafeAsync::queuePushSyncCell(uint8_t row, uint8_t col)
{
    uint8_t data = (row << 4) | col;
    return queuePush(LCD_OP_SYNC_CELL, data);
}

// ============================================
// ОБРАБОТКА ОЧЕРЕДИ
// ============================================
long LiquidCrystalSafeAsync::processQueue()
{
    if (!_initialized) return -1;

    // Ожидание выполнения
    if (_state == LCD_STATE_WAIT_EXECUTION) {
        if (micros() < _waitUntil) {
            return _waitUntil - micros();
        }
        _state = LCD_STATE_IDLE;
    }

    // Ожидание верификации
    if (_state == LCD_STATE_WAIT_VERIFY) {
        uint8_t addr = getAddr(_syncRow, _syncCol);
        if (verifyCursor(addr)) {
            readByte(true);
            uint8_t actual = readByte(false);
            if (actual == _currentData) {
                _buffer[_syncRow * _cols + _syncCol] = _currentData;
                _state = LCD_STATE_IDLE;
                _queueCount--;
                return 5;
            }
        }

        if (_retryCount < _maxRetries) {
            _retryCount++;
            uint8_t addr = getAddr(_syncRow, _syncCol);
            sendByte(LCD_SETDDRAMADDR | addr, false);
            delaySafe(40);
            sendByte(_currentData, true);
            delaySafe(40);
            _waitUntil = micros() + 40;
            _state = LCD_STATE_WAIT_VERIFY;
            return 40;
        } else {
            _errorCount++;
            _state = LCD_STATE_IDLE;
            _queueCount--;
            return -1;
        }
    }

    if (_queueCount == 0) return -1;

    uint8_t op = _queue[_queueHead];
    _queueHead = (_queueHead + 1) % LCD_QUEUE_SIZE;
    uint8_t data = _queue[_queueHead];
    _queueHead = (_queueHead + 1) % LCD_QUEUE_SIZE;
    _queueCount--;

    return executeCommand(op, data);
}

// ============================================
// ВЫПОЛНЕНИЕ КОМАНДЫ
// ============================================
long LiquidCrystalSafeAsync::executeCommand(uint8_t op, uint8_t data)
{
    switch (op) {
        case LCD_OP_CMD:
            sendByte(data, false);
            _waitUntil = micros() + 40;
            _state = LCD_STATE_WAIT_EXECUTION;
            return 40;

        case LCD_OP_DATA:
            sendByte(data, true);
            _waitUntil = micros() + 40;
            _state = LCD_STATE_WAIT_EXECUTION;
            return 40;

        case LCD_OP_SETCURSOR:
            sendByte(LCD_SETDDRAMADDR | data, false);
            _waitUntil = micros() + 40;
            _state = LCD_STATE_WAIT_EXECUTION;
            return 40;

        case LCD_OP_CLEAR:
            sendByte(LCD_CLEARDISPLAY, false);
            _waitUntil = micros() + 1520;
            _state = LCD_STATE_WAIT_EXECUTION;
            return 1520;

        case LCD_OP_HOME:
            sendByte(LCD_RETURNHOME, false);
            _waitUntil = micros() + 1520;
            _state = LCD_STATE_WAIT_EXECUTION;
            return 1520;

        case LCD_OP_SYNC_CELL:
            _syncRow = data >> 4;
            _syncCol = data & 0x0F;
            _currentData = _buffer[_syncRow * _cols + _syncCol];
            _retryCount = 0;

            uint8_t addr = getAddr(_syncRow, _syncCol);
            sendByte(LCD_SETDDRAMADDR | addr, false);
            delaySafe(40);

            if (verifyCursor(addr)) {
                readByte(true);
                uint8_t actual = readByte(false);
                if (actual == _currentData) {
                    _state = LCD_STATE_IDLE;
                    return 5;
                }
            }

            _retryCount = 1;
            sendByte(LCD_SETDDRAMADDR | addr, false);
            delaySafe(40);
            sendByte(_currentData, true);
            delaySafe(40);
            _waitUntil = micros() + 40;
            _state = LCD_STATE_WAIT_VERIFY;
            return 40;

        default:
            return -1;
    }
}

// ============================================
// ВЕРИФИКАЦИЯ КУРСОРА
// ============================================
bool LiquidCrystalSafeAsync::verifyCursor(uint8_t expectedAddr)
{
    uint8_t status = readByte(true);
    uint8_t currentAddr = status & 0x7F;
    return (currentAddr == expectedAddr);
}

// ============================================
// ПРИНУДИТЕЛЬНАЯ ОТПРАВКА ВСЕЙ ОЧЕРЕДИ
// ============================================
void LiquidCrystalSafeAsync::flush()
{
    while (processQueue() >= 0) {
        // Ждём
    }
}

// ============================================
// ВСПОМОГАТЕЛЬНЫЕ
// ============================================
inline uint8_t LiquidCrystalSafeAsync::getAddr(uint8_t row, uint8_t col)
{
    return _row_offsets[row] + col;
}

inline void LiquidCrystalSafeAsync::delaySafe(uint16_t us)
{
    delayMicroseconds(us);
}