/* ============================================================
   AgroMate Scan History Page — V2
   Paginated tabular reports of historical diagnoses with exports.
   ============================================================ */

let historyPage = 0;
const historyPageSize = 12;

async function renderHistory(container) {
  container.innerHTML = `
    <div class="page-header" style="display:flex; align-items:center; justify-content:space-between; flex-wrap:wrap; gap:var(--space-md);">
      <div>
        <h1 data-i18n="nav_history">📋 Scan History</h1>
        <p style="color:var(--text-tertiary);">Browse diagnosis logs, treatment recommendations, and export reports</p>
      </div>
      
      <div style="display:flex; gap:8px; align-items:center;">
        <select class="input" style="width:150px; height:38px; padding:0 10px;" id="historyFilter" onchange="loadHistoryLogs()">
          <option value="">All Scans</option>
          <option value="identify">Identification</option>
          <option value="diagnose">Disease Check</option>
          <option value="pest">Pests Check</option>
        </select>
        
        <button class="btn btn-secondary btn-sm" onclick="exportHistoryToCSV()">
          📥 Export CSV
        </button>
      </div>
    </div>

    <!-- Table Card -->
    <div class="card" style="padding:0; overflow:hidden;">
      <div style="overflow-x:auto;">
        <table style="width:100%; border-collapse:collapse; text-align:left; font-size:0.9rem;" id="historyTable">
          <thead>
            <tr style="border-bottom:2px solid var(--border-subtle); color:var(--text-secondary); background:rgba(255,255,255,0.02);">
              <th style="padding:14px var(--space-md);">ID</th>
              <th style="padding:14px var(--space-md);">Type</th>
              <th style="padding:14px var(--space-md);">Crop</th>
              <th style="padding:14px var(--space-md);">Condition</th>
              <th style="padding:14px var(--space-md);">Confidence</th>
              <th style="padding:14px var(--space-md);">Date</th>
            </tr>
          </thead>
          <tbody id="historyBody">
            <tr>
              <td colspan="6" style="text-align:center; padding:var(--space-2xl);">
                <div class="spinner" style="margin:0 auto;"></div>
              </td>
            </tr>
          </tbody>
        </table>
      </div>
    </div>

    <!-- Pagination Controller -->
    <div class="pagination" id="historyPagination" style="display:flex; justify-content:center; gap:8px; margin-top:var(--space-lg);"></div>
  `;

  loadHistoryLogs();
}

async function loadHistoryLogs() {
  const body = document.getElementById('historyBody');
  const pagination = document.getElementById('historyPagination');
  if (!body) return;

  const filter = document.getElementById('historyFilter');
  const scanType = filter ? filter.value : '';
  const offset = historyPage * historyPageSize;

  let url = `/scans/?limit=${historyPageSize}&offset=${offset}`;
  if (scanType) url += `&scan_type=${scanType}`;

  const data = await api.get(url);

  if (data && data.scans && data.scans.length > 0) {
    body.innerHTML = data.scans.map(scan => {
      const diag = scan.diagnoses && scan.diagnoses[0];
      const isHealthy = diag ? diag.is_healthy : true;
      const confidence = diag ? Math.round(diag.confidence * 100) : 0;
      const cropName = diag ? diag.plant_name : 'Unknown Plant';
      const condName = diag ? (isHealthy ? 'Healthy' : diag.disease_name) : 'Pending';
      const badgeStyle = isHealthy ? 'badge-success' : 'badge-danger';
      const date = formatDate(scan.created_at);

      return `
        <tr style="border-bottom:1px solid var(--border-subtle); hover:background:rgba(255,255,255,0.01);">
          <td style="padding:12px var(--space-md); font-family:var(--font-mono); color:var(--text-tertiary);">#${scan.id}</td>
          <td style="padding:12px var(--space-md);"><span class="badge badge-info">${scan.scan_type}</span></td>
          <td style="padding:12px var(--space-md); font-weight:600;">${cropName}</td>
          <td style="padding:12px var(--space-md);"><span class="badge ${badgeStyle}">${condName}</span></td>
          <td style="padding:12px var(--space-md);">${confidence}%</td>
          <td style="padding:12px var(--space-md); color:var(--text-tertiary); font-size:0.8rem;">${date}</td>
        </tr>
      `;
    }).join('');

    // Render Pagination
    const totalPages = Math.ceil((data.total || 0) / historyPageSize);
    if (pagination && totalPages > 1) {
      pagination.innerHTML = '';
      if (historyPage > 0) {
        pagination.innerHTML += `<button class="btn btn-secondary btn-sm" onclick="setHistoryPage(${historyPage - 1})">Prev</button>`;
      }
      for (let i = 0; i < totalPages; i++) {
        pagination.innerHTML += `
          <button class="btn ${i === historyPage ? 'btn-primary' : 'btn-secondary'} btn-sm" onclick="setHistoryPage(${i})">${i + 1}</button>
        `;
      }
      if (historyPage < totalPages - 1) {
        pagination.innerHTML += `<button class="btn btn-secondary btn-sm" onclick="setHistoryPage(${historyPage + 1})">Next</button>`;
      }
    } else if (pagination) {
      pagination.innerHTML = '';
    }
  } else {
    body.innerHTML = `
      <tr>
        <td colspan="6" style="padding:var(--space-2xl) 0;">
          <div class="empty-state">
            <span class="empty-state-icon">📋</span>
            <h3>No scan logs found</h3>
            <p style="font-size:0.8rem;">Ready to execute diagnostics? Head to the scanner module.</p>
            <button class="btn btn-primary" onclick="navigateTo('scan')" style="margin-top:var(--space-md);">Start Scanner</button>
          </div>
        </td>
      </tr>
    `;
    if (pagination) pagination.innerHTML = '';
  }
}

function setHistoryPage(page) {
  historyPage = page;
  loadHistoryLogs();
}

async function exportHistoryToCSV() {
  const data = await api.get('/scans/?limit=1000');
  if (!data || !data.scans || data.scans.length === 0) {
    showToast("No data to export", "warning");
    return;
  }

  const rows = [['Scan ID', 'Type', 'Plant Name', 'Condition', 'Confidence', 'Severity', 'Created At']];
  
  data.scans.forEach(scan => {
    const d = scan.diagnoses && scan.diagnoses[0];
    rows.push([
      scan.id,
      scan.scan_type || '',
      d ? d.plant_name || '' : '',
      d ? d.disease_name || '' : '',
      d ? `${Math.round(d.confidence * 100)}%` : '',
      d ? d.severity || '' : '',
      scan.created_at || ''
    ]);
  });

  const content = rows.map(r => r.map(cell => `"${String(cell).replace(/"/g, '""')}"`).join(',')).join('\n');
  const blob = new Blob([content], { type: 'text/csv' });
  const url = URL.createObjectURL(blob);
  
  const link = document.createElement('a');
  link.href = url;
  link.download = `agromate_history_${new Date().toISOString().split('T')[0]}.csv`;
  link.click();
  
  URL.revokeObjectURL(url);
  showToast("CSV file downloaded successfully", "success");
}
