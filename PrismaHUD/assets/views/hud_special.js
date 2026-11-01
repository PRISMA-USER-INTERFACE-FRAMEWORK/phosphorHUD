const root = document.getElementById('root');
const spName = document.getElementById('spName');
const spRows = document.getElementById('spRows');

setInterval(() => window.requestUpdate(''), 200);

function escH(s) { return String(s || '').replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;'); }

window.updateSpecial = json => {
  try {
    const d = JSON.parse(json);
    if (d.visible !== true) { root.classList.remove('visible'); return; }
    spName.textContent = d.name || '(unnamed)';
    spRows.innerHTML = (d.stats || []).map(s => `
      <div class="sp-row">
        <span class="sp-stat">${escH(s.label || s.key)}</span>
        <span class="sp-val">${s.value}</span>
        <span class="sp-desc">${escH(s.description)}</span>
      </div>`).join('');
    root.classList.add('visible');
  } catch {}
};

window.requestUpdate = window.requestUpdate || (() => {});
