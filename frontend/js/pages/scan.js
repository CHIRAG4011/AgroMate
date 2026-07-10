/* ============================================================
   AgroMate Scan & Diagnose Page — V2
   Auto-upload pipeline, laser scanners, and immediate voice results.
   ============================================================ */

let scanSelectedFile = null;
let scanMode = 'identify';  // 'identify' | 'diagnose' | 'pests' | 'nutrients'
let scanResult = null;

function renderScan(container) {
  container.innerHTML = `
    <div class="page-header">
      <h1 data-i18n="nav_scan">🔬 Scan & Diagnose</h1>
      <p style="color:var(--text-tertiary);">Capture a plant leaf or upload an image for immediate AI diagnostic reports</p>
    </div>

    <!-- Scan Modes -->
    <div class="scan-modes" style="display:flex; gap:8px; margin-bottom:var(--space-lg); overflow-x:auto; padding-bottom:8px;">
      <button class="btn ${scanMode === 'identify' ? 'btn-primary' : 'btn-secondary'}" onclick="setScanMode('identify')">
        🌿 Plant Identification
      </button>
      <button class="btn ${scanMode === 'diagnose' ? 'btn-primary' : 'btn-secondary'}" onclick="setScanMode('diagnose')">
        🦠 Disease Detection
      </button>
      <button class="btn ${scanMode === 'pests' ? 'btn-primary' : 'btn-secondary'}" onclick="setScanMode('pests')">
        🐛 Pest Detection
      </button>
      <button class="btn ${scanMode === 'nutrients' ? 'btn-primary' : 'btn-secondary'}" onclick="setScanMode('nutrients')">
        🍂 Nutrient deficiency
      </button>
    </div>

    <div class="grid-2">
      <!-- Left: Capture Box -->
      <div>
        ${scanSelectedFile ? renderScanPreview() : renderScanUploadZone()}
      </div>

      <!-- Right: Diagnostic Output -->
      <div id="scanResultContainer">
        ${scanResult ? renderScanResult() : renderScanPlaceholder()}
      </div>
    </div>

    <input type="file" id="scanFileInput" accept="image/*" style="display:none;" onchange="handleScanFile(event)">
  `;
  i18n.translateDOM();
}

function renderScanUploadZone() {
  return `
    <div class="scan-area" id="scanDropZone"
         ondragover="event.preventDefault(); this.classList.add('active');"
         ondragleave="this.classList.remove('active');"
         ondrop="handleScanDrop(event)"
         onclick="document.getElementById('scanFileInput').click()">
      <span class="scan-icon">📸</span>
      <h3 style="margin-top:var(--space-sm);">Drop image or Click to scan</h3>
      <p style="font-size:0.8rem; color:var(--text-tertiary);">Supports camera captures, JPEG, PNG, WEBP</p>
      
      <div style="display:flex; gap:8px; margin-top:var(--space-md);" onclick="event.stopPropagation();">
        <button class="btn btn-secondary btn-sm" onclick="document.getElementById('scanFileInput').click()">
          📁 Upload File
        </button>
        <button class="btn btn-primary btn-sm" onclick="openWebcamScanner()">
          📷 Open Camera
        </button>
      </div>
    </div>
  `;
}

function renderScanPreview() {
  const url = URL.createObjectURL(scanSelectedFile);
  return `
    <div style="position:relative; width:100%; border-radius:var(--radius-xl); overflow:hidden; border:var(--glass-border); background:#000; min-height:300px; display:flex; align-items:center; justify-content:center;">
      <img src="${url}" alt="Plant scan preview" style="max-width:100%; max-height:450px; display:block;">
      
      <!-- Animated scanning line -->
      <div id="scanLaserLine" style="position:absolute; left:0; width:100%; height:4px; background:linear-gradient(to bottom, rgba(34,197,94,0), var(--accent-primary), rgba(34,197,94,0)); top:0; animation: scanAnimation 2.5s infinite ease-in-out; display:none;"></div>
      
      <button class="btn btn-secondary btn-sm" onclick="clearScan()" style="position:absolute; top:12px; right:12px; border-radius:var(--radius-full); width:32px; height:32px; min-height:auto; padding:0;">
        ✕
      </button>
    </div>
  `;
}

