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
  const url = scanSelectedFile.mockUrl || URL.createObjectURL(scanSelectedFile);
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

function checkIsHealthy(res) {
  if (!res) return true;
  const disease = String(res.disease_name || res.issue_detected || '').trim();
  const pest = String(res.pest_name || '').trim();
  const deficiency = String(res.primary_deficiency || '').trim();

  for (let issue of [disease, pest, deficiency]) {
    if (issue && !['healthy', 'none', 'n/a', '', 'null'].includes(issue.toLowerCase())) {
      return false;
    }
  }
  if (res.deficiency_detected === true) return false;
  if (res.is_healthy === false || res.is_healthy === 'false' || res.is_healthy === 'False') return false;
  return true;
}

function renderScanResult() {
  const r = scanResult;
  const confValue = parseFloat(r.confidence) || 0.85;
  const confidence = (confValue * (confValue <= 1.0 ? 100 : 1)).toFixed(1);
  const isHealthy = checkIsHealthy(r);
  
  // ── Confidence bar color ──
  const confColor = confValue >= 0.75 ? 'var(--color-success)'
                  : confValue >= 0.45 ? '#f59e0b'
                  : 'var(--color-danger)';
  const confBadge = confValue >= 0.75 ? 'badge-success'
                  : confValue >= 0.45 ? 'badge-warning'
                  : 'badge-danger';

  // ── Severity badge helper ──
  function severityBadge(sev) {
    const map = { none: '#22c55e', mild: '#f59e0b', moderate: '#f97316', severe: '#ef4444' };
    const color = map[sev] || '#94a3b8';
    return `<span style="background:${color}22; color:${color}; border:1px solid ${color}44; padding:2px 10px; border-radius:99px; font-size:0.75rem; font-weight:700; text-transform:capitalize;">${sev || 'none'}</span>`;
  }

  // ── Info row helper ──
  function infoRow(label, value, opts = {}) {
    if (!value && value !== 0) return '';
    return `
      <div style="display:flex; justify-content:space-between; align-items:center; padding:8px 0; border-bottom:1px solid var(--border-subtle); gap:8px;">
        <span style="color:var(--text-tertiary); font-size:0.83rem; flex-shrink:0;">${label}</span>
        <span style="${opts.style || 'font-weight:600; text-align:right;'}">${value}</span>
      </div>`;
  }

  // ── Build mode-specific rows ──
  let mainRows = '';
  let extraCards = '';

  if (scanMode === 'identify') {
    const trefle = r.trefle_details || {};
    const family = r.family || trefle.genus || '—';
    const genus  = trefle.genus || '—';
    const year   = trefle.year ? ` (${trefle.year})` : '';
    const author = trefle.author ? ` — ${trefle.author}` : '';

    const imgHtml = trefle.image_url ? `
      <div style="margin-bottom:14px; border-radius:var(--radius-lg); overflow:hidden; max-height:180px; display:flex; align-items:center; justify-content:center; background:rgba(0,0,0,0.2);">
        <img src="${trefle.image_url}" alt="${r.plant_name}" style="width:100%; height:180px; object-fit:cover;" onerror="this.parentElement.style.display='none'">
      </div>` : '';

    const names = Array.isArray(r.common_names) && r.common_names.length
      ? `<div style="display:flex; flex-wrap:wrap; gap:4px; margin-top:4px;">
          ${r.common_names.slice(0,5).map(n => `<span style="background:var(--bg-input); border:1px solid var(--border-subtle); border-radius:99px; padding:2px 10px; font-size:0.75rem;">${n}</span>`).join('')}
         </div>` : '';

    mainRows = imgHtml + `
      ${infoRow('🌿 Common Name', `<strong>${r.plant_name || 'Unknown'}</strong>`, {style:'font-size:1.05rem;font-weight:700;text-align:right;'})}
      ${infoRow('🔬 Scientific', `<em>${r.scientific_name || '—'}</em>`)}
      ${infoRow('🌸 Family', family)}
      ${genus !== '—' ? infoRow('🌱 Genus', genus) : ''}
      ${trefle.bibliography ? infoRow('📚 Reference', `<span style="font-size:0.78rem; color:var(--text-tertiary);">${trefle.bibliography}${year}${author}</span>`) : ''}
    `;
    if (names) mainRows += `<div style="padding:8px 0;"><span style="color:var(--text-tertiary); font-size:0.83rem;">🏷️ Also known as</span>${names}</div>`;

    if (Array.isArray(r.top_5) && r.top_5.length > 1) {
      const top5Html = r.top_5.slice(1, 5).map(t => {
        const pct = ((parseFloat(t.confidence) || 0) * 100).toFixed(0);
        return `<div style="display:flex; justify-content:space-between; align-items:center; padding:4px 0; font-size:0.8rem;">
          <span style="color:var(--text-secondary);">${t.name}</span>
          <span style="color:var(--text-tertiary);">${pct}%</span>
        </div>`;
      }).join('');
      extraCards += `
        <div style="margin-top:12px; padding:10px 12px; background:var(--bg-input); border-radius:var(--radius-md); border:1px solid var(--border-subtle);">
          <div style="font-size:0.78rem; font-weight:700; color:var(--text-tertiary); margin-bottom:6px; letter-spacing:.05em; text-transform:uppercase;">Other Candidates</div>
          ${top5Html}
        </div>`;
    }

  } else if (scanMode === 'diagnose') {
    const condition = r.disease_name || (isHealthy ? 'Healthy Plant ✅' : 'Unknown Disease');
    const condColor = isHealthy ? 'var(--color-success)' : 'var(--color-danger)';
    mainRows = `
      ${infoRow('🌿 Crop', `<strong>${r.plant_name || 'Unknown'}</strong>`, {style:'font-weight:700;text-align:right;'})}
      ${infoRow('🦠 Condition', `<span style="color:${condColor}; font-weight:700;">${condition}</span>`)}
      ${r.severity ? infoRow('⚡ Severity', severityBadge(r.severity)) : ''}
      ${r.affected_area_percent != null ? infoRow('📐 Affected Area', `${parseFloat(r.affected_area_percent).toFixed(0)}%`) : ''}
    `;
    if (r.organic_treatment) extraCards += treatmentCard('🌿 Organic Remedy', r.organic_treatment, '#22c55e');
    if (r.treatment) extraCards += treatmentCard('🧪 Chemical Treatment', r.treatment, '#ef4444');

  } else if (scanMode === 'pests') {
    const pestFound = r.pest_name && r.pest_name !== 'None' && r.pest_name !== '';
    mainRows = `
      ${infoRow('🐛 Pest Detected', pestFound
          ? `<span style="color:var(--color-danger); font-weight:700;">${r.pest_name}</span>`
          : `<span style="color:var(--color-success); font-weight:700;">None Detected ✅</span>`)}
      ${r.count != null && r.count > 0 ? infoRow('🔢 Count', `~${r.count} observed`) : ''}
      ${r.severity ? infoRow('⚡ Severity', severityBadge(r.severity)) : ''}
    `;
    if (r.organic_treatment) extraCards += treatmentCard('🌿 Organic IPM', r.organic_treatment, '#22c55e');
    if (r.chemical_treatment) extraCards += treatmentCard('🧪 Chemical Control', r.chemical_treatment, '#ef4444');
    if (r.prevention) extraCards += treatmentCard('🛡️ Prevention Tips', r.prevention, '#3b82f6');

  } else if (scanMode === 'nutrients') {
    const defFound = r.deficiency_detected;
    mainRows = `
      ${infoRow('🍃 Deficiency', defFound
          ? `<span style="color:var(--color-danger); font-weight:700;">${r.primary_deficiency}</span>`
          : `<span style="color:var(--color-success); font-weight:700;">None Detected ✅</span>`)}
      ${r.symptoms_matched ? infoRow('🔍 Symptoms', `<span style="font-size:0.82rem; text-align:right;">${r.symptoms_matched}</span>`) : ''}
    `;
    if (r.organic_option) extraCards += treatmentCard('🌿 Organic Fix', r.organic_option, '#22c55e');
    if (r.treatment) extraCards += treatmentCard('💊 Fertilizer', r.treatment, '#f59e0b');
  }

  return `
    <div class="card" style="text-align:left; overflow:hidden; padding:0;">
      <div style="padding:14px 16px 12px; border-bottom:1px solid var(--border-subtle); display:flex; justify-content:space-between; align-items:center; gap:8px;">
        <div style="display:flex; align-items:center; gap:8px;">
          <span style="font-size:1.1rem; font-weight:700;">📋 AI Diagnostic Report</span>
          <button class="btn btn-secondary btn-sm" onclick="toggleScanSpeech()"
            style="width:26px;height:26px;min-height:auto;padding:0;border-radius:50%;display:inline-flex;align-items:center;justify-content:center;font-size:0.85rem;"
            title="Read AI Summary aloud">🔊</button>
        </div>
        <span class="badge ${confBadge}" style="white-space:nowrap;">${confidence}% match</span>
      </div>
      <div style="height:5px; background:var(--bg-input); position:relative;">
        <div style="position:absolute; left:0; top:0; height:100%; width:${confidence}%; background:${confColor}; border-radius:0 4px 4px 0; transition:width 0.8s ease;"></div>
      </div>
      <div style="padding:14px 16px;">
        <div style="display:flex; flex-direction:column;">
          ${mainRows}
        </div>
        ${extraCards}
        ${r.ai_summary ? `
          <div style="margin-top:14px; padding:12px 14px; background:linear-gradient(135deg, rgba(34,197,94,0.08), rgba(59,130,246,0.05)); border-radius:var(--radius-lg); border:1px solid rgba(34,197,94,0.2);">
            <div style="font-size:0.78rem; font-weight:700; color:var(--accent-primary); margin-bottom:6px; letter-spacing:.04em; text-transform:uppercase; display:flex; align-items:center; gap:5px;">
              ✨ AI Analysis
            </div>
            <div style="font-size:0.88rem; line-height:1.6; color:var(--text-primary);">${formatMessage(r.ai_summary)}</div>
          </div>
        ` : ''}
        <div style="display:flex; gap:8px; margin-top:14px;">
          <button class="btn btn-primary" onclick="triggerAskAIChat()" style="flex:1; font-size:0.85rem;">
            💬 Ask AI for Details
          </button>
          <button class="btn btn-secondary" onclick="clearScan()" style="flex:1; font-size:0.85rem;">
            🔄 Scan Another
          </button>
        </div>
      </div>
    </div>
  `;
}

