const hFill = document.getElementById('hFill'), hNum = document.getElementById('hNum');
const tFill = document.getElementById('tFill'), tNum = document.getElementById('tNum');
const sFill = document.getElementById('sFill'), sNum = document.getElementById('sNum');
const root = document.getElementById('root');
let _visState = false;
let _preview = false;

setInterval(() => window.requestUpdate(''), 1000);

window.updateStats = json => {
  try {
    const d = JSON.parse(json);
    const hunger = d.hunger || 0, thirst = d.thirst || 0, sleep = d.sleep || 0;
    const active = d.active === true;
    hFill.style.width = `${Math.min(100, hunger)}%`; hNum.textContent = Math.round(hunger);
    tFill.style.width = `${Math.min(100, thirst)}%`; tNum.textContent = Math.round(thirst);
    sFill.style.width = `${Math.min(100, sleep)}%`; sNum.textContent = Math.round(sleep);
    if (active !== _visState) {
      _visState = active;
      window.reportVisibility(JSON.stringify({id: 'prisma_survival', visible: active}));
    }
    root.classList.toggle('visible', _visState || _preview);
  } catch {}
};

window.setEditorPreview = v => {
  _preview = v === '1' || v === 1 || v === true;
  if (_preview) { hFill.style.width = '60%'; hNum.textContent = '60'; tFill.style.width = '45%'; tNum.textContent = '45'; sFill.style.width = '72%'; sNum.textContent = '72'; }
  root.classList.toggle('visible', _visState || _preview);
};

const DEF = {xf: 310 / 1920, yf: 980 / 1080, wf: 220 / 1920, hf: 58 / 1080};
const WIDGET_ID = 'prisma_survival';
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
