#ifndef TABLE_LOGIC_H
#define TABLE_LOGIC_H

#include <string>

enum class TapResultType {
    REJECTED_UID_NOT_ALLOWED,
    CHECK_IN_SUCCESS,
    CHECK_IN_REJECTED_NOT_OCCUPIED,
    CHECK_OUT_SUCCESS,
    REJECTED_ALREADY_USED_BY_OTHER
};

struct TapResult {
    TapResultType type;
    std::string uid;
    std::string reason;
    std::string activeUID;
};

enum class AutoCheckoutResult { NONE, WARNING_TRIGGERED, TIMEOUT_TRIGGERED };

class TableManager {
public:
    explicit TableManager(int tableId = 12, float occupiedThresholdCm = 20.0f,
                          unsigned long autoCheckoutTimeoutMs = 15000)
        : tableId_(tableId),
          occupiedThresholdCm_(occupiedThresholdCm),
          autoCheckoutTimeoutMs_(autoCheckoutTimeoutMs),
          isCheckedIn_(false),
          isReserved_(false),
          currentUID_(""),
          lastOccupiedTime_(0),
          warningPrinted_(false) {}

    bool isOccupied(float distanceCm) const { return (distanceCm > 0.0f && distanceCm < occupiedThresholdCm_); }

    bool isCheckedIn() const { return isCheckedIn_; }
    void setCheckedIn(bool val) { isCheckedIn_ = val; }

    bool isReserved() const { return isReserved_; }
    void setReserved(bool val) { isReserved_ = val; }

    const std::string& getCurrentUID() const { return currentUID_; }
    void setCurrentUID(const std::string& uid) { currentUID_ = uid; }

    int getTableId() const { return tableId_; }
    float getOccupiedThresholdCm() const { return occupiedThresholdCm_; }
    unsigned long getAutoCheckoutTimeoutMs() const { return autoCheckoutTimeoutMs_; }

    TapResult handleRFIDTap(const std::string& tappedUID, float distanceCm, bool isUIDAllowed) {
        if (!isUIDAllowed) {
            return {TapResultType::REJECTED_UID_NOT_ALLOWED, tappedUID, "UID_NOT_ALLOWED", currentUID_};
        }

        bool occupied = isOccupied(distanceCm);

        if (!isCheckedIn_) {
            if (occupied) {
                currentUID_ = tappedUID;
                isCheckedIn_ = true;
                isReserved_ = false;
                return {TapResultType::CHECK_IN_SUCCESS, currentUID_, "", ""};
            } else {
                return {TapResultType::CHECK_IN_REJECTED_NOT_OCCUPIED, tappedUID, "NOT_OCCUPIED", ""};
            }
        }

        // Already checked in: tapping same card triggers checkout
        if (tappedUID == currentUID_) {
            std::string checkedOutUID = currentUID_;
            isCheckedIn_ = false;
            currentUID_ = "";
            return {TapResultType::CHECK_OUT_SUCCESS, checkedOutUID, "", ""};
        }

        // Tapping different card while table is already occupied
        return {TapResultType::REJECTED_ALREADY_USED_BY_OTHER, tappedUID, "TABLE_ALREADY_USED_BY_OTHER_UID",
                currentUID_};
    }

    AutoCheckoutResult updateAutoCheckout(float distanceCm, unsigned long currentTimeMs,
                                          std::string& outCheckedOutUID) {
        bool occupied = isOccupied(distanceCm);

        if (!isCheckedIn_) {
            lastOccupiedTime_ = currentTimeMs;
            warningPrinted_ = false;
            return AutoCheckoutResult::NONE;
        }

        if (occupied) {
            lastOccupiedTime_ = currentTimeMs;
            warningPrinted_ = false;
            return AutoCheckoutResult::NONE;
        }

        unsigned long emptyDuration = currentTimeMs - lastOccupiedTime_;

        if (!warningPrinted_) {
            warningPrinted_ = true;
            return AutoCheckoutResult::WARNING_TRIGGERED;
        }

        if (emptyDuration >= autoCheckoutTimeoutMs_) {
            outCheckedOutUID = currentUID_;
            isCheckedIn_ = false;
            currentUID_ = "";
            warningPrinted_ = false;
            lastOccupiedTime_ = currentTimeMs;
            return AutoCheckoutResult::TIMEOUT_TRIGGERED;
        }

        return AutoCheckoutResult::NONE;
    }

    void resetOccupiedTimer(unsigned long currentTimeMs) {
        lastOccupiedTime_ = currentTimeMs;
        warningPrinted_ = false;
    }

private:
    int tableId_;
    float occupiedThresholdCm_;
    unsigned long autoCheckoutTimeoutMs_;

    bool isCheckedIn_;
    bool isReserved_;
    std::string currentUID_;

    unsigned long lastOccupiedTime_;
    bool warningPrinted_;
};

#endif  // TABLE_LOGIC_H
