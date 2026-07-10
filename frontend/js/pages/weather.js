/* ============================================================
   AgroMate Weather Advisor Page — V2 [NEW]
   Live weather reports, 5-day forecasts, irrigation schedules
   and canvas agricultural advisory charts.
   ============================================================ */

async function renderWeather(container) {
  container.innerHTML = `
    <div class="page-header" style="display:flex; justify-content:space-between; align-items:center; flex-wrap:wrap; gap:var(--space-md);">
      <div>
        <h1 data-i18n="nav_weather">🌦️ Weather Advisor</h1>
        <p style="color:var(--text-tertiary);">Live agricultural forecasts and irrigation advisories</p>
      </div>
      
      <div style="display:flex; gap:8px; align-items:center; background:var(--bg-card); padding:6px 12px; border-radius:var(--radius-xl); border:var(--glass-border);">
        <input type="text" class="input" id="weatherSearch" placeholder="Search City..." style="border:none; background:transparent; max-width:180px; height:32px; padding:0 8px; font-size:0.85rem;" onkeydown="handleWeatherInputKey(event)">
        <button class="btn btn-primary" onclick="searchWeatherCity()" style="width:36px; height:36px; min-height:36px; padding:0; border-radius:50%;">🔍</button>
        <button class="btn btn-secondary" onclick="requestWeatherGPS()" style="width:36px; height:36px; min-height:36px; padding:0; border-radius:50%;" title="GPS Locate">🌐</button>
      </div>
    </div>

    <!-- Active City Banner -->
    <div class="card card-accent" style="margin-bottom:var(--space-lg); text-align:left; display:flex; align-items:center; justify-content:space-between; flex-wrap:wrap; gap:var(--space-md);">
      <div style="display:flex; align-items:center; gap:var(--space-lg);">
        <span style="font-size:3.5rem;" id="weatherHeaderIcon">🌤️</span>
        <div>
          <h2 id="weatherHeaderCity" style="font-size:1.6rem; color:var(--text-primary);">New Delhi</h2>
          <p id="weatherHeaderDesc" style="text-transform:capitalize; color:var(--accent-secondary); font-weight:600;">Clear Sky</p>
        </div>
      </div>
      
      <div style="display:flex; gap:var(--space-xl); text-align:right;">
        <div>
          <span style="font-size:0.8rem; color:var(--text-tertiary); display:block;">TEMPERATURE</span>
          <h2 style="font-size:2.2rem; font-weight:800; line-height:1;" id="weatherHeaderTemp">25°C</h2>
        </div>
        <div>
          <span style="font-size:0.8rem; color:var(--text-tertiary); display:block;">RAIN FORECAST</span>
          <h2 style="font-size:2.2rem; font-weight:800; line-height:1;" id="weatherHeaderRain">0%</h2>
        </div>
      </div>
    </div>

    <div class="grid-3" style="margin-bottom:var(--space-lg);">
      <!-- Left Column: Irrigation Advisor -->
      <div class="card" style="display:flex; flex-direction:column; justify-content:space-between;">
        <div>
          <h3>💧 Irrigation Advisor</h3>
          <p style="font-size:0.8rem; color:var(--text-tertiary); margin-top:2px;">Water schedule recommendations based on weather</p>
          
          <div id="weatherIrrigationContent" style="margin-top:var(--space-lg); text-align:left; line-height:1.6;">
            <!-- Rendered in JS -->
          </div>
        </div>
      </div>

      <!-- Middle: Weather metrics grid -->
      <div class="card">
        <h3>📊 Atmosphere Metrics</h3>
        <p style="font-size:0.8rem; color:var(--text-tertiary); margin-top:2px;">Humidity, wind speed, pressure, and solar UV</p>
        
        <div class="weather-detail-grid" style="grid-template-columns: repeat(2, 1fr); margin-top:var(--space-md);">
          <div class="weather-detail-item">
            <span class="detail-icon">💧</span>
            <div class="detail-value" id="weatherValHumidity">—</div>
            <div class="detail-label">Humidity</div>
          </div>
          <div class="weather-detail-item">
            <span class="detail-icon">💨</span>
            <div class="detail-value" id="weatherValWind">—</div>
            <div class="detail-label">Wind Speed</div>
          </div>
          <div class="weather-detail-item">
            <span class="detail-icon">☀️</span>
            <div class="detail-value" id="weatherValUV">—</div>
            <div class="detail-label">Solar UV</div>
          </div>
          <div class="weather-detail-item">
            <span class="detail-icon">🎈</span>
            <div class="detail-value" id="weatherValPressure">—</div>
            <div class="detail-label">Pressure</div>
          </div>
        </div>
      </div>

      <!-- Right Column: Temperature trend canvas graph -->
      <div class="card" style="display:flex; flex-direction:column; justify-content:space-between;">
        <div>
          <h3>📈 5-Day Temp Trend</h3>
          <p style="font-size:0.8rem; color:var(--text-tertiary); margin-top:2px;">Hourly/daily temperature variation</p>
        </div>
        <div style="flex:1; display:flex; align-items:center; justify-content:center; margin-top:16px;">
          <canvas id="weatherTrendCanvas" width="260" height="140" style="width:100%; max-width:260px; height:140px;"></canvas>
        </div>
      </div>
    </div>

    <!-- 5-Day Forecast Grid -->
    <div class="card" style="text-align:left;">
      <h3 style="margin-bottom:var(--space-md);">📅 5-Day Agricultural Forecast</h3>
      <div id="weatherForecastGrid" style="display:flex; gap:var(--space-md); overflow-x:auto; padding-bottom:8px;">
        <!-- Filled in JS -->
      </div>
    </div>
  `;

  loadFullWeatherData();
}

