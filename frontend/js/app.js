/* ============================================================
   AgroMate Web Dashboard — App Core (V2)
   SPA Router, i18n localization, TTS Player, STT Recorder,
   API Service, WebSocket Manager, Utilities
   ============================================================ */

// ── Configuration ──
const API_BASE = window.location.origin + '/api';
const WS_BASE  = window.location.origin.replace('http', 'ws');

// ── App State ──
let currentPage = 'dashboard';
let theme = localStorage.getItem('agromate-theme') || 'dark';
let currentLanguage = localStorage.getItem('agromate-lang') || 'en';
let accessibilityMode = localStorage.getItem('agromate-a11y') || 'normal';
let colorblindMode = localStorage.getItem('agromate-colorblind') || 'false';

// ── i18n Translations Cache ──
let translations = {};

// ── Initialize ──
document.addEventListener('DOMContentLoaded', async () => {
  applyTheme(theme);
  applyAccessibility(accessibilityMode);
  applyColorblindMode(colorblindMode);
  
  // Initialize i18n
  await i18n.setLanguage(currentLanguage);
  
  // Hash-based routing
  const hash = window.location.hash.replace('#', '') || 'dashboard';
  navigateTo(hash);
  
  window.addEventListener('hashchange', () => {
    const page = window.location.hash.replace('#', '') || 'dashboard';
    navigateTo(page);
  });
  
  // Check server status
  checkServerStatus();
  setInterval(checkServerStatus, 30000);
  
  // Populating language list in picker modal (lazy)
  i18n.loadLanguages();
  
  // Start global voice control
  initGlobalVoiceCommander();
});


/* ============================================================
   INTERNATIONALIZATION (i18n) MODULE
   ============================================================ */

const i18n = {
  languages: [],
  
  async loadLanguages() {
    try {
      const data = await api.get('/i18n/languages');
      if (data && data.status === 'success') {
        this.languages = data.languages;
        this.renderLanguageGrid();
      }
    } catch (err) {
      console.error('Failed to load languages list:', err);
    }
  },

  async setLanguage(langCode) {
    currentLanguage = langCode;
    localStorage.setItem('agromate-lang', langCode);
    
    // Fetch translation bundle
    try {
      const data = await api.get(`/i18n/translations/${langCode}`);
      if (data && data.status === 'success') {
        translations = data.translations;
        
        // Update document direction
        const rtlLanguages = ['ur', 'ks', 'sd'];
        const dir = rtlLanguages.includes(langCode) ? 'rtl' : 'ltr';
        document.documentElement.setAttribute('dir', dir);
        
        // Update language indicator flag/text
        const langFlag = document.getElementById('currentLangFlag');
        if (langFlag) {
          const langInfo = this.languages.find(l => l.code === langCode);
          langFlag.textContent = langInfo ? langInfo.native : '🌐';
        }
        
        // Translate all nodes having data-i18n attribute
        this.translateDOM();
        
        // Re-render current page to apply translations dynamically
        navigateTo(currentPage);
      }
    } catch (err) {
      console.error(`Failed to load translation bundle for ${langCode}:`, err);
    }
  },

  translateDOM() {
    document.querySelectorAll('[data-i18n]').forEach(el => {
      const key = el.getAttribute('data-i18n');
      if (translations[key]) {
        // If it's an input or textarea, set placeholder instead of textContent
        if (el.tagName === 'INPUT' || el.tagName === 'TEXTAREA') {
          el.setAttribute('placeholder', translations[key]);
        } else {
          el.innerHTML = translations[key];
        }
      }
    });
  },

  t(key, params = {}) {
    let text = translations[key] || key;
    if (params) {
      Object.keys(params).forEach(k => {
        text = text.replace(new RegExp(`{${k}}`, 'g'), params[k]);
      });
    }
    return text;
  },

  renderLanguageGrid() {
    const grid = document.getElementById('langGrid');
    if (!grid) return;
    
    grid.innerHTML = this.languages.map(lang => `
      <div class="lang-option ${lang.code === currentLanguage ? 'active' : ''}" 
           onclick="i18n.selectLanguage('${lang.code}')">
        <span class="lang-native">${lang.native}</span>
        <span class="lang-name">${lang.name}</span>
      </div>
    `).join('');
  },

  selectLanguage(langCode) {
    this.setLanguage(langCode);
    hideLanguagePicker();
    showToast(this.t('server_online'), 'success'); // Quick confirmation
  }
};

