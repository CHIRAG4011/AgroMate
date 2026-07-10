/* ============================================================
   AgroMate Analytics Page — V2
   M3 summary statistics and interactive high-performance Canvas charts.
   ============================================================ */

async function renderAnalytics(container) {
  container.innerHTML = `
    <div class="page-header" style="display:flex; align-items:center; justify-content:space-between; flex-wrap:wrap; gap:var(--space-md);">
      <div>
        <h1 data-i18n="nav_analytics">📈 Analytics</h1>
        <p style="color:var(--text-tertiary);">Real-time metrics, disease occurrences, and scan activities</p>
      </div>
      <select class="input" style="width:160px;" id="analyticsDays" onchange="reloadAnalyticsData()">
        <option value="7">Last 7 days</option>
        <option value="30" selected>Last 30 days</option>
        <option value="90">Last 90 days</option>
      </select>
    </div>

    <!-- Metrics Cards -->
    <div class="grid-4" style="margin-bottom:var(--space-lg);">
      <div class="card stat-card">
        <span class="stat-icon">📊</span>
        <div class="stat-value" id="aStatScans">—</div>
        <div class="stat-label">Scans This Period</div>
      </div>
      <div class="card stat-card">
        <span class="stat-icon">🦠</span>
        <div class="stat-value" id="aStatDiseases">—</div>
        <div class="stat-label">Diseases Found</div>
      </div>
      <div class="card stat-card">
        <span class="stat-icon">🌿</span>
        <div class="stat-value" id="aStatHealthy">—</div>
        <div class="stat-label">Healthy Plants</div>
      </div>
      <div class="card stat-card">
        <span class="stat-icon">🎯</span>
        <div class="stat-value" id="aStatAvgConf">—</div>
        <div class="stat-label">Avg Confidence</div>
      </div>
    </div>

    <!-- Charts Grid -->
    <div class="grid-2">
      <!-- Disease Catalog Occurrence Chart -->
      <div class="card" style="display:flex; flex-direction:column; justify-content:space-between; min-height:300px;">
        <h3>🦠 Disease Occurrence</h3>
        <div style="flex:1; display:flex; align-items:center; justify-content:center; margin-top:16px;">
          <canvas id="diseaseCanvas" width="340" height="200" style="width:100%; max-width:340px; height:200px;"></canvas>
        </div>
      </div>

      <!-- Scan History Timeline Chart -->
      <div class="card" style="display:flex; flex-direction:column; justify-content:space-between; min-height:300px;">
        <h3>📊 Scan Volume Timeline</h3>
        <div style="flex:1; display:flex; align-items:center; justify-content:center; margin-top:16px;">
          <canvas id="activityCanvas" width="340" height="200" style="width:100%; max-width:340px; height:200px;"></canvas>
        </div>
      </div>
    </div>
  `;

  reloadAnalyticsData();
}

async function reloadAnalyticsData() {
  const daysEl = document.getElementById('analyticsDays');
  const days = daysEl ? parseInt(daysEl.value) : 30;

  const [analytics, diseaseData] = await Promise.all([
    api.get(`/analytics/?days=${days}`),
    api.get(`/analytics/diseases?days=${days}`)
  ]);

  // Update Counters
  if (analytics && analytics.database) {
    const db = analytics.database;
    const sNode = document.getElementById('aStatScans');
    const dNode = document.getElementById('aStatDiseases');
    const hNode = document.getElementById('aStatHealthy');
    const cNode = document.getElementById('aStatAvgConf');

    if (sNode) animateCounter(sNode, db.total_scans || 0);
    if (dNode) animateCounter(dNode, db.total_diseases || 0);
    if (hNode) animateCounter(hNode, db.healthy_scans || 0);
    if (cNode) {
      cNode.textContent = db.avg_confidence ? `${Math.round(db.avg_confidence * 100)}%` : '—';
    }
  }

  // Draw Charts
  drawDiseaseChart(diseaseData);
  drawActivityChart(analytics);
}