async function loadFullWeatherData() {
  const lat = localStorage.getItem('agromate-latitude') || '28.6139';
  const lon = localStorage.getItem('agromate-longitude') || '77.2090';
  const city = localStorage.getItem('agromate-selected-city') || 'New Delhi';

  document.getElementById('weatherHeaderCity').textContent = city;

  try {
    const data = await api.get(`/analytics/weather?lat=${lat}&lon=${lon}&city=${encodeURIComponent(city)}`);
    if (data && data.weather) {
      const w = data.weather;
      const forecast = data.forecast || [];
      const temp = Math.round(w.temperature || 25);
      const desc = w.description || 'clear sky';
      
      // Update Banner
      document.getElementById('weatherHeaderTemp').textContent = `${temp}°C`;
      document.getElementById('weatherHeaderIcon').textContent = getWeatherIcon(desc);
      document.getElementById('weatherHeaderDesc').textContent = desc;

      // rain check
      const rainChance = forecast.length > 0 ? (forecast[0].rain_chance || 0) : 0;
      document.getElementById('weatherHeaderRain').textContent = `${rainChance}%`;

      // Update metrics
      document.getElementById('weatherValHumidity').textContent = `${w.humidity || 60}%`;
      document.getElementById('weatherValWind').textContent = `${Math.round(w.wind_speed || 10)} km/h`;
      document.getElementById('weatherValUV').textContent = w.uv_index || 'Low';
      document.getElementById('weatherValPressure').textContent = `${w.pressure || 1012} hPa`;

      // Render Irrigation schedule
      renderIrrigationSchedule(temp, w.humidity, rainChance);

      // Render 5-Day Grid
      renderForecastGrid(forecast);

      // Draw Temp Trend Chart
      drawTempChart(forecast);
      
    } else {
      showToast("Weather feed error", "error");
    }
  } catch (err) {
    showToast("Failed to fetch weather forecast", "error");
  }
}

function renderIrrigationSchedule(temp, humidity, rainChance) {
  const el = document.getElementById('weatherIrrigationContent');
  if (!el) return;

  let advice = "Watering schedule: Standard watering (once in 2 days). Check soil moisture before watering.";
  let color = "var(--color-info)";
  let badgeText = "NORMAL WATERING";

  if (rainChance > 50) {
    advice = "⚠️ Rain forecasted soon (chance " + rainChance + "%). Please PAUSE manual irrigation today to prevent soil waterlogging and root rot.";
    color = "var(--color-warning)";
    badgeText = "SUSPEND WATERING";
  } else if (temp > 32) {
    advice = "🔥 Hot dry weather detected (" + temp + "°C). Plants will experience higher evapotranspiration. Water twice daily (early morning & late evening). Check mulch.";
    color = "var(--color-danger)";
    badgeText = "HIGH WATERING";
  } else if (humidity < 40) {
    advice = "🍃 Low atmospheric humidity (" + humidity + "%). Soil moisture evaporates quickly. Light supplemental spray is recommended for leafy crops.";
    color = "var(--color-warning)";
    badgeText = "MODERATE WATERING";
  }

  el.innerHTML = `
    <div style="margin-bottom:12px;">
      <span class="badge" style="background:${color}; color:#fff; font-weight:700;">${badgeText}</span>
    </div>
    <div style="font-size:0.9rem; line-height:1.5; color:var(--text-secondary);">${advice}</div>
    
    <div style="margin-top:16px; font-size:0.75rem; border-top:1px solid var(--border-subtle); padding-top:10px; color:var(--text-tertiary);">
      💡 <b>Crop Advice:</b> Evapotranspiration is active. Keep soil mulched to save 30% moisture.
    </div>
  `;
}

