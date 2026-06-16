const { app, BrowserWindow, dialog, ipcMain } = require('electron');
const { spawn } = require('node:child_process');
const fs = require('node:fs');
const path = require('node:path');

const config = require('./lib/config');
const runtime = require('./lib/runtime');

if (process.platform === 'linux') {
  app.commandLine.appendSwitch('ozone-platform-hint', 'auto');
  app.commandLine.appendSwitch('enable-features', 'WaylandWindowDecorations');
}

let mainWindow = null;
let clientProcess = null;
let stopping = false;
const logs = [];
const maxLogLines = 600;

function createWindow() {
  mainWindow = new BrowserWindow({
    width: 1040,
    height: 720,
    minWidth: 860,
    minHeight: 620,
    show: false,
    autoHideMenuBar: true,
    backgroundColor: '#f6f7f9',
    title: 'MWB Linux Bridge',
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      contextIsolation: true,
      nodeIntegration: false,
      sandbox: true
    }
  });

  mainWindow.once('ready-to-show', () => {
    mainWindow.show();
  });

  mainWindow.loadFile(path.join(__dirname, 'renderer', 'index.html'));
}

function appendLog(source, text) {
  const time = new Date().toLocaleTimeString();
  const lines = String(text || '').split(/\r?\n/).filter(Boolean);
  for (const line of lines) {
    logs.push({ time, source, line });
  }
  while (logs.length > maxLogLines) logs.shift();
  if (mainWindow && !mainWindow.isDestroyed()) {
    mainWindow.webContents.send('client:log', logs.slice(-80));
  }
}

function appPaths() {
  return config.paths(process.env);
}

function loadState() {
  const p = appPaths();
  const bridgeConfig = config.readBridgeConfig(p.config);
  const prefs = config.readPrefs(p.prefs);
  const binaryPath = prefs.binaryPath || runtime.resolveBinary('', process.env);
  const keyMode = bridgeConfig.securityKeyFile ? 'stored' : 'prompt';
  return {
    configPath: p.config,
    keyPath: bridgeConfig.securityKeyFile || p.key,
    prefsPath: p.prefs,
    config: {
      windowsIp: bridgeConfig.windowsIp,
      port: bridgeConfig.port,
      keyMode,
      hasStoredKey: Boolean(bridgeConfig.securityKeyFile && fs.existsSync(bridgeConfig.securityKeyFile)),
      binaryPath
    },
    runtime: runtime.runtimeStatus(binaryPath, process.env),
    running: Boolean(clientProcess),
    logs
  };
}

function normalizeSettings(payload) {
  const incoming = payload || {};
  const port = config.parsePort(incoming.port, 15101);
  return {
    windowsIp: String(incoming.windowsIp || '').trim(),
    port,
    key: config.stripWhitespace(incoming.key || ''),
    keyMode: incoming.keyMode === 'stored' ? 'stored' : 'prompt',
    binaryPath: String(incoming.binaryPath || '').trim()
  };
}

function saveSettings(payload) {
  const p = appPaths();
  const settings = normalizeSettings(payload);
  const bridgeConfig = {
    windowsIp: settings.windowsIp,
    port: settings.port,
    securityKeyFile: ''
  };

  if (settings.keyMode === 'stored') {
    bridgeConfig.securityKeyFile = p.key;
    if (settings.key) {
      config.writeKeyFile(p.key, settings.key);
    }
  }

  config.writeBridgeConfig(p.config, bridgeConfig);
  config.writePrefs(p.prefs, { binaryPath: settings.binaryPath });
  return loadState();
}

function validateStart(settings, state) {
  if (!settings.windowsIp) return 'Windows IP/host is required.';
  if (!state.runtime.binary.found) return 'mwb-client binary was not found. Build it first or choose its path.';
  if (settings.keyMode !== 'stored' && !settings.key) return 'Enter the security key or enable saved key file.';
  return '';
}

function stopClient() {
  if (!clientProcess) return { stopped: false };
  stopping = true;
  const proc = clientProcess;
  proc.kill('SIGTERM');
  setTimeout(() => {
    if (clientProcess === proc) proc.kill('SIGKILL');
  }, 2500);
  return { stopped: true };
}

function startClient(payload) {
  if (clientProcess) return loadState();

  const settings = normalizeSettings(payload);
  const savedState = saveSettings(settings);
  const error = validateStart(settings, savedState);
  if (error) {
    throw new Error(error);
  }

  const args = ['--config', savedState.configPath];
  const proc = spawn(savedState.runtime.binary.path, args, {
    cwd: path.dirname(savedState.runtime.binary.path),
    stdio: ['pipe', 'pipe', 'pipe'],
    env: {
      ...process.env,
      MWB_LINUX_BRIDGE_GUI: '1'
    }
  });

  clientProcess = proc;
  stopping = false;
  appendLog('gui', 'Started ' + savedState.runtime.binary.path + ' --config ' + savedState.configPath);

  if (settings.keyMode !== 'stored') {
    proc.stdin.write(settings.key + '\n');
    proc.stdin.end();
  }

  proc.stdout.on('data', (chunk) => appendLog('stdout', chunk.toString('utf8')));
  proc.stderr.on('data', (chunk) => appendLog('stderr', chunk.toString('utf8')));
  proc.on('error', (err) => {
    appendLog('error', err.message);
  });
  proc.on('exit', (code, signal) => {
    appendLog('gui', stopping ? 'Stopped.' : 'Exited with code=' + code + ' signal=' + signal);
    clientProcess = null;
    stopping = false;
    if (mainWindow && !mainWindow.isDestroyed()) {
      mainWindow.webContents.send('client:state', loadState());
    }
  });

  return loadState();
}

app.whenReady().then(() => {
  ipcMain.handle('state:load', () => loadState());
  ipcMain.handle('settings:save', (_event, payload) => saveSettings(payload));
  ipcMain.handle('client:start', (_event, payload) => startClient(payload));
  ipcMain.handle('client:stop', () => {
    stopClient();
    return loadState();
  });
  ipcMain.handle('logs:clear', () => {
    logs.length = 0;
    return [];
  });
  ipcMain.handle('binary:choose', async () => {
    const result = await dialog.showOpenDialog(mainWindow, {
      title: 'Choose mwb-client binary',
      properties: ['openFile'],
      filters: [{ name: 'Executable', extensions: ['*'] }]
    });
    if (result.canceled || !result.filePaths.length) return '';
    return result.filePaths[0];
  });

  createWindow();
});

app.on('window-all-closed', () => {
  stopClient();
  if (process.platform !== 'darwin') app.quit();
});
