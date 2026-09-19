#include <unity.h>

#include "CommandHandler.h"

void setUp(void) {}
void tearDown(void) {}

void test_trim_and_case_insensitivity(void) {
    CommandResult res = CommandHandler::parse("  LED-RED \r\n ");
    TEST_ASSERT_EQUAL(CommandType::LED_RED, res.type);
    TEST_ASSERT_TRUE(res.modifiesLedOverride);
    TEST_ASSERT_TRUE(res.manualLedOverride);
}

void test_led_color_commands(void) {
    CommandResult green = CommandHandler::parse("led-green");
    TEST_ASSERT_EQUAL(CommandType::LED_GREEN, green.type);

    CommandResult onCmd = CommandHandler::parse("led-on");
    TEST_ASSERT_EQUAL(CommandType::LED_GREEN, onCmd.type);

    CommandResult blue = CommandHandler::parse("led-blue");
    TEST_ASSERT_EQUAL(CommandType::LED_BLUE, blue.type);

    CommandResult yellow = CommandHandler::parse("led-yellow");
    TEST_ASSERT_EQUAL(CommandType::LED_YELLOW, yellow.type);

    CommandResult cyan = CommandHandler::parse("led-cyan");
    TEST_ASSERT_EQUAL(CommandType::LED_CYAN, cyan.type);

    CommandResult purple = CommandHandler::parse("led-purple");
    TEST_ASSERT_EQUAL(CommandType::LED_PURPLE, purple.type);
}

void test_auto_and_off_commands(void) {
    CommandResult off = CommandHandler::parse("led-off");
    TEST_ASSERT_EQUAL(CommandType::LED_OFF, off.type);
    TEST_ASSERT_TRUE(off.modifiesLedOverride);
    TEST_ASSERT_FALSE(off.manualLedOverride);

    CommandResult autoCmd = CommandHandler::parse("auto");
    TEST_ASSERT_EQUAL(CommandType::MODE_AUTO, autoCmd.type);
    TEST_ASSERT_TRUE(autoCmd.modifiesLedOverride);
    TEST_ASSERT_FALSE(autoCmd.manualLedOverride);
}

void test_reservation_commands(void) {
    CommandResult resv = CommandHandler::parse("reserve");
    TEST_ASSERT_EQUAL(CommandType::RESERVE, resv.type);
    TEST_ASSERT_TRUE(resv.modifiesReservation);
    TEST_ASSERT_TRUE(resv.isReserved);

    CommandResult cancel = CommandHandler::parse("cancel-reservation");
    TEST_ASSERT_EQUAL(CommandType::CANCEL_RESERVATION, cancel.type);
    TEST_ASSERT_TRUE(cancel.modifiesReservation);
    TEST_ASSERT_FALSE(cancel.isReserved);
}

void test_unknown_and_empty_commands(void) {
    CommandResult empty = CommandHandler::parse("");
    TEST_ASSERT_EQUAL(CommandType::UNKNOWN, empty.type);
    TEST_ASSERT_FALSE(empty.modifiesLedOverride);
    TEST_ASSERT_FALSE(empty.modifiesReservation);

    CommandResult unknown = CommandHandler::parse("some_random_payload_123");
    TEST_ASSERT_EQUAL(CommandType::UNKNOWN, unknown.type);
    TEST_ASSERT_FALSE(unknown.modifiesLedOverride);
}

int runUnityTests(void) {
    UNITY_BEGIN();
    RUN_TEST(test_trim_and_case_insensitivity);
    RUN_TEST(test_led_color_commands);
    RUN_TEST(test_auto_and_off_commands);
    RUN_TEST(test_reservation_commands);
    RUN_TEST(test_unknown_and_empty_commands);
    return UNITY_END();
}

#ifdef ARDUINO
#include <Arduino.h>
void setup() {
    delay(2000);  // Allow serial and USB connection to stabilize
    runUnityTests();
}

void loop() {}
#else
int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    return runUnityTests();
}
#endif
