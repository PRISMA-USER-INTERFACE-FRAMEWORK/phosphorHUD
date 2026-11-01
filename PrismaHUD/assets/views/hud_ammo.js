const loaded = document.getElementById('loaded');
const reserve = document.getElementById('reserve');
const ammoName = document.getElementById('ammoName');
const root = document.getElementById('root');
let _visState = false;

setInterval(() => window.requestUpdate(''), 250);

window.updateStats = json => {
  try {
    const d = JSON.parse(json);
    const shouldShow = d.hasWeapon === true;
    loaded.textContent = d.loaded != null ? d.loaded : '--';
    reserve.textContent = d.reserve != null ? d.reserve : '--';
    ammoName.textContent = d.name || 'NO AMMO';
    loaded.classList.toggle('low', d.loaded != null && d.loaded <= 5 && d.loaded > 0);
    if (shouldShow !== _visState) {
      _visState = shouldShow;
      root.classList.toggle('visible', shouldShow);
      window.reportVisibility(JSON.stringify({id: 'prisma_ammo', visible: shouldShow}));
    }
  } catch {}
};

const DEF = {xf: 1720 / 1920, yf: 940 / 1080, wf: 160 / 1920, hf: 60 / 1080};
const WIDGET_ID = 'prisma_ammo';
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
