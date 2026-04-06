/* pc_kbd.c - Poll for keyboard events from the PicoCalc MCU via I2C */

#include "periodic.h"
#include <hardware/gpio.h>
#include <hardware/i2c.h>
#include <stdbool.h>
#include <stdio.h>

#define FAST    1

#define KEY_ALT         0xA1
#define KEY_SHL         0xA2
#define KEY_SHR         0xA3
#define KEY_SYM         0xA4
#define KEY_CTRL        0xA5
#define KEY_CAPS_LOCK   0xC1
#define KEY_BREAK       0xD0

#define MOD_ALT         0x01
#define MOD_SHL         0x02
#define MOD_SHR         0x04
#define MOD_SYM         0x08
#define MOD_CTRL        0x10
#define MOD_CAPLOCK     0x20

#define KNO_ALT         0
#define KNO_SHL         1
#define KNO_SHR         2
#define KNO_SYM         3
#define KNO_CTRL        4
#define KNO_CAPLOCK     5
#define KNO_A           6
// Keys A to Z are key numbers 6 to 31
#define KNO_I           ('I' - 'A' + KNO_A)
#define KNO_0           32      // '0)'
#define KNO_1           33      // '1!'
#define KNO_2           34      // '2@'
#define KNO_3           35      // '3#'
#define KNO_4           36      // '4$'
#define KNO_5           37      // '5%'
#define KNO_6           38      // '6^'
#define KNO_7           39      // '7&'
#define KNO_8           40      // '8*'
#define KNO_9           41      // '9('
#define KNO_SPACE       42      // Space
#define KNO_SEMICOLON   43      // ';:'
#define KNO_QUOTE       44      // '\'"'
#define KNO_COMMA       45      // ',<'
#define KNO_STOP        46      // '.>'
#define KNO_ENTER       47      // Enter
#define KNO_BQUOTE      48      // '`~'
#define KNO_FSLASH      49      // '/?'
#define KNO_BSLASH      50      // '\\|'
#define KNO_MINUS       51      // '-_'
#define KNO_EQUALS      52      // '=+'
#define KNO_LSQBKT      53      // '[{'
#define KNO_RSQBKT      54      // ']}'
#define KNO_ESC         55      // Esc
#define KNO_BACK        56      // Backspace
#define KNO_LEFT        57      // Left
#define KNO_RIGHT       58      // Right
#define KNO_DOWN        59      // Down
#define KNO_UP          60      // Up
#define KNO_TAB         61      // Tab, Home
#define KNO_DEL         62      // Del, End
#define KNO_F1          63      // F1, F6
// Keys F1 to F5 are key numbers 63 to 67
#define KNO_COUNT       68

#define KNO_ALPHA(x)    (x - 'A' + KNO_A)
#define KNO_NUM(x)      (x + KNO_0)
#define KNO_FUNC(x)     (x + KNO_F1)

// BBC Basic key codes
#define KBB_CTRL_LEFT   0x80
#define KBB_CTRL_RIGHT  0x81
#define KBB_HOME        0x82
#define KBB_END         0x83
#define KBB_PGUP        0x84
#define KBB_PGDN        0x85
#define KBB_INS         0x86
#define KBB_DEL         0x87
#define KBB_LEFT        0x88
#define KBB_RIGHT       0x89
#define KBB_DOWN        0x8A
#define KBB_UP          0x8B
#define KBB_F1          0x91
#define KBB_SHFT_TAB    0x9B
#define KBB_CTRL_HOME   0x9C
#define KBB_CTRL_END    0x9D
#define KBB_CTRL_PGUP   0x9E
#define KBB_CTRL_PGDN   0x9F
#define KBB_SHFT_F1     0xA1
#define KBB_CTRL_F1     0xB1
#define KBB_ALT_F1      0xC1

typedef struct
    {
    uint8_t from;
    uint8_t to;
    } KEYMAP;

