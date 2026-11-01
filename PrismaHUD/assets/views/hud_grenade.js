const countEl = document.getElementById('count');
const lblEl = document.getElementById('lbl');
const root = document.getElementById('root');
let _visState = false;
let _preview = false;

setInterval(() => window.requestUpdate(''), 500);

window.updateStats = json => {
  try {
    const d = JSON.parse(json);
    const shouldShow = d.hasGrenade === true;
    if (!_preview) {
      countEl.textContent = d.count != null ? d.count : 0;
      lblEl.textContent = (d.name || 'GRENADE').toUpperCase().substring(0, 12);
      countEl.classList.toggle('low', d.count != null && d.count <= 2 && d.count > 0);
    }
    if (shouldShow !== _visState) {
      _visState = shouldShow;
      window.reportVisibility(JSON.stringify({id: 'prisma_grenade', visible: shouldShow}));
    }
    root.classList.toggle('visible', _visState || _preview);
  } catch {}
};

window.setEditorPreview = v => {
  _preview = v === '1' || v === 1 || v === true;
  if (_preview) { countEl.textContent = '3'; countEl.classList.remove('low'); lblEl.textContent = 'FRAG GRENADE'; }
  root.classList.toggle('visible', _visState || _preview);
};

const DEF = {xf: 1816 / 1920, yf: 870 / 1080, wf: 96 / 1920, hf: 52 / 1080};
const WIDGET_ID = 'prisma_grenade';
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
let _posSet = false;
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
