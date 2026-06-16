const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');

const runtime = require('../src/lib/runtime');

test('finds executable on PATH', () => {
  const root = fs.mkdtempSync(path.join(os.tmpdir(), 'mwb-runtime-test-'));
  const bin = path.join(root, 'mwb-client');
  fs.writeFileSync(bin, '#!/bin/sh\nexit 0\n', { mode: 0o755 });

  assert.equal(runtime.which('mwb-client', { PATH: root }), bin);
});

test('runtime status includes session fields', () => {
  const status = runtime.runtimeStatus('', {
    PATH: '',
    XDG_SESSION_TYPE: 'wayland',
    XDG_CURRENT_DESKTOP: 'sway',
    WAYLAND_DISPLAY: 'wayland-1'
  });

  assert.equal(status.session.sessionType, 'wayland');
  assert.equal(status.session.currentDesktop, 'sway');
  assert.equal(status.session.waylandDisplay, 'wayland-1');
  assert.equal(typeof status.binary.found, 'boolean');
});
