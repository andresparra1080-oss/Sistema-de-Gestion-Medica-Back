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

```

## 🚨 Módulo 4 — Emergencias

### `POST /api/emergencias`

Registra una emergencia y la asocia a uno o varios tripulantes. Requiere `Authorization: Bearer <token>` y que el rol tenga el permiso `GESTIONAR_EMERGENCIAS` (Capitán/Médico Principal y Cirujano Naval).

```json
{
  "tipo": "TRAUMATISMO",
  "sintomas": "Dolor intenso en el costado",
  "lesiones": "Costillas fracturadas",
  "diagnostico_inicial": "Fractura costal múltiple",
  "nivel_urgencia": "CRITICA",
  "pacientes": [1, 3]
}
```

| Campo | Obligatorio | Notas |
|---|---|---|
| `tipo` | Sí | Uno de los tipos con protocolo: `TRAUMATISMO`, `HEMORRAGIA`, `INTOXICACION`, `QUEMADURA`, `OTRO` |
| `sintomas`, `diagnostico_inicial` | Sí | Máx. 2000 caracteres |
| `lesiones` | No | `null` o ausente = sin lesiones |
| `nivel_urgencia` | No | `BAJA`, `MEDIA`, `ALTA`, `CRITICA`. Si no viene, se asume `ALTA` |
| `pacientes` | Sí | Lista de `id_paciente` (1 a 50, sin repetir). Todos deben existir y tener historia clínica |

`fecha_inicio`, `estado` (siempre `ACTIVA`) y el usuario que registra los pone el backend (hora de Colombia, UTC-5, y token); si el cliente los envía se ignoran.

Todo se guarda en **una sola transacción** (si algo falla no queda nada a medias):

1. La emergencia, con su fecha y hora de inicio automáticas.
2. El vínculo con cada paciente (`emergencia_paciente`).
3. Un evento `DIAGNOSTICO` en la historia clínica de **cada** paciente (`eventos_medicos`, enlazado por `id_emergencia`).
4. Una alerta `EMERGENCIA` (`alertas`), con la urgencia como prioridad. Nace con `notificada = false`.
5. Un registro en `logs_auditoria`.

Respuestas: `201` creada · `400` datos inválidos · `401` sin token · `403` rol sin permiso · `404` paciente inexistente · `409` paciente sin historia clínica o cuenta inactiva.

### Base de datos existente

La tabla `alertas` cambió (nuevo tipo `EMERGENCIA`). Una base nueva ya lo trae con `heartmed_sqlite.sql`; para una ya creada, ejecutar una sola vez:

```
sqlite3 heartmed_db.sqlite3 ".read Database/Scripts_Base_Datos/migracion_emergencias.sql"
```
