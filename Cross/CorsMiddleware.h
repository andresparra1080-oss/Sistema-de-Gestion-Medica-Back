#pragma once

#include <string>

#include "crow.h"

namespace Cors {
inline void applyHeaders(crow::response& res) {
    res.set_header("Access-Control-Allow-Origin", "http://localhost:5173");
    res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, PATCH, DELETE, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With, Accept");
    res.set_header("Access-Control-Allow-Credentials", "true");
    res.set_header("Vary", "Origin");
}

inline crow::response makeResponse(int status, const std::string& body) {
    crow::response res(status, body);
    applyHeaders(res);
    return res;
}

inline crow::response makeOptionsResponse() {
    crow::response res(200);
    applyHeaders(res);
    res.set_header("Access-Control-Max-Age", "86400");
    return res;
}
}  // namespace Cors
