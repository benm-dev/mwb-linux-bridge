const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');

function trim(value) {
  return String(value || '').trim();
}

function stripWhitespace(value) {
  return String(value || '').replace(/\s+/g, '');
}

function expandUserPath(input, env = process.env) {
  if (!input || input[0] !== '~') return input || '';
  const home = env.HOME || os.homedir();
  if (!home) return input;
  if (input === '~') return home;
  if (input.startsWith('~/')) return path.join(home, input.slice(2));
  return input;
}

function configDir(env = process.env) {
  if (env.XDG_CONFIG_HOME) return path.join(env.XDG_CONFIG_HOME, 'mwb-linux-bridge');
  const home = env.HOME || os.homedir();
  return path.join(home, '.config', 'mwb-linux-bridge');
}

function paths(env = process.env) {
  const dir = configDir(env);
  return {
    dir,
    config: path.join(dir, 'config'),
    key: path.join(dir, 'key'),
    prefs: path.join(dir, 'gui.json')
  };
}

function ensurePrivateDir(dir) {
  fs.mkdirSync(dir, { recursive: true, mode: 0o700 });
  try {
    fs.chmodSync(dir, 0o700);
  } catch {
    // Best effort. Existing directory ownership may prevent chmod.
  }
}

function parsePort(value, fallback = 15101) {
  const text = trim(value);
  if (!/^\d+$/.test(text)) return fallback;
  const parsed = Number(text);
  if (!Number.isInteger(parsed) || parsed < 1 || parsed > 65535) return fallback;
  return parsed;
}

function parseBridgeConfig(text) {
  const config = {
    windowsIp: '',
    port: 15101,
    securityKeyFile: ''
  };

  String(text || '').split(/\r?\n/).forEach((rawLine) => {
    const line = trim(rawLine);
    if (!line || line.startsWith('#')) return;
    const equals = line.indexOf('=');
    if (equals < 0) return;

    const key = trim(line.slice(0, equals));
    const value = trim(line.slice(equals + 1));
    if (key === 'windows_ip' || key === 'host') config.windowsIp = value;
    if (key === 'port') config.port = parsePort(value, config.port);
    if (key === 'security_key_file') config.securityKeyFile = expandUserPath(value);
  });

  return config;
}

function serializeBridgeConfig(config) {
  const lines = [
    '# MWB Linux Bridge config',
    'windows_ip=' + trim(config.windowsIp),
    'port=' + parsePort(config.port, 15101)
  ];
  if (config.securityKeyFile) {
    lines.push('security_key_file=' + config.securityKeyFile);
  }
  return lines.join('\n') + '\n';
}

function readBridgeConfig(configPath) {
  if (!fs.existsSync(configPath)) {
    return {
      windowsIp: '',
      port: 15101,
      securityKeyFile: ''
    };
  }
  return parseBridgeConfig(fs.readFileSync(configPath, 'utf8'));
}

function writeBridgeConfig(configPath, config) {
  ensurePrivateDir(path.dirname(configPath));
  fs.writeFileSync(configPath, serializeBridgeConfig(config), { mode: 0o600 });
  try {
    fs.chmodSync(configPath, 0o600);
  } catch {
    // Best effort.
  }
}

function readPrefs(prefsPath) {
  if (!fs.existsSync(prefsPath)) return {};
  try {
    const parsed = JSON.parse(fs.readFileSync(prefsPath, 'utf8'));
    return parsed && typeof parsed === 'object' ? parsed : {};
  } catch {
    return {};
  }
}

function writePrefs(prefsPath, prefs) {
  ensurePrivateDir(path.dirname(prefsPath));
  fs.writeFileSync(prefsPath, JSON.stringify(prefs, null, 2) + '\n', { mode: 0o600 });
  try {
    fs.chmodSync(prefsPath, 0o600);
  } catch {
    // Best effort.
  }
}

function writeKeyFile(keyPath, key) {
  ensurePrivateDir(path.dirname(keyPath));
  fs.writeFileSync(keyPath, stripWhitespace(key) + '\n', { mode: 0o600 });
  try {
    fs.chmodSync(keyPath, 0o600);
  } catch {
    // Best effort.
  }
}

function fileMode(pathname) {
  try {
    return fs.statSync(pathname).mode & 0o777;
  } catch {
    return null;
  }
}

module.exports = {
  configDir,
  expandUserPath,
  fileMode,
  parseBridgeConfig,
  parsePort,
  paths,
  readBridgeConfig,
  readPrefs,
  serializeBridgeConfig,
  stripWhitespace,
  writeBridgeConfig,
  writeKeyFile,
  writePrefs
};