function renderScanPlaceholder() {
  return `
    <div class="card" style="height:100%; display:flex; flex-direction:column; justify-content:center; align-items:center; text-align:center; padding:var(--space-2xl) var(--space-md); color:var(--text-secondary);">
      <span style="font-size:3rem; display:block; margin-bottom:12px;">🔬</span>
      <h3>Diagnostic Report Ready</h3>
      <p style="font-size:0.85rem; margin-top:8px; max-width:350px;">AI analyzer will print details here instantly once you select or capture a plant image.</p>
    </div>
  `;
}

function renderScanResult() {
  const r = scanResult;
  const confidence = (r.confidence * 100).toFixed(1);
  const isHealthy = r.is_healthy !== false;
  
  let rowsHtml = '';
  
  if (scanMode === 'identify') {
    rowsHtml = `
      <div style="display:flex; justify-content:space-between; padding:8px 0; border-bottom:1px solid var(--border-subtle);">
        <span style="color:var(--text-tertiary);">Common Name</span>
        <span style="font-weight:600;">${r.plant_name || 'Unknown'}</span>
      </div>
      <div style="display:flex; justify-content:space-between; padding:8px 0; border-bottom:1px solid var(--border-subtle);">
        <span style="color:var(--text-tertiary);">Scientific Name</span>
        <span style="font-style:italic;">${r.scientific_name || '—'}</span>
      </div>
      <div style="display:flex; justify-content:space-between; padding:8px 0; border-bottom:1px solid var(--border-subtle);">
        <span style="color:var(--text-tertiary);">Family</span>
        <span>${r.family || '—'}</span>
      </div>
    `;
  } else {
    rowsHtml = `
      <div style="display:flex; justify-content:space-between; padding:8px 0; border-bottom:1px solid var(--border-subtle);">
        <span style="color:var(--text-tertiary);">Target Crop</span>
        <span style="font-weight:600;">${r.plant_name || 'Unknown'}</span>
      </div>
      <div style="display:flex; justify-content:space-between; padding:8px 0; border-bottom:1px solid var(--border-subtle);">
        <span style="color:var(--text-tertiary);">Condition</span>
        <span style="font-weight:600; color:${isHealthy ? 'var(--color-success)' : 'var(--color-danger)'};">
          ${r.disease_name || r.pest_name || r.primary_deficiency || (isHealthy ? 'Healthy Plant' : 'Diseased')}
        </span>
      </div>
      ${r.severity ? `
        <div style="display:flex; justify-content:space-between; padding:8px 0; border-bottom:1px solid var(--border-subtle);">
          <span style="color:var(--text-tertiary);">Severity</span>
          <span style="font-weight:600; text-transform:capitalize;">${r.severity}</span>
        </div>
      ` : ''}
      ${r.organic_treatment || r.organic_option ? `
        <div style="margin-top:12px; padding:10px; background:rgba(34,197,94,0.06); border-radius:var(--radius-md); border:1px dashed rgba(34,197,94,0.3); text-align:left;">
          <div style="font-size:0.8rem; font-weight:700; color:var(--color-success); margin-bottom:4px;">🌿 Natural/Organic Remedy</div>
          <div style="font-size:0.85rem; line-height:1.4;">${r.organic_treatment || r.organic_option}</div>
        </div>
      ` : ''}
      ${r.treatment ? `
        <div style="margin-top:8px; padding:10px; background:rgba(239,68,68,0.04); border-radius:var(--radius-md); border:1px dashed rgba(239,68,68,0.2); text-align:left;">
          <div style="font-size:0.8rem; font-weight:700; color:var(--color-danger); margin-bottom:4px;">🧪 Standard Treatment</div>
          <div style="font-size:0.85rem; line-height:1.4;">${r.treatment}</div>
        </div>
      ` : ''}
    `;
  }

  return `
    <div class="card scan-result-card" style="text-align:left;">
      <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:var(--space-md);">
        <h3 style="display:flex; align-items:center; gap:8px;">
          📋 AI Diagnostic Report
          <button class="btn btn-secondary btn-sm" onclick="toggleScanSpeech()" style="width:28px; height:28px; min-height:auto; padding:0; border-radius:50%; display:inline-flex; align-items:center; justify-content:center; border:1px solid var(--border-subtle);" title="Speak/Stop AI Summary">
            🔊
          </button>
        </h3>
        <span class="badge ${r.confidence > 0.75 ? 'badge-success' : 'badge-warning'}">
          ${confidence}% match
        </span>
      </div>

      <div class="confidence-bar" style="margin-bottom:var(--space-md);">
        <div class="confidence-fill" style="width:${confidence}%;"></div>
      </div>

      <div style="display:flex; flex-direction:column; gap:4px;">
        ${rowsHtml}
      </div>

      ${r.ai_summary ? `
        <div style="margin-top:16px; padding:12px; background:rgba(34,197,94,0.08); border-radius:var(--radius-lg); border:1px solid rgba(34,197,94,0.2); text-align:left;">
          <div style="font-size:0.85rem; font-weight:700; color:var(--accent-primary); margin-bottom:6px; display:flex; align-items:center; gap:6px;">
            ✨ <span data-i18n="ai_analysis_report">AI Analysis & Cures</span>
          </div>
          <div style="font-size:0.9rem; line-height:1.5; color:var(--text-primary);">${r.ai_summary}</div>
        </div>
      ` : ''}

      <div style="display:flex; gap:var(--space-sm); margin-top:var(--space-lg);">
        <button class="btn btn-primary" onclick="triggerAskAIChat()" style="flex:1;">
          💬 <span data-i18n="more_details">More Details</span>
        </button>
        <button class="btn btn-secondary" onclick="clearScan()" style="flex:1;">
          🔄 Scan Another
        </button>
      </div>
    </div>
  `;
}

