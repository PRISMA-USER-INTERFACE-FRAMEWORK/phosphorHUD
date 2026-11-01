const VP = {w: window.innerWidth, h: window.innerHeight};
window.addEventListener('resize', () => { VP.w = window.innerWidth; VP.h = window.innerHeight; });

const S = {
  widgets: [],
  sel: [],
  snapOn: true,
  gridSize: 10,
  slots: [],
  imports: [],
  presets: [],
  icons: [],
  activeSlot: -1,
  undoStack: [],
  redoStack: [],
  hideKey: 0x71,
  combatOnly: false,
  manualHidden: false,
  rebinding: false,
};

function byId(id) { return S.widgets.find(w => w.id === id); }
function selSet(id) { return S.sel.indexOf(id) !== -1; }
function selSingle() { return S.sel.length === 1 ? byId(S.sel[0]) : null; }

const overlay = document.getElementById('overlay');
const propsBody = document.getElementById('propsBody');
const propsPanel = document.getElementById('props');
const propsToggle = document.getElementById('props-toggle');
let panelOpen = true;

propsToggle.addEventListener('click', () => {
  panelOpen = !panelOpen;
  propsPanel.classList.toggle('collapsed', !panelOpen);
  propsToggle.textContent = panelOpen ? '◄' : '►';
});

function snap(v) {
  if (!S.snapOn || S.gridSize <= 0) return Math.round(v);
  return Math.round(v / S.gridSize) * S.gridSize;
}
document.getElementById('btnSnap').addEventListener('click', e => {
  S.snapOn = !S.snapOn; e.currentTarget.classList.toggle('active', S.snapOn);
});
document.getElementById('selSnap').addEventListener('change', e => {
  S.gridSize = parseInt(e.target.value, 10) || 0;
  S.snapOn = S.gridSize > 0;
  document.getElementById('btnSnap').classList.toggle('active', S.snapOn);
});

const MAX_UNDO = 50;
function pushUndo(ids) {
  const entries = [];
  ids.forEach(id => {
    const w = byId(id);
    if (w) entries.push({id, x: w.x, y: w.y, s: w.s, r: w.r || 0});
  });
  if (!entries.length) return;
  S.undoStack.push(entries);
  if (S.undoStack.length > MAX_UNDO) S.undoStack.shift();
  S.redoStack = [];
  updateUndoButtons();
}
function applyUndoEntry(entries) {
  entries.forEach(e => {
    const w = byId(e.id);
    if (!w) return;
    w.x = e.x; w.y = e.y; w.s = e.s; w.r = e.r || 0;
    updateDZEl(w);
    emitMove(w);
  });
}
function updateUndoButtons() {
  document.getElementById('btnUndo').disabled = S.undoStack.length === 0;
  document.getElementById('btnRedo').disabled = S.redoStack.length === 0;
}
document.getElementById('btnUndo').addEventListener('click', doUndo);
document.getElementById('btnRedo').addEventListener('click', doRedo);
function doUndo() {
  if (!S.undoStack.length) return;
  const entries = S.undoStack.pop();
  const redoSnap = entries.map(e => {
    const w = byId(e.id);
    return w ? {id: e.id, x: w.x, y: w.y, s: w.s, r: w.r || 0} : e;
  });
  S.redoStack.push(redoSnap);
  applyUndoEntry(entries);
  renderSidebar();
  updateUndoButtons();
}
function doRedo() {
  if (!S.redoStack.length) return;
  const entries = S.redoStack.pop();
  const undoSnap = entries.map(e => {
    const w = byId(e.id);
    return w ? {id: e.id, x: w.x, y: w.y, s: w.s, r: w.r || 0} : e;
  });
  S.undoStack.push(undoSnap);
  applyUndoEntry(entries);
  renderSidebar();
  updateUndoButtons();
}

function frac(px, dim) { return px / dim; }
function quarter(w) { const r = ((w.r || 0) % 360 + 360) % 360; return r === 90 || r === 270; }
function bw(w) { return (quarter(w) ? w.dh : w.dw) * w.s; }
function bh(w) { return (quarter(w) ? w.dw : w.dh) * w.s; }
function emitMove(w) {
  window.moveWidget(JSON.stringify({
    id: w.id,
    x: frac(w.x, VP.w), y: frac(w.y, VP.h),
    s: w.s,
    r: w.r || 0,
  }));
}

