/* ============================================================
   AgroMate Robot Manager Page
   Connected robots, BLE scanning, robot controls
   ============================================================ */

async function renderRobots(container) {
  container.innerHTML = `
    <div class="page-header" style="display:flex;align-items:center;justify-content:space-between;flex-wrap:wrap;gap:var(--space-md);">
      <div>
        <h1>🤖 Robot Manager</h1>
        <p>Connect and control your AgroMate farming robots</p>
      </div>
      <div style="display:flex;gap:var(--space-sm);">
        <button class="btn btn-primary" onclick="scanForBots()" id="scanBotsBtn">
          📡 Scan for Bots
        </button>
        <button class="btn btn-ghost" onclick="refreshRobots()">
          🔄 Refresh
        </button>
      </div>
    </div>

    <div class="robots-grid" id="robotsGrid">
      <div class="card" style="padding:var(--space-2xl);">
        <div class="spinner" style="margin:0 auto;"></div>
      </div>
    </div>
  `;

  loadRobots();
}

async function loadRobots() {
  const grid = document.getElementById('robotsGrid');
  if (!grid) return;

  const data = await api.get('/robots/');

  if (data && data.robots && data.robots.length > 0) {
    grid.innerHTML = data.robots.map((robot, i) => `
      <div class="robot-card" style="animation-delay:${i * 100}ms;">
        <div class="robot-header">
          <div class="robot-avatar">🤖</div>
          <div>
            <div class="robot-name">${escapeHtml(robot.name || `Robot ${i + 1}`)}</div>
            <div class="robot-id">${escapeHtml(robot.robot_id || robot.id || 'Unknown')}</div>
          </div>
          <span class="badge ${robot.connected ? 'badge-success' : 'badge-danger'}" style="margin-left:auto;">
            ${robot.connected ? '● Online' : '● Offline'}
          </span>
        </div>

        <div class="robot-stats">
          <div class="robot-stat">
            <div class="value">${robot.battery || '—'}${robot.battery ? '%' : ''}</div>
            <div class="label">🔋 Battery</div>
          </div>
          <div class="robot-stat">
            <div class="value">${robot.scans_today || 0}</div>
            <div class="label">📷 Scans Today</div>
          </div>
          <div class="robot-stat">
            <div class="value">${robot.signal_strength || '—'}</div>
            <div class="label">📶 Signal</div>
          </div>
          <div class="robot-stat">
            <div class="value">${robot.last_seen ? formatRelativeTime(robot.last_seen) : '—'}</div>
            <div class="label">⏱️ Last Seen</div>
          </div>
        </div>

        <div class="robot-actions">
          <button class="btn btn-primary btn-sm" onclick="triggerRobotScan('${escapeHtml(robot.robot_id || robot.id)}')">
            📷 Scan
          </button>
          <button class="btn btn-accent btn-sm" onclick="makeRobotSpeak('${escapeHtml(robot.robot_id || robot.id)}')">
            🔊 Speak
          </button>
          <button class="btn btn-ghost btn-sm" onclick="viewRobotStatus('${escapeHtml(robot.robot_id || robot.id)}')">
            📋 Details
          </button>
        </div>
      </div>
    `).join('');
  } else {
    grid.innerHTML = `
      <div class="card" style="grid-column:1/-1;">
        <div class="empty-state">
          <div class="empty-icon">🤖</div>
          <div class="empty-title">No Robots Connected</div>
          <div class="empty-text">Click "Scan for Bots" to discover AgroMate robots nearby via Bluetooth.</div>
          <button class="btn btn-primary" style="margin-top:var(--space-lg);" onclick="scanForBots()">
            📡 Scan for Bots
          </button>
        </div>
      </div>
    `;

    // Show BLE info
    if (data && data.ble) {
      const bleAvailable = data.ble.available !== false;
      grid.innerHTML += `
        <div class="card" style="grid-column:1/-1;">
          <div style="display:flex;align-items:center;gap:var(--space-md);">
            <span class="badge ${bleAvailable ? 'badge-info' : 'badge-warning'}">
              ${bleAvailable ? '📡 BLE Available' : '⚠️ BLE Unavailable'}
            </span>
            <span style="font-size:var(--font-size-sm);color:var(--text-secondary);">
              ${bleAvailable ? 'Bluetooth Low Energy server is running' : 'BLE server is not running. Check server logs.'}
            </span>
          </div>
        </div>
      `;
    }
  }
}

