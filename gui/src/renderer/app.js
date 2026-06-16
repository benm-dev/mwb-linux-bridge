const fields = {
  form: document.getElementById('settingsForm'),
  windowsIp: document.getElementById('windowsIp'),
  port: document.getElementById('port'),
  binaryPath: document.getElementById('binaryPath'),
  securityKey: document.getElementById('securityKey'),
  storeKey: document.getElementById('storeKey'),
  keyPath: document.getElementById('keyPath'),
  configPath: document.getElementById('configPath'),
  runState: document.getElementById('runState'),
  sessionType: document.getElementById('sessionType'),
  desktopName: document.getElementById('desktopName'),
  displayServer: document.getElementById('displayServer'),
  uinputState: document.getElementById('uinputState'),
  binaryState: document.getElementById('binaryState'),
  logOutput: document.getElementById('logOutput'),
  saveSettings: document.getElementById('saveSettings'),
  startClient: document.getElementById('startClient'),
  stopClient: document.getElementById('stopClient'),
  chooseBinary: document.getElementById('chooseBinary'),
  refreshState: document.getElementById('refreshState'),
  clearLogs: document.getElementById('clearLogs')
};

let currentState = null;

function formSettings() {
  return {
    windowsIp: fields.windowsIp.value.trim(),
    port: fields.port.value.trim(),
    binaryPath: fields.binaryPath.value.trim(),
    key: fields.securityKey.value,
    keyMode: fields.storeKey.checked ? 'stored' : 'prompt'
  };
}

function setRunning(running) {
  fields.runState.textContent = running ? 'Running' : 'Stopped';
  fields.runState.classList.toggle('running', running);
  fields.startClient.disabled = running;
  fields.stopClient.disabled = !running;
}

function renderLogs(logs) {
  fields.logOutput.textContent = (logs || [])
    .map((entry) => `[${entry.time}] ${entry.source}: ${entry.line}`)
    .join('\n');
  fields.logOutput.scrollTop = fields.logOutput.scrollHeight;
}

function renderState(state) {
  currentState = state;
  fields.windowsIp.value = state.config.windowsIp || '';
  fields.port.value = String(state.config.port || 15101);
  fields.binaryPath.value = state.config.binaryPath || state.runtime.binary.path || '';
  fields.storeKey.checked = state.config.keyMode === 'stored';
  fields.securityKey.placeholder = state.config.hasStoredKey ? 'Saved key present' : 'Enter key';
  fields.configPath.textContent = state.configPath;
  fields.keyPath.textContent = state.config.keyMode === 'stored'
    ? `Key file: ${state.keyPath}`
    : 'Key is sent to the client through stdin when starting.';

  const session = state.runtime.session;
  fields.sessionType.textContent = session.sessionType;
  fields.desktopName.textContent = session.currentDesktop || session.desktopSession || 'unknown';
  fields.displayServer.textContent = session.waylandDisplay
    ? `Wayland (${session.waylandDisplay})`
    : (session.display ? `X11 (${session.display})` : 'unknown');

  const uinput = state.runtime.uinput;
  fields.uinputState.textContent = !uinput.exists
    ? '/dev/uinput missing'
    : (uinput.writable ? 'writable' : 'not writable');

  fields.binaryState.textContent = state.runtime.binary.found
    ? state.runtime.binary.path
    : 'not found';

  setRunning(state.running);
  renderLogs(state.logs);
}

async function loadState() {
  renderState(await window.mwb.loadState());
}

async function runAction(action) {
  try {
    const state = await action();
    if (state) renderState(state);
  } catch (error) {
    const line = `[${new Date().toLocaleTimeString()}] error: ${error.message}`;
    fields.logOutput.textContent += fields.logOutput.textContent ? `\n${line}` : line;
  }
}

fields.form.addEventListener('submit', (event) => {
  event.preventDefault();
  runAction(() => window.mwb.saveSettings(formSettings()));
});

fields.startClient.addEventListener('click', () => {
  runAction(() => window.mwb.startClient(formSettings()));
});

fields.stopClient.addEventListener('click', () => {
  runAction(() => window.mwb.stopClient());
});

fields.chooseBinary.addEventListener('click', async () => {
  const selected = await window.mwb.chooseBinary();
  if (selected) fields.binaryPath.value = selected;
});

fields.refreshState.addEventListener('click', () => {
  loadState();
});

fields.clearLogs.addEventListener('click', async () => {
  renderLogs(await window.mwb.clearLogs());
});

fields.storeKey.addEventListener('change', () => {
  if (!currentState) return;
  fields.keyPath.textContent = fields.storeKey.checked
    ? `Key file: ${currentState.keyPath}`
    : 'Key is sent to the client through stdin when starting.';
});

window.mwb.onLogs((logs) => renderLogs(logs));
window.mwb.onClientState((state) => renderState(state));

loadState();
