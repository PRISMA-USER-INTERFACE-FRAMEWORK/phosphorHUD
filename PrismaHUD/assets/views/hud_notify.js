const root = document.getElementById('root');
const stack = document.getElementById('stack');
let _visState = false;
let _preview = false;

setInterval(() => window.requestUpdate(''), 200);

function escH(s) { return String(s || '').replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;'); }

const MAX_VISIBLE = 5;
const LIFETIME_MS = 5000;

let _toasts = [];
let _nextId = 1;

function retire(t) {
  if (!t.el) return;
  t.el.classList.remove('in');
  t.el.classList.add('out');
  const el = t.el;
  t.el = null;
  setTimeout(() => { if (el.parentNode) el.parentNode.removeChild(el); }, 180);
}

function addToast(text, warning) {
  const now = performance.now();

  const last = _toasts.length ? _toasts[_toasts.length - 1] : null;
  if (last && last.el && last.text === text) {
    last.count += 1;
    last.born = now;
    const c = last.el.querySelector('.nt-count');
    if (c) { c.textContent = `x${last.count}`; c.style.display = ''; }
    return;
  }

  const t = { id: _nextId++, text, warning: !!warning, count: 1, born: now, el: null };

  const el = document.createElement('div');
  el.className = `nt${warning ? ' warn' : ''}`;
  el.innerHTML = `<span class="nt-bar"></span><span class="nt-text">${escH(text)}</span>` +
                 `<span class="nt-count" style="display:none"></span>`;
  stack.appendChild(el);
  t.el = el;
  requestAnimationFrame(() => el.classList.add('in'));

  _toasts.push(t);

  while (_toasts.filter(x => x.el).length > MAX_VISIBLE) {
    const oldest = _toasts.find(x => x.el);
    if (!oldest) break;
    retire(oldest);
  }
}

setInterval(() => {
  if (_preview) return;
  const now = performance.now();
  for (const t of _toasts) {
    if (t.el && (now - t.born) >= LIFETIME_MS) retire(t);
  }
  _toasts = _toasts.filter(t => t.el);

  const shouldShow = _toasts.length > 0;
  if (shouldShow !== _visState) {
    _visState = shouldShow;
    root.classList.toggle('visible', shouldShow);
    window.reportVisibility(JSON.stringify({ id: 'prisma_notify', visible: shouldShow }));
  }
}, 250);

window.pushToasts = json => {
  try {
    const d = JSON.parse(json);
    const list = d.toasts || [];
    const start = Math.max(0, list.length - MAX_VISIBLE);
    for (let i = start; i < list.length; ++i) addToast(list[i].text, list[i].warning);
    if (!_visState && _toasts.length) {
      _visState = true;
      root.classList.add('visible');
      window.reportVisibility(JSON.stringify({ id: 'prisma_notify', visible: true }));
    }
  } catch {}
};

window.setEditorPreview = v => {
  _preview = v === '1' || v === 1 || v === true;
  if (_preview) {
    stack.innerHTML = '';
    _toasts = [];
    ['Stimpak added.', 'Quest updated: Hunter', 'You are overencumbered.'].forEach((s, i) => {
      const el = document.createElement('div');
      el.className = `nt in${i === 2 ? ' warn' : ''}`;
      el.innerHTML = `<span class="nt-bar"></span><span class="nt-text">${escH(s)}</span>`;
      stack.appendChild(el);
    });
    root.classList.add('visible');
  } else {
    stack.innerHTML = '';
    _toasts = [];
    root.classList.toggle('visible', false);
    _visState = false;
  }
};

const DEF = { xf: 40 / 1920, yf: 260 / 1080, wf: 420 / 1920, hf: 200 / 1080 };
const WIDGET_ID = 'prisma_notify';
let _want = null, _applied = false;
function flushPos() {
  if (!_want || _applied) return;
  const W = window.innerWidth, H = window.innerHeight;
  if (!(W > 1) || !(H > 1)) return;
  root.style.left = `${_want.xf * W}px`;
  root.style.top = `${_want.yf * H}px`;
  root.style.setProperty('--s', _want.s); root.style.setProperty('--r', `${_want.r || 0}deg`);
  _applied = true;
  window.reportPosition(JSON.stringify({ id: WIDGET_ID, x: _want.xf, y: _want.yf, s: _want.s, r: _want.r || 0 }));
}
function applyPos(xf, yf, s, rot) { _want = { xf, yf, s, r: rot || 0 }; _applied = false; flushPos(); }
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
