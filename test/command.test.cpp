//#ifdef UNIT_TEST
#include "doctest.h"
#include "joystick.h"
#include <unistd.h>

extern "C" void hid_rx_cb(uint8_t *buf, uint16_t len);

SCENARIO("Test command") {


            GIVEN("a one second sleep duration") {
                WHEN("check ControllerDataReport") {
                    CHECK(sizeof(struct ControllerDataReport) == 0x39);
        }
                WHEN("check SpiReadReport") {
                    CHECK(sizeof(struct SpiReadReport) == 0x39);
        }

                WHEN("check Report81Response") {
                    CHECK(sizeof(struct Report81Response) == 0x39);
        }


        WHEN("check output_report_0x01_get_device_info ") {
            uint8_t usb_out_buf[0x40] =  {0};
            struct Report81Response *resp = (struct Report81Response *)&usb_out_buf[0x01];
            usb_out_buf[0x00] = kUsbReportIdInput81;

            resp->subcommand_ack = 0x82;
            resp->subcommand = 0x02;
            resp->cmd_0x02.firmware_version = 0x4803;
            resp->cmd_0x02.device_type = 0x03;
            resp->cmd_0x02.unk_0 = 0x02;

            // mac address
            // for (int i = 0; i < 6; i++)
            //    resp->cmd_0x02.mac[i] = i;

            const uint8_t response_h[] = {0x57, 0x30, 0xea, 0x8a, 0xbb, 0x7c};
            memcpy(resp->cmd_0x02.mac, response_h, sizeof(response_h));

            resp->cmd_0x02.unk_1 = 0x01;
            resp->cmd_0x02.use_spi_colors = 0x01;


                    CHECK( (*(uint16_t *)&usb_out_buf[0xD] == 0x0282));
        }



    }
}
//#endif