function makeDZEl(w) {
  const el = document.createElement('div');
  el.className = `wdz${w.locked ? ' locked' : ''}`;
  el.dataset.id = w.id;
  el.style.cssText = `left:${w.x}px;top:${w.y}px;width:${bw(w)}px;height:${bh(w)}px`;
  el.innerHTML =
    `<div class="wdz-label">${escH(w.name)}</div>` +
    `<div class="wdz-pos" id="wp-${w.id}">${fmt2(w.x / VP.w)}, ${fmt2(w.y / VP.h)}  ${w.s.toFixed(2)}&times;</div>`;
  return el;
}

function fmt2(v) { return v.toFixed(4); }

function renderOverlay() {
  overlay.innerHTML = '';
  S.widgets.forEach(w => overlay.appendChild(makeDZEl(w)));
  attachDrag();
  renderSidebar();
}

function updateDZEl(w) {
  const el = overlay.querySelector(`[data-id="${w.id}"]`);
  if (!el) return;
  el.style.left = `${w.x}px`; el.style.top = `${w.y}px`;
  el.style.width = `${bw(w)}px`; el.style.height = `${bh(w)}px`;
  const pos = document.getElementById(`wp-${w.id}`);
  if (pos) pos.innerHTML = `${fmt2(w.x / VP.w)}, ${fmt2(w.y / VP.h)}  ${w.s.toFixed(2)}&times;`;
}

function setSel(id, multi) {
  if (!multi) {
    S.sel = id ? [id] : [];
  } else {
    const idx = S.sel.indexOf(id);
    if (idx !== -1) S.sel.splice(idx, 1);
    else if (id) S.sel.push(id);
  }
  overlay.querySelectorAll('.wdz').forEach(el => {
    el.classList.toggle('sel', selSet(el.dataset.id));
  });
  document.querySelectorAll('.wl-item').forEach(el => {
    el.classList.toggle('sel', selSet(el.dataset.id));
  });
  if (!multi && id) { const _sw = byId(id); if (_sw) bringToFront(_sw); }
  updateAlignBar();
  renderProps();
}

function updateAlignBar() {
  const multi = S.sel.length >= 2;
  const any = S.sel.length >= 1;
  ['abLeft', 'abRight', 'abTop', 'abBottom', 'abHC', 'abVC', 'abCenter'].forEach(id => {
    document.getElementById(id).disabled = !any;
  });
  ['abDistH', 'abDistV'].forEach(id => {
    document.getElementById(id).disabled = !multi;
  });
}

let _drag = null;

function attachDrag() {
}

function ptInWidget(w, cx, cy) {
  return cx >= w.x && cx <= w.x + bw(w) && cy >= w.y && cy <= w.y + bh(w);
}
function hitTestWidget(cx, cy) {
  const pri = selSingle();
  if (pri && ptInWidget(pri, cx, cy)) return pri;
  for (let i = S.widgets.length - 1; i >= 0; i--)
    if (ptInWidget(S.widgets[i], cx, cy)) return S.widgets[i];
  return null;
}
function bringToFront(w) {
  overlay.querySelectorAll('.wdz').forEach(el => { el.style.zIndex = ''; });
  const el = overlay.querySelector(`[data-id="${w.id}"]`);
  if (el) el.style.zIndex = '4';
}
function isInteractiveChrome(t) {
  return !!(t && t.closest && t.closest(
    'button, input, select, textarea, .wl-item, .wl-eye, .slot-name, .slot-btn, #props-toggle, .rh'));
}
document.addEventListener('mousedown', e => {
  if (_drag) return;
  if (isInteractiveChrome(e.target)) return;
  const w = hitTestWidget(e.clientX, e.clientY);
  if (!w) return;
  setSel(w.id, !!e.shiftKey);
  if (w.locked) return;
  pushUndo([w.id]);
  _drag = {type: 'move', id: w.id, sx: e.clientX, sy: e.clientY, ox: w.x, oy: w.y};
  e.preventDefault();
});

