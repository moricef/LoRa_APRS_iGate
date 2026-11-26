// Run with Node and Chromium; no device or network connection is used.
import assert from 'node:assert/strict';
import { readFileSync, mkdtempSync, writeFileSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { resolve, dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { execFileSync } from 'node:child_process';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '../..');
const cpp = readFileSync(join(root, 'src/lora_utils.cpp'), 'utf8');
// The browser expects packet text, not server-generated HTML entities.
assert.match(cpp, /receivedPacket\.packet\s*=\s*cleanPacket\s*;/);
const js = readFileSync(join(root, 'data_embed/script.js'), 'utf8');
const start = js.indexOf('function loadReceivedPackets(packets) {');
const end = js.indexOf('function fetchReceivedPackets()', start);
assert.ok(start >= 0 && end > start);
const payloads = [
  'N7UV>APLRG1:>A & B "quoted" \'single\'',
  'N7UV>APLRG1:literal &gt; and &amp; must remain literal',
  'N7UV>APLRG1:<img src=x onerror="window.injected=true">',
];
const data = JSON.stringify(payloads).replace(/</g, '\\u003c');
const folder = mkdtempSync(join(tmpdir(), 'igate-packet-text-'));
try {
  const fixture = join(folder, 'index.html');
  writeFileSync(fixture, `<!doctype html><meta charset="utf-8">
<table id="received-packets"><tbody></tbody></table><pre id="result">FAIL</pre>
<script>
window.setTimeout = () => 0;
function formatPacketTime() { return {text: '12:00:00', estimated: false}; }
function fetchReceivedPackets() {}
${js.slice(start, end)}
const payloads = ${data};
loadReceivedPackets(payloads.map(packet => ({packet, RSSI: -80, SNR: 10})));
const rows = document.querySelectorAll('#received-packets tbody tr');
const ok = rows.length === payloads.length
  && [...rows].every((row, i) => row.cells[1].textContent === payloads[i])
  && document.querySelectorAll('#received-packets img').length === 0
  && !window.injected;
document.getElementById('result').textContent = ok ? 'PASS packet text and HTML isolation' : 'FAIL';
</script>`);
  const output = execFileSync(process.env.CHROMIUM || '/usr/bin/chromium',
    ['--headless', '--no-sandbox', '--disable-gpu', '--dump-dom', 'file://' + fixture],
    {encoding: 'utf8', timeout: 20000, stdio: ['ignore', 'pipe', 'pipe']});
  assert.ok(output.includes('<pre id="result">PASS packet text and HTML isolation</pre>'));
  console.log('PASS: raw >, literal entities, quotes, and inert HTML payload');
} finally {
  rmSync(folder, {recursive: true, force: true});
}
