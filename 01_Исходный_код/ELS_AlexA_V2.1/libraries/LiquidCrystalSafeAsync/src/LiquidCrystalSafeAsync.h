#ifndef LiquidCrystalSafeAsync_h
#define LiquidCrystalSafeAsync_h

#include <inttypes.h>
#include "Print.h"

// Команды HD44780
#define LCD_CLEARDISPLAY 0x01
#define LCD_RETURNHOME 0x02
#define LCD_ENTRYMODESET 0x04
#define LCD_DISPLAYCONTROL 0x08
#define LCD_CURSORSHIFT 0x10
#define LCD_FUNCTIONSET 0x20
#define LCD_SETCGRAMADDR 0x40
#define LCD_SETDDRAMADDR 0x80

// Флаги
#define LCD_ENTRYRIGHT 0x00
#define LCD_ENTRYLEFT 0x02
#define LCD_ENTRYSHIFTINCREMENT 0x01
#define LCD_ENTRYSHIFTDECREMENT 0x00
#define LCD_DISPLAYON 0x04
#define LCD_DISPLAYOFF 0x00
#define LCD_CURSORON 0x02
#define LCD_CURSOROFF 0x00
#define LCD_BLINKON 0x01
#define LCD_BLINKOFF 0x00
#define LCD_DISPLAYMOVE 0x08
#define LCD_CURSORMOVE 0x00
#define LCD_MOVERIGHT 0x04
#define LCD_MOVELEFT 0x00
#define LCD_8BITMODE 0x10
#define LCD_4BITMODE 0x00
#define LCD_2LINE 0x08
#define LCD_1LINE 0x00
#define LCD_5x10DOTS 0x04
#define LCD_5x8DOTS 0x00

#define LCD_MAX_ROWS 4
#define LCD_MAX_COLS 20
#define LCD_QUEUE_SIZE 128

// Типы операций в очереди
#define LCD_OP_CMD         0x01
#define LCD_OP_DATA        0x02
#define LCD_OP_SETCURSOR   0x03
#define LCD_OP_CLEAR       0x04
#define LCD_OP_HOME        0x05
#define LCD_OP_SYNC_CELL   0x06
#define LCD_OP_WAIT        0x07
#define LCD_OP_NOOP        0x00

class LiquidCrystalSafeAsync : public Print {
public:
    // 8-битный режим: RS, RW, EN, D0..D7
    LiquidCrystalSafeAsync(uint8_t rs, uint8_t rw, uint8_t enable,
                           uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3,
                           uint8_t d4, uint8_t d5, uint8_t d6, uint8_t d7,
                           uint8_t maxRetries = 3);

    // 4-битный режим с RW: RS, RW, EN, D4, D5, D6, D7
    LiquidCrystalSafeAsync(uint8_t rs, uint8_t rw, uint8_t enable,
                           uint8_t d4, uint8_t d5, uint8_t d6, uint8_t d7,
                           uint8_t maxRetries = 3);

    // 4-битный режим без RW: RS, EN, D4, D5, D6, D7
    LiquidCrystalSafeAsync(uint8_t rs, uint8_t enable,
                           uint8_t d4, uint8_t d5, uint8_t d6, uint8_t d7,
                           uint8_t maxRetries = 3);

    void begin(uint8_t cols, uint8_t rows, uint8_t charsize = LCD_5x8DOTS);
    void clear();
    void home();
    void setCursor(uint8_t col, uint8_t row);
    void noDisplay();
    void display();
    void noBlink();
    void blink();
    void noCursor();
    void cursor();
    void scrollDisplayLeft();
    void scrollDisplayRight();
    void leftToRight();
    void rightToLeft();
    void autoscroll();
    void noAutoscroll();
    void createChar(uint8_t location, uint8_t charmap[]);
    virtual size_t write(uint8_t value);

    long processQueue();
    void flush();
    void syncAll();

    uint32_t getErrorCount() const { return _errorCount; }
    void resetErrorCount() { _errorCount = 0; }
    void setMaxRetries(uint8_t retries) { _maxRetries = retries; }
    bool isBusy() const { return _state != LCD_STATE_IDLE; }
    bool is4Bit() const { return _fourBitMode; }

private:
    uint8_t _rs_pin, _rw_pin, _enable_pin;
    uint8_t _data_pins[8];
    uint8_t _numDataPins;
    bool _fourBitMode;
    bool _hasRW;
    uint8_t _data_shift;

    volatile uint8_t* _rs_port;
    uint8_t _rs_mask;
    volatile uint8_t* _rw_port;
    uint8_t _rw_mask;
    volatile uint8_t* _en_port;
    uint8_t _en_mask;
    volatile uint8_t* _data_out_port;
    volatile uint8_t* _data_in_pin;
    uint8_t _data_mask;
    uint8_t _data_bit_masks[8];

    uint8_t _cols, _rows, _numlines;
    uint8_t _displayfunction, _displaycontrol, _displaymode;
    uint8_t _row_offsets[4];
    uint8_t _cursorCol, _cursorRow;

    char _buffer[LCD_MAX_ROWS * LCD_MAX_COLS];

    volatile uint8_t _queue[LCD_QUEUE_SIZE];
    volatile uint8_t _queueHead;
    volatile uint8_t _queueTail;
    volatile uint8_t _queueCount;

    enum LcdState {
        LCD_STATE_IDLE,
        LCD_STATE_WAIT_EXECUTION,
        LCD_STATE_WAIT_VERIFY
    };
    volatile LcdState _state;
    volatile uint32_t _waitUntil;
    volatile uint8_t _syncRow, _syncCol;
    volatile uint8_t _retryCount;
    volatile uint8_t _currentData;

    uint32_t _errorCount;
    uint8_t _maxRetries;
    bool _initialized;

    void initPorts();
    void initQueue();
    bool queuePush(uint8_t op, uint8_t data = 0);
    bool queuePushCmd(uint8_t cmd);
    bool queuePushSetCursor(uint8_t col, uint8_t row);
    bool queuePushSyncCell(uint8_t row, uint8_t col);

    inline void fastSetRS(bool state);
    inline void fastSetRW(bool state);
    inline void fastSetEN(bool state);
    inline void fastSetData(uint8_t value);
    inline uint8_t fastGetData();
    inline void fastPulseEnable();

    void sendNibble(uint8_t value, bool isHigh);
    void sendByte(uint8_t value, bool isData);
    uint8_t readByte(bool isStatus = false);

    bool verifyCursor(uint8_t expectedAddr);
    long executeCommand(uint8_t op, uint8_t data);

    inline void delaySafe(uint16_t us);
    inline uint8_t getAddr(uint8_t row, uint8_t col);
};

#endif