function showLanguagePicker() {
  const modal = document.getElementById('langPickerOverlay');
  if (modal) {
    modal.classList.remove('hidden');
    i18n.renderLanguageGrid();
  }
}

function hideLanguagePicker() {
  const modal = document.getElementById('langPickerOverlay');
  if (modal) modal.classList.add('hidden');
}


/* ============================================================
   TEXT-TO-SPEECH (TTS) PLAYER MODULE
   ============================================================ */

class TTSPlayer {
  constructor() {
    this.audio = new Audio();
    this.isPlaying = false;
    this.currentText = '';
    this.sections = [];
    this.currentSectionIndex = 0;
    this.cacheKey = null;
    this.mode = 'simple'; // 'simple' or 'sectioned'
    
    this.audio.addEventListener('play', () => this.onPlay());
    this.audio.addEventListener('pause', () => this.onPause());
    this.audio.addEventListener('ended', () => this.onEnded());
    this.audio.addEventListener('timeupdate', () => this.onTimeUpdate());
  }

  async play(text, language = currentLanguage) {
    if (!text || text.trim() === '') return;
    this.stop();
    this.currentText = text;
    this.mode = 'simple';

    // Update label
    const label = document.getElementById('ttsPlayerLabel');
    if (label) label.textContent = 'Generating speech...';
    
    const engine = localStorage.getItem('agromate-voice-engine') || 'edge-tts';
    const gender = localStorage.getItem('agromate-voice-gender') || 'female';

    try {
      // POST requests to TTS endpoint
      const response = await fetch(`${API_BASE}/voice/tts`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ text, language, gender, engine })
      });

      if (!response.ok) throw new Error('TTS API error');
      
      const blob = await response.blob();
      const url = URL.createObjectURL(blob);
      
      this.audio.src = url;
      this.audio.play();
      
      const bar = document.getElementById('ttsPlayerBar');
      if (bar) bar.classList.remove('hidden');
    } catch (err) {
      console.error('TTS playback failed:', err);
      showToast('Voice synthesis failed', 'error');
    }
  }

  async playSections(text, language = currentLanguage) {
    if (!text || text.trim() === '') return;
    this.stop();
    this.currentText = text;
    this.mode = 'sectioned';

    const label = document.getElementById('ttsPlayerLabel');
    if (label) label.textContent = 'Analyzing report sections...';

    const engine = localStorage.getItem('agromate-voice-engine') || 'edge-tts';
    const gender = localStorage.getItem('agromate-voice-gender') || 'female';

    try {
      const data = await api.post('/voice/tts/sections', { text, language, gender, engine });
      if (!data || data.status !== 'success') throw new Error('Sectioned TTS failed');

      const result = data.result;
      this.sections = result.sections;
      this.cacheKey = result.cache_key;
      this.currentSectionIndex = 0;

      // Play first section
      this.playSection(0);
      
      const bar = document.getElementById('ttsPlayerBar');
      if (bar) bar.classList.remove('hidden');
    } catch (err) {
      console.error('Sectioned TTS play error:', err);
      this.play(text, language); // fallback to simple synthesis
    }
  }

  playSection(index) {
    if (index < 0 || index >= this.sections.length) return;
    this.currentSectionIndex = index;
    
    // Update play label with section title
    const label = document.getElementById('ttsPlayerLabel');
    const sec = this.sections[index];
    if (label) label.textContent = `Speaking: ${sec.title || `Section ${index + 1}`}`;

    this.audio.src = `${API_BASE}/voice/tts/sections/${this.cacheKey}/${index}`;
    this.audio.play();
  }

  togglePlayPause() {
    if (this.isPlaying) {
      this.audio.pause();
    } else {
      this.audio.play();
    }
  }

  stop() {
    this.audio.pause();
    this.audio.currentTime = 0;
    this.isPlaying = false;
    this.sections = [];
    this.currentSectionIndex = 0;
    this.cacheKey = null;
    
    const bar = document.getElementById('ttsPlayerBar');
    if (bar) bar.classList.add('hidden');
  }

  replay() {
    if (this.mode === 'sectioned') {
      this.playSection(this.currentSectionIndex);
    } else {
      this.audio.currentTime = 0;
      this.audio.play();
    }
  }

  onPlay() {
    this.isPlaying = true;
    const btn = document.getElementById('ttsPlayPauseBtn');
    if (btn) btn.textContent = '⏸';
  }

  onPause() {
    this.isPlaying = false;
    const btn = document.getElementById('ttsPlayPauseBtn');
    if (btn) btn.textContent = '▶';
  }

  onEnded() {
    if (this.mode === 'sectioned' && this.currentSectionIndex + 1 < this.sections.length) {
      // Auto advance to next section
      this.playSection(this.currentSectionIndex + 1);
    } else {
      this.stop();
    }
  }

  onTimeUpdate() {
    const progressFill = document.getElementById('ttsProgressBar');
    if (progressFill && this.audio.duration) {
      const pct = (this.audio.currentTime / this.audio.duration) * 100;
      progressFill.style.width = `${pct}%`;
    }
  }
}