// AgroMate BLE Service & Characteristic UUIDs (must match firmware config.h)
const AGROMATE_BLE_SERVICE_UUID = '12345678-1234-5678-1234-56789abcde01';
const AGROMATE_BLE_CHAR = {
  CMD_TX:    '12345678-1234-5678-1234-56789abcdf01', // Laptop → ESP
  CMD_RX:    '12345678-1234-5678-1234-56789abcdf02', // ESP → Laptop
  IMG_TX:    '12345678-1234-5678-1234-56789abcdf03',
  AUDIO_TX:  '12345678-1234-5678-1234-56789abcdf04',
  AUDIO_RX:  '12345678-1234-5678-1234-56789abcdf05',
  BATTERY:   '12345678-1234-5678-1234-56789abcdf06',
  SENSORS:   '12345678-1234-5678-1234-56789abcdf07',
  HEARTBEAT: '12345678-1234-5678-1234-56789abcdf08',
};

// Track active BLE connection
let activeBleDevice = null;
let activeBleServer = null;
let activeBleService = null;

async function scanForBots() {
  const btn = document.getElementById('scanBotsBtn');
  
  // Try Web Bluetooth API (Chrome only)
  if ('bluetooth' in navigator) {
    try {
      if (btn) {
        btn.disabled = true;
        btn.innerHTML = '<div class="spinner spinner-sm" style="display:inline-block;"></div> Scanning...';
      }
      
      // Request device with multiple filter options for reliable discovery.
      // Web Bluetooth matches if ANY filter in the array matches.
      // Filter 1: Match by name prefix (device name in advertising data)
      // Filter 2: Match by service UUID (UUID in scan response)
      const device = await navigator.bluetooth.requestDevice({
        filters: [
          { namePrefix: 'AgroMate' },
          { services: [AGROMATE_BLE_SERVICE_UUID] }
        ],
        optionalServices: [AGROMATE_BLE_SERVICE_UUID],
      });
      
      showToast(`Found device: ${device.name || device.id}`, 'success');
      
      // Listen for disconnect events
      device.addEventListener('gattserverdisconnected', () => {
        showToast(`${device.name || 'Robot'} disconnected`, 'warning');
        activeBleDevice = null;
        activeBleServer = null;
        activeBleService = null;
        loadRobots(); // Refresh the robot list
      });
      
      // Connect to GATT server
      if (btn) btn.innerHTML = '<div class="spinner spinner-sm" style="display:inline-block;"></div> Connecting...';
      const server = await device.gatt.connect();
      showToast(`Connected to ${device.name}!`, 'success');
      
      activeBleDevice = device;
      activeBleServer = server;
      
      // Discover the AgroMate custom BLE service
      try {
        if (btn) btn.innerHTML = '<div class="spinner spinner-sm" style="display:inline-block;"></div> Discovering services...';
        const service = await server.getPrimaryService(AGROMATE_BLE_SERVICE_UUID);
        activeBleService = service;
        showToast('AgroMate BLE service discovered!', 'success');
        
        // Subscribe to heartbeat notifications for connection monitoring
        try {
          const heartbeatChar = await service.getCharacteristic(AGROMATE_BLE_CHAR.HEARTBEAT);
          await heartbeatChar.startNotifications();
          heartbeatChar.addEventListener('characteristicvaluechanged', (event) => {
            console.log('[BLE] Heartbeat received from robot');
          });
          await new Promise(resolve => setTimeout(resolve, 200));
        } catch (e) {
          console.warn('[BLE] Could not subscribe to heartbeat:', e);
        }

        // Subscribe to sensor/battery notifications
        try {
          const batteryChar = await service.getCharacteristic(AGROMATE_BLE_CHAR.BATTERY);
          await batteryChar.startNotifications();
          batteryChar.addEventListener('characteristicvaluechanged', (event) => {
            const value = event.target.value;
            if (value.byteLength >= 1) {
              const batteryPct = value.getUint8(0);
              console.log(`[BLE] Battery: ${batteryPct}%`);
            }
          });
          await new Promise(resolve => setTimeout(resolve, 200));
        } catch (e) {
          console.warn('[BLE] Could not subscribe to battery:', e);
        }
        
      } catch (serviceErr) {
        console.warn('[BLE] Could not discover AgroMate service:', serviceErr);
        showToast('Connected but service discovery failed. Robot may need a firmware update.', 'warning');
      }
      
      // Sync the robot with the backend so it shows up in the robot list
      const robotId = device.id || device.name || 'unknown';
      try {
        await api.post('/robots/sync', {
          robot_id: robotId,
          name: device.name || 'AgroMate Robot',
          mac_address: robotId,
          connected: true,
          battery: 85,
          signal_strength: 'Excellent',
          scans_today: 0,
        });
      } catch (syncErr) {
        console.warn('[BLE] Backend sync failed (API-only mode):', syncErr);
      }
      
      // Refresh the robot list
      await refreshRobots();
    } catch (err) {
      if (err.name === 'NotFoundError') {
        showToast('No AgroMate robots found nearby', 'warning');
      } else if (err.message && err.message.includes('cancelled')) {
        // User cancelled — do nothing
      } else {
        showToast(`Scan error: ${err.message}`, 'error');
      }
    } finally {
      if (btn) {
        btn.disabled = false;
        btn.innerHTML = '📡 Scan for Bots';
      }
    }
  } else {
    showToast('Web Bluetooth is not supported in this browser. Please use Chrome.', 'warning');
  }
}

