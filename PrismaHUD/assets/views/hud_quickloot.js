const root = document.getElementById('root');
const qlIcon = document.getElementById('qlIcon');
const qlName = document.getElementById('qlName');
const qlSub = document.getElementById('qlSub');
const qlCount = document.getElementById('qlCount');
const qlList = document.getElementById('qlList');
const carryFill = document.getElementById('carryFill');
const carryGhost = document.getElementById('carryGhost');
const carryVal = document.getElementById('carryVal');
const totalVal = document.getElementById('totalVal');
const hideKeyCap = document.getElementById('hideKeyCap');
const takeHint = document.getElementById('takeHint');
const takeCap = document.getElementById('takeCap');
const takeAllHint = document.getElementById('takeAllHint');
const takeAllCap = document.getElementById('takeAllCap');
const transferHint = document.getElementById('transferHint');
const transferCap = document.getElementById('transferCap');
const perkHint = document.getElementById('perkHint');
const perkCap = document.getElementById('perkCap');
const perkLbl = document.getElementById('perkLbl');
const selectHint = document.getElementById('selectHint');
const selectCap = document.getElementById('selectCap');
const qlChoices = document.getElementById('qlChoices');

function _getMP() { const a = window.__prismaUI_modelPreview; return a && typeof a.show === 'function' ? a : null; }

let _statusWired = false;
const _meshReady = {};
function wireStatusOnce() {
  if (_statusWired) return;
  const api = window.__prismaUI_modelPreview;
  if (!api || typeof api.onStatus !== 'function') return;
  _statusWired = true;
  api.onStatus(s => {
    console.log(`[quickloot mesh] id=${s && s.id} phase=${s && s.phase} ok=${s && s.ok}${s && s.error ? ` err=${s.error}` : ''} nif=${s && s.nifPath}`);
    if (!s || !s.id) return;
    const m = /^f(.+)$/.exec(s.id);
    if (!m) return;
    const formId = m[1];
    const ready = !!(s.ok && (s.phase === 'loaded' || s.phase === 'textured'));
    if (ready) _meshReady[formId] = true; else delete _meshReady[formId];
    const el = qlList.querySelector(`.cat-icon[data-formid="${formId}"]`);
    if (el) el.style.visibility = ready ? 'hidden' : '';
  });
}

let _items = [];
let _sel = 0;
let _visState = false;
let _preview = false;

const CAT_ICON_FILES = {weap: 'cat-weap', armo: 'cat-armo', aid: 'cat-aid', ammo: 'cat-ammo', misc: 'cat-misc', junk: 'cat-misc'};

const ROW_PREVIEW_WINDOW = 10;
let _rowPreviewIds = {};
function updateRowPreviews() {
  const api = _getMP(); if (!api) return;
  wireStatusOnce();
  if (!_visState || _preview) {
    for (const pid in _rowPreviewIds) api.hide({id: pid});
    _rowPreviewIds = {};
    return;
  }
  const listRect = qlList.getBoundingClientRect();
  const lo = Math.max(0, _sel - ROW_PREVIEW_WINDOW), hi = Math.min(_items.length - 1, _sel + ROW_PREVIEW_WINDOW);
  const active = {};
  for (let i = lo; i <= hi; i++) {
    const item = _items[i];
    if (!item || !item.nifPath || !item.formId) continue;
    const el = qlList.querySelector(`.cat-icon[data-formid="${item.formId}"]`);
    if (!el) continue;
    const r = el.getBoundingClientRect();
    const onScreen = r.width > 2 && r.height > 2 && r.top >= listRect.top - 1 && r.bottom <= listRect.bottom + 1;
    if (!onScreen) continue;
    const id = `f${item.formId}`;
    active[id] = true;
    api.show({id, nifPath: item.nifPath, formType: item.formType || 'MISC',
              x: Math.round(r.left), y: Math.round(r.top), w: Math.round(r.width), h: Math.round(r.height),
              spin: 0, zoom: 0.95, brightness: 1.25});
  }
  for (const pid2 in _rowPreviewIds) if (!active[pid2]) api.hide({id: pid2});
  _rowPreviewIds = active;
}

