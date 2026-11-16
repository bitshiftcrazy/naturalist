#include QMK_KEYBOARD_H
#include "keymap_german.h"

enum layers {
    _BASE,
    _NAV,
};

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [_BASE] = LAYOUT(
        // Q       W       E       R       T            Y       U       I       O       P
        DE_Q,     DE_W,   DE_E,   DE_R,   DE_T,        DE_Y,   DE_U,   DE_I,   DE_O,   DE_P,

        // A       S       D       F       G            H       J       K       L       ;
        DE_A,     DE_S,   DE_D,   DE_F,   DE_G,        DE_H,   DE_J,   DE_K,   DE_L,   DE_SCLN,

        // Z       X       C       V       B       Tab   N       M       <       >       ?/
        DE_Z,     DE_X,   DE_C,   DE_V,   DE_B,   KC_TAB,
        DE_N,     DE_M,   DE_LABK, DE_RABK, DE_QUES,

        // Shift    Ctrl     Mod      Alt       Fn         Enter    Back     Esc      AltGr    Ctrl     Shift
        KC_LSFT,  KC_LCTL, KC_LGUI, KC_LALT, LT(_NAV, KC_SPC),
        KC_ENT,   KC_BSPC, KC_ESC,  KC_RALT, KC_RCTL, KC_RSFT
    ),

    [_NAV] = LAYOUT(
        // Fn + top row: 1..0, left-to-right
        KC_1,     KC_2,     KC_3,     KC_4,     KC_5,
        KC_6,     KC_7,     KC_8,     KC_9,     KC_0,

        // Fn + H = F1 help, Fn + J = up, K/L = PgUp/PgDn, , = Home
        _______,  _______,  _______,  _______,  _______,
        KC_F1,    KC_UP,    KC_PGUP,  KC_PGDN,  KC_HOME,

        // Right-side bottom: N M < > = left/down/right/end, ?/ = F2 cheatsheet
        _______,  _______,  _______,  _______,  _______,  _______,
        KC_LEFT,  KC_DOWN,  KC_RGHT,  KC_END,   KC_F2,

        // Keep all modifiers, navigation, Enter/Back/Esc transparent
        _______,  _______,  _______,  _______,  _______,
        _______,  _______,  _______,  _______,  _______, _______
    ),
};
