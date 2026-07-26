/* ============================================================
   AgroMate Dashboard Page — V2
   Material 3 UI, Weather Cards, Farm Health, Dynamic Activity
   ============================================================ */

async function renderDashboard(container) {
  container.innerHTML = `
    <div class="page-header">
      <h1 data-i18n="nav_dashboard">🌱 Dashboard</h1>
      <p style="color:var(--text-tertiary);" data-i18n="welcome">Welcome to AgroMate! I'm your AI farming assistant.</p>
    </div>

    <!-- Stats Grid -->
    <div class="grid-4" id="dashboardStats" style="margin-bottom:var(--space-lg);">
      <div class="card stat-card" style="border-left: 4px solid var(--accent-primary);">
        <div style="background: rgba(0, 255, 135, 0.1); width: 44px; height: 44px; border-radius: 12px; display: inline-flex; align-items: center; justify-content: center; margin-bottom: 8px;">
          <span style="font-size: 1.3rem;">🔬</span>
        </div>
        <div class="stat-value" id="statScans">—</div>
        <div class="stat-label" data-i18n="total_scans">Total Scans</div>
      </div>
      <div class="card stat-card" style="border-left: 4px solid var(--accent-secondary);">
        <div style="background: rgba(0, 229, 255, 0.1); width: 44px; height: 44px; border-radius: 12px; display: inline-flex; align-items: center; justify-content: center; margin-bottom: 8px;">
          <span style="font-size: 1.3rem;">🤖</span>
        </div>
        <div class="stat-value" id="statRobots">—</div>
        <div class="stat-label" data-i18n="connected_robots">Connected Robots</div>
      </div>
      <div class="card stat-card" style="border-left: 4px solid var(--color-danger);">
        <div style="background: rgba(255, 77, 77, 0.1); width: 44px; height: 44px; border-radius: 12px; display: inline-flex; align-items: center; justify-content: center; margin-bottom: 8px;">
          <span style="font-size: 1.3rem;">🦠</span>
        </div>
        <div class="stat-value" id="statDiseases">—</div>
        <div class="stat-label" data-i18n="diseases_detected">Diseases Detected</div>
      </div>
      <div class="card stat-card" style="border-left: 4px solid var(--color-warning);">
        <div style="background: rgba(251, 191, 36, 0.1); width: 44px; height: 44px; border-radius: 12px; display: inline-flex; align-items: center; justify-content: center; margin-bottom: 8px;">
          <span style="font-size: 1.3rem;">💬</span>
        </div>
        <div class="stat-value" id="statChats">—</div>
        <div class="stat-label" data-i18n="ai_conversations">AI Conversations</div>
      </div>
    </div>

    <!-- Dashboard Grid -->
    <div class="grid-2">
      <!-- Left Column: Actions & Robot Telemetry -->
      <div style="display:flex; flex-direction:column; gap:var(--space-lg);">
        <div class="card">
          <h3 style="margin-bottom:var(--space-md);">⚡ <span data-i18n="quick_actions">Quick Actions</span></h3>
          <div class="quick-actions">
            <button class="btn btn-primary btn-lg" onclick="navigateTo('scan')" style="flex:1; min-width:140px;">
              🔬 <span data-i18n="nav_scan">Scan & Diagnose</span>
            </button>
            <button class="btn btn-secondary btn-lg" onclick="navigateTo('chat')" style="flex:1; min-width:140px;">
              💬 <span data-i18n="nav_chat">AI Chat</span>
            </button>
            <button class="btn btn-secondary btn-lg" onclick="navigateTo('weather')" style="flex:1; min-width:140px;">
              🌦️ <span data-i18n="nav_weather">Weather</span>
            </button>
            <button class="btn btn-secondary btn-lg" onclick="navigateTo('knowledge')" style="flex:1; min-width:140px;">
              📚 <span data-i18n="nav_knowledge">Knowledge Base</span>
            </button>
          </div>
        </div>

        <!-- Telemetry Widget -->
        <div class="card">
          <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:var(--space-md);">
            <h3>🤖 <span data-i18n="live_robot_telemetry">Live Robot Telemetry</span></h3>
            <span class="badge badge-success" id="telemetryStatus">● Running</span>
          </div>
          <div class="telemetry-feed" id="telemetryFeed" style="max-height:220px; overflow-y:auto; font-family:var(--font-mono); font-size:0.8rem; background:rgba(0,0,0,0.2); padding:var(--space-md); border-radius:var(--radius-md); border:var(--glass-border); line-height:1.4;">
            <!-- Telemetry rows updated in JS -->
          </div>
          
          <!-- Last Scan Preview Card -->
          <div id="telemetryScanPreview" style="margin-top:var(--space-md); display:none; text-align:center;">
            <div style="font-size:0.75rem; color:var(--text-secondary); margin-bottom:6px; text-align:left; font-weight:600;">📸 Last Robot Scan Preview:</div>
            <div style="position:relative; border-radius:var(--radius-md); overflow:hidden; border:1px solid var(--border-subtle); background:#000; height:180px; display:flex; align-items:center; justify-content:center;">
              <img id="telemetryScanImg" src="" alt="Last robot scan" style="max-width:100%; max-height:100%; object-fit:contain;">
              <div id="telemetryScanLabel" style="position:absolute; bottom:0; left:0; right:0; background:rgba(0,0,0,0.7); padding:6px; font-size:0.75rem; color:#fff; text-align:left;"></div>
            </div>
          </div>

          <div style="display:flex; gap:var(--space-sm); margin-top:var(--space-md);">
            <button class="btn btn-primary btn-sm" onclick="triggerDashboardRobotScan()" style="flex:1;">📸 Scan Now</button>
            <button class="btn btn-secondary btn-sm" onclick="sendMockBotCmd('patrol')" style="flex:1;">🚀 Patrol</button>
          </div>
        </div>

        <!-- API Gateways Widget -->
        <div class="card" style="margin-top:0;">
          <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:var(--space-md);">
            <h3>📡 External API Gateways</h3>
            <button class="btn btn-secondary btn-sm" onclick="checkApiKeysStatus()" style="padding: 2px 8px; font-size: 0.75rem;">🔄 Refresh</button>
          </div>
          <div id="apiKeyStatusList" style="display:flex; flex-direction:column; gap:var(--space-sm); font-size: 0.85rem;">
            <div class="spinner" style="margin: 10px auto;"></div>
          </div>
        </div>
      </div>

      <!-- Right Column: Weather + Activity -->
      <div style="display:flex; flex-direction:column; gap:var(--space-lg);">
        <!-- Weather Summary Widget -->
        <div class="card card-accent" id="weatherWidget" onclick="navigateTo('weather')" style="cursor:pointer;">
          <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:var(--space-sm);">
            <h3 data-i18n="nav_weather">Weather</h3>
            <span class="badge badge-info" id="weatherLocName">Locating...</span>
          </div>
          <div id="weatherContent" style="display:flex; align-items:center; gap:var(--space-lg); justify-content:space-around;">
            <div class="spinner"></div>
          </div>
        </div>

        <!-- Crop Advisor Widget -->
        <div class="card" id="cropRecommendationsCard">
          <h3>🌾 <span data-i18n="seasonal_crop_advisory">Seasonal Crop Advisory</span></h3>
          <div id="cropRecommendationsContent" style="margin-top:var(--space-md);">
            <div class="spinner"></div>
          </div>
        </div>

        <!-- Recent Activity -->
        <div class="card" style="flex:1;">
          <h3 style="margin-bottom:var(--space-md);" data-i18n="nav_history">Scan History</h3>
          <div class="activity-list" id="activityList">
            <div class="skeleton" style="height:48px; margin-bottom:8px;"></div>
            <div class="skeleton" style="height:48px; margin-bottom:8px;"></div>
            <div class="skeleton" style="height:48px;"></div>
          </div>
        </div>
      </div>
    </div>
  </div>
  `;

  // Load dynamic data
  loadDashboardStats();
  loadWeatherWidget();
  loadRecentActivity();
  startLiveTelemetry();
  checkApiKeysStatus();
  loadLastScanPreview();

  // Re-translate current DOM (e.g. data-i18n tags)
  i18n.translateDOM();
}