async function triggerRobotScan(robotId) {
  // Try direct BLE if connected
  if (activeBleService) {
    try {
      const cmdChar = await activeBleService.getCharacteristic(AGROMATE_BLE_CHAR.CMD_TX);
      // Build a CAPTURE_REQUEST message (simplified: type byte 0x16, sequence 0, length 0, CRC 0x4FF7)
      const msg = new Uint8Array([0xA6, 0xAD, 0x16, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF7, 0x4F]);
      await cmdChar.writeValue(msg);
      showToast('Camera capture requested via BLE!', 'success');
      return;
    } catch (e) {
      console.warn('[BLE] Direct scan command failed, trying API:', e);
    }
  }
  // Fallback to backend API
  const result = await api.post(`/robots/${robotId}/scan`);
  if (result && result.status === 'success') {
    showToast('Camera capture requested!', 'success');
  } else {
    showToast('Failed to trigger scan', 'error');
  }
}

async function makeRobotSpeak(robotId) {
  const text = prompt('Enter text for the robot to speak:', 'Hello, I am AgroMate!');
  if (!text) return;
  
  const result = await api.post(`/robots/${robotId}/speak?text=${encodeURIComponent(text)}`);
  if (result && result.status === 'success') {
    showToast('Speech sent to robot!', 'success');
  } else {
    showToast('Failed to send speech', 'error');
  }
}

async function viewRobotStatus(robotId) {
  // Try backend API first for rich data
  const data = await api.get(`/robots/${robotId}/status`);
  if (data && data.robot) {
    const r = data.robot;
    const statusLines = [
      `Robot: ${r.name || robotId}`,
      `ID: ${r.robot_id || robotId}`,
      `Connected: ${r.connected ? 'Yes' : 'No'}`,
      `Battery: ${r.battery || '—'}%`,
      `Signal: ${r.signal_strength || '—'}`,
      `Last Seen: ${r.last_seen || '—'}`,
    ];
    // Add BLE connection info if available
    if (activeBleDevice) {
      statusLines.push(`Web BLE: Connected to ${activeBleDevice.name || activeBleDevice.id}`);
    }
    alert(statusLines.join('\n'));
  } else {
    // Fallback: show BLE-only info
    if (activeBleDevice) {
      alert(`Robot: ${activeBleDevice.name || 'AgroMate Robot'}\nBLE: Connected\nID: ${activeBleDevice.id || 'unknown'}`);
    } else {
      showToast('Could not retrieve robot details', 'error');
    }
  }
}

async function refreshRobots() {
  showToast('Refreshing robot list...', 'info');
  await loadRobots();
}
