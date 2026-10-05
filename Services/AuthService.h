#pragma once

#include <string>

class AuthService {
public:
    bool login(const std::string& email, const std::string& password);
    bool login(const std::string& email, const std::string& password, std::string& token, std::string& rol);
};