async function loadDashboardStats() {
  const analytics = await api.get('/analytics/?days=30');
  const robots = await api.get('/robots/');
  
  const statScans = document.getElementById('statScans');
  const statRobots = document.getElementById('statRobots');
  const statDiseases = document.getElementById('statDiseases');
  const statChats = document.getElementById('statChats');
  
  if (analytics && analytics.database) {
    const db = analytics.database;
    if (statScans) animateCounter(statScans, db.total_scans || 0);
    if (statDiseases) animateCounter(statDiseases, db.total_diagnoses || 0);
    if (statChats) animateCounter(statChats, db.total_chats || 0);
  } else {
    if (statScans) statScans.textContent = '0';
    if (statDiseases) statDiseases.textContent = '0';
    if (statChats) statChats.textContent = '0';
  }
  
  if (robots && robots.robots) {
    if (statRobots) animateCounter(statRobots, robots.robots.length);
  } else {
    if (statRobots) statRobots.textContent = '0';
  }
}

async function loadWeatherWidget() {
  const content = document.getElementById('weatherContent');
  const badge = document.getElementById('weatherLocName');
  if (!content) return;

  const lat = localStorage.getItem('agromate-latitude') || '28.6139'; // Default New Delhi
  const lon = localStorage.getItem('agromate-longitude') || '77.2090';
  const city = localStorage.getItem('agromate-selected-city') || 'New Delhi';

  if (badge) badge.textContent = city;

  const data = await api.get(`/analytics/weather?lat=${lat}&lon=${lon}&city=${encodeURIComponent(city)}`);
  
  if (data && data.weather) {
    const w = data.weather;
    const temp = Math.round(w.temperature || 25);
    const desc = w.description || 'clear sky';
    const humidity = w.humidity || 60;
    const wind = Math.round(w.wind_speed || 10);
    
    content.innerHTML = `
      <div style="font-size:3rem;">${getWeatherIcon(desc)}</div>
      <div style="text-align:left;">
        <div style="font-size:2.5rem; font-weight:800; line-height:1.1;">${temp}°C</div>
        <div style="font-size:0.9rem; color:var(--text-secondary); text-transform:capitalize; margin-top:2px;">${desc}</div>
      </div>
      <div style="text-align:left; font-size:0.8rem; color:var(--text-tertiary); line-height:1.5;">
        <div>💧 Humidity: ${humidity}%</div>
        <div>💨 Wind: ${wind} km/h</div>
      </div>
    `;
    loadDashboardCrops(temp);
  } else {
    content.innerHTML = `
      <div style="font-size:2rem; opacity:0.5;">🌤️</div>
      <div style="font-size:0.85rem; color:var(--text-tertiary);">Weather information currently unavailable</div>
    `;
    loadDashboardCrops(25);
  }
}