document.addEventListener('mousemove', e => {
  if (!_drag) return;
  const w = byId(_drag.id);
  if (!w) return;
  const dx = e.clientX - _drag.sx, dy = e.clientY - _drag.sy;
  const canvasTop = parseInt(getComputedStyle(overlay).top, 10) || 0;
  w.x = Math.max(0, Math.min(VP.w - bw(w), snap(_drag.ox + dx)));
  w.y = Math.max(-canvasTop, Math.min(VP.h - bh(w), snap(_drag.oy + dy)));
  updateDZEl(w);
  updatePropInputs(w);
  emitMove(w);
});

document.addEventListener('mouseup', () => { _drag = null; });

function alignSelected(fn) {
  if (!S.sel.length) return;
  const ws = S.sel.map(byId).filter(Boolean);
  pushUndo(S.sel.slice());
  ws.forEach(w => fn(w, ws));
  ws.forEach(w => { updateDZEl(w); emitMove(w); });
  renderProps();
}

document.getElementById('abLeft').addEventListener('click', () => {
  alignSelected((w, ws) => {
    w.x = ws.length === 1 ? 0 : Math.min(...ws.map(x => x.x));
  });
});
document.getElementById('abRight').addEventListener('click', () => {
  alignSelected((w, ws) => {
    w.x = ws.length === 1 ? VP.w - bw(w) : Math.max(...ws.map(x => x.x + bw(x))) - bw(w);
  });
});
document.getElementById('abTop').addEventListener('click', () => {
  alignSelected((w, ws) => {
    w.y = ws.length === 1 ? 0 : Math.min(...ws.map(x => x.y));
  });
});
document.getElementById('abBottom').addEventListener('click', () => {
  alignSelected((w, ws) => {
    w.y = ws.length === 1 ? VP.h - bh(w) : Math.max(...ws.map(x => x.y + bh(x))) - bh(w);
  });
});
document.getElementById('abHC').addEventListener('click', () => {
  alignSelected((w, ws) => {
    w.x = ws.length === 1 ? (VP.w - bw(w)) / 2 :
      ws.reduce((a, x) => a + x.x + bw(x) / 2, 0) / ws.length - bw(w) / 2;
  });
});
document.getElementById('abVC').addEventListener('click', () => {
  alignSelected((w, ws) => {
    w.y = ws.length === 1 ? (VP.h - bh(w)) / 2 :
      ws.reduce((a, x) => a + x.y + bh(x) / 2, 0) / ws.length - bh(w) / 2;
  });
});
document.getElementById('abCenter').addEventListener('click', () => {
  alignSelected(w => { w.x = (VP.w - bw(w)) / 2; w.y = (VP.h - bh(w)) / 2; });
});
document.getElementById('abDistH').addEventListener('click', () => {
  if (S.sel.length < 2) return;
  const ws = S.sel.map(byId).filter(Boolean).sort((a, b) => a.x - b.x);
  pushUndo(S.sel.slice());
  const totalW = ws.reduce((a, w) => a + bw(w), 0);
  const space = (ws[ws.length - 1].x + bw(ws[ws.length - 1]) - ws[0].x - totalW) / (ws.length - 1);
  let cx = ws[0].x;
  ws.forEach(w => { w.x = cx; cx += bw(w) + space; updateDZEl(w); emitMove(w); });
});
document.getElementById('abDistV').addEventListener('click', () => {
  if (S.sel.length < 2) return;
  const ws = S.sel.map(byId).filter(Boolean).sort((a, b) => a.y - b.y);
  pushUndo(S.sel.slice());
  const totalH = ws.reduce((a, w) => a + bh(w), 0);
  const space = (ws[ws.length - 1].y + bh(ws[ws.length - 1]) - ws[0].y - totalH) / (ws.length - 1);
  let cy = ws[0].y;
  ws.forEach(w => { w.y = cy; cy += bh(w) + space; updateDZEl(w); emitMove(w); });
});

function renderProps() {
  const w = selSingle();
  propsBody.innerHTML = buildPropsHTML(w);
  attachPropHandlers(w);
}