function escH(s) { return String(s || '').replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;'); }

function renderExtras(choices, ps) {
  let html = '';
  (choices || []).forEach(c => {
    const icon = c.icon ? `<img class="ql-choice-icon" src="${escH(c.icon)}" alt="">` : '';
    html += `<div class="ql-choice-btn" data-idx="${c.index}" onclick="window.activateChoice(JSON.stringify({index:${c.index}}))">
      ${CG.renderKey(c.key || String(c.index), ps)}${icon}<span class="ql-choice-lbl">${escH(c.label)}</span>
    </div>`;
  });
  qlChoices.innerHTML = html;
}

function renderFooterAction(hint, cap, key, ps) {
  cap.innerHTML = key ? CG.renderKey(key, ps) : '';
  hint.style.display = key ? '' : 'none';
}

function renderPerkFooter(perkButton, transferKey, ps) {
  if (perkButton && perkButton.label) {
    perkCap.innerHTML = CG.renderKey(perkButton.key || 'Space', ps);
    perkLbl.textContent = perkButton.label;
    perkHint.style.display = '';
  } else {
    perkHint.style.display = 'none';
  }
  transferHint.style.display = transferKey ? '' : 'none';
}

function valuePerWeight(item) {
  const v = Number(item.value), w = Number(item.weight);
  if (!isFinite(v) || !isFinite(w) || w <= 0 || v <= 0) return null;
  const r = v / w;
  return r < 10 ? r.toFixed(1) : String(Math.round(r));
}

function renderComponents(item) {
  if (!Array.isArray(item.components) || !item.components.length) return '';
  const parts = item.components
    .filter(c => c && c.name)
    .map(c => `<span class="comp-pair">${escH(c.name)}${c.count > 1 ? ` <b>${c.count}</b>` : ''}</span>`);
  return parts.length ? `<div class="ql-row3">${parts.join('')}</div>` : '';
}

function renderList() {
  if (!_items.length) {
    qlList.innerHTML = '<div style="padding:12px;font-size:11px;color:rgba(255,255,255,0.3)">Empty</div>';
    updateRowPreviews();
    return;
  }
  let html = '';
  _items.forEach((item, i) => {
    const r = item.rarity || 'common';
    const iconFile = CAT_ICON_FILES[item.category] || 'cat-misc';
    const meshOk = item.formId && _meshReady[item.formId];
    let stats = '';
    if (item.dmg) stats += `<span class="stat-pair">⚔${item.dmg}</span>`;
    if (item.dr) stats += `<span class="stat-pair">◈${item.dr}</span>`;
    if (item.rng) stats += `<span class="stat-pair">↔${item.rng}</span>`;
    if (item.ammoType) stats += `<span class="stat-pair" title="ammo type">${escH(item.ammoType)}</span>`;
    if (item.powerArmorBattery) {
      if (item.batteryChargeMixed) {
        stats += '<span class="stat-pair" title="multiple battery charge states">CHARGE MIXED</span>';
      } else {
        const charge = Number(item.batteryChargePercent);
        if (Number.isFinite(charge))
          stats += `<span class="stat-pair" title="remaining battery charge">CHARGE ${Math.round(Math.max(0, Math.min(100, charge)))}%</span>`;
      }
    }
    const vpw = valuePerWeight(item);
    if (vpw !== null)
      stats += `<span class="stat-pair" title="value per unit of weight"><img class="caps-icon" src="icons/cat-caps.png" alt="">/wt ${vpw}</span>`;
    html += `<div class="ql-item${i === _sel ? ' sel' : ''}${item.legendary ? ' legendary' : ''}" data-idx="${i}">
      <div class="ql-row1">
        <span class="rarity-dot rarity-${r}"></span>
        <img class="cat-icon" data-idx="${i}" data-formid="${escH(item.formId)}"${meshOk ? ' style="visibility:hidden"' : ''} src="icons/${iconFile}.png" alt="">
        <span class="item-name rarity-${r}">${escH(item.name)}</span>
        ${item.questItem ? '<span class="quest-badge" title="quest item">◆</span>' : ''}
        ${item.legendary ? '<span class="legendary-star" title="legendary">★</span>' : ''}
        ${item.count > 1 ? `<span class="item-count">×${item.count}</span>` : ''}
        <span class="item-val" title="value in caps"><img class="caps-icon" src="icons/cat-caps.png" alt="">${item.value}</span>
      </div>
      ${stats ? `<div class="ql-row2">${stats}</div>` : ''}
      ${renderComponents(item)}
    </div>`;
  });
  qlList.innerHTML = html;
  const selEl = qlList.querySelector('.ql-item.sel');
  if (selEl) selEl.scrollIntoView({block: 'nearest'});
  updateRowPreviews();
}

setInterval(() => {
  window.requestUpdate('');
  if (_visState && !_preview) updateRowPreviews();
}, 200);

window.updateLoot = json => {
  try {
    const d = JSON.parse(json);
    const shouldShow = d.visible === true;

    if (shouldShow) {
      const isCorpse = d.containerType === 'corpse';
      qlIcon.className = `ql-icon ${isCorpse ? 'corpse' : 'container'}`;
      qlIcon.textContent = isCorpse ? '☠' : '▣';
      qlName.textContent = d.containerName || 'Container';
      const _ps = d.controllerStyle === 'playstation';
      hideKeyCap.innerHTML = CG.renderKey(d.hideKeyName || '-', _ps);
      renderFooterAction(takeHint, takeCap, d.takeKey, _ps);
      renderFooterAction(takeAllHint, takeAllCap, d.takeAllKey, _ps);
      renderFooterAction(transferHint, transferCap, d.transferKey, _ps);
      renderFooterAction(selectHint, selectCap, d.selectKey, _ps);

      const transferOnly = d.transferOnly === true;
      root.classList.toggle('transfer-only', transferOnly);

      if (transferOnly) {
        qlSub.textContent = 'Settlement';
        qlCount.textContent = '';
        qlList.innerHTML = '<div class="ql-transfer-msg"><span class="key-cap">R</span> Transfer</div>';
        renderExtras(d.activateChoices || [], _ps);
        renderPerkFooter(d.perkButton, d.transferKey, _ps);
        _items = [];
        updateRowPreviews();
      } else {
        qlSub.textContent = `${(d.totalWeight || 0).toFixed(1)} lbs inside`;
        qlCount.textContent = `${(d.items || []).length} items`;

        _items = d.items || [];
        if (typeof d.sel === 'number') _sel = d.sel;
        _sel = Math.max(0, Math.min(_sel, _items.length - 1));
        renderList();
        renderExtras(d.activateChoices || [], _ps);
        renderPerkFooter(d.perkButton, d.transferKey, _ps);

        const cw = d.playerCarryWeight || 0, cm = d.playerCarryMax || 200;
        const selItem = _items[_sel];
        const sw = selItem ? (selItem.weight || 0) * (selItem.count || 1) : 0;
        const proj = cw + sw;
        const pct = Math.min(100, (cw / cm) * 100);
        const projPct = Math.min(100, (proj / cm) * 100);
        carryFill.style.width = `${pct}%`;
        carryFill.className = `carry-fill${proj > cm ? ' over' : projPct >= 80 ? ' warn' : ''}`;
        carryGhost.style.left = `${pct}%`;
        carryGhost.style.width = `${Math.max(0, projPct - pct)}%`;
        carryVal.textContent = `${cw.toFixed(0)} / ${cm.toFixed(0)}${sw > 0 ? `  +${sw.toFixed(1)}` : ''}`;
        carryVal.style.color = proj > cm ? 'rgba(240,120,60,0.95)' : 'rgba(255,255,255,0.45)';

        const tv = _items.reduce((a, x) => a + (x.value || 0) * (x.count || 1), 0);
        totalVal.innerHTML = `<img class="caps-icon" src="icons/cat-caps.png" alt="">${tv}`;
      }
    }

    if (shouldShow !== _visState) {
      _visState = shouldShow;
      window.reportVisibility(JSON.stringify({id: 'prisma_quickloot', visible: shouldShow}));
      if (shouldShow) {
        setTimeout(() => updateRowPreviews(), 150);
        setTimeout(() => updateRowPreviews(), 400);
      }
    }
    if (!_preview) root.classList.toggle('visible', _visState);
    updateRowPreviews();
  } catch {}
};

window.setEditorPreview = v => {
  _preview = v === '1' || v === 1 || v === true;
  if (_preview) {
    qlIcon.className = 'ql-icon container'; qlIcon.textContent = '▣';
    qlName.textContent = 'Container'; qlSub.textContent = '12.4 lbs inside'; qlCount.textContent = '3 items';
    _items = [{name: 'Fusion Core', category: 'ammo', rarity: 'uncommon', count: 1, value: 200, weight: 4,
               powerArmorBattery: true, batteryChargePercent: 63},
              {name: 'Combat Armor', category: 'armo', rarity: 'uncommon', count: 1, value: 120, weight: 22.5, dr: 38, legendary: true, questItem: true},
              {name: 'Stimpak', category: 'aid', rarity: 'common', count: 2, value: 45, weight: 0.1},
              {name: 'Desk Fan', category: 'misc', rarity: 'common', count: 1, value: 6, weight: 3,
               components: [{name: 'Steel', count: 2}, {name: 'Screw', count: 2}, {name: 'Plastic', count: 1}]}];
    _sel = 0; renderList();
    carryFill.style.width = '62%'; carryFill.className = 'carry-fill'; carryVal.textContent = '124 / 200';
    totalVal.innerHTML = '<img class="caps-icon" src="icons/cat-caps.png" alt="">211';
    root.classList.add('visible');
  } else {
    root.classList.toggle('visible', _visState);
  }
};

window.navItem = json => {
  try {
    const d = JSON.parse(json);
    if (d.dir === 'up') { _sel = Math.max(0, _sel - 1); renderList(); }
    if (d.dir === 'down') { _sel = Math.min(_items.length - 1, _sel + 1); renderList(); }
    if (d.dir === 'take' && _items[_sel]) {
      window.takeItem(JSON.stringify({formId: _items[_sel].formId}));
    }
    if (d.dir === 'takeAll') window.takeAll('');
  } catch {}
};

const DEF = {xf: 1480 / 1920, yf: 360 / 1080};
const WIDGET_ID = 'prisma_quickloot';
let _posSet = false;
let _last = null, _applied = false;
function flushPos() {
  if (!_last || _applied) return;
  const W = window.innerWidth, H = window.innerHeight;
  if (!(W > 1) || !(H > 1)) return;
  root.style.left = `${_last.x * W}px`; root.style.top = `${_last.y * H}px`;
  root.style.setProperty('--s', _last.s); root.style.setProperty('--r', `${_last.r || 0}deg`);
  _applied = true;
  window.reportPosition(JSON.stringify({id: WIDGET_ID, x: _last.x, y: _last.y, s: _last.s, r: _last.r || 0}));
}
function applyFrac(p) {
  _last = p;
  _applied = false;
  flushPos();
}
try { new ResizeObserver(() => flushPos()).observe(document.documentElement); } catch {}
window.setHUDPosition = json => {
  _posSet = true;
  try {
    const p = JSON.parse(json);
    applyFrac({x: p.x, y: p.y, s: typeof p.s === 'number' ? p.s : 1});
  } catch {}
};
window.requestUpdate = window.requestUpdate || (() => {});
window.reportPosition = window.reportPosition || (() => {});
window.reportVisibility = window.reportVisibility || (() => {});
window.takeItem = window.takeItem || (j => console.log(`takeItem:${j}`));
window.takeAll = window.takeAll || (() => console.log('takeAll'));
window.activateChoice = window.activateChoice || (j => console.log(`activateChoice:${j}`));
setTimeout(() => { if (!_posSet) applyFrac({x: DEF.xf, y: DEF.yf, s: 1}); }, 50);