function getWeatherIcon(desc) {
  const d = desc.toLowerCase();
  if (d.includes('clear') || d.includes('sun')) return '☀️';
  if (d.includes('cloud')) return '☁️';
  if (d.includes('rain') || d.includes('drizzle')) return '🌧️';
  if (d.includes('thunder') || d.includes('storm')) return '⛈️';
  if (d.includes('snow')) return '❄️';
  if (d.includes('fog') || d.includes('mist')) return '🌫️';
  return '🌤️';
}

async function loadDashboardCrops(temp) {
  const el = document.getElementById('cropRecommendationsContent');
  if (!el) return;

  const data = await api.get(`/plants/crops?temperature=${temp}`);
  if (data && data.status === 'success' && data.result) {
    const res = data.result;
    const recs = res.recommendations || [];
    if (recs.length === 0) {
      el.innerHTML = '<div style="font-size:0.85rem; color:var(--text-tertiary);">No seasonal recommendations available</div>';
      return;
    }
    
    const cropEmojis = {
      'Rice': '🌾', 'Cotton': '💮', 'Soybean': '🫘', 'Groundnut': '🥜',
      'Maize': '🌽', 'Sugarcane': '🎋', 'Wheat': '🌾', 'Chickpea': '🫘',
      'Mustard': '🌼', 'Potato': '🥔', 'Onion': '🧅', 'Barley': '🌾',
      'Pea': '🫛', 'Watermelon': '🍉', 'Muskmelon': '🍈', 'Cucumber': '🥒'
    };

    el.innerHTML = `
      <div style="font-size:0.8rem; font-weight:600; color:var(--accent-secondary); margin-bottom:8px;">
        Season: ${res.current_season} • ${recs.length} recommended crops
      </div>
      <div style="display:flex; flex-direction:column; gap:6px;">
        ${recs.slice(0, 2).map(r => {
          const emoji = cropEmojis[r.crop_name] || '🌱';
          const scorePercent = Math.round(r.suitability_score * 100);
          return `
            <div style="display:flex; align-items:center; gap:10px; background:rgba(255,255,255,0.03); padding:8px; border-radius:var(--radius-sm); border:1px solid var(--border-subtle);">
              <span style="font-size:1.5rem;">${emoji}</span>
              <div style="flex:1; text-align:left;">
                <div style="display:flex; justify-content:space-between; font-size:0.85rem; font-weight:600;">
                  <span>${r.crop_name}</span>
                  <span>${scorePercent}%</span>
                </div>
                <div style="font-size:0.75rem; color:var(--text-tertiary); display:flex; gap:10px; margin-top:2px;">
                  <span>💧 ${r.water_requirement}</span>
                  <span>🌍 ${r.ideal_soil}</span>
                </div>
              </div>
            </div>
          `;
        }).join('')}
      </div>
    `;
  } else {
    el.innerHTML = '<div style="font-size:0.85rem; color:var(--text-tertiary);">Failed to load recommendations</div>';
  }
}

