// Speech Timer Dashboard JavaScript

// Helper function to safely update an input value without disturbing user input
function safeUpdateInput(elementId, value) {
  const element = document.getElementById(elementId);
  if (element && document.activeElement !== element) {
    // Only update if the user is not currently editing this field
    element.value = value;
  }
}

function updateDashboard() {
  // Fetch all dashboard data in one request
  fetch('/api/dashboard')
    .then(r => r.json())
    .then(d => {
      console.log('Dashboard data received:', d);
      
      // Update mode
      const modeName = d.mode_name || d.mode || 'Unknown';
      document.getElementById('mode-name').textContent = modeName;
      
      // Update timer status
      const running = d.timer_running;
      document.getElementById('mode-dot').className = 'status-dot ' + (running ? 'status-online' : 'status-offline');
      document.getElementById('mode-status').textContent = running ? 'Timer Running' : 'Ready';
      
      // Update timer button
      const tbtn = document.getElementById('timer-btn');
      tbtn.className = running ? 'danger' : 'success';
      tbtn.value = running ? 'STOP' : 'START';
      tbtn.innerHTML = (running ? '&#x23F9;' : '&#x25B6;') + ' ' + (running ? 'Stop' : 'Start') + ' Timer';
      
      // Update clock color (optional) - only update preview and label, not input fields
      if (d.clock_color) {
        const c = d.clock_color;
        document.getElementById('color-preview').style.background = 'rgb(' + c.red + ',' + c.green + ',' + c.blue + ')';
        document.getElementById('color-label').textContent = 'RGB(' + c.red + ', ' + c.green + ', ' + c.blue + ')';
        // Do NOT update red-input, green-input, blue-input - let user retain their input values
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
  console.log('wireFormButtons - Found ' + buttons.length + ' buttons to wire');
  
  buttons.forEach((btn) => {
    const btnName = btn.getAttribute('name');
    const btnId = btn.getAttribute('id') || '(no id)';
    console.log('wireFormButtons - Wiring button:', btnId, 'name:', btnName);
    
    btn.addEventListener('click', (e) => {
      e.preventDefault();
      
      if (!btn.form) {
        console.error('Button click: No form found for button', btnName, btnId);
        return;
      }
      
      // Serialize all form elements into URLSearchParams
      const formData = new FormData(btn.form);
      
      // Add the button's name and value to the form data
      const btnValue = btn.getAttribute('value');
      formData.set(btnName, btnValue);
      
      // Convert FormData to URL-encoded format (application/x-www-form-urlencoded)
      const params = new URLSearchParams(formData);
      
      console.log('Button click: ' + btnName + ' = ' + btnValue + ', Submitting form with data:', params.toString());
      
      // Submit using fetch to ensure proper encoding
      fetch(btn.form.action, {
        method: btn.form.method,
        headers: {
          'Content-Type': 'application/x-www-form-urlencoded'
        },
        body: params.toString()
      })
      .then(response => {
        console.log(btnName + ' - Form submitted successfully, status:', response.status);
        // After successful submission, refresh the dashboard
        updateDashboard();
      })
      .catch(error => console.error(btnName + ' - Form submission error:', error));
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
