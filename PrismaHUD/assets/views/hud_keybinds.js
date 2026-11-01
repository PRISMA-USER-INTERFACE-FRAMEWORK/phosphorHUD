const root = document.getElementById('root');
const kbList = document.getElementById('kbList');
const kbModal = document.getElementById('kbModal');
const kbModalMsg = document.getElementById('kbModalMsg');

let _binds = [];
let _listening = null;
let _pending = null;

window.rebindBegin = window.rebindBegin || (() => {});
window.rebindApply = window.rebindApply || (() => {});
window.rebindCancel = window.rebindCancel || (() => {});
window.resetKeybinds = window.resetKeybinds || (() => {});
window.closeKeybinds = window.closeKeybinds || (() => {});

function escH(s) { return String(s || '').replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;'); }

function render() {
  let html = '', lastCat = null;
  _binds.forEach(b => {
    if (b.category !== lastCat) { html += `<div class="kb-cat">${escH(b.category)}</div>`; lastCat = b.category; }
    const custom = b.key !== b.defaultKey ? ' custom' : '';
    const listening = _listening && _listening.id === b.id;
    const capCls = `kb-cap${listening ? ' listening' : custom}`;
    const capTxt = listening ? 'Press key…' : `${b.requireShift ? 'Shift+' : ''}${escH(b.keyName)}`;
    html += `<div class="kb-row">
      <span class="kb-label">${escH(b.label)}</span>
      <span class="${capCls}">${capTxt}</span>
      <button class="kb-btn" data-rebind="${escH(b.id)}">Rebind</button>
    </div>`;
  });
  kbList.innerHTML = html;
  kbList.querySelectorAll('[data-rebind]').forEach(btn => {
    btn.onclick = () => startListen(btn.getAttribute('data-rebind'));
  });
}

function startListen(id) {
  _listening = {id};
  window.rebindBegin(JSON.stringify({id}));
  render();
}
function stopListen() {
  if (_listening) { _listening = null; window.rebindCancel(''); render(); }
}

window.initKeybinds = json => {
  try { _binds = JSON.parse(json) || []; } catch { _binds = []; }
  _listening = null; render();
};
window.rebindConflict = json => {
  try {
    const d = JSON.parse(json); _pending = d;
    kbModalMsg.innerHTML = `<b>${escH(d.keyName)}</b> is already bound to <b>${escH(d.conflict)}</b>. Rebinding will leave both actions on this key.`;
    kbModal.classList.add('on');
  } catch {}
};

document.addEventListener('keydown', e => {
  if (!_listening) return;
  e.preventDefault(); e.stopPropagation();
  if (e.keyCode === 27) { stopListen(); return; }
  const vk = e.keyCode || e.which;
  if (!vk) return;
  if (vk === 16 || vk === 17 || vk === 18) return;
  const id = _listening.id; _listening = null;
  window.rebindApply(JSON.stringify({id, vk, force: false}));
  render();
}, true);

document.getElementById('kbClose').onclick = () => window.closeKeybinds('');
document.getElementById('kbResetAll').onclick = () => window.resetKeybinds('');
document.getElementById('kbModalCancel').onclick = () => { kbModal.classList.remove('on'); _pending = null; };
document.getElementById('kbModalForce').onclick = () => {
  if (_pending) window.rebindApply(JSON.stringify({id: _pending.id, vk: _pending.vk, force: true}));
  kbModal.classList.remove('on'); _pending = null;
};

window.setVisible = v => { root.classList.toggle('visible', v === '1' || v === 1 || v === true); };
