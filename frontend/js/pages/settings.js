/* ============================================================
   AgroMate Settings Page — V2
   Languages switch, Accessibility scales, Theme and Voice configs.
   ============================================================ */

async function renderSettings(container) {
  const currentTheme = document.documentElement.getAttribute('data-theme') || 'dark';
  const isLargeText = accessibilityMode === 'large';
  const isColorblind = colorblindMode === 'true';

  container.innerHTML = `
    <div class="page-header">
      <h1 data-i18n="nav_settings">⚙️ Settings</h1>
      <p style="color:var(--text-tertiary);">Personalize AgroMate settings, speech voices, and accessibility filters</p>
    </div>

    <div style="display:flex; flex-direction:column; gap:var(--space-lg); max-width:800px; margin:0 auto;">
      
      <!-- Internationalization Settings -->
      <div class="card">
        <h3 style="margin-bottom:var(--space-md);">🌐 <span data-i18n="settings_language">Language</span></h3>
        <div class="form-group" style="margin-bottom:0;">
          <label class="form-label">System Interface & Voice Language</label>
          <select class="input" id="settingsLangSelect" onchange="changeSystemLanguage(this.value)">
            <!-- Options dynamically loaded from languages list -->
          </select>
        </div>
      </div>

      <!-- Voice and Audio Settings -->
      <div class="card">
        <h3 style="margin-bottom:var(--space-md);">🔊 <span data-i18n="settings_voice">Voice Settings</span></h3>
        
        <div class="setting-row">
          <div class="setting-info">
            <h4>Voice Engine</h4>
            <p>Select speech engine (Edge-TTS or Gemini Multimodal AI)</p>
          </div>
          <select class="input" id="voiceEngineSelect" style="width:140px;" onchange="saveVoiceSettings()">
            <option value="edge-tts" ${localStorage.getItem('agromate-voice-engine') === 'edge-tts' ? 'selected' : ''}>Edge-TTS</option>
            <option value="gemini" ${localStorage.getItem('agromate-voice-engine') !== 'edge-tts' ? 'selected' : ''}>Gemini Multimodal AI</option>
          </select>
        </div>

        <div class="setting-row">
          <div class="setting-info">
            <h4>Voice Gender</h4>
            <p>Select female or male AI speech assistant</p>
          </div>
          <select class="input" id="voiceGenderSelect" style="width:140px;" onchange="saveVoiceSettings()">
            <option value="female" ${localStorage.getItem('agromate-voice-gender') !== 'male' ? 'selected' : ''}>Female</option>
            <option value="male" ${localStorage.getItem('agromate-voice-gender') === 'male' ? 'selected' : ''}>Male</option>
          </select>
        </div>

        <div class="setting-row">
          <div class="setting-info">
            <h4>Voice Auto-Play</h4>
            <p>Automatically read scan results aloud</p>
          </div>
          <div class="toggle ${localStorage.getItem('agromate-voice-autoplay') !== 'false' ? 'active' : ''}" 
               onclick="toggleVoiceAutoplay(this)"></div>
        </div>

        <div style="margin-top:var(--space-md); text-align:left;">
          <button class="btn btn-secondary btn-sm" onclick="previewVoiceOutput()">
            🔊 Listen to Voice Sample
          </button>
        </div>
      </div>

      <!-- Accessibility Scales -->
      <div class="card">
        <h3 style="margin-bottom:var(--space-md);">👁️ <span data-i18n="settings_accessibility">Accessibility</span></h3>
        
        <div class="setting-row">
          <div class="setting-info">
            <h4>Larger Font Size</h4>
            <p>Optimize typography scale for elderly farmers</p>
          </div>
          <div class="toggle ${isLargeText ? 'active' : ''}" onclick="toggleAccessibilitySize(this)"></div>
        </div>

        <div class="setting-row">
          <div class="setting-info">
            <h4>Color-Blind Mode</h4>
            <p>Apply blue-contrast color filters for readability</p>
          </div>
          <div class="toggle ${isColorblind ? 'active' : ''}" onclick="toggleColorblindFilter(this)"></div>
        </div>
      </div>

      <!-- Appearance theme -->
      <div class="card">
        <h3 style="margin-bottom:var(--space-md);">🎨 <span data-i18n="settings_theme">Theme</span></h3>
        <div class="setting-row">
          <div class="setting-info">
            <h4 data-i18n="dark_mode">Dark Mode</h4>
            <p>Switch between dark and light themes</p>
          </div>
          <div class="toggle ${currentTheme === 'dark' ? 'active' : ''}" onclick="toggleDashboardTheme(this)"></div>
        </div>
      </div>

      <!-- Server configurations -->
      <div class="card">
        <h3 style="margin-bottom:var(--space-md);">🖥️ Server Connectivity</h3>
        
        <div class="setting-row">
          <div class="setting-info">
            <h4>API Status</h4>
            <p>Backend HTTP services</p>
          </div>
          <span id="setServerStatusBadge" class="badge">Checking...</span>
        </div>

        <div class="setting-row">
          <div class="setting-info">
            <h4>BLE Server Gateway</h4>
            <p>ESP32 Bluetooth connection</p>
          </div>
          <span id="setBleStatusBadge" class="badge">Checking...</span>
        </div>
      </div>

      <!-- Actions -->
      <div style="display:flex; gap:var(--space-sm); flex-wrap:wrap; margin-top:var(--space-sm);">
        <button class="btn btn-secondary" onclick="checkConfigStatus()">
          🔄 Connection Check
        </button>
        <button class="btn btn-danger btn-sm" onclick="clearAllData()">
          🗑️ Clear User Data
        </button>
      </div>

    </div>
  `;

  // Load language selector options
  loadSettingsLanguages();
  checkConfigStatus();
}

