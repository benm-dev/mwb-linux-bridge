const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('mwb', {
  loadState: () => ipcRenderer.invoke('state:load'),
  saveSettings: (settings) => ipcRenderer.invoke('settings:save', settings),
  startClient: (settings) => ipcRenderer.invoke('client:start', settings),
  stopClient: () => ipcRenderer.invoke('client:stop'),
  clearLogs: () => ipcRenderer.invoke('logs:clear'),
  chooseBinary: () => ipcRenderer.invoke('binary:choose'),
  onLogs: (callback) => {
    ipcRenderer.on('client:log', (_event, logs) => callback(logs));
  },
  onClientState: (callback) => {
    ipcRenderer.on('client:state', (_event, state) => callback(state));
  }
});