function buildPropsHTML(w) {
  let html = '';
  if (w) {
    html += `<div class="props-name">${escH(w.name)}</div>`;
    html += `<div class="coord-grid">${cc('X', 'px', fmt2(w.x / VP.w))}${cc('Y', 'py', fmt2(w.y / VP.h))}</div>`;
    html += `<div style="margin-bottom:6px"><label style="font-size:9px;color:var(--text-dim);display:block;margin-bottom:3px">Scale (${w.s.toFixed(2)}&times;)</label>
      <div style="display:flex;gap:6px;align-items:center">
        <input type="range" id="scaleRange" min="0.4" max="2.5" step="0.05" value="${w.s}" style="flex:1">
        <input type="number" id="scaleNum" min="0.4" max="2.5" step="0.05" value="${w.s.toFixed(2)}"
          style="width:52px;height:22px;padding:0 5px;background:rgba(0,0,0,0.4);border:1px solid var(--border);color:var(--text);font-size:11px;border-radius:2px"></div></div>`;
    if (w.canSetIcon && S.icons.length) {
      html += `<label style="font-size:9px;color:var(--text-dim);display:block;margin:6px 0 3px">Icon</label>
        <div class="icon-pick">` + S.icons.map(ic => {
          const on = (w.icon || '') === ic.file;
          const inner = ic.file
            ? `<span class="icon-swatch" style="-webkit-mask-image:url('icons/${escH(ic.file)}');mask-image:url('icons/${escH(ic.file)}')"></span>`
            : '&#x2014;';
          return `<button class="icon-btn${on ? ' active' : ''}" data-icon="${escH(ic.file)}" title="${escH(ic.label)}">${inner}</button>`;
        }).join('') + `</div>`;
    }
    html += `<div class="prop-row">
      <button class="prop-btn" id="btnRot">Rotate ${((w.r || 0) % 360 + 360) % 360}&deg;</button>
    </div>`;
    html += `<div class="prop-row">
      <button class="prop-btn" id="btnRT">Reset default</button>
      <button class="prop-btn${w.locked ? ' active' : ''}" id="btnLock">${w.locked ? '&#x25CF; Locked' : '&#x25CB; Lock'}</button>
    </div>`;
  } else {
    html += '<div class="props-empty">Select a widget<br>to edit its position<br><br>Shift+click to<br>multi-select</div>';
  }
  html += buildWidgetListHTML();
  html += buildSlotsHTML();
  html += buildPresetsHTML();
  html += buildImportsHTML();
  html += buildSettingsHTML();
  return html;
}

const VK_NAMES = {8: 'Back', 9: 'Tab', 13: 'Enter', 27: 'Esc', 32: 'Space', 33: 'PgUp', 34: 'PgDn', 35: 'End', 36: 'Home',
  37: 'Left', 38: 'Up', 39: 'Right', 40: 'Down', 45: 'Ins', 46: 'Del',
  112: 'F1', 113: 'F2', 114: 'F3', 115: 'F4', 116: 'F5', 117: 'F6', 118: 'F7', 119: 'F8', 120: 'F9', 121: 'F10', 122: 'F11', 123: 'F12',
  186: ';', 187: '=', 188: ',', 189: '-', 190: '.', 191: '/', 192: '`', 219: '[', 220: '\\', 221: ']', 222: "'"};
function keyName(vk) {
  if (VK_NAMES[vk]) return VK_NAMES[vk];
  if (vk >= 48 && vk <= 57) return String.fromCharCode(vk);
  if (vk >= 65 && vk <= 90) return String.fromCharCode(vk);
  if (vk >= 96 && vk <= 105) return `Num${vk - 96}`;
  return `Key ${vk}`;
}
function buildSettingsHTML() {
  const kn = S.rebinding ? 'Press a key…' : keyName(S.hideKey);
  return `<div class="section-label" style="margin-top:14px">HUD SETTINGS</div>
    <div class="prop-row"><span style="flex:1;font-size:10px;color:var(--text-dim);align-self:center">Hide HUD key</span>
      <button class="prop-btn${S.rebinding ? ' active' : ''}" id="btnRebind" style="flex:0 0 84px">${escH(kn)}</button></div>
    <div class="wl-item" id="btnCombatOnly" style="cursor:pointer">
      <span class="wl-eye">${S.combatOnly ? '&#x25c9;' : '&#x25cb;'}</span>
      <span class="wl-name">Show HUD only in combat</span></div>
    <div style="font-size:9px;color:var(--text-dim);margin-top:4px;line-height:1.5">
      ${keyName(S.hideKey)} toggles the whole HUD. Combat mode hides it until you enter combat.</div>`;
}
function attachSettingsHandlers() {
  const rb = document.getElementById('btnRebind');
  if (rb) rb.addEventListener('click', () => {
    S.rebinding = true; renderSidebar();
    const onKey = e => {
      if ([16, 17, 18].indexOf(e.keyCode) !== -1) return;
      e.preventDefault();
      document.removeEventListener('keydown', onKey, true);
      S.rebinding = false; S.hideKey = e.keyCode;
      window.setHideKey(JSON.stringify({vk: e.keyCode}));
      renderSidebar();
    };
    document.addEventListener('keydown', onKey, true);
  });
  const co = document.getElementById('btnCombatOnly');
  if (co) co.addEventListener('click', () => {
    S.combatOnly = !S.combatOnly;
    window.setCombatOnly(JSON.stringify({on: S.combatOnly}));
    renderSidebar();
  });
}

