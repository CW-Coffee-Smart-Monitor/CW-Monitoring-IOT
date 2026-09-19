#include <unity.h>

#include "TableLogic.h"

void setUp(void) {
    // runs before each test
}

void tearDown(void) {
    // runs after each test
}

void test_occupancy_detection(void) {
    TableManager table(12, 20.0f, 15000);

    TEST_ASSERT_TRUE(table.isOccupied(10.5f));
    TEST_ASSERT_TRUE(table.isOccupied(19.9f));
    TEST_ASSERT_FALSE(table.isOccupied(20.0f));
    TEST_ASSERT_FALSE(table.isOccupied(35.0f));
    TEST_ASSERT_FALSE(table.isOccupied(-1.0f));  // sensor error / out of range
}

void test_checkin_success_when_occupied(void) {
    TableManager table(12, 20.0f, 15000);

    TapResult res = table.handleRFIDTap("AA:BB:CC:DD", 15.0f, true);

    TEST_ASSERT_EQUAL(TapResultType::CHECK_IN_SUCCESS, res.type);
    TEST_ASSERT_EQUAL_STRING("AA:BB:CC:DD", res.uid.c_str());
    TEST_ASSERT_TRUE(table.isCheckedIn());
    TEST_ASSERT_EQUAL_STRING("AA:BB:CC:DD", table.getCurrentUID().c_str());
}

void test_checkin_rejected_when_uid_disallowed(void) {
    TableManager table(12, 20.0f, 15000);

    TapResult res = table.handleRFIDTap("BAD_CARD_123", 10.0f, false);

    TEST_ASSERT_EQUAL(TapResultType::REJECTED_UID_NOT_ALLOWED, res.type);
    TEST_ASSERT_EQUAL_STRING("UID_NOT_ALLOWED", res.reason.c_str());
    TEST_ASSERT_FALSE(table.isCheckedIn());
}

void test_checkin_rejected_when_not_occupied(void) {
    TableManager table(12, 20.0f, 15000);

    // Card is valid, but distance is > 20cm (nobody is sitting)
    TapResult res = table.handleRFIDTap("AA:BB:CC:DD", 50.0f, true);

    TEST_ASSERT_EQUAL(TapResultType::CHECK_IN_REJECTED_NOT_OCCUPIED, res.type);
    TEST_ASSERT_EQUAL_STRING("NOT_OCCUPIED", res.reason.c_str());
    TEST_ASSERT_FALSE(table.isCheckedIn());
}

void test_checkout_same_card(void) {
    TableManager table(12, 20.0f, 15000);

    // Initial checkin
    table.handleRFIDTap("USER_01", 10.0f, true);
    TEST_ASSERT_TRUE(table.isCheckedIn());

    // Tap same card to check out
    TapResult res = table.handleRFIDTap("USER_01", 10.0f, true);
    TEST_ASSERT_EQUAL(TapResultType::CHECK_OUT_SUCCESS, res.type);
    TEST_ASSERT_EQUAL_STRING("USER_01", res.uid.c_str());
    TEST_ASSERT_FALSE(table.isCheckedIn());
    TEST_ASSERT_EQUAL_STRING("", table.getCurrentUID().c_str());
}

void test_reject_different_card_when_already_checked_in(void) {
    TableManager table(12, 20.0f, 15000);

    // Checkin USER_01
    table.handleRFIDTap("USER_01", 10.0f, true);

    // USER_02 taps
    TapResult res = table.handleRFIDTap("USER_02", 10.0f, true);
    TEST_ASSERT_EQUAL(TapResultType::REJECTED_ALREADY_USED_BY_OTHER, res.type);
    TEST_ASSERT_EQUAL_STRING("TABLE_ALREADY_USED_BY_OTHER_UID", res.reason.c_str());
    TEST_ASSERT_EQUAL_STRING("USER_01", res.activeUID.c_str());
    TEST_ASSERT_TRUE(table.isCheckedIn());
    TEST_ASSERT_EQUAL_STRING("USER_01", table.getCurrentUID().c_str());
}

void test_auto_checkout_timeout_trigger(void) {
    TableManager table(12, 20.0f, 15000);  // 15s timeout
    table.handleRFIDTap("USER_01", 10.0f, true);
    table.resetOccupiedTimer(1000);

    std::string checkedOutUID;

    // Meja masih ada orang pada t=2000
    AutoCheckoutResult r1 = table.updateAutoCheckout(15.0f, 2000, checkedOutUID);
    TEST_ASSERT_EQUAL(AutoCheckoutResult::NONE, r1);
    TEST_ASSERT_TRUE(table.isCheckedIn());

    // Meja ditinggal (jarak 100cm) pada t=3000 -> warning
    AutoCheckoutResult r2 = table.updateAutoCheckout(100.0f, 3000, checkedOutUID);
    TEST_ASSERT_EQUAL(AutoCheckoutResult::WARNING_TRIGGERED, r2);
    TEST_ASSERT_TRUE(table.isCheckedIn());

    // Meja masih kosong pada t=10000 (belum 15 detik sejak t=2000)
    AutoCheckoutResult r3 = table.updateAutoCheckout(100.0f, 10000, checkedOutUID);
    TEST_ASSERT_EQUAL(AutoCheckoutResult::NONE, r3);
    TEST_ASSERT_TRUE(table.isCheckedIn());

    // Meja kosong melewati 15 detik (t=17001, delta = 15001ms)
    AutoCheckoutResult r4 = table.updateAutoCheckout(100.0f, 17001, checkedOutUID);
    TEST_ASSERT_EQUAL(AutoCheckoutResult::TIMEOUT_TRIGGERED, r4);
    TEST_ASSERT_EQUAL_STRING("USER_01", checkedOutUID.c_str());
    TEST_ASSERT_FALSE(table.isCheckedIn());
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_occupancy_detection);
    RUN_TEST(test_checkin_success_when_occupied);
    RUN_TEST(test_checkin_rejected_when_uid_disallowed);
    RUN_TEST(test_checkin_rejected_when_not_occupied);
    RUN_TEST(test_checkout_same_card);
    RUN_TEST(test_reject_different_card_when_already_checked_in);
    RUN_TEST(test_auto_checkout_timeout_trigger);
    return UNITY_END();
}