static const KEYMAP pc_kmap[] =
    {
    {0x08, KNO_BACK},       // Backspace
    {0x09, KNO_TAB},        // Tab
    {0x0A, KNO_ENTER},      // Enter
    {0x20, KNO_SPACE},      // ' '
    {0x21, KNO_1},          // '!'
    {0x22, KNO_QUOTE},      // '"'
    {0x23, KNO_3},          // '#'
    {0x24, KNO_4},          // '$'
    {0x25, KNO_5},          // '%'
    {0x26, KNO_7},          // '&'
    {0x27, KNO_QUOTE},      // '\''
    {0x28, KNO_9},          // '('
    {0x29, KNO_0},          // ')'
    {0x2A, KNO_8},          // '*'
    {0x2B, KNO_EQUALS},     // '+'
    {0x2C, KNO_COMMA},      // ','
    {0x2D, KNO_MINUS},      // '-'
    {0x2E, KNO_STOP},       // '.'
    {0x2F, KNO_FSLASH},     // '/'
    {0x3A, KNO_SEMICOLON},  // ':'
    {0x3B, KNO_SEMICOLON},  // ';'
    {0x3C, KNO_COMMA},      // '<'
    {0x3D, KNO_EQUALS},     // '='
    {0x3E, KNO_STOP},       // '>'
    {0x3F, KNO_FSLASH},     // '?'
    {0x40, KNO_2},          // '@'
    {0x5B, KNO_LSQBKT},     // '['
    {0x5C, KNO_BSLASH},     // '\\'
    {0x5D, KNO_RSQBKT},     // ']'
    {0x5E, KNO_6},          // '^'
    {0x5F, KNO_MINUS},      // '_'
    {0x60, KNO_BQUOTE},     // '`'
    {0x7B, KNO_LSQBKT},     // '{'
    {0x7C, KNO_BSLASH},     // '|'
    {0x7D, KNO_RSQBKT},     // '}'
    {0x7E, KNO_BQUOTE},     // '~'
    {0xB1, KNO_ESC},        // Esc
    {0xB4, KNO_LEFT},       // Left
    {0xB5, KNO_UP},         // Up
    {0xB6, KNO_DOWN},       // Down
    {0xB7, KNO_RIGHT},      // Right
    {0xD1, KNO_I},          // Insert
    {0xD2, KNO_TAB},        // Home
    {0xD4, KNO_DEL},        // Del
    {0xD5, KNO_DEL},        // End
    {0xD6, KNO_UP},         // Page Up
    {0xD7, KNO_DOWN}        // Page Down
    };

static const uint8_t num_shft[] = {')', '!', '@', '#', '$', '%', '^', '&', '*', '('};
static const uint8_t asc_unsh[] = {' ', ';', '\'', ',', '.', '\r', '`', '/', '\\', '-', '=', '[', ']', '\e', '\b'};
static const uint8_t asc_shft[] = {' ', ':', '"', '<', '>', '\r', '~', '?', '|', '_', '+', '{', '}', '\e', '\b'};