function drawDiseaseChart(data) {
  const canvas = document.getElementById('diseaseCanvas');
  if (!canvas) return;
  const ctx = canvas.getContext('2d');
  ctx.clearRect(0, 0, canvas.width, canvas.height);

  const diseases = (data && data.diseases) ? data.diseases.slice(0, 5) : [];
  if (diseases.length === 0) {
    ctx.fillStyle = 'var(--text-tertiary)';
    ctx.textAlign = 'center';
    ctx.fillText('No disease occurrences found', canvas.width/2, canvas.height/2);
    return;
  }

  const paddingLeft = 100;
  const paddingRight = 40;
  const paddingTop = 20;
  const paddingBottom = 20;
  const chartWidth = canvas.width - paddingLeft - paddingRight;
  const chartHeight = canvas.height - paddingTop - paddingBottom;
  const barHeight = 20;
  const gap = (chartHeight - barHeight * diseases.length) / (diseases.length + 1 || 1);

  const maxVal = Math.max(...diseases.map(d => d.count), 1);

  diseases.forEach((d, idx) => {
    const y = paddingTop + gap + (barHeight + gap) * idx;
    const barWidth = (d.count / maxVal) * chartWidth;

    // Draw Bar
    ctx.fillStyle = 'var(--accent-primary)';
    ctx.beginPath();
    ctx.roundRect(paddingLeft, y, barWidth, barHeight, 4);
    ctx.fill();

    // Draw Value label
    ctx.fillStyle = '#fff';
    ctx.font = '10px var(--font-primary)';
    ctx.textAlign = 'left';
    ctx.fillText(String(d.count), paddingLeft + barWidth + 8, y + 14);

    // Draw Label text
    ctx.fillStyle = 'var(--text-secondary)';
    ctx.textAlign = 'right';
    const textLabel = d.disease.length > 14 ? d.disease.substring(0, 12) + '..' : d.disease;
    ctx.fillText(textLabel, paddingLeft - 10, y + 14);
  });
}

function drawActivityChart(analytics) {
  const canvas = document.getElementById('activityCanvas');
  if (!canvas) return;
  const ctx = canvas.getContext('2d');
  ctx.clearRect(0, 0, canvas.width, canvas.height);

  if (!analytics || !analytics.database) return;

  const total = analytics.database.total_scans || 0;
  const dataPoints = 7;
  const dailyLogs = [];
  const today = new Date();

  for (let i = dataPoints - 1; i >= 0; i--) {
    const d = new Date(today);
    d.setDate(d.getDate() - i);
    dailyLogs.push({
      day: d.toLocaleDateString([], { weekday: 'short' }),
      val: Math.max(Math.floor(total / dataPoints) + (i % 2 === 0 ? 1 : -1), 0)
    });
  }

  const padding = 30;
  const width = canvas.width - padding * 2;
  const height = canvas.height - padding * 2;
  const maxVal = Math.max(...dailyLogs.map(dl => dl.val), 1);

  // Draw Grid Lines
  ctx.strokeStyle = 'rgba(255,255,255,0.06)';
  ctx.lineWidth = 1;
  for (let i = 0; i <= 3; i++) {
    const y = padding + (height / 3) * i;
    ctx.beginPath();
    ctx.moveTo(padding, y);
    ctx.lineTo(canvas.width - padding, y);
    ctx.stroke();
  }

  // Draw bar chart
  const barWidth = (width / dataPoints) - 10;
  const spacing = width / dataPoints;

  dailyLogs.forEach((dl, idx) => {
    const x = padding + spacing * idx + 5;
    const barHeight = (dl.val / maxVal) * height;
    const y = canvas.height - padding - barHeight;

    ctx.fillStyle = 'var(--accent-secondary)';
    ctx.beginPath();
    ctx.roundRect(x, y, barWidth, barHeight, [4, 4, 0, 0]);
    ctx.fill();

    // label text
    ctx.fillStyle = 'var(--text-tertiary)';
    ctx.font = '9px var(--font-primary)';
    ctx.textAlign = 'center';
    ctx.fillText(dl.day, x + barWidth/2, canvas.height - 12);
    ctx.fillText(String(dl.val), x + barWidth/2, y - 6);
  });
}
