// Speech Timer Dashboard JavaScript
function updateDashboard() {
  // Fetch all dashboard data in one request
  fetch('/api/dashboard')
    .then(r => r.json())
    .then(d => {
      // Update mode
      const modeName = d.mode_name || d.mode || 'Unknown';
      document.getElementById('mode-name').textContent = modeName;
      
      // Update timer status
      const running = d.timer_running;
      document.getElementById('mode-dot').className = 'status-dot ' + (running ? 'status-online' : 'status-offline');
      document.getElementById('mode-status').textContent = running ? 'Timer Running' : 'Ready';
      
      // Update LED status
      const led = (typeof d.led_on === 'boolean') ? d.led_on : (d.led === 'on');
      document.getElementById('led-status').textContent = led ? 'On' : 'Off';
      
      // Update LED button
      const btn = document.getElementById('led-btn');
      btn.className = led ? 'danger' : 'success';
      btn.value = led ? 'OFF' : 'ON';
      btn.innerHTML = (led ? '&#x1F4A1;' : '&#x1F4A1;') + ' LED ' + (led ? 'Off' : 'On');
      
      // Update timer button
      const tbtn = document.getElementById('timer-btn');
      tbtn.className = running ? 'danger' : 'success';
      tbtn.value = running ? 'STOP' : 'START';
      tbtn.innerHTML = (running ? '&#x23F9;' : '&#x25B6;') + ' ' + (running ? 'Stop' : 'Start') + ' Timer';
      
      // Update clock color (optional)
      if (d.clock_color) {
        const c = d.clock_color;
        document.getElementById('color-preview').style.background = 'rgb(' + c.red + ',' + c.green + ',' + c.blue + ')';
        document.getElementById('color-label').textContent = 'RGB(' + c.red + ', ' + c.green + ', ' + c.blue + ')';
        // Update color input fields
        document.getElementById('red-input').value = c.red;
        document.getElementById('green-input').value = c.green;
        document.getElementById('blue-input').value = c.blue;
      }

      // Update temperature
      const tempC = d.system && d.system.cpu ? d.system.cpu.temperature_c : undefined;
      if (tempC !== undefined) {
        document.getElementById('temp-c').innerHTML = tempC.toFixed(1) + '&deg;C';
        document.getElementById('temp-f').innerHTML = (tempC * 1.8 + 32).toFixed(1) + '&deg;F';
      }
      
      // Update WiFi status
      if (d.system && d.system.wifi) {
        document.getElementById('wifi-status').innerHTML = d.system.wifi.connected ? '&#x2705; Connected' : '&#x274C; Disconnected';
        document.getElementById('ip-addr').textContent = d.system.wifi.ip_address || 'Unknown';
      }
      document.getElementById('request-count').textContent = d.request_count || 0;

      // Update time
      const timeValue = d.current_time || '--:--';
      const timeSet = (typeof d.time_set === 'boolean') ? d.time_set : false;
      document.getElementById('time-value').textContent = timeValue;
      document.getElementById('time-label').textContent = timeSet ? 'Time synchronized' : 'Time not set';
    })
    .catch(e => console.error('Dashboard fetch error:', e));
  
  // Update refresh status
  document.getElementById('refresh-status').textContent = 'Last updated: ' + new Date().toLocaleTimeString();
}

// Auto-update every 5 seconds
setInterval(updateDashboard, 5000);

// Initial update
updateDashboard();

// Submit forms only on explicit button clicks
function wireFormButtons() {
  const buttons = document.querySelectorAll('button[data-submit="true"]');
  buttons.forEach((btn) => {
    btn.addEventListener('click', () => {
      if (btn.form) {
        btn.form.submit();
      }
    });
  });
}

document.addEventListener('DOMContentLoaded', wireFormButtons);

// Log resource sizes to the browser console for diagnostics
window.addEventListener('load', () => {
  const navEntries = performance.getEntriesByType('navigation');
  if (navEntries.length > 0) {
    const nav = navEntries[0];
    console.log('Page load size (bytes):', nav.transferSize || nav.decodedBodySize || 0);
  }
  const resources = performance.getEntriesByType('resource');
  resources.forEach((r) => {
    if (r.name.indexOf('/css/dashboard.css') !== -1 || r.name.indexOf('/js/dashboard.js') !== -1) {
      console.log('Resource loaded:', r.name, 'bytes:', r.transferSize || r.decodedBodySize || 0);
    }
  });
});