static const KEYMAP test_key[] =
    {
    {2, KNO_CTRL },         // CTRL
    {3, KNO_ALT },          // ALT
    {4, KNO_SHL },          // left SHIFT
    {5, KNO_CTRL },         // left CTRL
    {6, KNO_ALT },          // left ALT
    {7, KNO_SHR },          // right SHIFT
    {17, KNO_ALPHA('Q') },  // Q
    {18, KNO_NUM(3) },      // 3
    {19, KNO_NUM(4) },      // 4
    {20, KNO_NUM(5) },      // 5
    {21, KNO_FUNC(4) },     // F4
    {22, KNO_NUM(8) },      // 8
    {24, KNO_MINUS },       // -
    {26, KNO_LEFT },        // LEFT
    {34, KNO_ALPHA('W') },  // W
    {35, KNO_ALPHA('E') },  // E
    {36, KNO_ALPHA('T') },  // T
    {37, KNO_NUM(7) },      // 7
    {38, KNO_ALPHA('I') },  // I
    {39, KNO_NUM(9) },      // 9
    {40, KNO_NUM(0) },      // 0
    {42, KNO_DOWN },        // DOWN
    {46, KNO_BQUOTE },      // `
    {48, KNO_BACK },        // BACKSPACE
    {49, KNO_NUM(1) },      // 1
    {50, KNO_NUM(2) },      // 2
    {51, KNO_ALPHA('D') },  // D
    {52, KNO_ALPHA('R') },  // R
    {53, KNO_NUM(6) },      // 6
    {54, KNO_ALPHA('U') },  // U
    {55, KNO_ALPHA('O') },  // O
    {56, KNO_ALPHA('P') },  // P
    {57, KNO_LSQBKT },      // [
    {58, KNO_UP },          // UP
    {65, KNO_CAPLOCK },     // CAPS LOCK
    {66, KNO_ALPHA('A') },  // A
    {67, KNO_ALPHA('X') },  // X
    {68, KNO_ALPHA('F') },  // F
    {69, KNO_ALPHA('Y') },  // Y
    {70, KNO_ALPHA('J') },  // J
    {71, KNO_ALPHA('K') },  // K
    {74, KNO_ENTER },       // RETURN
    {82, KNO_ALPHA('S') },  // S
    {83, KNO_ALPHA('C') },  // C
    {84, KNO_ALPHA('G') },  // G
    {85, KNO_ALPHA('H') },  // H
    {86, KNO_ALPHA('N') },  // N
    {87, KNO_ALPHA('L') },  // L
    {88, KNO_SEMICOLON },   // ;
    {89, KNO_RSQBKT },      // ]
    {90, KNO_DEL },         // DELETE
    {97, KNO_TAB },         // TAB
    {98, KNO_ALPHA('Z') },  // Z
    {99, KNO_SPACE },       // SPACE
    {100, KNO_ALPHA('V') }, // V
    {101, KNO_ALPHA('B') }, // B
    {102, KNO_ALPHA('M') }, // M
    {103, KNO_COMMA },      // ,
    {104, KNO_STOP },       // .
    {105, KNO_FSLASH },     // /
    {113, KNO_ESC },        // ESCAPE
    {114, KNO_FUNC(1) },    // 1
    {115, KNO_FUNC(2) },    // 2
    {116, KNO_FUNC(3) },    // 3
    {117, KNO_FUNC(5) },    // 5
    {121, KNO_BSLASH },     // \
    {122, KNO_RIGHT }       // RIGHT
    };
    
static uint32_t key_state[(KNO_COUNT - 1) / 32 + 1];

bool bPrtScrn = false;

// Defined in BBC.h
#define ESCFLG  0x80;
extern unsigned char flags ;	// BASIC's Boolean flags byte

void putkey(uint8_t key);

static void pc_kbd_init (void)
    {
    gpio_pull_up (PICO_KEYBOARD_SCL_PIN);
    gpio_pull_up (PICO_KEYBOARD_SDA_PIN);
    gpio_set_function (PICO_KEYBOARD_SCL_PIN, GPIO_FUNC_I2C);
    gpio_set_function (PICO_KEYBOARD_SDA_PIN, GPIO_FUNC_I2C);
    int speed = i2c_init(I2C_INSTANCE(PICO_KEYBOARD_I2C), PICO_KEYBOARD_SPEED);
    // uint8_t test;
    // int stat = i2c_read_blocking (I2C_INSTANCE(PICO_KEYBOARD_I2C), PICO_KEYBOARD_ADDR, &test, 1, false);
    // printf ("I2C Status = %d\n", stat);
#if FAST
    i2c_inst_t *i2c = I2C_INSTANCE(PICO_KEYBOARD_I2C);
    i2c->hw->enable = 0;
    i2c->hw->tar = PICO_KEYBOARD_ADDR;
    i2c->hw->enable = 1;
#endif
    }

static void pc_lcd_backlight (uint8_t bright)
    {
    uint8_t msg[2];
    uint8_t rpl[2];
    msg[0] = 0x85;
    msg[1] = bright;
    // i2c_write_timeout_us (I2C_INSTANCE(PICO_KEYBOARD_I2C), PICO_KEYBOARD_ADDR, msg, 2, false, 1000);
    i2c_write_blocking (I2C_INSTANCE(PICO_KEYBOARD_I2C), PICO_KEYBOARD_ADDR, msg, 2, false);
    sleep_us (1000);
    // i2c_read_timeout_us (I2C_INSTANCE(PICO_KEYBOARD_I2C), PICO_KEYBOARD_ADDR, msg, 2, false, 1000);
    int nrd = i2c_read_blocking (I2C_INSTANCE(PICO_KEYBOARD_I2C), PICO_KEYBOARD_ADDR, rpl, 2, false);
    // printf ("nrd = %d, rpl = %d, %d\n", nrd, rpl[0], rpl[1]);
    }

