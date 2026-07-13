/*
 * Сравнение LiquidCrystalSafeAsync и стандартной LiquidCrystal на Wokwi
 * Arduino Mega 2560 + LCD 2004, 4-bit режим без RW
 *
 * Выбор библиотеки: lcd_config.h или env в platformio.ini
 *
 * Пины LCD (PORTE):
 *   RS=30, EN=32, D4=33, D5=34, D6=35, D7=36
 *
 * Симуляция нагрузки CNC:
 *   Timer1 — энкодер шпинделя (до 1000 RPM, 3000 имп/об)
 *   Timer5 — STEP двух осей, окружность R=100 мм
 *   Timer3 — плавный разгон RPM и скорости плоттера
 *
 * Шаговые (PORTA): STEP_X=22, STEP_Y=24, DIR_X=26, DIR_Y=28
 */

#include "lcd_config.h"
#include <avr/pgmspace.h>
#include <avr/interrupt.h>

#if defined(USE_LCD_SAFE_ASYNC)
#include "LiquidCrystalSafeAsync.h"
LiquidCrystalSafeAsync lcd(30, 32, 33, 34, 35, 36);
#elif defined(USE_LCD_STANDARD)
#include <LiquidCrystal.h>
LiquidCrystal lcd(30, 32, 33, 34, 35, 36);
#endif

#define MARQUEE_LEN 200
#define MARQUEE_ROW 1

#define SPINDLE_RPM_MIN         500UL
#define SPINDLE_RPM_MAX         1000UL
#define ENCODER_PPR             3000UL
#define RAMP_MS                 120000UL
#define RAMP_TIMER_HZ           10
#define RAMP_STEP               10UL

#define STEPS_PER_MM            1600L
#define CIRCLE_RADIUS_MM        100L
#define CIRCLE_RADIUS_STEPS     (CIRCLE_RADIUS_MM * STEPS_PER_MM)

#define PLOTTER_FEED_MM_MIN     100UL
#define PLOTTER_FEED_MM_MAX     1000UL

#define CIRCLE_R_UNIT           (CIRCLE_RADIUS_STEPS >> 8)
#define TRIG_TABLE_SIZE         256

#define STEP_X_PIN              22
#define STEP_Y_PIN              24
#define DIR_X_PIN               26
#define DIR_Y_PIN               28

#define STEP_X_BIT              0
#define STEP_Y_BIT              2
#define DIR_X_BIT               4
#define DIR_Y_BIT               6

static const char MARQUEE[] PROGMEM =
    "WARNING: EEPROM is on strike! It forgot your WiFi password AND your birthday. "
    "The cat napped on Mega2560; IDE says: purrmission denied. Stack overflow? "
    "No, just spaghetti code with dreams. BZZT!!     ";

static const int16_t SIN_TABLE[TRIG_TABLE_SIZE] PROGMEM = {
     0,   804,  1608,  2410,  3212,  4011,  4808,  5602,  6393,  7179,  7962,  8739,  9512, 10278, 11039, 11793,
 12539, 13279, 14010, 14732, 15446, 16151, 16846, 17530, 18204, 18868, 19519, 20159, 20787, 21403, 22005, 22594,
 23170, 23731, 24279, 24811, 25329, 25832, 26319, 26790, 27245, 27683, 28105, 28510, 28898, 29268, 29621, 29956,
 30273, 30571, 30852, 31113, 31356, 31580, 31785, 31971, 32137, 32285, 32412, 32521, 32609, 32678, 32728, 32757,
 32767, 32757, 32728, 32678, 32609, 32521, 32412, 32285, 32137, 31971, 31785, 31580, 31356, 31113, 30852, 30571,
 30273, 29956, 29621, 29268, 28898, 28510, 28105, 27683, 27245, 26790, 26319, 25832, 25329, 24811, 24279, 23731,
 23170, 22594, 22005, 21403, 20787, 20159, 19519, 18868, 18204, 17530, 16846, 16151, 15446, 14732, 14010, 13279,
 12539, 11793, 11039, 10278,  9512,  8739,  7962,  7179,  6393,  5602,  4808,  4011,  3212,  2410,  1608,   804,
     0,  -804, -1608, -2410, -3212, -4011, -4808, -5602, -6393, -7179, -7962, -8739, -9512,-10278,-11039,-11793,
-12539,-13279,-14010,-14732,-15446,-16151,-16846,-17530,-18204,-18868,-19519,-20159,-20787,-21403,-22005,-22594,
-23170,-23731,-24279,-24811,-25329,-25832,-26319,-26790,-27245,-27683,-28105,-28510,-28898,-29268,-29621,-29956,
-30273,-30571,-30852,-31113,-31356,-31580,-31785,-31971,-32137,-32285,-32412,-32521,-32609,-32678,-32728,-32757,
-32767,-32757,-32728,-32678,-32609,-32521,-32412,-32285,-32137,-31971,-31785,-31580,-31356,-31113,-30852,-30571,
-30273,-29956,-29621,-29268,-28898,-28510,-28105,-27683,-27245,-26790,-26319,-25832,-25329,-24811,-24279,-23731,
-23170,-22594,-22005,-21403,-20787,-20159,-19519,-18868,-18204,-17530,-16846,-16151,-15446,-14732,-14010,-13279,
-12539,-11793,-11039,-10278, -9512, -8739, -7962, -7179, -6393, -5602, -4808, -4011, -3212, -2410, -1608,  -804
};