const ttsPlayer = new TTSPlayer();


/* ============================================================
   SPEECH-TO-TEXT (STT) RECORDING MODULE
   ============================================================ */

class STTRecorder {
  constructor() {
    this.mediaRecorder = null;
    this.audioChunks = [];
    this.isRecording = false;
    this.onTranscriptionCallback = null;
  }

  async start(buttonEl, onTranscription) {
    if (this.isRecording) return;
    this.onTranscriptionCallback = onTranscription;

    try {
      const stream = await navigator.mediaDevices.getUserMedia({ audio: true });
      this.audioChunks = [];
      this.mediaRecorder = new MediaRecorder(stream);
      
      this.mediaRecorder.ondataavailable = (event) => {
        this.audioChunks.push(event.data);
      };

      this.mediaRecorder.onstop = async () => {
        const audioBlob = new Blob(this.audioChunks, { type: 'audio/wav' });
        
        // Visual indicator back to normal
        buttonEl.classList.remove('recording');
        
        // Upload & transcribe
        const text = await this.uploadAndTranscribe(audioBlob);
        if (this.onTranscriptionCallback) {
          this.onTranscriptionCallback(text);
        }
      };

      this.mediaRecorder.start();
      this.isRecording = true;
      
      // Visual indicator
      buttonEl.classList.add('recording');
      showToast(i18n.t('listening'), 'info');

    } catch (err) {
      console.error('Failed to start microphone recording:', err);
      showToast('Microphone access denied or error occurred', 'error');
    }
  }

  stop() {
    if (!this.isRecording) return;
    this.mediaRecorder.stop();
    // Stop all media tracks
    this.mediaRecorder.stream.getTracks().forEach(track => track.stop());
    this.isRecording = false;
  }

  async uploadAndTranscribe(audioBlob) {
    const formData = new FormData();
    formData.append('file', audioBlob, 'voice.wav');

    // Display loader
    showToast(i18n.t('thinking'), 'info');

    try {
      const data = await api.postForm(`/voice/stt?language=${currentLanguage}`, formData);
      if (data && data.status === 'success') {
        return data.text;
      }
    } catch (err) {
      console.error('STT upload failed:', err);
      showToast(i18n.t('error'), 'error');
    }
    return '';
  }
}

const sttRecorder = new STTRecorder();


/* ============================================================
   ROUTER AND VIEW ENGINE
   ============================================================ */

