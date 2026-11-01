let _ap = 100, _apMax = 100;
const apFill = document.getElementById('apFill');
const apNum = document.getElementById('apNum');
const icon = document.getElementById('icon');
const DEFAULT_ICON = "icons/running-man.png";
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
    _ap = d.ap || 0; _apMax = d.apMax || 1;
  } catch {}
  const good = _apMax > 0 ? Math.max(0, Math.min(1, _ap / _apMax)) : 1;
  apFill.style.width = `${good * 100}%`;
  setIconSev(1 - good);
  apNum.textContent = Math.round(_ap);
  apNum.classList.toggle('show', good <= 0.75);
};

const DEF = {xf: 30 / 1920, yf: 940 / 1080, wf: 260 / 1920, hf: 36 / 1080};
const WIDGET_ID = 'prisma_ap';
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
