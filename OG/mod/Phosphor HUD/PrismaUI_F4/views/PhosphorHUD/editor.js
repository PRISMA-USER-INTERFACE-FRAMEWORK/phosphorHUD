(function () {
  "use strict";
  var VW = 1920, VH = 1080, MIN_W = 120, MIN_H = 60, MAX_UNDO = 50, KEEP = 0.4;
  var MOVES = {ArrowLeft: [-1, 0], ArrowRight: [1, 0], ArrowUp: [0, -1], ArrowDown: [0, 1]};
  var ALIGN = [["L", "Left"], ["R", "Right"], ["T", "Top"], ["B", "Bottom"], ["HC", "H-ctr"], ["VC", "V-ctr"], ["C", "Centre"], ["DH", "Dist H"], ["DV", "Dist V"]];
  var PE = !!window.PointerEvent, EV = PE ? {down: "pointerdown", move: "pointermove", up: "pointerup", cancel: "pointercancel"} : {down: "mousedown", move: "mousemove", up: "mouseup", cancel: "mouseup"};
  var S = {open: false, built: false, sel: [], undo: [], redo: [], snapN: 10, base: "", drag: null};
  var stage, overlay, root, boxes = {}, rows = {}, labels = {}, ui = {};

  function $(id) { return document.getElementById(id); }
  function each(sel, fn) { Array.prototype.forEach.call(root.querySelectorAll(sel), fn); }
  function selected(id) { return S.sel.indexOf(id) !== -1; }
  function allIds() { return hudLayout.groups.map(function (gr) { return gr.id; }); }
  function r2(v) { return Math.round(v * 100) / 100; }
  function clampS(v) { return Math.min(1.5, Math.max(0.5, v)); }
  function clampX(v, w) { return Math.min(VW - KEEP * w, Math.max((KEEP - 1) * w, v)); }
  function clampY(v, h) { return Math.min(VH - KEEP * h, Math.max((KEEP - 1) * h, v)); }
  function snapV(v) { return S.snapN ? Math.round(v / S.snapN) * S.snapN : v; }
  function ext(its, f) { return its.map(function (it) { return f(it.b); }); }
  function avg(a) { return a.reduce(function (p, c) { return p + c; }, 0) / a.length; }
  function parentOf(id) {
    var p = document.querySelector('[data-edit="' + id + '"]').parentElement.closest("[data-edit]");
    return p ? p.getAttribute("data-edit") : null;
  }
  function topSel(ids) {
    return ids.filter(function (id) {
      var p = parentOf(id);
      return !p || ids.indexOf(p) === -1;
    });
  }
  function parentScale(id) {
    var p = parentOf(id);
    return p ? cur(p).s : 1;
  }
  function px(v) { return /px$/.test(v) ? parseFloat(v) : 0; }

  function cur(id) {
    var g = (hudLayout.get().groups || {})[id] || {};
    return {
      dx: typeof g.dx === "number" ? g.dx : 0,
      dy: typeof g.dy === "number" ? g.dy : 0,
      s: typeof g.s === "number" ? g.s : 1,
      hidden: g.hidden === true
    };
  }
  function setG(id, patch) {
    var g = cur(id), n = {dx: g.dx, dy: g.dy, s: g.s, hidden: g.hidden};
    for (var k in patch) n[k] = patch[k];
    hudLayout.setGroup(id, n);
  }

  function scaleK() { return stage.getBoundingClientRect().width / VW || 1; }

  function measure(el) {
    var r = el.getBoundingClientRect(), css;
    if (r.width || r.height) return r;
    css = el.style.cssText;
    el.style.setProperty("display", "block", "important");
    r = el.getBoundingClientRect();
    el.style.cssText = css;
    return r;
  }

  function boxOf(id) {
    var g = cur(id), el = document.querySelector('[data-edit="' + id + '"]'), k = scaleK();
    var sr = stage.getBoundingClientRect(), r = measure(el), par, pb, cs, w, h;
    if (r.width || r.height) {
      return {left: (r.left - sr.left) / k, top: (r.top - sr.top) / k, w: Math.max(r.width / k, MIN_W), h: Math.max(r.height / k, MIN_H), dx: g.dx, dy: g.dy};
    }
    par = el.parentElement.closest("[data-edit]");
    pb = par ? boxOf(par.getAttribute("data-edit")) : null;
    if (pb) return {left: pb.left + g.dx, top: pb.top + g.dy, w: MIN_W * g.s, h: MIN_H * g.s, dx: g.dx, dy: g.dy};
    cs = getComputedStyle(el);
    w = (px(cs.width) || MIN_W) * g.s;
    h = (px(cs.height) || MIN_H) * g.s;
    return {left: (VW - w) / 2 + g.dx, top: (VH - h) / 2 + g.dy, w: w, h: h, dx: g.dx, dy: g.dy};
  }

  function moveTo(id, b, left, top) {
    var f = parentScale(id);
    setG(id, {dx: b.dx + (clampX(left, b.w) - b.left) / f, dy: b.dy + (clampY(top, b.h) - b.top) / f});
  }

  function select(id, multi) {
    if (!id) S.sel = [];
    else if (!multi) S.sel = [id];
    else S.sel = selected(id) ? S.sel.filter(function (x) { return x !== id; }) : S.sel.concat([id]);
    refresh();
  }

  function push(stack, entry) {
    stack.push(entry);
    if (stack.length > MAX_UNDO) stack.shift();
  }
  function snapshot(ids) { return ids.map(function (id) { return {id: id, g: cur(id)}; }); }
  function pushUndo(ids) {
    if (!ids.length) return;
    push(S.undo, snapshot(ids));
    S.redo = [];
  }
  function swap(from, to) {
    if (!from.length) return;
    var entry = from.pop();
    push(to, snapshot(entry.map(function (e) { return e.id; })));
    entry.forEach(function (e) { hudLayout.setGroup(e.id, e.g); });
  }
  function doUndo() { swap(S.undo, S.redo); }
  function doRedo() { swap(S.redo, S.undo); }

  function resetIds(ids) {
    if (!ids.length) return;
    pushUndo(ids);
    ids.forEach(function (id) { hudLayout.reset(id); });
  }
  function resetAll() {
    pushUndo(allIds());
    hudLayout.reset();
  }

  function distribute(its, horiz) {
    var k = horiz ? "left" : "top", n = horiz ? "w" : "h";
    var sorted = its.slice().sort(function (a, c) { return a.b[k] - c.b[k]; });
    var first = sorted[0].b, last = sorted[sorted.length - 1].b, total = 0;
    sorted.forEach(function (it) { total += it.b[n]; });
    var gap = (last[k] + last[n] - first[k] - total) / (sorted.length - 1), at = first[k];
    sorted.forEach(function (it) {
      moveTo(it.id, it.b, horiz ? at : it.b.left, horiz ? it.b.top : at);
      at += it.b[n] + gap;
    });
  }

  function alignSel(kind) {
    var ids = topSel(S.sel), multi = ids.length > 1, dist = kind === "DH" || kind === "DV";
    if (!ids.length || (dist && !multi)) return;
    var its = ids.map(function (id) { return {id: id, b: boxOf(id)}; });
    pushUndo(ids);
    if (dist) { distribute(its, kind === "DH"); return; }
    var minL = Math.min.apply(null, ext(its, function (b) { return b.left; })),
      maxR = Math.max.apply(null, ext(its, function (b) { return b.left + b.w; })),
      minT = Math.min.apply(null, ext(its, function (b) { return b.top; })),
      maxB = Math.max.apply(null, ext(its, function (b) { return b.top + b.h; })),
      cx = avg(ext(its, function (b) { return b.left + b.w / 2; })),
      cy = avg(ext(its, function (b) { return b.top + b.h / 2; }));
    its.forEach(function (it) {
      var b = it.b, L = b.left, T = b.top;
      switch (kind) {
        case "L": L = multi ? minL : 0; break;
        case "R": L = (multi ? maxR : VW) - b.w; break;
        case "T": T = multi ? minT : 0; break;
        case "B": T = (multi ? maxB : VH) - b.h; break;
        case "HC": L = (multi ? cx : VW / 2) - b.w / 2; break;
        case "VC": T = (multi ? cy : VH / 2) - b.h / 2; break;
        case "C": L = (VW - b.w) / 2; T = (VH - b.h) / 2; break;
      }
      moveTo(it.id, b, L, T);
    });
  }

  function startDrag(e, id, handle) {
    var ids = handle ? [id] : topSel([id].concat(S.sel.filter(function (x) { return x !== id; })));
    S.drag = {scale: handle, x: e.clientX, y: e.clientY, k: scaleK(), pushed: false,
      items: ids.map(function (x) { return {id: x, b: boxOf(x), s: cur(x).s}; })};
    window.addEventListener(EV.move, onDragMove);
    window.addEventListener(EV.up, endDrag);
    window.addEventListener(EV.cancel, endDrag);
  }
  function endDrag() {
    S.drag = null;
    window.removeEventListener(EV.move, onDragMove);
    window.removeEventListener(EV.up, endDrag);
    window.removeEventListener(EV.cancel, endDrag);
  }
  function onDragMove(e) {
    var d = S.drag;
    if (!d) return;
    if (!d.pushed) { pushUndo(d.items.map(function (it) { return it.id; })); d.pushed = true; }
    var dx = (e.clientX - d.x) / d.k, dy = (e.clientY - d.y) / d.k, p = d.items[0];
    if (d.scale) {
      var f = 1 + (dx + dy) / (p.b.w + p.b.h);
      setG(p.id, {s: clampS(r2(p.s * f))});
      return;
    }
    if (S.snapN) { dx = snapV(p.b.left + dx) - p.b.left; dy = snapV(p.b.top + dy) - p.b.top; }
    d.items.forEach(function (it) { moveTo(it.id, it.b, it.b.left + dx, it.b.top + dy); });
  }

  function onOverlayDown(e) {
    if (!S.open || e.button !== 0) return;
    var box = e.target.closest(".hed-box");
    if (!box) { if (!e.shiftKey) select(null, false); return; }
    var id = box.getAttribute("data-hed"), handle = e.target.classList.contains("hed-h");
    if (e.shiftKey && !handle) select(id, true);
    else if (!selected(id)) select(id, false);
    if (!selected(id)) return;
    e.preventDefault();
    startDrag(e, id, handle);
  }

  function onKey(e) {
    if (!S.open) return;
    if (e.key === "Escape") { closeEditor(); return; }
    var t = e.target, typing = t && /^(INPUT|SELECT|TEXTAREA)$/.test(t.tagName);
    if (typing) return;
    var ctrl = e.ctrlKey || e.metaKey, k = e.key;
    if (ctrl && (k === "z" || k === "Z")) { e.preventDefault(); doUndo(); return; }
    if (ctrl && (k === "y" || k === "Y")) { e.preventDefault(); doRedo(); return; }
    var mv = MOVES[k], step = e.shiftKey ? 10 : 1, ids = topSel(S.sel);
    if (!mv || !ids.length) return;
    e.preventDefault();
    pushUndo(ids);
    ids.forEach(function (id) {
      var b = boxOf(id);
      moveTo(id, b, b.left + mv[0] * step, b.top + mv[1] * step);
    });
  }

  function setField(inp, on, val) {
    inp.disabled = !on;
    if (document.activeElement !== inp) inp.value = on ? val : "";
  }
  function fieldChange(key, raw) {
    var id = S.sel[0], v = parseFloat(raw);
    if (S.sel.length !== 1 || isNaN(v)) { refresh(); return; }
    pushUndo([id]);
    var patch = {};
    patch[key] = key === "s" ? clampS(r2(v)) : v;
    setG(id, patch);
  }

  function refresh() {
    if (!S.open) return;
    hudLayout.groups.forEach(function (gr) {
      var id = gr.id, g = cur(id), b = boxOf(id), el = boxes[id], r = rows[id], on = selected(id);
      el.style.left = b.left + "px";
      el.style.top = b.top + "px";
      el.style.width = b.w + "px";
      el.style.height = b.h + "px";
      el.className = "hed-box" + (on ? " sel" : "") + (g.hidden ? " off" : "");
      el.querySelector(".hed-pos").textContent = Math.round(g.dx) + ", " + Math.round(g.dy) + "  " + r2(g.s).toFixed(2) + "x";
      r.row.classList.toggle("sel", on);
      r.hide.checked = g.hidden;
    });
    var one = S.sel.length === 1 ? S.sel[0] : null, g1 = one ? cur(one) : null;
    ui.name.textContent = one ? labels[one] : S.sel.length ? S.sel.length + " selected" : "Nothing selected";
    setField(ui.dx, one, one ? r2(g1.dx) : "");
    setField(ui.dy, one, one ? r2(g1.dy) : "");
    setField(ui.s, one, one ? g1.s.toFixed(2) : "");
    ui.undo.disabled = !S.undo.length;
    ui.redo.disabled = !S.redo.length;
    each("[data-al]", function (b) {
      var dist = b.getAttribute("data-al") === "DH" || b.getAttribute("data-al") === "DV";
      b.disabled = !S.sel.length || (dist && topSel(S.sel).length < 2);
    });
  }

  function markup() {
    var rowsHtml = hudLayout.groups.map(function (gr) {
      return '<div class="hed-row" data-row="' + gr.id + '"><input type="checkbox" title="Hide"><span>' + gr.label + '</span></div>';
    }).join("");
    var alignHtml = ALIGN.map(function (a) { return '<button data-al="' + a[0] + '">' + a[1] + '</button>'; }).join("");
    return '<div class="hed-bar">' +
        '<div class="hed-brand">Phosphor <span>HUD</span> layout</div>' +
        '<span class="hed-hint">Drag to move &middot; corner to scale &middot; arrows nudge &middot; shift-click multi-select</span>' +
        '<div class="hed-spacer"></div>' +
        '<select id="hedSnap" title="Snap grid">' +
          '<option value="0">Snap off</option><option value="5">Snap 5</option>' +
          '<option value="10" selected>Snap 10</option><option value="20">Snap 20</option></select>' +
        '<button id="hedUndo" title="Undo (Ctrl+Z)">Undo</button>' +
        '<button id="hedRedo" title="Redo (Ctrl+Y)">Redo</button>' +
        '<button id="hedResetSel">Reset sel</button>' +
        '<button id="hedResetAll">Reset all</button>' +
        '<button id="hedDock" title="Move the panel to the other side">Dock</button>' +
        '<button id="hedSave">Save</button>' +
        '<button id="hedCancel" title="Close and discard changes since opening">Cancel</button>' +
        '<button id="hedDone" class="pri" title="Save and close (Esc)">Done</button>' +
      '</div>' +
      '<div class="hed-align"><span class="hed-al-label">Align</span>' + alignHtml + '</div>' +
      '<div class="hed-panel">' +
        '<div class="hed-ph">Groups</div>' +
        '<div class="hed-pb hed-list">' + rowsHtml + '</div>' +
        '<div class="hed-ph">Selected</div>' +
        '<div class="hed-pb">' +
          '<div class="hed-name" id="hedName"></div>' +
          '<div class="hed-fields">' +
            '<div class="hed-field"><label>X</label><input id="hedDx" type="number" step="1"></div>' +
            '<div class="hed-field"><label>Y</label><input id="hedDy" type="number" step="1"></div>' +
            '<div class="hed-field"><label>Scale</label><input id="hedS" type="number" step="0.05" min="0.5" max="1.5"></div>' +
          '</div>' +
        '</div>' +
        '<div class="hed-pb"><div class="hed-name" id="hedSaved"></div></div>' +
      '</div>';
  }

  function build() {
    stage = $("stage");
    overlay = document.createElement("div");
    overlay.id = "hedOverlay";
    overlay.className = "hed-overlay";
    hudLayout.groups.forEach(function (gr) {
      var el = document.createElement("div");
      el.className = "hed-box";
      el.setAttribute("data-hed", gr.id);
      el.innerHTML = '<span class="hed-label">' + gr.label + '</span><span class="hed-pos"></span><span class="hed-h"></span>';
      overlay.appendChild(el);
      boxes[gr.id] = el;
      labels[gr.id] = gr.label;
    });
    stage.appendChild(overlay);
    root = document.createElement("div");
    root.id = "hedRoot";
    root.className = "hed-root";
    root.innerHTML = markup();
    document.body.appendChild(root);

    ui.name = $("hedName");
    ui.dx = $("hedDx");
    ui.dy = $("hedDy");
    ui.s = $("hedS");
    ui.undo = $("hedUndo");
    ui.redo = $("hedRedo");
    ui.saved = $("hedSaved");

    each("[data-row]", function (r) {
      var id = r.getAttribute("data-row"), hide = r.querySelector("input");
      rows[id] = {row: r, hide: hide};
      r.classList.toggle("hed-child", !!parentOf(id));
      r.addEventListener("click", function (e) { if (e.target !== hide) select(id, e.shiftKey); });
      hide.addEventListener("change", function () { pushUndo([id]); setG(id, {hidden: hide.checked}); });
    });
    each("[data-al]", function (b) {
      b.addEventListener("click", function () { alignSel(b.getAttribute("data-al")); });
    });
    ui.dx.addEventListener("change", function () { fieldChange("dx", ui.dx.value); });
    ui.dy.addEventListener("change", function () { fieldChange("dy", ui.dy.value); });
    ui.s.addEventListener("change", function () { fieldChange("s", ui.s.value); });
    $("hedSnap").addEventListener("change", function (e) { S.snapN = parseInt(e.target.value, 10) || 0; });
    ui.undo.addEventListener("click", doUndo);
    ui.redo.addEventListener("click", doRedo);
    $("hedResetSel").addEventListener("click", function () { resetIds(S.sel.slice()); });
    $("hedResetAll").addEventListener("click", resetAll);
    $("hedSave").addEventListener("click", function () {
      S.base = JSON.stringify(hudLayout.get());
      hudLayout.save();
      ui.saved.textContent = "Saved";
      setTimeout(function () { ui.saved.textContent = ""; }, 2000);
    });
    $("hedDock").addEventListener("click", function () { root.classList.toggle("left"); });
    $("hedCancel").addEventListener("click", function () { closeEditor(true); });
    $("hedDone").addEventListener("click", function () { closeEditor(); });

    overlay.addEventListener(EV.down, onOverlayDown);
    document.addEventListener("keydown", onKey);
    window.addEventListener("resize", refresh);
    hudLayout.onChange(refresh);
  }

  function notifyClosed() {
    if (window.hudSendToGame) window.hudSendToGame("editorClosed", "");
  }
  function openEditor() {
    if (S.open) return;
    if (!window.hudLayout || !$("stage")) { notifyClosed(); return; }
    if (!S.built) { build(); S.built = true; }
    S.open = true;
    S.base = JSON.stringify(hudLayout.get());
    S.undo = [];
    S.redo = [];
    S.sel = [];
    overlay.classList.add("open");
    root.classList.add("open");
    stage.classList.add("editing");
    refresh();
  }
  function closeEditor(discard) {
    if (!S.open) return;
    S.open = false;
    endDrag();
    if (discard === true) hudLayout.apply(S.base);
    else if (JSON.stringify(hudLayout.get()) !== S.base) hudLayout.save();
    overlay.classList.remove("open");
    root.classList.remove("open");
    stage.classList.remove("editing");
    notifyClosed();
  }

  window.hudEditor = {
    open: openEditor,
    close: closeEditor,
    toggle: function () { if (S.open) closeEditor(); else openEditor(); },
    isOpen: function () { return S.open; }
  };
  window.hudEditorOpen = openEditor;
  window.hudEditorClose = closeEditor;
  window.hudEditorToggle = window.hudEditor.toggle;
})();
