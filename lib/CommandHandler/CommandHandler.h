#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include <algorithm>
#include <cctype>
#include <string>

enum class CommandType {
    UNKNOWN,
    LED_GREEN,
    LED_RED,
    LED_BLUE,
    LED_YELLOW,
    LED_CYAN,
    LED_PURPLE,
    LED_OFF,
    MODE_AUTO,
    RESERVE,
    CANCEL_RESERVATION
};

struct CommandResult {
    CommandType type;
    bool modifiesLedOverride;
    bool manualLedOverride;
    bool modifiesReservation;
    bool isReserved;
};

class CommandHandler {
public:
    static std::string trim(const std::string& str) {
        size_t first = str.find_first_not_of(" \t\r\n");
        if (first == std::string::npos)
            return "";
        size_t last = str.find_last_not_of(" \t\r\n");
        return str.substr(first, (last - first + 1));
    }

    static std::string toLower(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
        return s;
    }

    static CommandResult parse(const std::string& rawCommand) {
        std::string cmd = toLower(trim(rawCommand));

        CommandResult res;
        res.type = CommandType::UNKNOWN;
        res.modifiesLedOverride = false;
        res.manualLedOverride = false;
        res.modifiesReservation = false;
        res.isReserved = false;

        if (cmd.empty()) {
            return res;
        }

        if (cmd == "led-on" || cmd == "led-green") {
            res.type = CommandType::LED_GREEN;
            res.modifiesLedOverride = true;
            res.manualLedOverride = true;
        } else if (cmd == "led-red") {
            res.type = CommandType::LED_RED;
            res.modifiesLedOverride = true;
            res.manualLedOverride = true;
        } else if (cmd == "led-blue") {
            res.type = CommandType::LED_BLUE;
            res.modifiesLedOverride = true;
            res.manualLedOverride = true;
        } else if (cmd == "led-yellow") {
            res.type = CommandType::LED_YELLOW;
            res.modifiesLedOverride = true;
            res.manualLedOverride = true;
        } else if (cmd == "led-cyan") {
            res.type = CommandType::LED_CYAN;
            res.modifiesLedOverride = true;
            res.manualLedOverride = true;
        } else if (cmd == "led-purple") {
            res.type = CommandType::LED_PURPLE;
            res.modifiesLedOverride = true;
            res.manualLedOverride = true;
        } else if (cmd == "led-off") {
            res.type = CommandType::LED_OFF;
            res.modifiesLedOverride = true;
            res.manualLedOverride = false;
        } else if (cmd == "auto") {
            res.type = CommandType::MODE_AUTO;
            res.modifiesLedOverride = true;
            res.manualLedOverride = false;
        } else if (cmd == "reserve") {
            res.type = CommandType::RESERVE;
            res.modifiesReservation = true;
            res.isReserved = true;
        } else if (cmd == "cancel-reservation") {
            res.type = CommandType::CANCEL_RESERVATION;
            res.modifiesReservation = true;
            res.isReserved = false;
        }

        return res;
    }
};

#endif  // COMMAND_HANDLER_H