function navigateTo(page) {
  currentPage = page;
  window.location.hash = page;
  
  // Stop TTS player on page change if desired
  // ttsPlayer.stop();

  // Update sidebar active state
  document.querySelectorAll('.nav-item').forEach(item => {
    item.classList.toggle('active', item.dataset.page === page);
  });
  
  // Close mobile sidebar
  const sidebar = document.getElementById('sidebar');
  const overlay = document.getElementById('sidebarOverlay');
  if (sidebar) sidebar.classList.remove('open');
  if (overlay) overlay.classList.remove('active');
  
  // Render page
  const container = document.getElementById('pageContainer');
  if (!container) return;
  
  // Fade out → render → fade in
  container.style.animation = 'none';
  container.offsetHeight; // reflow
  container.style.animation = 'fadeInPage 0.4s ease';
  
  switch (page) {
    case 'dashboard':  renderDashboard(container); break;
    case 'chat':       renderChat(container); break;
    case 'scan':       renderScan(container); break;
    case 'weather':    renderWeather(container); break;
    case 'robots':     renderRobots(container); break;
    case 'history':    renderHistory(container); break;
    case 'analytics':  renderAnalytics(container); break;
    case 'knowledge':  renderKnowledge(container); break;
    case 'settings':   renderSettings(container); break;
    default:           renderDashboard(container); break;
  }
}


/* ============================================================
   SIDEBAR
   ============================================================ */

function toggleSidebar() {
  const sidebar = document.getElementById('sidebar');
  const overlay = document.getElementById('sidebarOverlay');
  sidebar.classList.toggle('open');
  overlay.classList.toggle('active');
}


/* ============================================================
   API SERVICE
   ============================================================ */

const api = {
  async get(endpoint) {
    try {
      const res = await fetch(`${API_BASE}${endpoint}`);
      if (!res.ok) throw new Error(`HTTP ${res.status}`);
      return await res.json();
    } catch (err) {
      console.error(`API GET ${endpoint}:`, err);
      return null;
    }
  },
  
  async post(endpoint, body = {}) {
    try {
      const res = await fetch(`${API_BASE}${endpoint}`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(body),
      });
      if (!res.ok) throw new Error(`HTTP ${res.status}`);
      return await res.json();
    } catch (err) {
      console.error(`API POST ${endpoint}:`, err);
      return null;
    }
  },
  
  async postForm(endpoint, formData) {
    try {
      const res = await fetch(`${API_BASE}${endpoint}`, {
        method: 'POST',
        body: formData,
      });
      if (!res.ok) throw new Error(`HTTP ${res.status}`);
      return await res.json();
    } catch (err) {
      console.error(`API POST form ${endpoint}:`, err);
      return null;
    }
  },
  
  async delete(endpoint) {
    try {
      const res = await fetch(`${API_BASE}${endpoint}`, { method: 'DELETE' });
      if (!res.ok) throw new Error(`HTTP ${res.status}`);
      return await res.json();
    } catch (err) {
      console.error(`API DELETE ${endpoint}:`, err);
      return null;
    }
  }
};


/* ============================================================
   WEBSOCKET MANAGER
   ============================================================ */

class WSManager {
  constructor() {
    this.connections = {};
  }
  
  connect(channel, onMessage) {
    if (this.connections[channel]) {
      this.connections[channel].close();
    }
    
    const ws = new WebSocket(`${WS_BASE}/${channel}`);
    
    ws.onopen = () => console.log(`WS connected: ${channel}`);
    ws.onmessage = (e) => {
      try {
        const data = JSON.parse(e.data);
        onMessage(data);
      } catch (err) {
        console.error('WS message parse error:', err);
      }
    };
    ws.onerror = (err) => console.error(`WS error (${channel}):`, err);
    ws.onclose = () => {
      console.log(`WS disconnected: ${channel}`);
      // Auto-reconnect after 5s
      setTimeout(() => {
        if (this.connections[channel] === ws) {
          this.connect(channel, onMessage);
        }
      }, 5000);
    };
    
    this.connections[channel] = ws;
    return ws;
  }
  
