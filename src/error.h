#pragma once

#include <string>

// Thrown as exception for user input errors (code 1) and internal errors (code 2).
struct Error {
    int code;
    std::string message;
};