function cc(label, field, val) {
  return `<div class="coord-cell"><label>${label} (frac)</label>
    <input type="number" step="0.0001" data-f="${field}" value="${val}"></div>`;
}

function attachPropHandlers(w) {
  propsBody.querySelectorAll('input[data-f]').forEach(inp => {
    inp.addEventListener('input', () => {
      const v = parseFloat(inp.value) || 0, w2 = byId(S.sel[0]);
      if (!w2) return;
      if (inp.dataset.f === 'px') w2.x = v * VP.w;
      else if (inp.dataset.f === 'py') w2.y = v * VP.h;
      updateDZEl(w2); emitMove(w2);
    });
  });
  function setScale(v) {
    const w2 = byId(S.sel[0]); if (!w2) return;
    v = Math.max(0.4, Math.min(2.5, parseFloat(v) || 1));
    w2.s = v;
    const rng = document.getElementById('scaleRange'), num = document.getElementById('scaleNum');
    if (rng && document.activeElement !== rng) rng.value = v;
    if (num && document.activeElement !== num) num.value = v.toFixed(2);
    updateDZEl(w2); emitMove(w2);
  }
  const sr = document.getElementById('scaleRange');
  if (sr) sr.addEventListener('input', e => { pushUndo([S.sel[0]]); setScale(e.target.value); });
  const sn = document.getElementById('scaleNum');
  if (sn) sn.addEventListener('input', e => { pushUndo([S.sel[0]]); setScale(e.target.value); });
  document.querySelectorAll('[data-icon]').forEach(btn => {
    btn.addEventListener('click', () => {
      const w2 = byId(S.sel[0]); if (!w2) return;
      w2.icon = btn.dataset.icon;
      window.setWidgetIcon(JSON.stringify({id: w2.id, icon: w2.icon}));
      renderProps();
    });
  });
  const rb = document.getElementById('btnRot');
  if (rb) rb.addEventListener('click', () => {
    const w2 = byId(S.sel[0]); if (!w2) return;
    pushUndo([S.sel[0]]);
    w2.r = (((w2.r || 0) + 90) % 360);
    updateDZEl(w2); emitMove(w2); renderProps();
  });
  const rt = document.getElementById('btnRT');
  if (rt) rt.addEventListener('click', () => resetWidget(S.sel[0]));
  const lk = document.getElementById('btnLock');
  if (lk && w) lk.addEventListener('click', () => {
    w.locked = !w.locked;
    const el = overlay.querySelector(`[data-id="${w.id}"]`);
    if (el) el.classList.toggle('locked', w.locked);
    lk.textContent = w.locked ? '● Locked' : '○ Lock';
    lk.classList.toggle('active', w.locked);
  });
  attachListHandlers();
  attachSlotHandlers();
  attachPresetHandlers();
  attachImportHandlers();
  attachSettingsHandlers();
}

function updatePropInputs(w) {
  [{f: 'px', v: w.x / VP.w}, {f: 'py', v: w.y / VP.h}].forEach(p => {
    const inp = propsBody.querySelector(`[data-f="${p.f}"]`);
    if (inp && document.activeElement !== inp) inp.value = fmt2(p.v);
  });
  const rng = document.getElementById('scaleRange'), num = document.getElementById('scaleNum');
  if (rng && document.activeElement !== rng) rng.value = w.s;
  if (num && document.activeElement !== num) num.value = w.s.toFixed(2);
}

