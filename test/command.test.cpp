//#ifdef UNIT_TEST
#include "doctest.h"
#include "joystick.h"
#include <unistd.h>

extern "C" void hid_rx_cb(uint8_t *buf, uint16_t len);
extern uint8_t usbTestBuf[0x40];

static const uint8_t mac_addr[0x06] = {0x57, 0x30, 0xea, 0x8a, 0xbb, 0x7c};

SCENARIO ("Test command")
{


        GIVEN("a one second sleep duration")
        {

            WHEN("check output_report_0x01_get_device_info ")
            {
                uint8_t rq[] = {0x01, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                            0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};


                hid_rx_cb(rq, 0x40);

                // check response !
                CHECK(usbTestBuf[0x00] == 0x21);
                CHECK(usbTestBuf[0x00] == 0x21);
            }


    }
}
//#endif