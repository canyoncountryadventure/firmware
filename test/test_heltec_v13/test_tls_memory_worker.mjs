import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';

const source = await readFile(new URL('../../cloudflare/cces-mesh-remote/worker.js', import.meta.url), 'utf8');
const { default: worker, validTlsMemory } = await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
const report = JSON.parse(await readFile(process.argv[2], 'utf8'));
assert.equal(validTlsMemory(report), true);
assert.equal(report.boot, 41);
assert.equal(report.active, 0);
assert.equal(report.records.length, 6);
assert.equal(report.first_failure[1], 40);
assert.equal(report.first_failure[7], -16);
for (const row of report.records) {
  assert.equal(row[5], 4);
  assert.equal(row[9], 120000);
  assert.equal(row[21], 120000); // Post-destruction sample.
  assert.equal(row[23], 80000); // Lifetime low watermark observes the simulated TLS allocation.
}

const bad = structuredClone(report);
bad.records[0][21] = 'secret';
assert.equal(validTlsMemory(bad), false);
assert.equal(validTlsMemory({ ...report, records: [...report.records, report.records[0]] }), false);

const inserts = [];
const DB = {
  prepare(sql) {
    return {
      bind(...args) {
        return {
          async run() { inserts.push({ sql, args }); },
          async first() { return undefined; },
        };
      },
    };
  },
};
const body = {
  firmware: 'HELTEC-GATEWAY-V13', uptime_seconds: 900, config_version: 22,
  queue_depth: 0, last_http_status: 201, tls_memory: report,
  note: 'remote-ota: loader_ready=1,phase=4,attempts=0,applied=heltec-test-r6,fw_kv=1',
};
const key = 'local-test-value';
const request = value => new Request('https://example.invalid/v10/health', {
  method: 'POST', headers: { 'x-ingest-key': key }, body: JSON.stringify(value),
});
assert.equal((await worker.fetch(request(body), { DB, GATEWAY_KEY: key })).status, 200);
assert.equal(inserts.filter(x => x.sql.includes('mesh_gateway_tls_diag')).length, 1);
assert.equal((await worker.fetch(request({ ...body, padding: 'x'.repeat(4200) }), { DB, GATEWAY_KEY: key })).status, 200);
assert.equal(inserts.filter(x => x.sql.includes('mesh_gateway_tls_diag')).length, 2);
assert.equal((await worker.fetch(request({ ...body, tls_memory: bad }), { DB, GATEWAY_KEY: key })).status, 200);
assert.equal(inserts.filter(x => x.sql.includes('mesh_gateway_tls_diag')).length, 2);
assert.equal((await worker.fetch(request(body), { DB, GATEWAY_KEY: 'different' })).status, 401);
assert.equal(inserts.filter(x => x.sql.includes('mesh_gateway_tls_diag')).length, 2);
assert.equal((await worker.fetch(request({ ...body, padding: 'x'.repeat(8192) }), { DB, GATEWAY_KEY: key })).status, 413);
console.log('PASS: captured memory stages, failure retention, schema bounds, authenticated storage and OTA confirmation compatibility');
