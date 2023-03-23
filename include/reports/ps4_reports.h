#pragma once
#include <stdint.h>

#define PS3_STICK_CENTER 0x80
#define PS3_ACCEL_CENTER 0x0200

typedef struct {
    uint8_t report_id; /* 0x5 */
    uint8_t valid_flag0;
    uint8_t valid_flag1;

    uint8_t reserved1;

    uint8_t motor_right;
    uint8_t motor_left;

    uint8_t lightbar_red;
    uint8_t lightbar_green;
    uint8_t lightbar_blue;
    uint8_t lightbar_blink_on;
    uint8_t lightbar_blink_off;
    uint8_t reserved[21];
} __attribute__((packed)) ps4_output_report;

typedef struct {
    uint8_t report_id;
    uint8_t leftStickX;
    uint8_t leftStickY;
    uint8_t rightStickX;
    uint8_t rightStickY;

    // 4 bits for the d-pad.
    union {
        struct {
            bool dpadUp : 1;
            bool dpadDown : 1;
            bool dpadLeft : 1;
            bool dpadRight : 1;
        };
        uint8_t dpad : 4;
    };

    // 14 bits for buttons.
    bool x : 1;  // square
    bool a : 1;  // cross
    bool b : 1;  // circle
    bool y : 1;  // triangle

    bool leftShoulder : 1;   // l1
    bool rightShoulder : 1;  // r1
    bool l2 : 1;             // l2
    bool r2 : 1;             // r2

    bool back : 1;  // select
    bool start : 1;
    bool leftThumbClick : 1;   // l3
    bool rightThumbClick : 1;  // r3

    bool guide : 1;    // ps
    bool capture : 1;  // touchpad click
    // 6 bit report counter.
    uint32_t report_counter : 6;

    uint32_t left_trigger : 8;
    uint32_t right_trigger : 8;

    uint32_t padding : 24;
    uint8_t mystery[22];
    uint8_t touchpad_data[8];
    uint8_t mystery_2[21];
} __attribute__((packed)) PS4Gamepad_Data_t;

typedef struct
{
    uint8_t reportId;

    uint8_t unused1;
    uint8_t strum;
    uint8_t whammy;
    uint8_t tilt;

    //      0
    //   7     1
    // 6    15   2
    //   5     3
    //      4
    union {
        struct {
            bool dpadUp : 1;
            bool dpadDown : 1;
            bool dpadLeft : 1;
            bool dpadRight : 1;
        };
        uint8_t dpad : 4;
    };
    bool x : 1;  // square, white1
    bool a : 1;  // cross, black1
    bool b : 1;  // circle, black2
    bool y : 1;  // triangle, black3

    bool leftShoulder : 1;   // white2, l1
    bool rightShoulder : 1;  // white3, r1
    bool l2 : 1;             // ghtv, l2
    bool : 1;
    bool select : 1;  // select
    bool : 1;
    bool leftThumbClick : 1;   // pause, l3
    bool rightThumbClick : 1;  // heroPower, r3

    uint8_t unused2[57];
} __attribute__((packed)) PS4GHLGuitar_Data_t;