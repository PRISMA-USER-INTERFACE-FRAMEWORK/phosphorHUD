(function() {
  "use strict";
  window.hudSendToGame = function(name, json) {
    if (typeof window[name] === "function") window[name](json);
    else console.log("hudSendToGame: " + name + " " + json);
  };
  var LAYOUT_DEFAULT = { dx: 0, dy: 0, s: 1, hidden: false },
    LAYOUT_GROUPS = [
      { id: "vitals", label: "Vitals" },
      { id: "party", label: "Companions" },
      { id: "compass", label: "Compass" },
      { id: "kit", label: "Weapon and limbs" },
      { id: "feed", label: "Notification feed" },
      { id: "rcol", label: "Tracker and challenges" },
      { id: "msg", label: "Message" },
      { id: "target", label: "Enemy target" },
      { id: "hint", label: "Activation prompt" },
      { id: "loot", label: "Quick loot" },
      { id: "lvlup", label: "Level-up banner" },
      { id: "salert", label: "Settlement attack" },
      { id: "quest", label: "Quest banner" }
    ],
    CENTRED = { compass: 1, target: 1, hint: 1, lvlup: 1, quest: 1 },
    CAN_TRANSLATE = !!(window.CSS && CSS.supports && CSS.supports("translate", "1px")),
    layoutState = {},
    layoutListeners = [];
  if (!CAN_TRANSLATE) console.warn("Phosphor HUD layout is limited: CSS translate is not supported here");

  function clamp(v, lo, hi) {
    return v < lo ? lo : v > hi ? hi : v;
  }

  function layoutKnown(id) {
    return LAYOUT_GROUPS.some(function(g) {
      return g.id === id;
    });
  }

  function layoutEntry(v, base) {
    v = v && typeof v === "object" ? v : {};
    base = base || LAYOUT_DEFAULT;
    return {
      dx: typeof v.dx === "number" && isFinite(v.dx) ? clamp(v.dx, -1920, 1920) : base.dx,
      dy: typeof v.dy === "number" && isFinite(v.dy) ? clamp(v.dy, -1080, 1080) : base.dy,
      s: typeof v.s === "number" && isFinite(v.s) ? clamp(v.s, 0.5, 1.5) : base.s,
      hidden: typeof v.hidden === "boolean" ? v.hidden : base.hidden
    };
  }

  function layoutIsDefault(e) {
    return e.dx === 0 && e.dy === 0 && e.s === 1 && !e.hidden;
  }

  function layoutStore(id, e) {
    if (layoutIsDefault(e)) delete layoutState[id];
    else layoutState[id] = e;
  }

  function layoutRender() {
    LAYOUT_GROUPS.forEach(function(g) {
      var el = document.querySelector('[data-edit="' + g.id + '"]'),
        e = layoutState[g.id] || LAYOUT_DEFAULT;
      if (!el) return;
      if (CAN_TRANSLATE) {
        el.style.translate = e.dx || e.dy ? e.dx + "px " + e.dy + "px" : "";
        el.style.scale = e.s === 1 ? "" : String(e.s);
        el.style.transformOrigin = CENTRED[g.id] && e.s !== 1 ? "0 50%" : "";
      } else if (!CENTRED[g.id] && (el.style.transform || getComputedStyle(el).transform === "none")) {
        el.style.transform = e.dx || e.dy || e.s !== 1 ? "translate(" + e.dx + "px," + e.dy + "px) scale(" + e.s + ")" : "";
      }
      if (e.hidden) el.setAttribute("data-edit-hidden", "");
      else el.removeAttribute("data-edit-hidden");
    });
  }

  function layoutGet() {
    var groups = {};
    Object.keys(layoutState).forEach(function(id) {
      var e = layoutState[id];
      groups[id] = { dx: e.dx, dy: e.dy, s: e.s, hidden: e.hidden };
    });
    return { version: 1, groups: groups };
  }

  function layoutEmit() {
    var snap = layoutGet();
    layoutListeners.forEach(function(cb) {
      cb(snap);
    });
  }

  function layoutApply(layout) {
    if (typeof layout === "string") {
      try {
        layout = JSON.parse(layout);
      } catch (e) {
        return false;
      }
    }
    if (!layout || layout.version !== 1 || !layout.groups || typeof layout.groups !== "object") return false;
    layoutState = {};
    Object.keys(layout.groups).forEach(function(id) {
      if (layoutKnown(id)) layoutStore(id, layoutEntry(layout.groups[id]));
    });
    layoutRender();
    layoutEmit();
    return true;
  }

  function layoutSetGroup(id, partial) {
    if (!layoutKnown(id)) return false;
    layoutStore(id, layoutEntry(partial, layoutState[id] || LAYOUT_DEFAULT));
    layoutRender();
    layoutEmit();
    return true;
  }

  function layoutReset(id) {
    if (id === undefined || id === null) layoutState = {};
    else if (layoutKnown(id)) delete layoutState[id];
    else return false;
    layoutRender();
    layoutEmit();
    return true;
  }

  function layoutOnChange(cb) {
    if (typeof cb !== "function") return;
    layoutListeners.push(cb);
    cb(layoutGet());
  }

  window.hudLayout = {
    groups: LAYOUT_GROUPS,
    get: layoutGet,
    apply: layoutApply,
    setGroup: layoutSetGroup,
    reset: layoutReset,
    onChange: layoutOnChange,
    save: function() {
      window.hudSendToGame("saveLayout", JSON.stringify(layoutGet()));
    }
  };
  window.hudLayoutApply = layoutApply;
  if (window.hud) window.hud.layout = layoutApply;
})();