function loadSettingsLanguages() {
  const select = document.getElementById('settingsLangSelect');
  if (!select) return;

  // Use list of languages loaded in i18n module
  select.innerHTML = i18n.languages.map(l => `
    <option value="${l.code}" ${l.code === currentLanguage ? 'selected' : ''}>
      ${l.native} (${l.name})
    </option>
  `).join('');
}

async function changeSystemLanguage(lang) {
  await i18n.setLanguage(lang);
  showToast("Language updated successfully", "success");
}

function toggleVoiceAutoplay(el) {
  const active = el.classList.toggle('active');
  localStorage.setItem('agromate-voice-autoplay', active ? 'true' : 'false');
  showToast(active ? "Voice auto-play enabled" : "Voice auto-play disabled", "info");
}

function saveVoiceSettings() {
  const engine = document.getElementById('voiceEngineSelect').value;
  const gender = document.getElementById('voiceGenderSelect').value;
  localStorage.setItem('agromate-voice-engine', engine);
  localStorage.setItem('agromate-voice-gender', gender);
  showToast("Voice preferences saved", "success");
}

function previewVoiceOutput() {
  const gender = localStorage.getItem('agromate-voice-gender') || 'female';
  
  const sampleText = {
    'en': 'Hello! I am your AgroMate voice assistant. I am ready to support your farm.',
    'hi': 'नमस्ते! मैं आपका एग्रोमेट आवाज सहायक हूँ। मैं आपके खेत की सहायता के लिए तैयार हूँ।',
    'gu': 'નમસ્તે! હું તમારો એગ્રોમેટ અવાજ સહાયક છું. હું તમારા ખેતરની મદદ માટે તૈયાર છું.'
  };

  const text = sampleText[currentLanguage] || 'Speech synthesis test successfully completed.';
  
  ttsPlayer.play(text, currentLanguage);
}

function toggleAccessibilitySize(el) {
  const active = el.classList.toggle('active');
  const mode = active ? 'large' : 'normal';
  applyAccessibility(mode);
  showToast(active ? "Large font mode active" : "Standard font mode active", "info");
}

function toggleColorblindFilter(el) {
  const active = el.classList.toggle('active');
  const mode = active ? 'true' : 'false';
  applyColorblindMode(mode);
  showToast(active ? "Color-blind filters active" : "Color-blind filters disabled", "info");
}

function toggleDashboardTheme(el) {
  el.classList.toggle('active');
  toggleTheme();
}

async function checkConfigStatus() {
  const serverBadge = document.getElementById('setServerStatusBadge');
  const bleBadge = document.getElementById('setBleStatusBadge');

  if (serverBadge) {
    serverBadge.className = 'badge badge-warning';
    serverBadge.textContent = 'Checking...';
  }
  if (bleBadge) {
    bleBadge.className = 'badge badge-warning';
    bleBadge.textContent = 'Checking...';
  }

  // Ping Server Health
  try {
    const res = await fetch(`${API_BASE.replace('/api', '')}/health`);
    if (res.ok && serverBadge) {
      serverBadge.className = 'badge badge-success';
      serverBadge.textContent = 'Online';
    } else if (serverBadge) {
      serverBadge.className = 'badge badge-danger';
      serverBadge.textContent = 'Error';
    }
  } catch {
    if (serverBadge) {
      serverBadge.className = 'badge badge-danger';
      serverBadge.textContent = 'Offline';
    }
  }

  // Ping BLE Gateway
  try {
    const root = await fetch(`${API_BASE.replace('/api', '')}/`);
    const data = await root.json();
    if (bleBadge) {
      if (data.ble_status && data.ble_status.running) {
        bleBadge.className = 'badge badge-success';
        bleBadge.textContent = 'Running';
      } else {
        bleBadge.className = 'badge badge-danger';
        bleBadge.textContent = 'Offline';
      }
    }
  } catch {
    if (bleBadge) {
      bleBadge.className = 'badge badge-danger';
      bleBadge.textContent = 'Offline';
    }
  }
}

async function clearAllData() {
  if (confirm("Reset application? This will wipe your preferences and chat logs.")) {
    localStorage.clear();
    await api.delete('/chat/history');
    showToast("Cleared application data. Restarting...", "success");
    setTimeout(() => window.location.reload(), 1200);
  }
}