static PRD_FUNC pnext = NULL;

static uint8_t mapkey (const KEYMAP *map, int n2, uint8_t key)
    {
    int n1 = -1;
    while (n2 - n1 > 1)
        {
        int n3 = (n1 + n2) / 2;
        if (key == map[n3].from)
            {
            return map[n3].to;
            }
        else if (key < map[n3].from)
            {
            n2 = n3;
            }
        else
            {
            n1 = n3;
            }
        }
    return 0xFF;
    }

static uint8_t key_no (uint8_t key)
    {
    if ((key >= 'A') && (key <= 'Z'))
        {
        key -= 'A' - KNO_A;
        }
    else if ((key >= 'a') && (key <= 'z'))
        {
        key -= 'a' - KNO_A;
        }
    else if ((key >= '0') && (key <= '9'))
        {
        key -= '0' - KNO_0;
        }
    else
        {
        key = mapkey (pc_kmap, count_of (pc_kmap), key);
        }
    return key;
    }

static void key_down (uint8_t key)
    {
    int kb = key >> 5;
    key &= 0x1F;
    key_state[kb] |= 1 << key;
    }

static void key_up (uint8_t key)
    {
    int kb = key >> 5;
    key &= 0x1F;
    key_state[kb] &= ~(1 << key);
    }

static int key_test (uint8_t key)
    {
    int kb = key >> 5;
    key &= 0x1F;
    if (key_state[kb] & (1 << key)) key = -1;
    else key = 0;
    return key;
    }

static uint8_t asc_key (uint8_t key)
    {
    if (key < KNO_A)
        {
        key = 0;
        }
    else if (key < KNO_0)
        {
        // Letter keys
        if ((key == KNO_I) && (key_state[0] & MOD_ALT))
            {
            // Alt + I = Ins
            key = KBB_INS;
            }
        else
            {
            key += 'a' - KNO_A;
            if (key_state[0] & (MOD_SHL | MOD_SHR | MOD_CAPLOCK)) key &= 0x5F;
            if (key_state[0] & MOD_CTRL) key &= 0x1F;
            }
        }
    else if (key < KNO_SPACE)
        {
        // Number keys
        key -= KNO_0;
        if (key_state[0] & (MOD_SHL | MOD_SHR)) key = num_shft[key];
        else key += '0';
        if (key_state[0] & MOD_CTRL) key &= 0x1F;
        }
    else if (key < KNO_LEFT)
        {
        // Symbol keys
        key -= KNO_SPACE;
        if (key_state[0] & (MOD_SHL | MOD_SHR)) key = asc_shft[key];
        else key = asc_unsh[key];
        if (key_state[0] & MOD_CTRL) key &= 0x1F;
        }
    else if (key < KNO_DOWN)
        {
        // Left or Right
        if (key_state[0] & MOD_CTRL) key += KBB_CTRL_LEFT - KNO_LEFT;
        else key += KBB_LEFT - KNO_LEFT;
        }
    else if (key < KNO_TAB)
        {
        // Down or Up
        if (key_state[0] & (MOD_SHL | MOD_SHR))
            {
            // Page Down or Page Up
            if (key_state[0] & MOD_CTRL) key += KBB_CTRL_PGDN - KNO_DOWN;
            else key += KBB_PGDN - KNO_DOWN;
            }
        else
            {
            key += KBB_DOWN - KNO_DOWN;
            }
        }
    else if (key == KNO_TAB)
        {
        if (key_state[0] & MOD_SHL)
            {
            // Home
            if (key_state[0] & MOD_CTRL) key = KBB_CTRL_HOME;
            else key = KBB_HOME;
            }
        else
            {
            // Tab
            if (key_state[0] & MOD_SHR) key = KBB_SHFT_TAB;
            else key = '\t';
            }
        }
    else if (key == KNO_DEL)
        {
        if (key_state[0] & (MOD_SHL | MOD_SHR))
            {
            // End
            if (key_state[0] & MOD_CTRL) key = KBB_CTRL_END;
            else key = KBB_END;
            }
        else
            {
            // Del
            key = KBB_DEL;
            }
        }
    else if (key < KNO_COUNT)
        {
        // Function keys
        key += KBB_F1 - KNO_F1;
        if (key_state[0] & MOD_SHL) key += 5;
        if (key_state[0] & MOD_SHR) key += KBB_SHFT_F1 - KBB_F1;
        else if (key_state[0] & MOD_CTRL) key += KBB_CTRL_F1 - KBB_F1;
        else if (key_state[0] & MOD_ALT) key += KBB_ALT_F1 - KBB_F1;
        }
    else
        {
        key = 0;
        }
    return key;
    }