static int32_t circleCoordFromSin(int16_t sinVal)
{
    return ((int32_t)sinVal * CIRCLE_R_UNIT) >> 7;
}

static void formatMm3(int32_t steps, char *buf, uint8_t bufLen)
{
    int32_t milli = (steps * 1000L) / STEPS_PER_MM;
    if (milli < 0) {
        milli = -milli;
        int32_t whole = milli / 1000;
        int32_t frac = milli % 1000;
        snprintf(buf, bufLen, "-%ld.%03ld", (long)whole, (long)frac);
    } else {
        int32_t whole = milli / 1000;
        int32_t frac = milli % 1000;
        snprintf(buf, bufLen, "%ld.%03ld", (long)whole, (long)frac);
    }
}
volatile uint32_t encRevCount = 0;
volatile uint32_t encPulseCount = 0;
volatile uint32_t encIntervalUs = 0;
volatile uint32_t encPeriodUs = 40UL;

volatile int32_t stepPosX = CIRCLE_RADIUS_STEPS;
volatile int32_t stepPosY = 0;
volatile uint8_t circleIdx = 0;

volatile uint32_t spindleRpm = SPINDLE_RPM_MIN;
volatile uint32_t spindleEncHz = 0;
volatile uint32_t plotterFeedMmMin = PLOTTER_FEED_MM_MIN;
volatile uint32_t stepperHz = 0;
volatile uint8_t rampPercent = 0;
volatile bool statusDisplayDirty = true;

#if defined(USE_LCD_SAFE_ASYNC)
static void processLcd(unsigned long usBudget)
{
    unsigned long start = micros();
    while ((micros() - start) < usBudget) {
        if (lcd.processQueue() < 0) break;
    }
}
#else
static inline void processLcd(unsigned long)
{
}
#endif

static void showMarquee(uint16_t offset)
{
    char window[21];
    for (uint8_t i = 0; i < 20; i++) {
        window[i] = pgm_read_byte(&MARQUEE[(offset + i) % MARQUEE_LEN]);
    }
    window[20] = '\0';
    lcd.setCursor(0, MARQUEE_ROW);
    lcd.print(window);
}

static void showTopLine()
{
    char line0[21];
    uint32_t feed = plotterFeedMmMin;
    char libTag;

#if defined(USE_LCD_SAFE_ASYNC)
    snprintf(line0, sizeof(line0), "Err:%lu F:%4lu",
             lcd.getErrorCount(), feed);
    libTag = 'A';
#else
    snprintf(line0, sizeof(line0), "F:%4lu mm/m", feed);
    libTag = 'S';
#endif

    line0[19] = '\0';

    lcd.setCursor(0, 0);
    lcd.print(line0);
    lcd.setCursor(19, 0);
    lcd.print(libTag);
}

static void showCncStatus()
{
    char line2[21];
    char line3[21];
    char xBuf[12];
    char yBuf[12];

    showTopLine();

    uint32_t rev = encRevCount;
    int32_t posX;
    int32_t posY;

    noInterrupts();
    posX = stepPosX;
    posY = stepPosY;
    interrupts();

    formatMm3(posX, xBuf, sizeof(xBuf));
    formatMm3(posY, yBuf, sizeof(yBuf));

    snprintf(line2, sizeof(line2), "X:%s Y:%s", xBuf, yBuf);
    snprintf(line3, sizeof(line3), "Rev:%3lu RPM:%4lu", rev, spindleRpm);

    lcd.setCursor(0, 2);
    lcd.print(line2);
    lcd.setCursor(0, 3);
    lcd.print(line3);
}

