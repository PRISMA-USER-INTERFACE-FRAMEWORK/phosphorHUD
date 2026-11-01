const actionText = document.getElementById('actionText');
const keyBadge = document.getElementById('keyBadge');
const targetName = document.getElementById('targetName');
const extras = document.getElementById('extras');
const root = document.getElementById('root');
let _visState = false;
let _preview = false;

function escH(s) { return String(s || '').replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;'); }

let _ps = false;
function renderExtras(list) {
  if (!list || !list.length) { extras.innerHTML = ''; return; }
  extras.innerHTML = list.map(e => {
    const cap = e.key ? CG.renderKey(e.key, _ps) : '';
    return `<div class="extra-hint">${cap}${escH(e.label)}</div>`;
  }).join('');
}

setInterval(() => window.requestUpdate(''), 100);

window.updatePrompt = json => {
  try {
    const d = JSON.parse(json);
    const shouldShow = d.hasTarget === true && (d.text || '').length > 0;
    if (!_preview) {
      _ps = d.controllerStyle === 'playstation';
      if (d.activateKey) keyBadge.innerHTML = CG.renderKey(d.activateKey, _ps);
      actionText.textContent = d.text || '';
      targetName.textContent = d.subtext || '';
      renderExtras(d.extras);
    }
    if (shouldShow !== _visState) {
      _visState = shouldShow;
      window.reportVisibility(JSON.stringify({id: 'prisma_prompt', visible: shouldShow}));
    }
    root.classList.toggle('visible', _visState || _preview);
  } catch {}
};

window.setEditorPreview = v => {
  _preview = v === '1' || v === 1 || v === true;
  if (_preview) {
    actionText.textContent = 'Activate'; targetName.textContent = 'TERMINAL';
    renderExtras([{key: 'R', label: 'Transfer'}]);
  }
  root.classList.toggle('visible', _visState || _preview);
};

const DEF = {xf: 760 / 1920, yf: 570 / 1080, wf: 400 / 1920, hf: 72 / 1080};
const WIDGET_ID = 'prisma_prompt';
let _posSet = false;

let _want = null, _applied = false;
function flushPos() {
  if (!_want || _applied) return;
  const W = window.innerWidth, H = window.innerHeight;
  if (!(W > 1) || !(H > 1)) return;
  const r = document.getElementById('root');
  r.style.left = `${_want.xf * W}px`; r.style.top = `${_want.yf * H}px`;
  r.style.setProperty('--s', _want.s); r.style.setProperty('--r', `${_want.r || 0}deg`);
  _applied = true;
  window.reportPosition(JSON.stringify({id: WIDGET_ID, x: _want.xf, y: _want.yf, s: _want.s, r: _want.r || 0}));
}
function applyPos(xf, yf, s, rot) {
  _want = {xf, yf, s, r: rot || 0};
  _applied = false;
  flushPos();
}
try { new ResizeObserver(() => flushPos()).observe(document.documentElement); } catch {}

window.setHUDPosition = json => {
  _posSet = true;
  try {
    const p = JSON.parse(json);
    applyPos(p.x, p.y, typeof p.s === 'number' ? p.s : 1, typeof p.r === 'number' ? p.r : 0);
  } catch {}
};
window.requestUpdate = window.requestUpdate || (() => {});
window.reportPosition = window.reportPosition || (() => {});
window.reportVisibility = window.reportVisibility || (() => {});
setTimeout(() => { if (!_posSet) applyPos(DEF.xf, DEF.yf, 1, 0); }, 50);
