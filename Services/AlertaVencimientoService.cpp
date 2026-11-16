#include "Services/AlertaVencimientoService.h"

#include <chrono>
#include <exception>
#include <iostream>

AlertaVencimientoService::~AlertaVencimientoService() {
    detener();
}

bool AlertaVencimientoService::generarAlertas() {
    const char* sql = R"SQL(
        INSERT INTO alertas (
            id_lote,
            tipo_alerta,
            mensaje,
            prioridad
        )
        SELECT
            l.id_lote,
            'VENCIMIENTO_PROXIMO',
            'El lote ID ' || l.id_lote ||
            ' vence el ' || l.fecha_vencimiento || '.',
            'MEDIA'
        FROM lotes AS l
        WHERE l.cantidad > 0
          AND l.fecha_vencimiento BETWEEN
              date('now', '-5 hours')
              AND date('now', '-5 hours', '+30 days')
          AND NOT EXISTS (
              SELECT 1
              FROM alertas AS a
              WHERE a.id_lote = l.id_lote
                AND a.tipo_alerta = 'VENCIMIENTO_PROXIMO'
                AND a.resuelta = 0
          );
    )SQL";

    char* error = nullptr;
    const int resultado =
        sqlite3_exec(conexion_, sql, nullptr, nullptr, &error);

    if (resultado != SQLITE_OK) {
        std::cerr << "[KAN-27] Error al generar alertas: "
                  << (error != nullptr
                          ? error
                          : sqlite3_errmsg(conexion_))
                  << std::endl;

        sqlite3_free(error);
        return false;
    }

    const int nuevas = sqlite3_changes(conexion_);
    if (nuevas > 0) {
        std::cout << "[KAN-27] Alertas de vencimiento creadas: "
                  << nuevas << std::endl;
    }

    return true;
}

bool AlertaVencimientoService::iniciar(
    const std::string& rutaBaseDatos) {

    if (conexion_ != nullptr) {
        return true;
    }

    const int resultado = sqlite3_open_v2(
        rutaBaseDatos.c_str(),
        &conexion_,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_FULLMUTEX,
        nullptr
    );

    if (resultado != SQLITE_OK) {
        std::cerr << "[KAN-27] No se pudo abrir la base: "
                  << (conexion_ != nullptr
                          ? sqlite3_errmsg(conexion_)
                          : "Sin conexion")
                  << std::endl;

        if (conexion_ != nullptr) {
            sqlite3_close(conexion_);
            conexion_ = nullptr;
        }

        return false;
    }

    sqlite3_busy_timeout(conexion_, 5000);

    if (sqlite3_exec(
            conexion_,
            "PRAGMA foreign_keys = ON;",
            nullptr,
            nullptr,
            nullptr) != SQLITE_OK) {
        std::cerr << "[KAN-27] No se pudo configurar SQLite: "
                  << sqlite3_errmsg(conexion_) << std::endl;
        sqlite3_close(conexion_);
        conexion_ = nullptr;
        return false;
    }

    if (!generarAlertas()) {
        sqlite3_close(conexion_);
        conexion_ = nullptr;
        return false;
    }

    {
        std::lock_guard<std::mutex> bloqueo(mutex_);
        detener_ = false;
    }

    try {
        hilo_ = std::thread([this]() {
            std::unique_lock<std::mutex> bloqueo(mutex_);

            while (!condicion_.wait_for(
                bloqueo,
                std::chrono::seconds(60),
                [this]() { return detener_; })) {

                bloqueo.unlock();
                generarAlertas();
                bloqueo.lock();
            }
        });
    } catch (const std::exception& error) {
        std::cerr << "[KAN-27] No se pudo iniciar la revision: "
                  << error.what() << std::endl;

        sqlite3_close(conexion_);
        conexion_ = nullptr;
        return false;
    }

    std::cout
        << "[KAN-27] Revision de vencimientos cada 60 segundos"
        << std::endl;

    return true;
}

void AlertaVencimientoService::detener() {
    {
        std::lock_guard<std::mutex> bloqueo(mutex_);
        detener_ = true;
    }

    condicion_.notify_all();

    if (hilo_.joinable()) {
        hilo_.join();
    }

    if (conexion_ != nullptr) {
        sqlite3_close(conexion_);
        conexion_ = nullptr;
    }
}