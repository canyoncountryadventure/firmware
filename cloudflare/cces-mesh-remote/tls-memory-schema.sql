CREATE TABLE IF NOT EXISTS mesh_gateway_tls_diag (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    gateway_id TEXT NOT NULL,
    received_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    firmware TEXT,
    uptime_seconds INTEGER,
    boot INTEGER NOT NULL,
    diagnostic_json TEXT NOT NULL CHECK(length(diagnostic_json) <= 4096)
);
CREATE INDEX IF NOT EXISTS idx_gateway_tls_received
ON mesh_gateway_tls_diag(gateway_id, id);