async function loadRecentActivity() {
  const el = document.getElementById('activityList');
  if (!el) return;

  const scans = await api.get('/scans/?limit=3');
  if (scans && scans.scans && scans.scans.length > 0) {
    el.innerHTML = scans.scans.map(scan => {
      const diag = scan.diagnoses && scan.diagnoses[0];
      const isHealthy = scan.scan_type === 'identification' || (diag && (diag.disease_name === 'Healthy' || diag.disease_name === 'none'));
      const title = diag ? diag.plant_name : 'Unknown Plant';
      const detail = diag ? (isHealthy ? 'Healthy' : diag.disease_name) : 'No disease info';
      const icon = isHealthy ? '✅' : '⚠️';
      const time = formatRelativeTime(scan.created_at);

      return `
        <div style="display:flex; align-items:center; justify-content:space-between; padding:8px 0; border-bottom:1px solid var(--border-subtle);">
          <div style="display:flex; align-items:center; gap:10px;">
            <span style="font-size:1.2rem;">${icon}</span>
            <div style="text-align:left;">
              <div style="font-size:0.85rem; font-weight:600;">${title}</div>
              <div style="font-size:0.75rem; color:var(--text-tertiary);">${detail}</div>
            </div>
          </div>
          <span style="font-size:0.75rem; color:var(--text-tertiary);">${time}</span>
        </div>
      `;
    }).join('');
  } else {
    el.innerHTML = `
      <div class="empty-state" style="padding:var(--space-md) 0;">
        <span class="empty-state-icon" style="font-size:2rem; margin-bottom:var(--space-sm);">📋</span>
        <h3>No activity records</h3>
        <p style="font-size:0.75rem; color:var(--text-tertiary);">Scan plants to build diagnosis history</p>
      </div>
    `;
  }
}

let teleTimer = null;

function startLiveTelemetry() {
  const el = document.getElementById('telemetryFeed');
  if (!el) return;

  const initialLogs = [
    '🤖 System Initialized. Checking connections...',
    '📶 BLE link online. Connected to AgroMate S3 Client.',
    '🔋 Telemetry: Battery 96% • Signal strength -52dBm',
    '📊 Sensors calibrating... Ready.',
  ];

  el.innerHTML = initialLogs.map(l => `<div style="margin-bottom:2px;">[${new Date().toLocaleTimeString()}] ${l}</div>`).join('');
  
  if (teleTimer) clearInterval(teleTimer);
  
  teleTimer = setInterval(() => {
    if (currentPage !== 'dashboard') {
      clearInterval(teleTimer);
      return;
    }
    const moisture = Math.floor(Math.random() * 10 + 40);
    const temp = Math.floor(Math.random() * 4 + 22);
    const time = new Date().toLocaleTimeString();
    
    el.innerHTML += `<div style="margin-bottom:2px;">[${time}] Telemetry: Moisture ${moisture}% • Temp ${temp}°C • Status OK</div>`;
    el.scrollTop = el.scrollHeight;

    const rows = el.children;
    if (rows.length > 8) rows[0].remove();
  }, 5000);
}

function sendMockBotCmd(cmd) {
  showToast(`Sent "${cmd.toUpperCase()}" command to AgroMate robot`, 'success');
}