function buildWidgetListHTML() {
  if (!S.widgets.length) return '';
  let html = '<div class="section-label">WIDGETS</div>';
  S.widgets.forEach(w => {
    html += `<div class="wl-item${selSet(w.id) ? ' sel' : ''}" data-id="${w.id}">
      <span class="wl-eye" data-eye="${w.id}" title="Toggle visibility">${w.visible !== false ? '&#x25cf;' : '&#x25cb;'}</span>
      <span class="wl-name">${escH(w.name)}</span>
    </div>`;
  });
  return html;
}

function attachListHandlers() {
  document.querySelectorAll('.wl-item').forEach(el => {
    el.addEventListener('click', e => {
      if (e.target.classList.contains('wl-eye')) return;
      setSel(el.dataset.id, !!e.shiftKey);
    });
  });
  document.querySelectorAll('[data-eye]').forEach(el => {
    el.addEventListener('click', e => {
      e.stopPropagation();
      const id = el.dataset.eye, w = byId(id);
      if (!w) return;
      w.visible = w.visible === false ? true : false;
      el.innerHTML = w.visible !== false ? '&#x25cf;' : '&#x25cb;';
      window.setWidgetVisible(JSON.stringify({id, visible: w.visible !== false}));
    });
  });
}

function renderSidebar() { renderProps(); }

function buildPresetsHTML() {
  if (!S.presets.length) return '';
  let html = '<div class="section-label" style="margin-top:14px">PRESETS</div>';
  S.presets.forEach(p => {
    html += `<div class="slot-item"><div class="slot-row">
        <span class="slot-name">${escH(p.name)}</span>
        <button class="slot-btn" data-preset="${escH(p.name)}">Apply</button>
      </div></div>`;
  });
  return html;
}

function attachPresetHandlers() {
  document.querySelectorAll('[data-preset]').forEach(b => {
    b.addEventListener('click', () => {
      pushUndo(S.widgets.map(w => w.id));
      window.applyPreset(JSON.stringify({name: b.dataset.preset}));
    });
  });
}

function buildImportsHTML() {
  let html = '<div class="section-label" style="margin-top:14px">EXPORT / IMPORT</div>';
  html += `<div class="slot-item"><div class="slot-row">
      <input class="slot-name-input" id="expName" type="text" maxlength="40" placeholder="layout name" style="display:flex">
      <button class="slot-btn" id="btnExport">Export</button>
    </div></div>`;
  if (!S.imports.length) {
    html += '<div class="slot-item"><div class="slot-row"><span class="slot-name">no exports yet</span></div></div>';
    return html;
  }
  S.imports.forEach(stem => {
    html += `<div class="slot-item"><div class="slot-row">
        <span class="slot-name" title="${escH(stem)}">${escH(stem)}</span>
        <button class="slot-btn" data-import="${escH(stem)}">Import</button>
      </div></div>`;
  });
  return html;
}

function attachImportHandlers() {
  const btn = document.getElementById('btnExport');
  const nameEl = document.getElementById('expName');
  if (btn && nameEl) {
    const doExport = () => {
      const name = nameEl.value.trim();
      if (!name) return;
      window.exportLayout(JSON.stringify({name}));
      nameEl.value = '';
    };
    btn.addEventListener('click', doExport);
    nameEl.addEventListener('keydown', e => { if (e.key === 'Enter') { e.preventDefault(); doExport(); } });
  }
  document.querySelectorAll('[data-import]').forEach(b => {
    b.addEventListener('click', () => {
      window.importLayout(JSON.stringify({name: b.dataset.import}));
      window.requestWidgets('');
    });
  });
}

function buildSlotsHTML() {
  if (!S.slots.length) return '';
  let html = '<div class="section-label" style="margin-top:14px">SAVE SLOTS</div>';
  S.slots.forEach(slot => {
    const active = slot.index === S.activeSlot;
    html += `<div class="slot-item${active ? ' active-slot' : ''}" data-si="${slot.index}">
      <div class="slot-row">
        <span class="slot-name${active ? ' active' : ''}" id="sn-${slot.index}" title="click to rename">${escH(slot.name || `Slot ${slot.index + 1}`)}</span>
        <input class="slot-name-input" id="si-${slot.index}" type="text" maxlength="20">
        <button class="slot-btn" data-load="${slot.index}">Load</button>
        <button class="slot-btn" data-save="${slot.index}">Save</button>
      </div>
    </div>`;
  });
  return html;
}

