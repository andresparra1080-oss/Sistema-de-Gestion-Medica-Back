#pragma once

#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

#include <sqlite3.h>

class AlertaVencimientoService {
public:
    AlertaVencimientoService() = default;
    ~AlertaVencimientoService();

    AlertaVencimientoService(const AlertaVencimientoService&) = delete;
    AlertaVencimientoService& operator=(
        const AlertaVencimientoService&) = delete;

    bool iniciar(const std::string& rutaBaseDatos);
    void detener();

private:
    bool generarAlertas();

    sqlite3* conexion_ = nullptr;
    std::thread hilo_;
    std::mutex mutex_;
    std::condition_variable condicion_;
    bool detener_ = false;
};