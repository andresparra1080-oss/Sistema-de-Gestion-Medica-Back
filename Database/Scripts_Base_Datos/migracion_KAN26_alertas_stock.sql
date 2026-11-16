BEGIN TRANSACTION;

-- Al registrar un medicamento con existencias inferiores al minimo.
CREATE TRIGGER IF NOT EXISTS trg_kan26_medicamento_ai_alerta_stock
AFTER INSERT ON medicamentos
WHEN NEW.cantidad_total_disponible < NEW.umbral_minimo
BEGIN
    INSERT INTO alertas (
        id_medicamento,
        tipo_alerta,
        mensaje,
        prioridad
    )
    SELECT
        NEW.id_medicamento,
        'STOCK_BAJO',
        'Stock bajo del medicamento ID ' || NEW.id_medicamento ||
        ': disponible ' || NEW.cantidad_total_disponible ||
        ', minimo ' || NEW.umbral_minimo || '.',
        'MEDIA'
    WHERE NOT EXISTS (
        SELECT 1
        FROM alertas
        WHERE id_medicamento = NEW.id_medicamento
          AND tipo_alerta = 'STOCK_BAJO'
          AND resuelta = 0
    );
END;

-- Los triggers existentes recalculan el stock despues de cada movimiento.
-- Este trigger detecta ese cambio y tambien los cambios del umbral.
CREATE TRIGGER IF NOT EXISTS trg_kan26_medicamento_au_alerta_stock
AFTER UPDATE OF cantidad_total_disponible, umbral_minimo ON medicamentos
WHEN NEW.cantidad_total_disponible < NEW.umbral_minimo
BEGIN
    INSERT INTO alertas (
        id_medicamento,
        tipo_alerta,
        mensaje,
        prioridad
    )
    SELECT
        NEW.id_medicamento,
        'STOCK_BAJO',
        'Stock bajo del medicamento ID ' || NEW.id_medicamento ||
        ': disponible ' || NEW.cantidad_total_disponible ||
        ', minimo ' || NEW.umbral_minimo || '.',
        'MEDIA'
    WHERE NOT EXISTS (
        SELECT 1
        FROM alertas
        WHERE id_medicamento = NEW.id_medicamento
          AND tipo_alerta = 'STOCK_BAJO'
          AND resuelta = 0
    );
END;

-- Revisar tambien los medicamentos que ya estaban registrados.
INSERT INTO alertas (
    id_medicamento,
    tipo_alerta,
    mensaje,
    prioridad
)
SELECT
    m.id_medicamento,
    'STOCK_BAJO',
    'Stock bajo del medicamento ID ' || m.id_medicamento ||
    ': disponible ' || m.cantidad_total_disponible ||
    ', minimo ' || m.umbral_minimo || '.',
    'MEDIA'
FROM medicamentos AS m
WHERE m.cantidad_total_disponible < m.umbral_minimo
  AND NOT EXISTS (
      SELECT 1
      FROM alertas AS a
      WHERE a.id_medicamento = m.id_medicamento
        AND a.tipo_alerta = 'STOCK_BAJO'
        AND a.resuelta = 0
  );

COMMIT;