const LABELS = {0: 'N', 45: 'NE', 90: 'E', 135: 'SE', 180: 'S', 225: 'SW', 270: 'W', 315: 'NW'};
const PX_PER_DEG = 3.4;
const D_MIN = -180, D_MAX = 540;
const EASE = 'transform 0.08s linear', EASE_BLIP = 'left 0.08s linear';
const strip = document.getElementById('strip');
const questBlip = document.getElementById('questBlip');
const root = document.getElementById('root');

(() => {
  strip.style.width = `${(D_MAX - D_MIN) * PX_PER_DEG}px`;
  let html = '';
  for (let d = D_MIN; d <= D_MAX; d += 5) {
    const x = (d - D_MIN) * PX_PER_DEG;
    const norm = ((d % 360) + 360) % 360;
    const label = LABELS[norm];
    const major = norm % 45 === 0;
    html += `<div class="t ${major ? 'major' : 'minor'}" style="left:${x}px"></div>`;
    if (label) html += `<div class="lbl${norm % 90 === 0 ? ' card' : ''}" style="left:${x}px">${label}</div>`;
  }
  strip.innerHTML = html;
})();

let _disp = 0, _questDir = -1, _init = false;
let _markers = [];
const markersEl = document.getElementById('markers');
let _blipPool = [];
function ensurePool(n) {
  while (_blipPool.length < n) {
    const el = document.createElement('div');
    el.className = 'blip loc';
    el.innerHTML = '<svg viewBox="0 0 20 24" preserveAspectRatio="none">'
      + '<polygon points="10,3 16,9 16,15 10,21 4,15 4,9" fill="rgba(190,225,235,0.10)"'
      + ' stroke="rgba(190,225,235,0.85)" stroke-width="2" stroke-linejoin="round"/></svg>';
    markersEl.appendChild(el);
    _blipPool.push(el);
  }
  for (let i = n; i < _blipPool.length; ++i) _blipPool[i].style.display = 'none';
}

function place(h, animate) {
  const w = root.offsetWidth || 420, center = w / 2;
  const off = center - (h + 180) * PX_PER_DEG;
  strip.style.transition = animate ? EASE : 'none';
  strip.style.transform = `translate3d(${off}px,0,0)`;

  ensurePool(_markers.length);
  for (let i = 0; i < _markers.length; ++i) {
    const m = _markers[i], el = _blipPool[i];
    let mrel = ((m.dir - h) % 360 + 360) % 360; if (mrel > 180) mrel -= 360;
    const mpx = center + mrel * PX_PER_DEG;
    if (mpx > 2 && mpx < w - 2) {
      el.style.transition = animate ? EASE_BLIP : 'none';
      el.style.left = `${mpx}px`;
      const t = Math.max(0, Math.min(1, (m.dist || 0) / 60000));
      el.style.opacity = `${(1 - t * 0.75).toFixed(2)}`;
      el.style.display = 'block';
    } else el.style.display = 'none';
  }

  if (typeof _questDir === 'number' && _questDir >= 0) {
    let rel = ((_questDir - h) % 360 + 360) % 360; if (rel > 180) rel -= 360;
    const px = center + rel * PX_PER_DEG;
    if (px > 2 && px < w - 2) {
      questBlip.style.transition = animate ? EASE_BLIP : 'none';
      questBlip.style.left = `${px}px`;
      questBlip.style.display = 'block';
    } else questBlip.style.display = 'none';
  } else questBlip.style.display = 'none';
}

setInterval(() => window.requestUpdate(''), 50);

window.updateHeading = json => {
  let h = 0;
  try {
    const d = JSON.parse(json);
    h = d.heading || 0; _questDir = typeof d.questDir === 'number' ? d.questDir : -1;
    if (Array.isArray(d.markers)) _markers = d.markers;
  } catch { return; }

  if (!_init) { _init = true; _disp = h; place(_disp, false); return; }

  if (_disp < -90 || _disp > 450) {
    _disp = ((_disp % 360) + 360) % 360;
    place(_disp, false);
    void strip.offsetWidth;
  }

  const delta = ((h - _disp + 540) % 360) - 180;
  _disp += delta;
  place(_disp, true);
};

const DEF = {xf: 750 / 1920, yf: 14 / 1080, wf: 420 / 1920, hf: 24 / 1080};
const WIDGET_ID = 'prisma_compass';
let _posSet = false;
function applyPos(xf, yf, s, rot) {
  const W = window.innerWidth, H = window.innerHeight;
  if (!(W > 1) || !(H > 1)) return;
  const r = document.getElementById('root');
  r.style.left = `${xf * W}px`; r.style.top = `${yf * H}px`; r.style.setProperty('--s', s); r.style.setProperty('--r', `${rot || 0}deg`);
  window.reportPosition(JSON.stringify({id: WIDGET_ID, x: xf, y: yf, s, r: rot || 0}));
}
window.setHUDPosition = json => {
  _posSet = true;
  try {
    const p = JSON.parse(json);
    applyPos(p.x, p.y, typeof p.s === 'number' ? p.s : 1, typeof p.r === 'number' ? p.r : 0);
  } catch {}
};
window.requestUpdate = window.requestUpdate || (() => {});
window.reportPosition = window.reportPosition || (() => {});
setTimeout(() => { if (!_posSet) applyPos(DEF.xf, DEF.yf, 1, 0); }, 50);
