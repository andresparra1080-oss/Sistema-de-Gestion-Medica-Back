# ⚓ HeartMed — Backend API (C++ & Crow)

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Status](https://img.shields.io/badge/Status-In%20Development-brightgreen)](#)
[![C++](https://img.shields.io/badge/C%2B%2B-17%20%2F%2020-00599C?logo=cplusplus)](https://isocpp.org/)
[![Framework](https://img.shields.io/badge/Framework-Crow%20v1.2-000000)](#)

## 📌 Descripción General

**HeartMed API** es el servicio backend centralizado para el sistema de gestión médica e inventario de la tripulación de los **Piratas Heart**, liderados por el Capitán y Cirujano **Trafalgar Law**.

El sistema está desarrollado como una **API RESTful desacoplada** en **C++** utilizando el microframework **Crow**. Se encarga de procesar las reglas de negocio clínicas, gestionar la persistencia de datos y responder en formato JSON estructurado para ser consumido por el frontend (React + Tailwind CSS).

---

## 🛠️ Stack Tecnológico

* **Lenguaje:** C++17 / C++20
* **Microframework HTTP:** [Crow Framework](https://crowcpp.org/)
* **Manejo de JSON:** `nlohmann/json`
* **Sistema de Construcción:** CMake (v3.14+)
* **Contenedorización:** Docker (Multi-stage build)

---

## 📂 Estructura del Proyecto

El backend sigue una arquitectura en capas desacoplada:

```text
├── .github/          # Workflows de CI/CD (opcional)
├── Cross/            # Middlewares (CORS, JWT), loggers y utilidades globales
├── Database/         # Conexión, cliente BD y scripts de migración
├── Repositories/     # Capa de acceso a datos y operaciones CRUD
├── Services/         # Capa de lógica de negocio y reglas del dominio
├── Routes/           # Controladores HTTP y serialización/deserialización JSON
├── entry/            # Punto de entrada principal (main.cpp)
├── CMakeLists.txt    # Configuración del build del proyecto
├── Dockerfile        # Imagen Docker optimizada para producción
└── README.md         # Documentación principal