  send(channel, data) {
    const ws = this.connections[channel];
    if (ws && ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify(data));
    }
  }
  
  disconnect(channel) {
    if (this.connections[channel]) {
      this.connections[channel].close();
      delete this.connections[channel];
    }
  }
  
  disconnectAll() {
    Object.keys(this.connections).forEach(ch => this.disconnect(ch));
  }
}

const wsManager = new WSManager();


/* ============================================================
   THEME, ACCESSIBILITY AND SPECIAL MODES
   ============================================================ */

function applyTheme(t) {
  theme = t;
  document.documentElement.setAttribute('data-theme', t);
  localStorage.setItem('agromate-theme', t);
}

function applyAccessibility(mode) {
  accessibilityMode = mode;
  document.documentElement.setAttribute('data-accessibility', mode);
  localStorage.setItem('agromate-a11y', mode);
}

function applyColorblindMode(enabled) {
  colorblindMode = enabled;
  document.documentElement.setAttribute('data-colorblind', enabled);
  localStorage.setItem('agromate-colorblind', enabled);
}

function toggleTheme() {
  applyTheme(theme === 'dark' ? 'light' : 'dark');
  if (currentPage === 'settings') {
    const container = document.getElementById('pageContainer');
    renderSettings(container);
  }
}


/* ============================================================
   SERVER STATUS CHECK
   ============================================================ */

async function checkServerStatus() {
  try {
    const res = await fetch(`${API_BASE.replace('/api', '')}/health`);
    const dot = document.getElementById('serverStatusDot');
    const text = document.getElementById('serverStatusText');
    const offlineIndicator = document.getElementById('offlineIndicator');
    
    if (res.ok) {
      if (dot) dot.classList.remove('offline');
      if (text) text.textContent = i18n.t('server_online');
      if (offlineIndicator) offlineIndicator.classList.add('hidden');
    } else {
      if (dot) dot.classList.add('offline');
      if (text) text.textContent = 'Server Error';
      if (offlineIndicator) offlineIndicator.classList.remove('hidden');
    }
  } catch {
    const dot = document.getElementById('serverStatusDot');
    const text = document.getElementById('serverStatusText');
    const offlineIndicator = document.getElementById('offlineIndicator');
    
    if (dot) dot.classList.add('offline');
    if (text) text.textContent = i18n.t('server_offline');
    if (offlineIndicator) offlineIndicator.classList.remove('hidden');
  }
}


/* ============================================================
   UTILITIES
   ============================================================ */

function formatTime(date) {
  return new Date(date).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
}

function formatDate(date) {
  return new Date(date).toLocaleDateString([], { month: 'short', day: 'numeric', year: 'numeric' });
}

function formatRelativeTime(dateString) {
  const now = new Date();
  const date = new Date(dateString);
  const diff = Math.floor((now - date) / 1000);
  
  if (diff < 60) return 'Just now';
  if (diff < 3600) return `${Math.floor(diff / 60)}m ago`;
  if (diff < 86400) return `${Math.floor(diff / 3600)}h ago`;
  if (diff < 604800) return `${Math.floor(diff / 86400)}d ago`;
  return formatDate(dateString);
}

function escapeHtml(text) {
  const div = document.createElement('div');
  div.textContent = text;
  return div.innerHTML;
}

