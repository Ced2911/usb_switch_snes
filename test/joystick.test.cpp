//#ifdef UNIT_TEST
#include "doctest.h"
#include "joystick.h"
#include <unistd.h>

void test_struct() {
    JoystickInput_t joystickInput = {
        .report_id = 0x30
    };
    uint8_t * data = (uint8_t *)&joystickInput;
    TEST_ASSERT_EQUAL(0x30, joystickInput.report_id);
    TEST_ASSERT_EQUAL(0x30, data[0]);
}


SCENARIO("Test struct") {

  GIVEN("a one second sleep duration") {
    JoystickInput_t joystickInput = {
        .report_id = 0x30
    };
    WHEN("call sleep with this duration") {
      THEN("it's expected nobody interupted our rest") {
        CHECK(sizeof(JoystickInput_t) == 63);
      }
    }
  }
} 
//#endif