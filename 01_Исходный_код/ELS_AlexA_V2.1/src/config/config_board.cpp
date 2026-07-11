#include "config_board.h"

/* D30..D37 → Async..Grind на панели слева направо (инверсия от сырого scan). */
static const uint8_t mode_pin_remap[8] = BOARD_MODE_PIN_REMAP;

uint8_t board_remap_mode_pin(uint8_t scan_mode) {
    if (scan_mode < 1 || scan_mode > 8) {
        return scan_mode;
    }
    return mode_pin_remap[scan_mode - 1];
}
