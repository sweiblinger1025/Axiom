#pragma once
#include <string>

struct ax_last_error {
    std::string msg;
    void set(const char* s) { msg = (s ? s : ""); }
    void set(const std::string& s) { msg = s; }
    const char* c_str() const { return msg.c_str(); }
};