// Helper: renders a coloured treatment/info card
function treatmentCard(title, text, accentColor) {
  if (!text) return '';
  return `
    <div style="margin-top:10px; padding:11px 13px; background:${accentColor}0d; border-radius:var(--radius-md); border:1px dashed ${accentColor}44;">
      <div style="font-size:0.78rem; font-weight:700; color:${accentColor}; margin-bottom:5px; letter-spacing:.04em; text-transform:uppercase;">${title}</div>
      <div style="font-size:0.84rem; line-height:1.5; color:var(--text-primary);">${formatMessage(text)}</div>
    </div>`;
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

  // Animated progress messages shown while waiting
  const steps = [
    '🔍 Enhancing image quality...',
    '🌿 Identifying plant species...',
    '🤖 Consulting AI vision model...',
    '📊 Analyzing scan data...',
    '✨ Generating diagnosis report...'
  ];
  let stepIdx = 0;
  showToast(steps[0], 'info');
  const stepInterval = setInterval(() => {
    stepIdx = (stepIdx + 1) % steps.length;
    showToast(steps[stepIdx], 'info');
  }, 5000);

  // 90-second abort timeout so the UI never hangs indefinitely
  const controller = new AbortController();
  const scanTimeout = setTimeout(() => controller.abort(), 90000);

  try {
    const res = await fetch(`${API_BASE}${endpoint}`, {
      method: 'POST',
      body: formData,
      signal: controller.signal,
    });
    clearInterval(stepInterval);
    clearTimeout(scanTimeout);
    if (line) line.style.display = 'none';

    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    const data = await res.json();

    if (data && data.result) {
      scanResult = data.result;
      showToast('✅ Analysis complete!', 'success');
      
      // Auto-set session context in chatbot engine for detailed lookup follow-ups
      await api.post('/chat/context', {
        session_id: 'default',
        scan_context: {
          plant_name: scanResult.plant_name || 'Plant',
          scientific_name: scanResult.scientific_name || '',
          disease_name: scanResult.disease_name || scanResult.pest_name || scanResult.primary_deficiency || '',
          confidence: parseFloat(scanResult.confidence) || 1.0,
          severity: scanResult.severity || 'unknown',
          is_healthy: checkIsHealthy(scanResult)
        }
      });

      // Auto TTS the short summary (respect autoplay toggle)
      const textToSpeak = scanResult.ai_summary || generateShortTtsSummary(scanResult);
      if (localStorage.getItem('agromate-voice-autoplay') !== 'false') {
        ttsPlayer.play(textToSpeak, currentLanguage);
      }

    } else {
      showToast('⚠️ AI Analysis failed — please try again', 'error');
    }
  } catch (err) {
    clearInterval(stepInterval);
    clearTimeout(scanTimeout);
    if (line) line.style.display = 'none';
    if (err.name === 'AbortError') {
      showToast('⏱️ Scan timed out. Check your connection and try again.', 'error');
    } else {
      showToast('❌ Connection error — make sure the server is running', 'error');
    }
    console.error('Scan error:', err);
  }

  renderScan(document.getElementById('pageContainer'));
}