function renderForecastGrid(forecast) {
  const el = document.getElementById('weatherForecastGrid');
  if (!el) return;

  if (forecast.length === 0) {
    el.innerHTML = '<div style="color:var(--text-tertiary);">No forecast entries available</div>';
    return;
  }

  el.innerHTML = forecast.map(f => {
    let day = 'Date';
    try {
      day = new Date(f.datetime).toLocaleDateString([], { weekday: 'short' });
    } catch(e){}
    const icon = getWeatherIcon(f.description || '');
    return `
      <div class="weather-detail-item" style="flex:1; min-width:110px; display:flex; flex-direction:column; justify-content:space-between; height:120px;">
        <span style="font-size:0.8rem; font-weight:600; color:var(--text-secondary);">${day}</span>
        <span style="font-size:1.8rem; margin:4px 0;">${icon}</span>
        <span style="font-size:1.1rem; font-weight:700;">${Math.round(f.temperature)}°C</span>
        <span style="font-size:0.7rem; color:var(--text-tertiary); overflow:hidden; text-overflow:ellipsis; white-space:nowrap;" title="${f.description || ''}">
          ${f.description || 'Clear'}
        </span>
      </div>
    `;
  }).join('');
}

function drawTempChart(forecast) {
  const canvas = document.getElementById('weatherTrendCanvas');
  if (!canvas) return;
  const ctx = canvas.getContext('2d');
  
  // Clear canvas
  ctx.clearRect(0, 0, canvas.width, canvas.height);
  
  if (forecast.length === 0) return;

  const padding = 20;
  const width = canvas.width - padding * 2;
  const height = canvas.height - padding * 2;

  // Find min/max temperature values
  const temps = forecast.map(f => f.temperature);
  const maxTemp = Math.max(...temps) + 2;
  const minTemp = Math.min(...temps) - 2;
  const tempRange = maxTemp - minTemp || 1;

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

  // Draw chart line
  ctx.strokeStyle = 'var(--accent-primary)';
  ctx.lineWidth = 3;
  ctx.beginPath();

  const points = forecast.map((f, i) => {
    const x = padding + (width / (forecast.length - 1)) * i;
    const y = canvas.height - padding - ((f.temperature - minTemp) / tempRange) * height;
    return { x, y, temp: Math.round(f.temperature) };
  });

  points.forEach((p, i) => {
    if (i === 0) {
      ctx.moveTo(p.x, p.y);
    } else {
      ctx.lineTo(p.x, p.y);
    }
  });
  ctx.stroke();

  // Draw gradient area below chart line
  const grad = ctx.createLinearGradient(0, padding, 0, canvas.height - padding);
  grad.addColorStop(0, 'rgba(34,197,94,0.25)');
  grad.addColorStop(1, 'rgba(34,197,94,0.01)');
  ctx.fillStyle = grad;
  ctx.beginPath();
  ctx.moveTo(points[0].x, canvas.height - padding);
  points.forEach(p => ctx.lineTo(p.x, p.y));
  ctx.lineTo(points[points.length - 1].x, canvas.height - padding);
  ctx.closePath();
  ctx.fill();

  // Draw dots and text
  ctx.fillStyle = '#fff';
  ctx.font = '9px var(--font-primary)';
  ctx.textAlign = 'center';
  points.forEach((p, i) => {
    ctx.beginPath();
    ctx.arc(p.x, p.y, 4, 0, Math.PI * 2);
    ctx.fillStyle = 'var(--accent-secondary)';
    ctx.fill();
    ctx.fillStyle = '#fff';
    ctx.fillText(`${p.temp}°`, p.x, p.y - 8);
  });
}

function handleWeatherInputKey(e) {
  if (e.key === 'Enter') {
    e.preventDefault();
    searchWeatherCity();
  }
}

async function searchWeatherCity() {
  const val = document.getElementById('weatherSearch').value.trim();
  if (val.length < 2) return;

  try {
    const res = await fetch(`https://geocoding-api.open-meteo.com/v1/search?name=${encodeURIComponent(val)}&count=1&language=en&format=json`);
    if (res.ok) {
      const data = await res.json();
      if (data.results && data.results.length > 0) {
        const item = data.results[0];
        const dispName = `${item.name}, ${item.admin1 || ''} (${item.country})`;
        localStorage.setItem('agromate-latitude', item.latitude);
        localStorage.setItem('agromate-longitude', item.longitude);
        localStorage.setItem('agromate-selected-city', dispName);
        
        loadFullWeatherData();
        showToast(`Weather location updated: ${item.name}`, 'success');
        document.getElementById('weatherSearch').value = '';
      } else {
        showToast("City not found", "warning");
      }
    }
  } catch {
    showToast("Geocoding failed", "error");
  }
}

function requestWeatherGPS() {
  if (navigator.geolocation) {
    navigator.geolocation.getCurrentPosition(
      (position) => {
        localStorage.setItem('agromate-latitude', position.coords.latitude);
        localStorage.setItem('agromate-longitude', position.coords.longitude);
        localStorage.removeItem('agromate-selected-city');
        loadFullWeatherData();
        showToast("Location updated via GPS", "success");
      },
      () => {
        showToast("GPS denied, using IP fallback", "warning");
      }
    );
  }
}