function setScanMode(mode) {
  scanMode = mode;
  scanResult = null;
  scanSelectedFile = null;
  renderScan(document.getElementById('pageContainer'));
}

function handleScanFile(e) {
  const file = e.target.files[0];
  if (file && file.type.startsWith('image/')) {
    scanSelectedFile = file;
    renderScan(document.getElementById('pageContainer'));
    autoTriggerScan();
  }
}

function handleScanDrop(e) {
  e.preventDefault();
  const zone = document.getElementById('scanDropZone');
  if (zone) zone.classList.remove('active');

  const file = e.dataTransfer.files[0];
  if (file && file.type.startsWith('image/')) {
    scanSelectedFile = file;
    renderScan(document.getElementById('pageContainer'));
    autoTriggerScan();
  }
}

async function autoTriggerScan() {
  const line = document.getElementById('scanLaserLine');
  if (line) line.style.display = 'block';

  const formData = new FormData();
  formData.append('image', scanSelectedFile);

  let endpoint = `/plants/identify?language=${currentLanguage}`;
  if (scanMode === 'diagnose') endpoint = `/plants/diagnose?language=${currentLanguage}`;
  else if (scanMode === 'pests') endpoint = `/plants/pests?language=${currentLanguage}`;
  else if (scanMode === 'nutrients') endpoint = `/plants/nutrients?language=${currentLanguage}`;

  showToast('AI analysis running...', 'info');

  try {
    const data = await api.postForm(endpoint, formData);
    if (line) line.style.display = 'none';

    if (data && data.result) {
      scanResult = data.result;
      showToast('Analysis complete!', 'success');
      
      // Auto-set session context in chatbot engine for detailed lookup follow-ups
      await api.post('/chat/context', {
        session_id: 'default',
        scan_context: {
          plant_name: scanResult.plant_name || 'Plant',
          scientific_name: scanResult.scientific_name || '',
          disease_name: scanResult.disease_name || scanResult.pest_name || scanResult.primary_deficiency || '',
          confidence: scanResult.confidence || 1.0,
          severity: scanResult.severity || 'unknown',
          is_healthy: scanResult.is_healthy !== false
        }
      });

      // Auto TTS the short summary (respect autoplay toggle)
      const textToSpeak = scanResult.ai_summary || generateShortTtsSummary(scanResult);
      if (localStorage.getItem('agromate-voice-autoplay') !== 'false') {
        ttsPlayer.play(textToSpeak, currentLanguage);
      }

    } else {
      showToast('AI Analysis failed', 'error');
    }
  } catch (err) {
    if (line) line.style.display = 'none';
    showToast('Analysis connection failed', 'error');
  }

  renderScan(document.getElementById('pageContainer'));
}

