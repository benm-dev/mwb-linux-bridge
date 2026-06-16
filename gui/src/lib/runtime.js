const fs = require('node:fs');
const path = require('node:path');

function isExecutable(file) {
  try {
    fs.accessSync(file, fs.constants.X_OK);
    return true;
  } catch {
    return false;
  }
}

function firstExecutable(candidates) {
  return candidates.find((candidate) => candidate && isExecutable(candidate)) || '';
}

function which(command, env = process.env) {
  const pathValue = env.PATH || '';
  for (const dir of pathValue.split(':')) {
    if (!dir) continue;
    const candidate = path.join(dir, command);
    if (isExecutable(candidate)) return candidate;
  }
  return '';
}

function repoRootFromGui() {
  return path.resolve(__dirname, '..', '..', '..');
}

function binaryCandidates(env = process.env) {
  const root = repoRootFromGui();
  return [
    env.MWB_CLIENT_PATH,
    path.join(root, 'build', 'mwb_client'),
    which('mwb-client', env),
    '/usr/local/bin/mwb-client',
    '/usr/bin/mwb-client'
  ].filter(Boolean);
}

function resolveBinary(preferred, env = process.env) {
  if (preferred && isExecutable(preferred)) return preferred;
  return firstExecutable(binaryCandidates(env));
}

function uinputStatus() {
  const device = '/dev/uinput';
  const exists = fs.existsSync(device);
  let writable = false;
  if (exists) {
    try {
      fs.accessSync(device, fs.constants.W_OK);
      writable = true;
    } catch {
      writable = false;
    }
  }
  return { device, exists, writable };
}

function sessionInfo(env = process.env) {
  return {
    sessionType: env.XDG_SESSION_TYPE || 'unknown',
    currentDesktop: env.XDG_CURRENT_DESKTOP || '',
    desktopSession: env.DESKTOP_SESSION || '',
    display: env.DISPLAY || '',
    waylandDisplay: env.WAYLAND_DISPLAY || '',
    ozoneHint: env.ELECTRON_OZONE_PLATFORM_HINT || 'auto'
  };
}

function runtimeStatus(preferredBinary, env = process.env) {
  const resolvedBinary = resolveBinary(preferredBinary, env);
  return {
    binary: {
      path: resolvedBinary,
      found: Boolean(resolvedBinary),
      candidates: binaryCandidates(env)
    },
    session: sessionInfo(env),
    uinput: uinputStatus()
  };
}

module.exports = {
  binaryCandidates,
  isExecutable,
  resolveBinary,
  runtimeStatus,
  sessionInfo,
  uinputStatus,
  which
};