function attachSlotHandlers() {
  document.querySelectorAll('[data-load]').forEach(btn => {
    btn.addEventListener('click', () => {
      const idx = parseInt(btn.dataset.load, 10);
      S.activeSlot = idx;
      window.loadSlot(JSON.stringify({index: idx}));
      renderSidebar();
    });
  });
  document.querySelectorAll('[data-save]').forEach(btn => {
    btn.addEventListener('click', () => {
      const idx = parseInt(btn.dataset.save, 10);
      const name = (S.slots[idx] && S.slots[idx].name) || `Slot ${idx + 1}`;
      S.activeSlot = idx;
      window.saveSlot(JSON.stringify({index: idx, name}));
      renderSidebar();
    });
  });
  document.querySelectorAll('.slot-name').forEach(nameEl => {
    nameEl.addEventListener('click', () => {
      const idx = parseInt(nameEl.id.slice(3), 10);
      const inputEl = document.getElementById(`si-${idx}`);
      if (!inputEl || Number.isNaN(idx)) return;
      nameEl.style.display = 'none';
      inputEl.style.display = 'flex';
      inputEl.value = (S.slots[idx] && S.slots[idx].name) || '';
      inputEl.focus();
      inputEl.select();

      let done = false;
      const finish = commitIt => {
        if (done) return;
        done = true;
        inputEl.removeEventListener('blur', onBlur);
        inputEl.removeEventListener('keydown', onKey);
        nameEl.style.display = '';
        inputEl.style.display = 'none';
        if (!commitIt) return;
        const name = inputEl.value.trim() || `Slot ${idx + 1}`;
        if (S.slots[idx]) S.slots[idx].name = name;
        nameEl.textContent = name;
        window.renameSlot(JSON.stringify({index: idx, name}));
        renderSidebar();
      };
      const onKey = e => {
        if (e.key === 'Enter') { e.preventDefault(); finish(true); }
        else if (e.key === 'Escape') { e.preventDefault(); finish(false); }
      };
      const onBlur = () => finish(true);
      inputEl.addEventListener('keydown', onKey);
      inputEl.addEventListener('blur', onBlur);
    });
  });
}

function resetWidget(id) {
  const w = byId(id); if (!w) return;
  pushUndo([id]);
  w.x = w.defaultX * VP.w; w.y = w.defaultY * VP.h; w.s = 1; w.r = 0;
  updateDZEl(w); updatePropInputs(w); emitMove(w);
}

document.getElementById('btnResetSel').addEventListener('click', () => {
  S.sel.forEach(id => resetWidget(id));
});
document.getElementById('btnResetAll').addEventListener('click', () => {
  S.widgets.forEach(w => resetWidget(w.id));
});
document.getElementById('btnSave').addEventListener('click', () => {
  const out = {};
  S.widgets.forEach(w => {
    out[w.id] = {x: frac(w.x, VP.w), y: frac(w.y, VP.h), s: w.s, r: w.r || 0};
  });
  window.saveLayout(JSON.stringify(out));
  S.undoStack = []; S.redoStack = []; updateUndoButtons();
});
document.getElementById('btnClose').addEventListener('click', () => window.closeEditor(''));

document.addEventListener('keydown', e => {
  if (e.key === 'Escape') { window.closeEditor(''); return; }
  if ((e.ctrlKey || e.metaKey) && e.key === 's') { e.preventDefault(); document.getElementById('btnSave').click(); return; }
  if ((e.ctrlKey || e.metaKey) && e.key === 'z') { e.preventDefault(); doUndo(); return; }
  if ((e.ctrlKey || e.metaKey) && e.key === 'y') { e.preventDefault(); doRedo(); return; }
  if (e.key === 'Tab') {
    e.preventDefault();
    if (!S.widgets.length) return;
    let idx = S.sel.length ? S.widgets.findIndex(w => w.id === S.sel[0]) : -1;
    idx = (idx + 1) % S.widgets.length;
    setSel(S.widgets[idx].id, false);
    return;
  }
  if (!S.sel.length) return;
  if (!['ArrowLeft', 'ArrowRight', 'ArrowUp', 'ArrowDown'].includes(e.key)) return;
  e.preventDefault();
  const step = e.shiftKey ? 10 : 1;
  S.sel.forEach(id => {
    const w = byId(id); if (!w || w.locked) return;
    if (e.key === 'ArrowLeft') w.x -= step;
    if (e.key === 'ArrowRight') w.x += step;
    if (e.key === 'ArrowUp') w.y -= step;
    if (e.key === 'ArrowDown') w.y += step;
    updateDZEl(w); emitMove(w);
  });
  if (S.sel.length === 1) updatePropInputs(byId(S.sel[0]));
});