async function checkApiKeysStatus() {
  const el = document.getElementById('apiKeyStatusList');
  if (!el) return;

  el.innerHTML = '<div class="spinner" style="margin: 10px auto;"></div>';

  try {
    const res = await api.get('/analytics/keys');
    if (res && res.status === 'success' && res.keys) {
      const keys = res.keys;
      const apiInfo = {
        gemini: { label: 'Gemini AI API', icon: '🤖' },
        openai: { label: 'OpenAI API', icon: '🧠' },
        elevenlabs: { label: 'ElevenLabs Premium TTS', icon: '🗣️' },
        openweather: { label: 'OpenWeather Map API', icon: '🌦️' },
        plant_id: { label: 'Plant.id Diagnostic API', icon: '🌿' },
        plantnet: { label: 'Pl@ntNet Reference API', icon: '🍂' },
        trefle: { label: 'Trefle Botanical Database', icon: '📚' }
      };

      el.innerHTML = Object.keys(apiInfo).map(keyId => {
        const keyData = keys[keyId] || { status: 'not_connected', error: 'Missing' };
        const info = apiInfo[keyId];
        
        let badgeClass = 'badge-warning';
        let badgeText = 'Not Configured';
        let statusIcon = '⚪';

        if (keyData.status === 'connected') {
          badgeClass = 'badge-success';
          badgeText = 'Connected';
          statusIcon = '✅';
        } else if (keyData.status === 'error') {
          badgeClass = 'badge-danger';
          badgeText = 'Error';
          statusIcon = '❌';
        }

        const errorDetail = keyData.error && keyData.status === 'error' ? 
          `<div style="font-size:0.7rem; color:var(--color-danger); margin-top:2px;">${keyData.error}</div>` : '';

        return `
          <div style="display:flex; align-items:center; justify-content:space-between; padding:8px 0; border-bottom:1px solid var(--border-subtle);">
            <div style="display:flex; align-items:center; gap:8px;">
              <span style="font-size:1.1rem;">${info.icon}</span>
              <div style="text-align:left;">
                <div style="font-weight:600;">${info.label}</div>
                ${errorDetail}
              </div>
            </div>
            <div style="display:flex; align-items:center; gap:6px;">
              <span style="font-size:0.9rem;">${statusIcon}</span>
              <span class="badge ${badgeClass}">${badgeText}</span>
            </div>
          </div>
        `;
      }).join('');
    } else {
      el.innerHTML = '<div style="font-size:0.85rem; color:var(--text-tertiary);">Error reading API configurations</div>';
    }
  } catch (err) {
    el.innerHTML = '<div style="font-size:0.85rem; color:var(--text-tertiary);">Failed to query API statuses</div>';
  }
}

async function loadLastScanPreview() {
  const container = document.getElementById('telemetryScanPreview');
  const img = document.getElementById('telemetryScanImg');
  const label = document.getElementById('telemetryScanLabel');
  if (!container || !img || !label) return;

  try {
    const data = await api.get('/scans/?limit=1');
    if (data && data.scans && data.scans.length > 0) {
      const scan = data.scans[0];
      const diag = scan.diagnoses && scan.diagnoses[0];
      if (diag && scan.image_path) {
        img.src = scan.image_path;
        label.innerHTML = `<strong>${diag.plant_name}</strong> • ${diag.disease_name} (Conf: ${(diag.confidence * 100).toFixed(1)}%)`;
        container.style.display = 'block';
      }
    }
  } catch (err) {
    console.error('Failed to load last scan preview:', err);
  }
}

async function triggerDashboardRobotScan() {
  showToast('Fetching active robots...', 'info');
  try {
    const res = await api.get('/robots/');
    if (res && res.robots && res.robots.length > 0) {
      const connectedRobot = res.robots.find(r => r.connected || r.state === 'active');
      if (connectedRobot) {
        showToast(`Triggering camera scan on ${connectedRobot.name}...`, 'info');
        const scanRes = await api.post(`/robots/${connectedRobot.robot_id}/scan`);
        if (scanRes && scanRes.status === 'success') {
          showToast('Scan command sent successfully!', 'success');
        } else {
          showToast('Failed to send scan command', 'error');
        }
      } else {
        showToast('No active connected robots found!', 'error');
      }
    } else {
      showToast('No robots registered in backend!', 'error');
    }
  } catch (err) {
    showToast('Failed to trigger robot scan', 'error');
  }
}