function generateShortTtsSummary(res) {
  if (!res) return 'Scan complete';
  const isHealthy = checkIsHealthy(res);
  const plant = res.plant_name || 'plant';
  
  if (scanMode === 'identify') {
    const sci = res.scientific_name ? ` (scientific name: ${res.scientific_name})` : '';
    return `Identified ${plant}${sci} with ${Math.round((parseFloat(res.confidence)||0.9)*100)} percent confidence.`;
  }

  if (isHealthy) {
    return `The ${plant} appears healthy with no visible diseases or pests. Continue regular watering and maintenance.`;
  }

  const issue = res.disease_name || res.pest_name || res.primary_deficiency || 'disease';
  const severity = res.severity || 'moderate';
  const treatment = res.organic_treatment || res.treatment || '';
  const treatText = treatment ? `. Recommended treatment: ${treatment}` : '';

  return `${plant} diagnosed with ${issue}, severity ${severity}${treatText}.`;
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
    // Inject custom GPU-accelerated SVG sharpening filter
    let svgFilters = document.getElementById('cameraSvgFilters');
    if (!svgFilters) {
      const svgContainer = document.createElement('div');
      svgContainer.id = 'cameraSvgFilters';
      svgContainer.style.display = 'none';
      svgContainer.innerHTML = `
        <svg xmlns="http://www.w3.org/2000/svg">
          <defs>
            <filter id="svg-sharpen">
              <feConvolveMatrix order="3" kernelMatrix="0 -1 0 -1 5 -1 0 -1 0" preserveAlpha="true"/>
            </filter>
          </defs>
        </svg>
      `;
      document.body.appendChild(svgContainer);
    }

    // Try camera constraints starting from highest quality down to standard fallbacks
    let stream;
    const constraintsList = [
      {
        video: {
          facingMode: 'environment',
          width: { ideal: 1920 },
          height: { ideal: 1080 },
          frameRate: { ideal: 30 }
        }
      },
      {
        video: {
          facingMode: 'environment',
          width: { ideal: 1280 },
          height: { ideal: 720 }
        }
      },
      {
        video: {
          facingMode: 'environment'
        }
      },
      {
        video: true
      }
    ];

    for (const constraints of constraintsList) {
      try {
        stream = await navigator.mediaDevices.getUserMedia(constraints);
        break;
      } catch (e) {
        console.warn("Camera constraint failed, trying fallback...", e);
      }
    }

    if (!stream) {
      showToast('Failed to open camera. Please check permissions.', 'error');
      return;
    }

    // Apply continuous autofocus, exposure, and white balance settings if supported
    const track = stream.getVideoTracks()[0];
    if (track && track.getCapabilities && track.applyConstraints) {
      try {
        const capabilities = track.getCapabilities();
        const advancedConstraints = {};
        
        if (capabilities.focusMode && capabilities.focusMode.includes('continuous')) {
          advancedConstraints.focusMode = 'continuous';
        }
        if (capabilities.exposureMode && capabilities.exposureMode.includes('continuous')) {
          advancedConstraints.exposureMode = 'continuous';
        }
        if (capabilities.whiteBalanceMode && capabilities.whiteBalanceMode.includes('continuous')) {
          advancedConstraints.whiteBalanceMode = 'continuous';
        }
        
        if (Object.keys(advancedConstraints).length > 0) {
          await track.applyConstraints({ advanced: [advancedConstraints] });
        }
      } catch (err) {
        console.warn("Advanced camera capabilities application failed:", err);
      }
    }
    
    const modal = document.createElement('div');
    modal.className = 'modal-overlay';
    modal.style.display = 'flex';
    
    modal.innerHTML = `
      <div class="modal-content" style="max-width:550px; display:flex; flex-direction:column; gap:16px;">
        <h3 style="margin-bottom: 0;">📸 Live Camera Scanner</h3>
        <p style="font-size:0.75rem; color:var(--text-tertiary); margin:-10px 0 0;" id="scanResInfo">Initializing camera...</p>
        
        <div style="position:relative; width:100%; border-radius:var(--radius-md); overflow:hidden; background:#000; aspect-ratio: 4/3; display: flex; align-items: center; justify-content: center;">
          <video id="scanVideo" autoplay playsinline style="width:100%; height:100%; object-fit:cover; display:block; image-rendering: -webkit-optimize-contrast; transition: filter 0.1s ease;"></video>
          <!-- Focus guide overlay -->
          <div style="position:absolute; inset:15%; border:2px dashed var(--accent-primary); pointer-events:none; border-radius:8px;"></div>
        </div>

        <!-- Camera Enhancements Panel -->
        <div style="background: rgba(255,255,255,0.04); padding: 12px; border-radius: var(--radius-md); border: 1px solid var(--border-subtle); display: flex; flex-direction: column; gap: 8px;">
          <div style="display: flex; align-items: center; justify-content: space-between;">
            <strong style="font-size: 0.8rem; color: var(--text-primary);">🎨 Quality & Color Controls</strong>
            <button class="btn btn-secondary btn-sm" id="cameraResetBtn" style="padding: 2px 8px; min-height: auto; font-size: 0.7rem; border-radius: var(--radius-sm);">Reset</button>
          </div>
          
          <!-- Brightness -->
          <div style="display: flex; align-items: center; justify-content: space-between; gap: 8px;">
            <label style="font-size: 0.75rem; min-width: 75px; color: var(--text-secondary);">Brightness:</label>
            <input type="range" id="cameraBrightnessRange" min="50" max="150" value="100" style="flex: 1; accent-color: var(--accent-primary); cursor: pointer;">
            <span id="cameraBrightnessVal" style="font-size: 0.75rem; min-width: 35px; text-align: right; font-family: monospace;">100%</span>
          </div>

          <!-- Contrast -->
          <div style="display: flex; align-items: center; justify-content: space-between; gap: 8px;">
            <label style="font-size: 0.75rem; min-width: 75px; color: var(--text-secondary);">Contrast:</label>
            <input type="range" id="cameraContrastRange" min="50" max="150" value="100" style="flex: 1; accent-color: var(--accent-primary); cursor: pointer;">
            <span id="cameraContrastVal" style="font-size: 0.75rem; min-width: 35px; text-align: right; font-family: monospace;">100%</span>
          </div>

          <!-- Saturation -->
          <div style="display: flex; align-items: center; justify-content: space-between; gap: 8px;">
            <label style="font-size: 0.75rem; min-width: 75px; color: var(--text-secondary);">Saturation:</label>
            <input type="range" id="cameraSaturationRange" min="50" max="200" value="100" style="flex: 1; accent-color: var(--accent-primary); cursor: pointer;">
            <span id="cameraSaturationVal" style="font-size: 0.75rem; min-width: 35px; text-align: right; font-family: monospace;">100%</span>
          </div>

          <!-- Flicker Fix & Presets -->
          <div style="display: flex; justify-content: space-between; gap: 10px; margin-top: 4px;">
            <div style="display: flex; align-items: center; gap: 4px; flex: 1;">
              <label style="font-size: 0.75rem; color: var(--text-secondary); white-space: nowrap;">Flicker Fix:</label>
              <select id="cameraFlickerSelect" style="flex: 1; background: var(--bg-card); border: 1px solid var(--border-subtle); color: var(--text-primary); border-radius: 4px; font-size: 0.75rem; padding: 2px; cursor: pointer;">
                <option value="auto">Auto</option>
                <option value="50">50 Hz (India/EU)</option>
                <option value="60">60 Hz (US/Japan)</option>
              </select>
            </div>
            <div style="display: flex; align-items: center; gap: 4px; flex: 1;">
              <label style="font-size: 0.75rem; color: var(--text-secondary); white-space: nowrap;">Preset:</label>
              <select id="cameraPresetSelect" style="flex: 1; background: var(--bg-card); border: 1px solid var(--border-subtle); color: var(--text-primary); border-radius: 4px; font-size: 0.75rem; padding: 2px; cursor: pointer;">
                <option value="natural">Natural</option>
                <option value="vibrant">Vibrant Leaves</option>
                <option value="high-contrast">High Contrast</option>
                <option value="indoor">Indoor Light</option>
                <option value="custom" disabled>Custom</option>
              </select>
            </div>
          </div>
        </div>

        <div style="display:flex; gap:10px;">
          <button class="btn btn-primary" id="scanCaptureBtn" style="flex:1;">📸 Capture & Enhance</button>
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

    // Grab controls elements
    const brightnessRange = document.getElementById('cameraBrightnessRange');
    const contrastRange = document.getElementById('cameraContrastRange');
    const saturationRange = document.getElementById('cameraSaturationRange');
    const flickerSelect = document.getElementById('cameraFlickerSelect');
    const presetSelect = document.getElementById('cameraPresetSelect');
    const resetBtn = document.getElementById('cameraResetBtn');

    const brightnessVal = document.getElementById('cameraBrightnessVal');
    const contrastVal = document.getElementById('cameraContrastVal');
    const saturationVal = document.getElementById('cameraSaturationVal');

    // Function to apply filters to the video live preview
    function updateVideoFilters() {
      const b = brightnessRange.value / 100;
      const c = contrastRange.value / 100;
      const s = saturationRange.value / 100;
      
      brightnessVal.textContent = brightnessRange.value + '%';
      contrastVal.textContent = contrastRange.value + '%';
      saturationVal.textContent = saturationRange.value + '%';

      // Live update of video CSS filter with dynamic brightness, contrast, saturation, and GPU sharpening
      video.style.filter = `url(#svg-sharpen) brightness(${b}) contrast(${c}) saturate(${s})`;
    }

    // Input event listeners
    brightnessRange.oninput = () => {
      presetSelect.value = 'custom';
      updateVideoFilters();
    };
    contrastRange.oninput = () => {
      presetSelect.value = 'custom';
      updateVideoFilters();
    };
    saturationRange.oninput = () => {
      presetSelect.value = 'custom';
      updateVideoFilters();
    };

    presetSelect.onchange = () => {
      const preset = presetSelect.value;
      if (preset === 'natural') {
        brightnessRange.value = 100;
        contrastRange.value = 100;
        saturationRange.value = 100;
      } else if (preset === 'vibrant') {
        brightnessRange.value = 102;
        contrastRange.value = 110;
        saturationRange.value = 135;
      } else if (preset === 'high-contrast') {
        brightnessRange.value = 95;
        contrastRange.value = 120;
        saturationRange.value = 110;
      } else if (preset === 'indoor') {
        brightnessRange.value = 110;
        contrastRange.value = 105;
        saturationRange.value = 115;
      }
      updateVideoFilters();
    };

    resetBtn.onclick = () => {
      presetSelect.value = 'natural';
      brightnessRange.value = 100;
      contrastRange.value = 100;
      saturationRange.value = 100;
      updateVideoFilters();
    };

    // Handle flicker fix constraints dynamic switching
    flickerSelect.onchange = async () => {
      const freq = flickerSelect.value;
      const track = stream.getVideoTracks()[0];
      if (track && track.applyConstraints) {
        try {
          const constraints = {};
          if (freq === '50') {
            constraints.powerLineFrequency = 50;
          } else if (freq === '60') {
            constraints.powerLineFrequency = 60;
          } else {
            constraints.powerLineFrequency = 0; // standard/auto
          }
          await track.applyConstraints({ advanced: [constraints] });
        } catch (e) {
          console.warn("Could not apply powerLineFrequency constraint", e);
        }
      }
    };

    // Initialize filters
    updateVideoFilters();

    document.getElementById('scanCaptureBtn').onclick = () => {
      // Capture at the full native video resolution (not the display size)
      const canvas = document.createElement('canvas');
      canvas.width = video.videoWidth;
      canvas.height = video.videoHeight;
      const ctx = canvas.getContext('2d');

      // Set high quality smoothing settings
      ctx.imageSmoothingEnabled = true;
      ctx.imageSmoothingQuality = 'high';

      // Apply dynamic GPU filter values to canvas drawing operation
      const b = brightnessRange.value / 100;
      const c = contrastRange.value / 100;
      const s = saturationRange.value / 100;
      
      // Inject SVG filter + CSS color filters directly into the Canvas Context Filter pipeline
      ctx.filter = `url(#svg-sharpen) brightness(${b}) contrast(${c}) saturate(${s})`;
      
      // Draw the video frame with the applied GPU filters
      ctx.drawImage(video, 0, 0, canvas.width, canvas.height);
      
      canvas.toBlob((blob) => {
        scanSelectedFile = new File([blob], 'camera-capture.jpg', { type: 'image/jpeg' });
        
        // Clean up stream
        stream.getTracks().forEach(t => t.stop());
        
        // Remove SVG filters from DOM
        const svgFilters = document.getElementById('cameraSvgFilters');
        if (svgFilters) svgFilters.remove();
        
        modal.remove();
        renderScan(document.getElementById('pageContainer'));
        autoTriggerScan();
      }, 'image/jpeg', 0.95);  // Higher JPEG quality for cleaner image
    };

    document.getElementById('scanCancelBtn').onclick = () => {
      stream.getTracks().forEach(t => t.stop());
      const svgFilters = document.getElementById('cameraSvgFilters');
      if (svgFilters) svgFilters.remove();
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