window.initWidgets = json => {
  VP.w = window.innerWidth; VP.h = window.innerHeight;
  if (!(VP.w > 1) || !(VP.h > 1)) { setTimeout(() => window.initWidgets(json), 32); return; }
  try {
    S.widgets = JSON.parse(json).map(item => ({
      id: item.id,
      name: item.name,
      x: item.x * VP.w,
      y: item.y * VP.h,
      s: typeof item.s === 'number' ? item.s : 1,
      r: typeof item.r === 'number' ? item.r : 0,
      icon: typeof item.icon === 'string' ? item.icon : '',
      canSetIcon: item.canSetIcon === true,
      dw: item.defaultW * 1920,
      dh: item.defaultH * 1080,
      defaultX: item.defaultX,
      defaultY: item.defaultY,
      defaultW: item.defaultW,
      defaultH: item.defaultH,
      visible: item.visible !== false,
      locked: false,
    }));
    S.undoStack = []; S.redoStack = []; updateUndoButtons();
    renderOverlay();
    updateAlignBar();
  } catch (e) { console.error(`initWidgets: ${e}`); }
};

window.initIcons = json => {
  try { S.icons = JSON.parse(json) || []; } catch { S.icons = []; }
  renderProps();
};
window.initPresets = json => {
  try { S.presets = JSON.parse(json) || []; } catch { S.presets = []; }
  renderProps();
};
window.initImports = json => {
  try { S.imports = JSON.parse(json) || []; } catch { S.imports = []; }
  renderProps();
};
window.initSlots = json => {
  try {
    S.slots = JSON.parse(json);
    renderSidebar();
  } catch {}
};

window.initSettings = json => {
  try {
    const d = JSON.parse(json);
    if (typeof d.hideKey === 'number') S.hideKey = d.hideKey;
    if (typeof d.combatOnly === 'boolean') S.combatOnly = d.combatOnly;
    if (typeof d.manualHidden === 'boolean') S.manualHidden = d.manualHidden;
    S.rebinding = false;
    renderSidebar();
  } catch {}
};

window.setHUDPosition = () => {};

window.moveWidget = window.moveWidget || (j => console.log(`moveWidget: ${j}`));
window.saveLayout = window.saveLayout || (j => console.log(`saveLayout: ${j}`));
window.closeEditor = window.closeEditor || (() => console.log('closeEditor'));
window.setWidgetVisible = window.setWidgetVisible || (j => console.log(`setWidgetVisible: ${j}`));
window.loadSlot = window.loadSlot || (j => console.log(`loadSlot: ${j}`));
window.saveSlot = window.saveSlot || (j => console.log(`saveSlot: ${j}`));
window.renameSlot = window.renameSlot || (j => console.log(`renameSlot: ${j}`));
window.setWidgetIcon = window.setWidgetIcon || (j => console.log(`setWidgetIcon: ${j}`));
window.applyPreset = window.applyPreset || (j => console.log(`applyPreset: ${j}`));
window.exportLayout = window.exportLayout || (j => console.log(`exportLayout: ${j}`));
window.importLayout = window.importLayout || (j => console.log(`importLayout: ${j}`));
window.requestWidgets = window.requestWidgets || (() => {});
window.setHideKey = window.setHideKey || (j => console.log(`setHideKey: ${j}`));
window.setCombatOnly = window.setCombatOnly || (j => console.log(`setCombatOnly: ${j}`));

function escH(s) {
  return String(s || '').replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
}

updateAlignBar();
updateUndoButtons();
