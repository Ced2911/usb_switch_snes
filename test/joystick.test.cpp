//#ifdef UNIT_TEST
#include "doctest.h"
#include "joystick.h"
#include <unistd.h>


size_t test1() {
  return sizeof(struct JoystickInput_t);
}


SCENARIO("Test struct") {
    

  GIVEN("a one second sleep duration") {
    WHEN("call sleep with this duration") {
      THEN("it's expected nobody interupted our rest") {
        CHECK(test1() == 0x39)
      }
    }
  }
} 
//#endif