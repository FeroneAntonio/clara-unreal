#pragma once
#include "ClaraCyberModel.h"

namespace ClaraCyber {
// A pending test owns no model effects. Only its final result may update the exercise.
class Terminal {
public:
    std::string command(Model& model, const std::string& input);
    bool tick(Model& model, double seconds);
    bool cancel();
    bool busy() const { return active; }
    const std::string& transcript() const { return history; }
private:
    bool active = false, reachable = false;
    int day = 0, sent = 0;
    char path = 0;
    double elapsed = 0, interval = 1;
    std::string pending, preview, address, history;
    void append(const std::string& text);
};

// These explanations inspect state and never award points or complete a test.
const char* CurrentTask(const Model& model);
const char* CommandFeedback(const Model& model, const std::string& input, const std::string& result);
const char* ActionFeedback(const Model& model, const std::string& name, int number, const std::string& value, bool accepted);
const char* NormalLogFeedback(int day);
}
