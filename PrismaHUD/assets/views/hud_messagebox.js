const scrim = document.getElementById('scrim');
const mbHeader = document.getElementById('mbHeader');
const mbBody = document.getElementById('mbBody');
const mbButtons = document.getElementById('mbButtons');
const navHint = document.getElementById('navHint');
const confirmHint = document.getElementById('confirmHint');
const cancelHint = document.getElementById('cancelHint');

let _buttons = [];
let _sel = 0;
let _open = false;

function escH(s) {
  return String(s ?? '').replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
}

let _psStyle = false;
let _usingGamepad = false;

function neutralGlyph(canon) {
  const sym = { DUp: '▲', DDown: '▼', DLeft: '◄', DRight: '►', DPad: '✚' }[canon] || canon;
  return `<span class="mb-btnchip">${sym}</span>`;
}

function scaleformToHtml(raw) {
  if (!raw) return '';
  const glyphFonts = /\$?(controller_buttons|buttons)/i;

  let inGlyph = false;

  return String(raw).split(/(<[^>]*>)/g).map(tok => {
    if (!tok.startsWith('<')) {
      if (!inGlyph) return escH(tok);
      let out = '';
      for (const ch of tok) {
        if (!ch.trim()) continue;
        const c = CG.CHAR[ch];
        if (c) out += _usingGamepad ? CG.chip(c, _psStyle) : neutralGlyph(c);
      }
      return out;
    }

    const tag = tok.slice(1, -1).trim();
    const closing = tag.startsWith('/');
    const name = (tag.match(/^\/?\s*([a-zA-Z]+)/) || [])[1]?.toLowerCase();

    switch (name) {
      case 'br': return '<br>';
      case 'p':  return closing ? '<br>' : '';
      case 'b':  return closing ? '</strong>' : '<strong>';
      case 'i':  return closing ? '</em>' : '<em>';
      case 'font': {
        if (closing) {
          const wasGlyph = inGlyph;
          inGlyph = false;
          return wasGlyph ? '' : '</span>';
        }
        const face = (tag.match(/face\s*=\s*['"]([^'"]*)['"]/i) || [])[1] || '';
        if (glyphFonts.test(face)) { inGlyph = true; return ''; }
        return '<span>';
      }
      default: return '';
    }
  }).join('');
}

function render() {
  mbButtons.innerHTML = _buttons.map((label, i) =>
    `<div class="mb-btn${i === _sel ? ' sel' : ''}" data-idx="${i}">
       <span class="mb-idx">${i + 1}</span>${scaleformToHtml(label)}
     </div>`).join('');
}

function move(delta) {
  if (!_buttons.length) return;
  _sel = (_sel + delta + _buttons.length) % _buttons.length;
  render();
}

let _answered = false;
function answer(i) {
  if (!_open || _answered) return;
  if (i < 0 || i >= _buttons.length) return;
  _answered = true;
  window.mbAnswer(JSON.stringify({index: i}));
}

mbButtons.addEventListener('click', e => {
  const btn = e.target.closest('.mb-btn');
  if (btn) answer(Number(btn.dataset.idx));
});
mbButtons.addEventListener('mousemove', e => {
  const btn = e.target.closest('.mb-btn');
  if (!btn) return;
  const i = Number(btn.dataset.idx);
  if (i !== _sel) { _sel = i; render(); }
});

document.addEventListener('keydown', e => {
  if (!_open) return;
  switch (e.key) {
    case 'ArrowDown': case 's': move(1); break;
    case 'ArrowUp':   case 'w': move(-1); break;
    case 'Enter': case ' ': answer(_sel); break;
    case 'Escape': answer(_buttons.length - 1); break;
    default:
      if (e.key >= '1' && e.key <= '9') answer(Number(e.key) - 1);
      return;
  }
  e.preventDefault();
});

window.showMessageBox = json => {
  try {
    const d = JSON.parse(json);
    _buttons = Array.isArray(d.buttons) ? d.buttons : [];
    _sel = 0;
    _answered = false;
    _open = true;
    _psStyle = d.controllerStyle === 'playstation';
    _usingGamepad = d.usingGamepad === true;
    if (d.navHint)     navHint.innerHTML = CG.renderKey(d.navHint, _psStyle);
    if (d.confirmHint) confirmHint.innerHTML = CG.renderKey(d.confirmHint, _psStyle);
    if (d.cancelHint)  cancelHint.innerHTML = CG.renderKey(d.cancelHint, _psStyle);
    mbHeader.textContent = d.header || '';
    mbBody.innerHTML = scaleformToHtml(d.body);
    render();
    scrim.classList.add('visible');
  } catch {}
};

window.hideMessageBox = () => {
  _open = false;
  _answered = false;
  scrim.classList.remove('visible');
};

window.mbAnswer = window.mbAnswer || (j => console.log(`mbAnswer:${j}`));
window.mbTick   = window.mbTick   || (() => {});

setInterval(() => { if (_open) window.mbTick(''); }, 250);
