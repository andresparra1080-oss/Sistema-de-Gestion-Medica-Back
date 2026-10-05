#pragma once

#include <algorithm>
#include <string>
#include <vector>

#include "crow.h"

struct CorsMiddleware {
    struct context {};

    static bool origenPermitido(const std::string& origin) {
        static const std::vector<std::string> originsPermitidos = {
            "http://localhost:5173",
            "http://127.0.0.1:5173",
            "http://localhost:3000",
            "http://127.0.0.1:3000",
            "http://localhost:4200",
            "http://127.0.0.1:4200"
        };

        return std::find(originsPermitidos.begin(), originsPermitidos.end(), origin) != originsPermitidos.end();
    }

    void before_handle(crow::request& req, crow::response& res, context&) {
        const std::string origin = req.get_header_value("Origin");
        const std::string defaultOrigin = "http://localhost:5173";
        const std::string allowedMethods = "GET, POST, PUT, PATCH, DELETE, OPTIONS";
        const std::string allowedHeaders = "Content-Type, Authorization, X-Requested-With, Accept";

        if (!origin.empty() && origenPermitido(origin)) {
            res.set_header("Access-Control-Allow-Origin", origin);
        } else {
            res.set_header("Access-Control-Allow-Origin", defaultOrigin);
        }

        res.set_header("Access-Control-Allow-Methods", allowedMethods);
        res.set_header("Access-Control-Allow-Headers", allowedHeaders);
        res.set_header("Access-Control-Allow-Credentials", "true");
        res.set_header("Vary", "Origin");

        if (req.method == crow::HTTPMethod::Options) {
            res.code = 200;
            res.set_header("Access-Control-Max-Age", "86400");
            res.end();
        }
    }

    void after_handle(crow::request&, crow::response&, context&) {}
};