static uint32_t rpmToEncoderHz(uint32_t rpm)
{
    return (rpm * ENCODER_PPR) / 60UL;
}

static uint32_t feedMmMinToStepHz(uint32_t feedMmMin)
{
    return (feedMmMin * (uint32_t)STEPS_PER_MM) / 60UL;
}

static void setTimer16CtcHz(volatile uint8_t *tccrA,
                            volatile uint8_t *tccrB,
                            volatile uint16_t *ocr,
                            volatile uint16_t *tcnt,
                            uint8_t wgmBit,
                            uint32_t freqHz)
{
    if (freqHz < 1) {
        freqHz = 1;
    }

    static const uint16_t prescalers[] = {1, 8, 64, 256, 1024};
    static const uint8_t csBits[] = {0x01, 0x02, 0x03, 0x04, 0x05};

    uint32_t bestDiff = 0xFFFFFFFFUL;
    uint8_t bestIdx = 0;
    uint16_t bestOcr = 1;

    for (uint8_t i = 0; i < 5; i++) {
        uint32_t ticksPerSecond = (uint32_t)F_CPU / prescalers[i];
        if (freqHz > ticksPerSecond) {
            continue;
        }

        uint32_t ocrPlus1 = ticksPerSecond / freqHz;
        if (ocrPlus1 < 1) {
            ocrPlus1 = 1;
        }
        if (ocrPlus1 > 65536UL) {
            continue;
        }

        uint32_t actualHz = ticksPerSecond / ocrPlus1;
        uint32_t diff = (actualHz > freqHz) ? (actualHz - freqHz) : (freqHz - actualHz);
        if (diff < bestDiff) {
            bestDiff = diff;
            bestIdx = i;
            bestOcr = (uint16_t)(ocrPlus1 - 1UL);
        }
    }

    uint8_t savedSreg = SREG;
    cli();
    *tccrA = 0;
    *tccrB = (1 << wgmBit) | csBits[bestIdx];
    *ocr = bestOcr;
    *tcnt = 0;
    SREG = savedSreg;
}

static void setSpindleEncoderHz(uint32_t freqHz)
{
    if (freqHz < 1) {
        freqHz = 1;
    }
    encPeriodUs = 1000000UL / freqHz;
    setTimer16CtcHz(&TCCR1A, &TCCR1B, &OCR1A, &TCNT1, WGM12, freqHz);
}

static void setStepperFromFeed(uint32_t feedMmMin)
{
    uint32_t freqHz = feedMmMinToStepHz(feedMmMin);
    if (freqHz < 1) {
        freqHz = 1;
    }
    stepperHz = freqHz;
    setTimer16CtcHz(&TCCR5A, &TCCR5B, &OCR5A, &TCNT5, WGM52, freqHz);
}

static void initStepperPins()
{
    pinMode(STEP_X_PIN, OUTPUT);
    pinMode(STEP_Y_PIN, OUTPUT);
    pinMode(DIR_X_PIN, OUTPUT);
    pinMode(DIR_Y_PIN, OUTPUT);

    PORTA &= ~((1 << STEP_X_BIT) | (1 << STEP_Y_BIT));
}

static inline void pulseStepX(int8_t dir)
{
    if (dir > 0) {
        PORTA |= (1 << DIR_X_BIT);
    } else {
        PORTA &= ~(1 << DIR_X_BIT);
    }
    PORTA |= (1 << STEP_X_BIT);
    PORTA &= ~(1 << STEP_X_BIT);
}

static inline void pulseStepY(int8_t dir)
{
    if (dir > 0) {
        PORTA |= (1 << DIR_Y_BIT);
    } else {
        PORTA &= ~(1 << DIR_Y_BIT);
    }
    PORTA |= (1 << STEP_Y_BIT);
    PORTA &= ~(1 << STEP_Y_BIT);
}

static void initTimer1Encoder()
{
    TIMSK1 = (1 << OCIE1A);
    setSpindleEncoderHz(rpmToEncoderHz(SPINDLE_RPM_MIN));
}

static void initTimer5Stepper()
{
    TIMSK5 = (1 << OCIE5A);
    setStepperFromFeed(PLOTTER_FEED_MM_MIN);
}