// Simple markdown formatting for chat bubbles
function formatMessage(text) {
  if (!text) return '';
  let html = escapeHtml(text);
  // Bold
  html = html.replace(/\*\*(.*?)\*\*/g, '<strong>$1</strong>');
  // Italic
  html = html.replace(/\*(.*?)\*/g, '<em>$1</em>');
  // Code inline
  html = html.replace(/`(.*?)`/g, '<code style="background:var(--bg-input);padding:2px 6px;border-radius:4px;font-size:0.85em;">$1</code>');
  // Markdown links
  html = html.replace(/\[(.*?)\]\((.*?)\)/g, '<a href="$2" target="_blank">$1</a>');
  // Line breaks
  html = html.replace(/\n/g, '<br>');
  
  // Custom headers conversion (V2 Detailed Response)
  html = html.replace(/## (.*?)(<br>|$)/g, '<h4 style="margin-top:16px;margin-bottom:8px;color:var(--accent-secondary);">$1</h4>');
  
  // Bullet points
  html = html.replace(/^- (.*?)(<br>|$)/gm, '• $1$2');
  return html;
}

// Toast notifications
function showToast(message, type = 'info') {
  const existing = document.querySelector('.toast');
  if (existing) existing.remove();
  
  const toast = document.createElement('div');
  toast.className = 'toast';
  toast.style.cssText = `
    position: fixed;
    bottom: 24px;
    right: 24px;
    padding: 12px 20px;
    border-radius: 12px;
    font-size: 0.875rem;
    font-weight: 500;
    z-index: 1000;
    animation: slideInBubble 0.3s ease;
    backdrop-filter: blur(12px);
    border: 1px solid var(--border-subtle);
    max-width: 400px;
  `;
  
  const colors = {
    success: 'background: hsla(142,71%,45%,0.15); color: var(--color-success); border-color: hsla(142,71%,45%,0.3);',
    error:   'background: hsla(0,84%,60%,0.15); color: var(--color-danger); border-color: hsla(0,84%,60%,0.3);',
    warning: 'background: hsla(45,93%,47%,0.15); color: var(--color-warning); border-color: hsla(45,93%,47%,0.3);',
    info:    'background: hsla(217,91%,60%,0.15); color: var(--color-info); border-color: hsla(217,91%,60%,0.3);',
  };
  
  toast.style.cssText += colors[type] || colors.info;
  toast.textContent = message;
  document.body.appendChild(toast);
  
  setTimeout(() => {
    toast.style.animation = 'slideUpBar 0.3s ease reverse';
    setTimeout(() => toast.remove(), 300);
  }, 3000);
}

// Animated counter
function animateCounter(element, target, duration = 1000) {
  if (!element) return;
  const start = parseInt(element.textContent) || 0;
  const increment = (target - start) / (duration / 16);
  let current = start;
  
  const timer = setInterval(() => {
    current += increment;
    if ((increment > 0 && current >= target) || (increment < 0 && current <= target)) {
      current = target;
      clearInterval(timer);
    }
    element.textContent = Math.round(current).toLocaleString();
  }, 16);
}

/* ============================================================
   GLOBAL VOICE COMMANDER
   ============================================================ */
let globalVoiceRecognition = null;

function initGlobalVoiceCommander() {
  // Autoplay welcome on first user interaction (browsers block autoplay without interaction)
  const welcomeSpeech = () => {
    const welcomed = sessionStorage.getItem('agromate-welcomed');
    if (!welcomed) {
      sessionStorage.setItem('agromate-welcomed', 'true');
      const msg = i18n.t('welcome') || "Welcome to AgroMate! I'm your AI farming assistant.";
      ttsPlayer.play(msg, currentLanguage);
    }
    document.removeEventListener('click', welcomeSpeech);
  };
  document.addEventListener('click', welcomeSpeech);

  // Initialize browser SpeechRecognition
  const SpeechRecognition = window.SpeechRecognition || window.webkitSpeechRecognition;
  if (!SpeechRecognition) {
    const badge = document.getElementById('voiceControlIndicator');
    if (badge) badge.style.display = 'none';
    console.warn("Speech recognition not supported in this browser.");
    return;
  }

  if (globalVoiceRecognition) {
    try {
      globalVoiceRecognition.stop();
    } catch(e) {}
  }

  const recognition = new SpeechRecognition();
  globalVoiceRecognition = recognition;
  recognition.continuous = true;
  recognition.interimResults = false;
  
  // Set correct locale for recognition
  if (currentLanguage === 'hi') {
    recognition.lang = 'hi-IN';
  } else if (currentLanguage === 'gu') {
    recognition.lang = 'gu-IN';
  } else if (currentLanguage === 'sd') {
    recognition.lang = 'sd-IN'; // Sindhi
  } else if (currentLanguage === 'ur') {
    recognition.lang = 'ur-PK'; // Urdu
  } else {
    recognition.lang = 'en-IN';
  }

  recognition.onresult = (event) => {
    const resultIndex = event.resultIndex;
    const transcript = event.results[resultIndex][0].transcript.trim().toLowerCase();
    console.log("Global Voice command recognized:", transcript);

    // Command mapping in English, Hindi, Gujarati, Sindhi, and Urdu keywords
    const cmdScan = ['scan', 'camera', 'स्कैन', 'કેમેરા', 'ڪيمرا', 'کیمرہ'];
    const cmdDashboard = ['dashboard', 'डैशबोर्ड', 'ડેશ્બોર્ડ', 'ڊيش بورڊ', 'ڈیش بورڈ'];
    const cmdChat = ['chat', 'चैट', 'ચેટ', 'چيٽ', 'چیٹ', 'ai'];
    const cmdWeather = ['weather', 'मौसम', 'હવામાન', 'موسم'];
    const cmdRobots = ['robot', 'रोबोट', 'રોબોટ', 'روبوٽ', 'روبوٹ'];
    const cmdHistory = ['history', 'इतिहास', 'ઇતિહાસ', 'تاريخ', 'تاریخ'];
    const cmdAnalytics = ['analytics', 'विश्लेषण', 'એનાલિટિક્સ'];
    const cmdSettings = ['settings', 'सेटिंग्स', 'સેટિંગ્સ', 'سيٽنگون', 'ترتیبات'];

    const matches = (list) => list.some(word => transcript.includes(word));

    if (matches(cmdScan)) {
      navigateTo('scan');
      ttsPlayer.play(i18n.t('scan_plant') || "Opening scan camera", currentLanguage);
    } else if (matches(cmdDashboard)) {
      navigateTo('dashboard');
      ttsPlayer.play("Opening dashboard", currentLanguage);
    } else if (matches(cmdChat)) {
      navigateTo('chat');
      ttsPlayer.play(i18n.t('welcome') || "Opening chat assistant", currentLanguage);
    } else if (matches(cmdWeather)) {
      navigateTo('weather');
      ttsPlayer.play("Opening weather forecast", currentLanguage);
    } else if (matches(cmdRobots)) {
      navigateTo('robots');
      ttsPlayer.play("Opening robot manager", currentLanguage);
    } else if (matches(cmdHistory)) {
      navigateTo('history');
      ttsPlayer.play("Opening scan history", currentLanguage);
    } else if (matches(cmdAnalytics)) {
      navigateTo('analytics');
      ttsPlayer.play("Opening analytics dashboard", currentLanguage);
    } else if (matches(cmdSettings)) {
      navigateTo('settings');
      ttsPlayer.play("Opening settings", currentLanguage);
    }
  };

  recognition.onerror = (event) => {
    console.error("Global SpeechRecognition error:", event.error);
    const badge = document.getElementById('voiceControlIndicator');
    if (event.error === 'not-allowed') {
      if (badge) badge.style.display = 'none';
    }
  };

  recognition.onend = () => {
    // Auto restart if still on the page
    if (globalVoiceRecognition === recognition) {
      try {
        recognition.start();
      } catch (e) {}
    }
  };

  // Start listening
  try {
    recognition.start();
    const badge = document.getElementById('voiceControlIndicator');
    if (badge) badge.style.display = 'flex';
    console.log("Global voice commander active and listening...");
  } catch (e) {
    console.error("Failed to start speech recognition:", e);
  }
}

// Hook language change to update speech recognition locale
const originalSetLanguage = i18n.setLanguage;
i18n.setLanguage = async function(langCode) {
  await originalSetLanguage.call(i18n, langCode);
  initGlobalVoiceCommander();
};
