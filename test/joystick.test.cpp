//#ifdef UNIT_TEST

#include "joystick.h"

void test_struct() {
    JoystickInput_t joystickInput = {
        .report_id = 0x30
    };
    uint8_t * data = (uint8_t *)&joystickInput;
    TEST_ASSERT_EQUAL(0x30, joystickInput.report_id);
    TEST_ASSERT_EQUAL(0x30, data[0]);
}


int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_struct);
    UNITY_END();
    return 0;
}

//#endif