static void initTimer3Ramp()
{
    TCCR3A = 0;
    TCCR3B = (1 << WGM32) | (1 << CS32) | (1 << CS30);
    OCR3A = ((uint32_t)F_CPU / 1024UL / RAMP_TIMER_HZ) - 1UL;
    TCNT3 = 0;
    TIMSK3 = (1 << OCIE3A);
}

ISR(TIMER1_COMPA_vect)
{
    encIntervalUs = encPeriodUs;

    encPulseCount++;
    if (encPulseCount >= ENCODER_PPR) {
        encPulseCount = 0;
        encRevCount++;
    }
}

ISR(TIMER5_COMPA_vect)
{
    int16_t sinVal = (int16_t)pgm_read_word(&SIN_TABLE[circleIdx]);
    int16_t cosVal = (int16_t)pgm_read_word(&SIN_TABLE[(circleIdx + 64) & 0xFF]);
    int32_t targetX = circleCoordFromSin(cosVal);
    int32_t targetY = circleCoordFromSin(sinVal);
    bool moved = false;

    if (stepPosX < targetX) {
        pulseStepX(1);
        stepPosX++;
        moved = true;
    } else if (stepPosX > targetX) {
        pulseStepX(-1);
        stepPosX--;
        moved = true;
    }

    if (stepPosY < targetY) {
        pulseStepY(1);
        stepPosY++;
        moved = true;
    } else if (stepPosY > targetY) {
        pulseStepY(-1);
        stepPosY--;
        moved = true;
    }

    if (!moved && stepPosX == targetX && stepPosY == targetY) {
        circleIdx = (circleIdx + 1) & 0xFF;
    }
}

ISR(TIMER3_COMPA_vect)
{
    static uint16_t rampTicks = 0;
    rampTicks++;

    uint32_t elapsedMs = ((uint32_t)rampTicks * 1000UL) / RAMP_TIMER_HZ;
    if (elapsedMs > RAMP_MS) {
        elapsedMs = RAMP_MS;
    }

    rampPercent = (uint8_t)((elapsedMs * 100UL) / RAMP_MS);

    uint32_t newRpm = SPINDLE_RPM_MIN +
        ((SPINDLE_RPM_MAX - SPINDLE_RPM_MIN) * elapsedMs) / RAMP_MS;
    newRpm = (newRpm / RAMP_STEP) * RAMP_STEP;
    if (newRpm < SPINDLE_RPM_MIN) {
        newRpm = SPINDLE_RPM_MIN;
    } else if (newRpm > SPINDLE_RPM_MAX) {
        newRpm = SPINDLE_RPM_MAX;
    }
    uint32_t newEncHz = rpmToEncoderHz(newRpm);

    uint32_t newFeedMmMin = PLOTTER_FEED_MM_MIN +
        ((PLOTTER_FEED_MM_MAX - PLOTTER_FEED_MM_MIN) * elapsedMs) / RAMP_MS;
    newFeedMmMin = (newFeedMmMin / RAMP_STEP) * RAMP_STEP;
    if (newFeedMmMin < PLOTTER_FEED_MM_MIN) {
        newFeedMmMin = PLOTTER_FEED_MM_MIN;
    } else if (newFeedMmMin > PLOTTER_FEED_MM_MAX) {
        newFeedMmMin = PLOTTER_FEED_MM_MAX;
    }

    spindleRpm = newRpm;
    spindleEncHz = newEncHz;
    plotterFeedMmMin = newFeedMmMin;

    setSpindleEncoderHz(newEncHz);
    setStepperFromFeed(newFeedMmMin);
    statusDisplayDirty = true;
}

void setup()
{
    initStepperPins();

    lcd.begin(20, 4);

    showMarquee(0);
    showCncStatus();
#if defined(USE_LCD_SAFE_ASYNC)
    lcd.flush();
#endif

    initTimer1Encoder();
    initTimer5Stepper();
    initTimer3Ramp();
}

void loop()
{
    processLcd(50);

    static uint32_t lastScroll = 0;
    if (millis() - lastScroll >= 200) {
        lastScroll = millis();

        static uint16_t scrollPos = 0;
        showMarquee(scrollPos);
        scrollPos = (scrollPos + 1) % MARQUEE_LEN;

        processLcd(2000);
    }

    static uint32_t lastStatus = 0;
    if (statusDisplayDirty || (millis() - lastStatus >= 250)) {
        lastStatus = millis();
        statusDisplayDirty = false;
        showCncStatus();
        processLcd(2000);
    }
}
