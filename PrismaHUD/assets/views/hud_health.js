let _hp = 100, _hpMax = 100, _rads = 0, _radsMax = 1000;
const hpFill = document.getElementById('hpFill');
const radFill = document.getElementById('radFill');
const hpNum = document.getElementById('hpNum');
const icon = document.getElementById('icon');
const DEFAULT_ICON = "icons/heart-rate.png";
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

function setIconSev(sev) {
  sev = Math.max(0, Math.min(1, sev));
  icon.style.backgroundColor = `hsl(${Math.round(120 * (1 - sev))},80%,55%)`;
}

setInterval(() => window.requestUpdate(''), 250);

window.updateStats = json => {
  try {
    const d = JSON.parse(json);
    _hp = +d.hp || 0;
    _hpMax = +d.hpMax || 1;
    _rads = Math.max(0, +d.rads || 0);
    _radsMax = Math.max(1, +d.radsMax || 1000);
  } catch {}
  render();
};

function render() {
  const rad = Math.max(0, Math.min(1, _rads / _radsMax));
  const available = 1 - rad;
  const rawGood = _hpMax > 0 ? Math.max(0, Math.min(1, _hp / _hpMax)) : 1;
  const good = Math.min(rawGood, available);

  hpFill.style.width = `${good * 100}%`;
  radFill.style.width = `${rad * 100}%`;
  setIconSev(1 - good);
  hpNum.textContent = Math.round(_hp);
  hpNum.classList.toggle('show', good <= 0.75);
}

const DEF = {xf: 30 / 1920, yf: 880 / 1080, wf: 260 / 1920, hf: 56 / 1080};
const WIDGET_ID = 'prisma_health';
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
window.reportIconSupport = window.reportIconSupport || (() => {});
window.reportIconSupport(JSON.stringify({id: WIDGET_ID}));
setTimeout(() => { if (!_posSet) applyPos(DEF.xf, DEF.yf, 1, 0); }, 50);