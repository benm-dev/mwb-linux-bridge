const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');

const config = require('../src/lib/config');

test('parses bridge config aliases and port', () => {
  const parsed = config.parseBridgeConfig(`
    # comment
    host=windows-box.local
    port=15102
    security_key_file=~/key
  `);

  assert.equal(parsed.windowsIp, 'windows-box.local');
  assert.equal(parsed.port, 15102);
  assert.ok(parsed.securityKeyFile.endsWith('/key'));
});

test('writes private config and key files', () => {
  const root = fs.mkdtempSync(path.join(os.tmpdir(), 'mwb-gui-test-'));
  const configPath = path.join(root, 'mwb-linux-bridge', 'config');
  const keyPath = path.join(root, 'mwb-linux-bridge', 'key');

  config.writeBridgeConfig(configPath, {
    windowsIp: '192.168.1.10',
    port: 15101,
    securityKeyFile: keyPath
  });
  config.writeKeyFile(keyPath, ' abc def \n');

  assert.equal(config.readBridgeConfig(configPath).windowsIp, '192.168.1.10');
  assert.equal(fs.readFileSync(keyPath, 'utf8'), 'abcdef\n');
  assert.equal(config.fileMode(configPath), 0o600);
  assert.equal(config.fileMode(keyPath), 0o600);
});

test('resolves XDG config paths', () => {
  const resolved = config.paths({
    XDG_CONFIG_HOME: '/tmp/example-config',
    HOME: '/home/example'
  });

  assert.equal(resolved.config, '/tmp/example-config/mwb-linux-bridge/config');
  assert.equal(resolved.key, '/tmp/example-config/mwb-linux-bridge/key');
});
