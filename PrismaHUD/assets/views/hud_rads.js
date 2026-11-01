let _rads = 0, _radsMax = 1000;
const radFill = document.getElementById('radFill');
const radNum = document.getElementById('radNum');
const icon = document.getElementById('icon');
const DEFAULT_ICON = "icons/radioactive.png";
function setIconImage(path) {
  const url = `url("${path}")`;
  icon.style.webkitMaskImage = url; icon.style.maskImage = url;
}
setIconImage(DEFAULT_ICON);
window.setWidgetIcon = json => {
  try {
    const f = JSON.parse(json).icon;
    setIconImage(f ? `icons/${f}` : DEFAULT_ICON);
  } catch {}
};
const root = document.getElementById('root');
let _visState = false;
let _preview = false;

function setIconSev(sev) {
  sev = Math.max(0, Math.min(1, sev));
  icon.style.backgroundColor = `hsl(${Math.round(120 * (1 - sev))},80%,55%)`;
}

setInterval(() => window.requestUpdate(''), 500);

window.updateStats = json => {
  try {
    const d = JSON.parse(json);
    _rads = d.rads || 0; _radsMax = d.radsMax || 1000;
  } catch {}
  const frac = Math.max(0, Math.min(1, _rads / _radsMax));
  radFill.style.width = `${frac * 100}%`;
  const good = 1 - Math.min(1, _rads / 600);
  setIconSev(1 - good);
  radNum.textContent = Math.round(_rads);
  radNum.classList.toggle('show', good <= 0.75);
  const shouldShow = _rads > 0;
  if (shouldShow !== _visState) {
    _visState = shouldShow;
    window.reportVisibility(JSON.stringify({id: 'prisma_rads', visible: shouldShow}));
  }
  root.classList.toggle('visible', _visState || _preview);
};

window.setEditorPreview = v => {
  _preview = v === '1' || v === 1 || v === true;
  if (_preview) { radFill.style.width = '45%'; setIconSev(0.45); radNum.textContent = '150'; radNum.classList.add('show'); }
  root.classList.toggle('visible', _visState || _preview);
};

const DEF = {xf: 30 / 1920, yf: 838 / 1080, wf: 200 / 1920, hf: 44 / 1080};
const WIDGET_ID = 'prisma_rads';
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
window.reportIconSupport = window.reportIconSupport || (() => {});
window.reportIconSupport(JSON.stringify({id: WIDGET_ID}));
setTimeout(() => { if (!_posSet) applyPos(DEF.xf, DEF.yf, 1, 0); }, 50);
