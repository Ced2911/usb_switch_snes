#pragma once

// Input
#define REPORT_ID_GAMEPAD 0x30
#define REPORT_ID_SUBCOMMAND 0x21
#define REPORT_ID_0X81 0x81

// Output
/*
Rumble and subcommand.

The OUTPUT 1 report is how all normal subcommands are sent. 
It also includes rumble data.
*/
#define REPORT_ID_OUT_0x01 0x01
/*
Rumble only.
*/
#define REPORT_ID_OUT_0x16 0x10
#define REPORT_ID_OUT_0x80 0x80
#define REPORT_ID_OUT_0x82 0x82

#pragma pack(push, 1)
typedef struct
{
    /* data */
    uint8_t report_id;

    union {
        struct
        {

            // 10btn, 1bit
            // 4btn, 1bit
            // 2btn, 1bit
            union {
                struct
                {
                    uint8_t a0 : 1;
                    uint8_t a1 : 1;
                    uint8_t a2 : 1;
                    uint8_t a3 : 1;
                    uint8_t a4 : 1;
                    uint8_t a5 : 1;
                    uint8_t a6 : 1;
                    uint8_t a7 : 1;
                    uint8_t a8 : 1;
                    uint8_t a9 : 1;
                    uint8_t b0 : 1;
                    uint8_t b1 : 1;
                    uint8_t b2 : 1;
                    uint8_t b3 : 1;
                    // dummy, charging_grip
                    uint8_t c0 : 1;
                    uint8_t c1 : 1;
                } b;
                uint16_t u16;
            } btn_0;

            // 4axis, 16bit
            uint16_t lx;
            uint16_t ly;
            uint16_t rx;
            uint16_t ry;
            // 4btn, 1bit
            // 4btn, 1bit
            union {
                struct
                {
                    // dpad ?
                    uint8_t a0 : 1;
                    uint8_t a1 : 1;
                    uint8_t a2 : 1;
                    uint8_t a3 : 1;

                    // sr/sl/zl/zr ?
                    uint8_t b0 : 1;
                    uint8_t b1 : 1;
                    uint8_t b2 : 1;
                    uint8_t b3 : 1;
                } b;
                uint8_t u8;
            } btn_1;

            // 52, 8bit ...
        } joystick;
        /* data */

        uint8_t data[0x20];
    };

} JoystickInput_t;
