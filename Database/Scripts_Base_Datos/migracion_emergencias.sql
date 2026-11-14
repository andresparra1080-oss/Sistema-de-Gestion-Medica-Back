-- =====================================================================
-- HeartMed - Migración del módulo 4: registrar emergencias
-- Para bases YA creadas (los datos existentes se conservan).
--   sqlite3 heartmed_db.sqlite3 ".read migracion_emergencias.sql"
-- Una base nueva no la necesita: heartmed_sqlite.sql ya trae estos cambios.
--
-- Qué cambia (REV #21):
--   * alertas: nuevo tipo_alerta EMERGENCIA y nueva columna id_emergencia
--     (FK a emergencias). SQLite no permite cambiar un CHECK, por eso la
--     tabla se reconstruye. Un tipo = una sola referencia (chk_alertas_referencia).
--   * eventos_medicos: nueva columna id_emergencia (NULL). Une el evento que
--     queda en el historial del paciente con la emergencia que lo originó.
--
-- Ejecutar UNA sola vez. Las FK se apagan durante la reconstrucción
-- (PRAGMA foreign_keys no funciona dentro de una transacción).
-- =====================================================================

PRAGMA foreign_keys = OFF;

BEGIN TRANSACTION;

CREATE TABLE alertas_nueva (
    id_alerta         INTEGER PRIMARY KEY AUTOINCREMENT,
    id_medicamento    INTEGER      NULL,
    id_lote           INTEGER      NULL,
    id_tratamiento    INTEGER      NULL,
    id_emergencia     INTEGER      NULL,
    tipo_alerta       TEXT         NOT NULL
        CONSTRAINT chk_alertas_tipo_alerta
        CHECK (tipo_alerta IN ('STOCK_BAJO', 'VENCIMIENTO_PROXIMO', 'DOSIS_PENDIENTE', 'EMERGENCIA')),
    mensaje           VARCHAR(300) NOT NULL,
    prioridad         TEXT         NOT NULL
        CONSTRAINT chk_alertas_prioridad
        CHECK (prioridad IN ('BAJA', 'MEDIA', 'ALTA', 'CRITICA')),
    notificada        INTEGER      NOT NULL DEFAULT 0
        CONSTRAINT chk_alertas_notificada CHECK (notificada IN (0, 1)),
    resuelta          INTEGER      NOT NULL DEFAULT 0
        CONSTRAINT chk_alertas_resuelta CHECK (resuelta IN (0, 1)),
    fecha_generacion  DATETIME     NOT NULL DEFAULT (datetime('now', '-5 hours')),
    fecha_resolucion  DATETIME     NULL,
    CONSTRAINT chk_alertas_mensaje_texto CHECK (trim(mensaje) <> '' AND length(mensaje) <= 300),
    CONSTRAINT chk_alertas_referencia
        CHECK ((tipo_alerta = 'STOCK_BAJO'
                    AND id_medicamento IS NOT NULL AND id_lote IS NULL
                    AND id_tratamiento IS NULL AND id_emergencia IS NULL)
            OR (tipo_alerta = 'VENCIMIENTO_PROXIMO'
                    AND id_lote IS NOT NULL AND id_medicamento IS NULL
                    AND id_tratamiento IS NULL AND id_emergencia IS NULL)
            OR (tipo_alerta = 'DOSIS_PENDIENTE'
                    AND id_tratamiento IS NOT NULL AND id_medicamento IS NULL
                    AND id_lote IS NULL AND id_emergencia IS NULL)
            OR (tipo_alerta = 'EMERGENCIA'
                    AND id_emergencia IS NOT NULL AND id_medicamento IS NULL
                    AND id_lote IS NULL AND id_tratamiento IS NULL)),
    CONSTRAINT chk_alertas_resuelta_fecha
        CHECK ((resuelta = 1 AND fecha_resolucion IS NOT NULL)
            OR (resuelta = 0 AND fecha_resolucion IS NULL)),
    CONSTRAINT chk_alertas_fechas
        CHECK (fecha_resolucion IS NULL OR fecha_resolucion >= fecha_generacion),
    CONSTRAINT chk_alertas_fecha_generacion_formato
        CHECK (datetime(fecha_generacion) IS fecha_generacion),
    CONSTRAINT chk_alertas_fecha_resolucion_formato
        CHECK (datetime(fecha_resolucion) IS fecha_resolucion),
    CONSTRAINT fk_alertas_medicamentos
        FOREIGN KEY (id_medicamento) REFERENCES medicamentos (id_medicamento)
        ON DELETE RESTRICT ON UPDATE CASCADE,
    CONSTRAINT fk_alertas_lotes
        FOREIGN KEY (id_lote) REFERENCES lotes (id_lote)
        ON DELETE RESTRICT ON UPDATE CASCADE,
    CONSTRAINT fk_alertas_tratamientos
        FOREIGN KEY (id_tratamiento) REFERENCES tratamientos (id_tratamiento)
        ON DELETE RESTRICT ON UPDATE CASCADE,
    CONSTRAINT fk_alertas_emergencias
        FOREIGN KEY (id_emergencia) REFERENCES emergencias (id_emergencia)
        ON DELETE RESTRICT ON UPDATE CASCADE
);

INSERT INTO alertas_nueva (id_alerta, id_medicamento, id_lote, id_tratamiento, tipo_alerta, mensaje,
                           prioridad, notificada, resuelta, fecha_generacion, fecha_resolucion)
SELECT id_alerta, id_medicamento, id_lote, id_tratamiento, tipo_alerta, mensaje,
       prioridad, notificada, resuelta, fecha_generacion, fecha_resolucion
  FROM alertas;

DROP TABLE alertas;
ALTER TABLE alertas_nueva RENAME TO alertas;

CREATE INDEX idx_alertas_id_medicamento ON alertas (id_medicamento);
CREATE INDEX idx_alertas_id_lote        ON alertas (id_lote);
CREATE INDEX idx_alertas_id_tratamiento ON alertas (id_tratamiento);
CREATE INDEX idx_alertas_id_emergencia  ON alertas (id_emergencia);

CREATE UNIQUE INDEX uk_alertas_stock_bajo_abierta
    ON alertas (id_medicamento)
    WHERE tipo_alerta = 'STOCK_BAJO' AND resuelta = 0;
CREATE UNIQUE INDEX uk_alertas_vencimiento_abierta
    ON alertas (id_lote)
    WHERE tipo_alerta = 'VENCIMIENTO_PROXIMO' AND resuelta = 0;
-- Una emergencia genera una sola alerta de tipo EMERGENCIA.
CREATE UNIQUE INDEX uk_alertas_emergencia
    ON alertas (id_emergencia)
    WHERE tipo_alerta = 'EMERGENCIA';

-- SQLite permite agregar una FK con ALTER TABLE si la columna nace en NULL.
ALTER TABLE eventos_medicos
    ADD COLUMN id_emergencia INTEGER NULL
    REFERENCES emergencias (id_emergencia) ON DELETE RESTRICT ON UPDATE CASCADE;

CREATE INDEX idx_eventos_medicos_id_emergencia ON eventos_medicos (id_emergencia);

COMMIT;

PRAGMA foreign_keys = ON;
PRAGMA foreign_key_check;