static void pc_kbd_poll (void)
    {
    uint8_t msg[2];
#if FAST
    i2c_inst_t *i2c = I2C_INSTANCE(PICO_KEYBOARD_I2C);
    bool txempty = i2c->hw->raw_intr_stat & I2C_IC_RAW_INTR_STAT_TX_EMPTY_BITS;
    size_t nrd = i2c_get_hw(i2c)->rxflr;
    if (txempty && (nrd >= 2))
        {
        while (nrd > 2)
            {
            uint32_t dump = i2c_get_hw(i2c)->data_cmd;
            --nrd;
            }
        msg[0] = (uint8_t) i2c_get_hw(i2c)->data_cmd;
        msg[1] = (uint8_t) i2c_get_hw(i2c)->data_cmd;
#else
    int nrd = i2c_read_timeout_us(I2C_INSTANCE(PICO_KEYBOARD_I2C), PICO_KEYBOARD_ADDR, msg, 2, false, 10000);
    if (nrd == 2)
        {
#endif
        uint8_t key = msg[1];
        // printf ("kbd status = (0x%02X, 0x%02X)\n", msg[0], msg[1]);
        if (msg[0] == 1)
            {
            // Key down
            if (key == KEY_BREAK)
                {
                flags |= ESCFLG;
                }
            else if ((key >= KEY_ALT) && (key <= KEY_CTRL))
                {
                key_state[0] |= 1 << (key - KEY_ALT);
                }
            else if (key == KEY_CAPS_LOCK)
                {
                key_state[0] ^= MOD_CAPLOCK;
                }
            else
                {
                key = key_no (key);
                if (key < KNO_COUNT)
                    {
                    key_down (key);
                    key = asc_key (key);
                    if (key > 0) putkey (key);
                    }
                }
            }
        else if (msg[0] == 3)
            {
            // Key up
            if (key == KEY_BREAK)
                {
                }
            else if ((key >= KEY_ALT) && (key <= KEY_CTRL))
                {
                key_state[0] &= ~(1 << (key - KEY_ALT));
                }
            else if (key == KEY_CAPS_LOCK)
                {
                }
            else
                {
                key = key_no (key);
                if (key < KNO_COUNT) key_up (key);
                }
            }
        }
    uint8_t req = 0x09;
#if FAST
    if (txempty)
        {
        i2c->hw->data_cmd = req;
        i2c->hw->data_cmd = I2C_IC_DATA_CMD_CMD_BITS;
        i2c->hw->data_cmd = I2C_IC_DATA_CMD_CMD_BITS | I2C_IC_DATA_CMD_STOP_BITS;
        }
#else
    int nwr = i2c_write_timeout_us(I2C_INSTANCE(PICO_KEYBOARD_I2C), PICO_KEYBOARD_ADDR, &req, 1, false, 10000);
#endif
    // printf ("nwr = %d\n", nwr);
    if (pnext) pnext();
    }

void setup_keyboard (void)
    {
    pc_kbd_init();
    pc_lcd_backlight (128);
    pnext = add_periodic (pc_kbd_poll);
    }

int testkey (int key)
    {
    key = mapkey (test_key, count_of(test_key), key);
    if (key == 0xFF) key = 0;
    else key = key_test (key);
    return key;
    }