function generateShortTtsSummary(res) {
  const isHealthy = res.is_healthy !== false;
  const plant = res.plant_name || 'plant';
  
  if (scanMode === 'identify') {
    return i18n.t('plant_identified', {
      plant_name: plant,
      confidence: Math.round(res.confidence * 100)
    });
  }

  if (isHealthy) {
    return i18n.t('healthy_plant');
  }

  const issue = res.disease_name || res.pest_name || res.primary_deficiency || 'stress';
  return i18n.t('disease_detected', {
    plant_name: plant,
    disease_name: issue,
    severity: res.severity || 'mild'
  });
}

function toggleScanSpeech() {
  if (ttsPlayer.currentText && ttsPlayer.mode === 'simple') {
    ttsPlayer.stop();
  } else {
    const textToSpeak = scanResult.ai_summary || generateShortTtsSummary(scanResult);
    ttsPlayer.play(textToSpeak, currentLanguage);
  }
}

function clearScan() {
  ttsPlayer.stop();
  scanSelectedFile = null;
  scanResult = null;
  renderScan(document.getElementById('pageContainer'));
}

async function openWebcamScanner() {
  try {
    // Request the highest quality the camera can provide
    const stream = await navigator.mediaDevices.getUserMedia({
      video: {
        facingMode: 'environment',
        width:  { ideal: 1280, min: 640 },
        height: { ideal: 720,  min: 480 },
        frameRate: { ideal: 30 }
      }
    });
    
    const modal = document.createElement('div');
    modal.className = 'modal-overlay';
    modal.style.display = 'flex';
    
    modal.innerHTML = `
      <div class="modal-content" style="max-width:600px; display:flex; flex-direction:column; gap:16px;">
        <h3>📸 Live Camera Scanner</h3>
        <p style="font-size:0.75rem; color:var(--text-tertiary); margin:-10px 0 0;" id="scanResInfo">Initializing camera...</p>
        <div style="position:relative; width:100%; border-radius:var(--radius-md); overflow:hidden; background:#000;">
          <video id="scanVideo" autoplay playsinline style="width:100%; display:block;"></video>
          <!-- Focus guide overlay -->
          <div style="position:absolute; inset:15%; border:2px dashed var(--accent-primary); pointer-events:none; border-radius:8px;"></div>
        </div>
        <div style="display:flex; gap:10px;">
          <button class="btn btn-primary" id="scanCaptureBtn" style="flex:1;">📸 Capture</button>
          <button class="btn btn-secondary" id="scanCancelBtn" style="flex:1;">Cancel</button>
        </div>
      </div>
    `;

    document.body.appendChild(modal);
    
    const video = document.getElementById('scanVideo');
    video.srcObject = stream;

    // Show actual resolution once video metadata loads
    video.onloadedmetadata = () => {
      const info = document.getElementById('scanResInfo');
      if (info) info.textContent = `Camera: ${video.videoWidth}×${video.videoHeight}px`;
    };

    document.getElementById('scanCaptureBtn').onclick = () => {
      // Capture at the full native video resolution (not the display size)
      const canvas = document.createElement('canvas');
      canvas.width = video.videoWidth;
      canvas.height = video.videoHeight;
      const ctx = canvas.getContext('2d');
      
      // Apply mild sharpening via unsharp mask for better plant detail
      ctx.drawImage(video, 0, 0);
      
      canvas.toBlob((blob) => {
        scanSelectedFile = new File([blob], 'camera-capture.jpg', { type: 'image/jpeg' });
        stream.getTracks().forEach(t => t.stop());
        modal.remove();
        renderScan(document.getElementById('pageContainer'));
        autoTriggerScan();
      }, 'image/jpeg', 0.95);  // Higher JPEG quality for cleaner image
    };

    document.getElementById('scanCancelBtn').onclick = () => {
      stream.getTracks().forEach(t => t.stop());
      modal.remove();
    };

  } catch (err) {
    showToast('Failed to open camera', 'error');
  }
}

function triggerAskAIChat() {
  // Pre-populate chat screen with a trigger message.
  // The backend chat session context is already loaded.
  const isHi = currentLanguage === 'hi';
  const query = isHi ? "मुझे इस पौधे की विस्तृत जानकारी और उपाय बताएं।" : "Please give me a detailed report about this plant scan.";

  chatMessages.push({ role: 'user', content: query });
  localStorage.setItem('agromate-chat-messages', JSON.stringify(chatMessages));

  navigateTo('chat');

  // Trigger click automatically inside chat page loader logic
  setTimeout(() => {
    fetchMoreDetails(chatMessages.length - 1);
  }, 100);
}
