(function() {
  "use strict";
  var LOCAL = location.protocol === "file:" || /Ultralight/i.test(navigator.userAgent);
  /* open hud.html#preview from disk to see the demo with your local portraits */
  var IN_GAME = LOCAL && location.hash !== "#preview";
  if (IN_GAME) document.documentElement.classList.add("ingame");
  var reduce = window.matchMedia && matchMedia("(prefers-reduced-motion: reduce)").matches;
  var $ = function(id) {
    return document.getElementById(id);
  };
  var stage = $("stage"),
    vp = $("viewport");

  /* ---------- scaling ---------- */
  function fit() {
    var w = vp.clientWidth,
      h = vp.clientHeight;
    if (!w || !h) return;
    var s = Math.min(w / 1920, h / 1080);
    stage.style.setProperty("--wide-gutter", Math.max(0, (w / s - 1920) / 2) + "px");
    stage.style.transform = "translate(" + (w - 1920 * s) / 2 + "px," + (h - 1080 * s) / 2 + "px) scale(" + s + ")";
    if (typeof layoutCoresSoon === "function") layoutCoresSoon();
  }
  if (window.ResizeObserver) new ResizeObserver(fit).observe(vp);
  window.addEventListener("resize", fit);
  fit();

  /* ---------- state ---------- */
  var S = {
    hp: 212,
    hpMax: 310,
    ap: 64,
    apMax: 90,
    rads: 140,
    level: 23,
    xp: 0.72,
    heading: 20,
    markers: [{
        id: "q1",
        t: "door",
        b: 42,
        dist: 142,
        dz: 12,
        label: "Relay Tower",
        active: true
      },
      {
        id: "q2",
        t: "quest",
        b: 200,
        dist: 1830,
        label: "Salt and Rust",
        active: false
      },
      {
        id: "sanc",
        t: "settlement",
        b: -35,
        dist: 90,
        label: "Sanctuary Hills",
        disc: true
      },
      {
        id: "vault",
        t: "vault",
        b: -68,
        dist: 780,
        dz: -15,
        label: "Vault 111",
        disc: true
      },
      {
        id: "mill",
        t: "factory",
        b: -12,
        dist: 520,
        label: "Old Mill",
        disc: false
      },
      {
        id: "rr",
        t: "landmark",
        b: 150,
        dist: 640,
        label: "Red Rocket",
        disc: true
      },
      {
        id: "concord",
        t: "city",
        b: 95,
        dist: 1200,
        label: "Concord",
        disc: false
      },
      {
        id: "cave",
        t: "cave",
        b: -112,
        dist: 410,
        label: "Cave",
        disc: false
      },
      {
        id: "comp",
        t: "ally",
        b: -160,
        dist: 6,
        label: "Companion"
      },
      {
        id: "e1",
        t: "enemy",
        b: 118
      }
    ],
    clip: 9,
    clipMax: 12,
    reserve: 216,
    weapon: "10mm Pistol",
    grenades: 8,
    needs: {
      food: 0.7,
      water: 0.45,
      rest: 0.24
    },
    limbs: {
      head: 1,
      torso: 1,
      larm: 1,
      rarm: 0.6,
      lleg: 0,
      rleg: 1
    },
    combat: false,
    target: null,
    loot: null,
    explosive: {
      name: "Frag Grenade",
      short: "FRAG"
    },
    time: {
      hour: 21.7,
      timescale: 20,
      h24: false,
      set: false
    },
    companion: null,
    tracker: null
  };

  function esc(s) {
    return String(s).replace(/[&<>"]/g, function(c) {
      return {
        "&": "&amp;",
        "<": "&lt;",
        ">": "&gt;",
        '"': "&quot;"
      } [c];
    });
  }

  function clamp(v, a, b) {
    return Math.max(a, Math.min(b, v));
  }

  function wake(el) {
    var list = el.hasAttribute("data-dim") ? [el] : el.querySelectorAll("[data-dim]");
    Array.prototype.forEach.call(list, function(n) {
      n.classList.add("awake");
      clearTimeout(n._t);
      n._t = setTimeout(function() {
        n.classList.remove("awake");
        renderContext();
      }, 5000);
    });
    if (S.settings) renderContext();
  }

  function pop(parent, text, cls) {
    var p = document.createElement("span");
    p.className = "pop" + (cls ? " " + cls : "");
    p.textContent = text;
    parent.appendChild(p);
    setTimeout(function() {
      p.remove();
    }, 1700);
  }

  /* ---------- vitals ---------- */
  function setBar(fill, ghost, frac, prop) {
    var prev = parseFloat(fill.dataset.v);
    if (isNaN(prev)) prev = frac;
    fill.style[prop] = frac * 100 + "%";
    if (frac < prev) {
      ghost.style.transition = "none";
      ghost.style[prop] = prev * 100 + "%";
      void ghost.offsetWidth;
      setTimeout(function() {
        ghost.style.transition = "";
        ghost.style[prop] = frac * 100 + "%";
      }, 380);
    } else ghost.style[prop] = frac * 100 + "%";
    fill.dataset.v = frac;
  }
  /* ---------- RDR2-style cores ---------- */
  var KICON = {
    hp: '<path d="M9.6 4h4.8v5.6H20v4.8h-5.6V20H9.6v-5.6H4V9.6h5.6z"/>',
    ap: '<path d="M13.6 3L6 13.4h5.2L10.2 21 18 10.4h-5.3z"/>',
    rad: (function() {
      var d = '<circle cx="12" cy="12" r="1.7"/>';
      [-90, 30, 150].forEach(function(a) {
        var r0 = 3.4,
          r1 = 9.2,
          a0 = (a - 30) * Math.PI / 180,
          a1 = (a + 30) * Math.PI / 180;

        function pt(r, t) {
          return (12 + r * Math.cos(t)).toFixed(2) + " " + (12 + r * Math.sin(t)).toFixed(2);
        }
        d += '<path d="M' + pt(r0, a0) + 'L' + pt(r1, a0) + 'A' + r1 + ' ' + r1 + ' 0 0 1 ' + pt(r1, a1) + 'L' + pt(r0, a1) + 'A' + r0 + ' ' + r0 + ' 0 0 0 ' + pt(r0, a0) + 'z"/>';
      });
      return d;
    })(),
    food: '<path d="M8 4v6c0 1.2.9 2 2 2v8M10 4v5M12 4v6c0 1.2-.9 2-2 2M16.5 4c-1.6 0-2.5 2-2.5 5.5s1 3 2.5 3V20"/>',
    water: '<path d="M12 3.5c3.4 4.3 5.5 7.2 5.5 10.2a5.5 5.5 0 0 1-11 0c0-3 2.1-5.9 5.5-10.2z"/>',
    rest: '<path d="M15 4.5a8 8 0 1 0 4.8 11.7A7 7 0 0 1 15 4.5z"/>',
    wt: '<path d="M7.5 9h9a1.8 1.8 0 0 1 1.8 1.6L19.5 20h-15l1.2-9.4A1.8 1.8 0 0 1 7.5 9z"/><path d="M9.5 9V7.5a2.5 2.5 0 0 1 5 0V9"/>'
  };
  var KLABEL = {
    hp: "HP",
    ap: "AP",
    rad: "RAD",
    food: "Food",
    water: "Water",
    rest: "Rest",
    wt: "Weight"
  };
  /* Segmented rings drawn as plain arcs: Ultralight (PrismaUI) has no SVG masks or pathLength.
     A ring is n segments; each layer (track, ghost, value, bite) is n arc paths clipped to a range. */
  function arcD(cx, r, a0, a1) {
    var t0 = (a0 - 90) * Math.PI / 180,
      t1 = (a1 - 90) * Math.PI / 180;
    return "M" + (cx + r * Math.cos(t0)).toFixed(2) + " " + (cx + r * Math.sin(t0)).toFixed(2) +
      "A" + r + " " + r + " 0 " + (a1 - a0 > 180 ? 1 : 0) + " 1 " + (cx + r * Math.cos(t1)).toFixed(2) + " " + (cx + r * Math.sin(t1)).toFixed(2);
  }

  function ringLayer(cls, cx, r, n, gap) {
    var h = "";
    for (var k = 0; k < n; k++) h += '<path class="' + cls + '" d=""/>';
    return '<g data-ring="' + [cx, r, n, gap].join(",") + '">' + h + "</g>";
  }
  /* show the layer from fraction lo to hi (0..1, clockwise from the top) */
  function setRing(g, lo, hi) {
    if (g._lo === lo && g._hi === hi) return;
    g._lo = lo;
    g._hi = hi;
    var p = g.getAttribute("data-ring").split(",").map(Number),
      cx = p[0],
      r = p[1],
      n = p[2],
      gap = p[3];
    var paths = g.children,
      step = 360 / n,
      a = lo * 360,
      b = hi * 360;
    for (var k = 0; k < n; k++) {
      var s0 = k * step + gap / 2,
        s1 = (k + 1) * step - gap / 2;
      var x0 = Math.max(s0, a),
        x1 = Math.min(s1, b);
      paths[k].setAttribute("d", x1 - x0 > 0.05 ? arcD(cx, r, x0, x1) : "");
    }
  }
  var cores = {};

  function buildCore(key) {
    var el = $("k-" + key),
      sm = el.classList.contains("sm"),
      segs = sm ? 10 : 20,
      gap = sm ? 8.6 : 3.2;
    var circ = function(cls, extra) {
      return '<circle class="' + cls + '" cx="32" cy="32" r="28" pathLength="100"' + (extra || "") + '/>';
    };
    el.innerHTML =
      '<svg viewBox="0 0 64 64">' +
      '<circle class="disc" cx="32" cy="32" r="24.5"/>' +
      ringLayer("trk", 32, 28, segs, gap) + ringLayer("ghost", 32, 28, segs, gap) + ringLayer("val", 32, 28, segs, gap) +
      (key === "hp" || key === "ap" ? ringLayer("bite", 32, 28, segs, gap) : "") +
      '<circle class="fort" cx="32" cy="32" r="31.5" stroke-dasharray="2 2.6"/>' +
      '<clipPath id="kc-' + key + '"><rect class="clip" x="-2" y="-1" width="28" height="26"/></clipPath>' +
      '<g transform="translate(19 19) scale(1.083)">' +
      '<g class="ic dim">' + KICON[key] + '</g>' +
      '<g class="ic lit' + (key === "hp" || key === "ap" || key === "rad" ? " solid" : "") + '" clip-path="url(#kc-' + key + ')" style="clip-path:url(#kc-' + key + ')">' + KICON[key] + '</g>' +
      '</g>' +
      '</svg>' +
      '<small>' + KLABEL[key] + '<b></b></small>';
    var layer = function(c) {
      var p = el.querySelector("." + c);
      return p ? p.parentNode : null;
    };
    setRing(layer("trk"), 0, 1);
    cores[key] = {
      el: el,
      val: layer("val"),
      ghost: layer("ghost"),
      bite: layer("bite"),
      clip: el.querySelector(".clip"),
      num: el.querySelector("small b"),
      v: null
    };
  }
  ["hp", "ap", "rad", "food", "water", "rest", "wt"].forEach(buildCore);

  function setCore(key, frac, num, fill) {
    var c = cores[key];
    frac = clamp(frac, 0, 1);
    setRing(c.val, 0, frac);
    /* the lost part lingers as a pale ghost for a moment */
    if (c.v !== null && frac < c.v) {
      var g = c.ghost;
      setRing(g, 0, c.v);
      clearTimeout(g._t);
      g._t = setTimeout(function() {
        setRing(g, 0, frac);
      }, 450);
    } else if (frac !== c.v) setRing(c.ghost, 0, frac);
    c.v = frac;
    /* inner core: the icon empties from the top as the value drops */
    var f = clamp(fill === undefined ? 1 : fill, 0, 1),
      h = 26 * f;
    if (c.fillH !== h) {
      c.fillH = h;
      c.clip.setAttribute("y", (-1 + 26 - h).toFixed(2));
      c.clip.setAttribute("height", h.toFixed(2));
    }
    if (num !== undefined && c.num.textContent !== String(num)) c.num.textContent = num;
  }

  function renderVitals() {
    var radFrac = clamp(S.rads / 1000, 0, 1);
    S.hp = clamp(S.hp, 0, S.hpMax * (1 - radFrac));
    var hpF = S.hp / S.hpMax;
    setCore("hp", hpF, Math.round(S.hp));
    setRing(cores.hp.bite, 1 - radFrac, 1);
    cores.hp.el.classList.toggle("low", hpF < 0.25);
    /* survival Fatigue works like radiation for AP: it takes a red bite out of the end of the ring */
    var fat = S.survival.on ? clamp(S.survival.fatigue, 0, 0.9) : 0;
    S.ap = clamp(S.ap, 0, S.apMax * (1 - fat));
    setCore("ap", S.ap / S.apMax, Math.round(S.ap));
    setRing(cores.ap.bite, 1 - fat, 1);
    cores.ap.el.classList.toggle("low", S.ap / S.apMax < 0.15);
    setCore("rad", radFrac > 0 ? Math.max(0.03, radFrac) : 0, Math.round(S.rads), radFrac);
    cores.rad.el.classList.toggle("crit", radFrac > 0.6);
    renderVitalVisibility();
  }
  /* Ring size / text size: keep the vitals row clear of the compass. Measure the single-row layout and, if its right
     edge would come within 16 design px of the compass, lift food/water/rest onto their own row above the rings. */
  function layoutCores() {
    var row = $("cores"),
      comp = $("compass");
    if (!row || !comp) return;
    var k = (stage.getBoundingClientRect().width / 1920) || 1;
    row.classList.remove("two-rows");
    var right = -Infinity;
    [].forEach.call(row.children, function(c) {
      if (c.id === "needsGap" || c.classList.contains("away") || getComputedStyle(c).display === "none") return;
      right = Math.max(right, c.getBoundingClientRect().right);
    });
    row.classList.toggle("two-rows", right > comp.getBoundingClientRect().left - 16 * k);
  }
  var coresTimer = 0;

  function layoutCoresSoon() {
    layoutCores();
    clearTimeout(coresTimer);
    coresTimer = setTimeout(layoutCores, 500); /* after the rings' .45 s width transitions settle */
  }
  var visKey = null;

  function renderVitalVisibility() {
    var dynamic = S.settings.hideFullVitals;
    var hpCap = S.hpMax * (1 - clamp(S.rads / 1000, 0, 1));
    var apCap = S.apMax * (1 - (S.survival.on ? clamp(S.survival.fatigue, 0, 0.9) : 0));
    // The native snapshot rounds HP/AP to whole points; compare at the same precision.
    var hideHP = dynamic && S.hp > 0 && hpCap > 0 && Math.round(S.hp) >= Math.round(hpCap);
    var hideAP = dynamic && S.ap > 0 && apCap > 0 && Math.round(S.ap) >= Math.round(apCap);
    var hideRad = dynamic && S.rads <= 0;
    cores.hp.el.classList.toggle("vital-hidden", hideHP);
    cores.ap.el.classList.toggle("vital-hidden", hideAP);
    cores.rad.el.classList.toggle("vital-hidden", hideRad);
    $("cores").classList.toggle("vitals-empty", hideHP && hideAP && hideRad);
    var key = [hideHP, hideAP, hideRad].join();
    if (key !== visKey) {
      visKey = key;
      layoutCoresSoon();
    }
  }

  function renderFortify() {
    cores.hp.el.classList.toggle("fortified", hasEffect("buffout"));
    cores.ap.el.classList.toggle("fortified", hasEffect("nuka") || hasEffect("jet"));
    cores.rad.el.classList.toggle("fortified", hasEffect("radx"));
  }

  function renderXP() {
    $("lvl").textContent = S.level;
    $("xpFill").style.width = S.xp * 100 + "%";
    $("xpPct").textContent = Math.round(S.xp * 100) + "%";
  }
  /* Warzone - Armor Plates System (optional mod, LuBuCake). The plugin sends `armor` only while that mod's esp is
     loaded, so without it this core is never built. Option A of the armor design: one arc per plate on a 64px ring
     with the armor value and plates carried underneath. Sizes are in ring units (circumference 2*pi*26). */
  var PLATE_RING = 2 * Math.PI * 26,
    PLATE_GAP = 3,
    armorCore = null,
    armorLast = null;
  /* Same 24-unit space and dim/lit wrapper as the KICON glyphs (the in-game renderer drew those but not a bare
     64-unit glyph), with every number separated so a strict path parser reads it. */
  var PLATE_ICON = '<path d="M5.5 4.6 H18.4 L20.3 7.4 V15.7 L12 21.2 L3.7 15.7 V7.4 Z"/>' +
    '<path d="M8.3 10.2 H15.7 M8.3 13.9 H15.7"/>';
  /* the broken state's crack (Option A design), shown only while .broken */
  var PLATE_CRACK = '<path class="crack" d="M12.9 4.6 L10.2 11.1 L13.9 12.9 L11.1 21.2"/>';

  function plateArcs(c, n, fracs) {
    var seg = PLATE_RING / n,
      i, h = "";
    if (c.n !== n) {
      c.n = n;
      for (i = 0; i < n; i++) {
        var off = (-(i * seg + PLATE_GAP / 2)).toFixed(2),
          len = (seg - PLATE_GAP).toFixed(2);
        h += '<circle class="pt" cx="32" cy="32" r="26" transform="rotate(-90 32 32)" stroke-dasharray="' + len + ' ' + PLATE_RING.toFixed(2) + '" stroke-dashoffset="' + off + '"/>' +
          '<circle class="pa" cx="32" cy="32" r="26" transform="rotate(-90 32 32)" stroke-dasharray="0 ' + PLATE_RING.toFixed(2) + '" stroke-dashoffset="' + off + '"/>';
      }
      c.arcs.innerHTML = h;
    }
    var fills = c.arcs.querySelectorAll(".pa");
    for (i = 0; i < fills.length; i++)
      fills[i].setAttribute("stroke-dasharray", ((seg - PLATE_GAP) * fracs[i]).toFixed(2) + " " + PLATE_RING.toFixed(2));
  }

  function setArmor(a) {
    armorLast = a;
    /* settings.showArmorWheel (MCM "Show armor wheel") hides the core; showing it again starts without a stale pop */
    if (!a || S.settings.showArmorWheel === false) {
      if (armorCore) {
        armorCore.el.classList.add("off");
        armorCore.v = null;
        layoutCoresSoon();
      }
      return;
    }
    if (!armorCore) {
      var el = document.createElement("div");
      el.className = "core plates";
      el.id = "k-armor";
      el.style.setProperty("--c", "#a9c8e8");
      /* join the page before the SVG is parsed, like the other cores (they fill an element already in the page) */
      $("cores").insertBefore(el, $("k-wt"));
      el.innerHTML = '<svg viewBox="0 0 64 64"><circle class="disc" cx="32" cy="32" r="24.5"/><g class="arcs"></g>' +
        /* exactly the HP core's icon markup: a clip path on the lit layer and the faint "solid" fill. The game's renderer
           laid the bare outline out correctly but never painted it until the lit layer had a clip path (2026-10-19). */
        '<clipPath id="kc-armor"><rect x="-2" y="-1" width="28" height="26"/></clipPath>' +
        '<g transform="translate(19 19) scale(1.083)"><g class="ic dim">' + PLATE_ICON + '</g>' +
        '<g class="ic lit solid" clip-path="url(#kc-armor)" style="clip-path:url(#kc-armor)">' + PLATE_ICON + PLATE_CRACK + '</g></g>' +
        '</svg><small><span class="lbl">ARMOR</span><b></b></small>';
      armorCore = {
        el: el,
        arcs: el.querySelector(".arcs"),
        num: el.querySelector("small b"),
        lbl: el.querySelector("small .lbl"),
        n: 0,
        v: null
      };
    }
    if (armorCore.el.classList.contains("off")) {
      armorCore.el.classList.remove("off");
      layoutCoresSoon();
    }
    var cur = Math.max(0, +a.current || 0),
      per = Math.max(1, +a.perPlate || 1),
      max = Math.max(per, +a.max || 0);
    var n = Math.min(8, Math.max(1, Math.round(max / per))),
      stock = Math.max(0, Math.round(+a.stock || 0)),
      fr = [];
    for (var i = 0; i < n; i++) fr.push(clamp(cur / per - i, 0, 1));
    plateArcs(armorCore, n, fr);
    armorCore.el.classList.toggle("broken", cur <= 0);
    armorCore.lbl.textContent = cur <= 0 ? "BROKEN" : "ARMOR";
    armorCore.num.textContent = Math.round(cur) + " · ×" + stock;
    if (armorCore.v !== null && cur < armorCore.v) {
      pop(armorCore.el, "-" + Math.round(armorCore.v - cur));
      wake($("vitals"));
    }
    armorCore.v = cur;
  }

  /* leveling: perk points (and LevelUpMenuEx skill points) stay on screen until spent */
  S.perkPoints = 0;
  S.skillPoints = 0;
  S.skillsPerLevel = 0;
  S.perkKey = "";

  function renderPerks() {
    var c = $("perkChip"),
      parts = [];
    if (S.perkPoints > 0) parts.push(S.perkPoints + (S.perkPoints === 1 ? " perk" : " perks"));
    if (S.skillPoints > 0) parts.push(S.skillPoints + " skill");
    c.hidden = !parts.length || !S.settings.perkReminder;
    c.textContent = parts.join(" · ");
  }

  function showLevelUp(from, to, perks, skills) {
    var gained = to - from;
    $("luNum").textContent = to;
    var sub = gained > 1 ? "Levels " + (from + 1) + "–" + to + " · " : "";
    sub += "<b>+" + esc(perks) + " perk point" + (perks === 1 ? "" : "s") + "</b>";
    if (skills) sub += " · <b>+" + esc(skills) + " skill points</b>";
    $("luSub").innerHTML = sub;
    $("luHint").hidden = !S.perkKey;
    $("luKey").textContent = S.perkKey;
    if (S.settings.levelUpBanner) enqueueBanner(function() {
      var el = $("lvlup");
      el.classList.remove("show");
      void el.offsetWidth;
      el.classList.add("show");
      clearTimeout(el._t);
      el._t = setTimeout(function() {
        el.classList.remove("show");
      }, 4200);
    }, 4200);
    notify("Level " + to, "+" + perks + " perk point" + (perks === 1 ? "" : "s") + (skills ? " · +" + skills + " skill points" : ""), "level");
  }

  function addXP(n, exact) {
    pop($("xp"), "+" + n + " XP");
    var from = S.level;
    if (exact) {
      if (exact.level !== undefined) S.level = +exact.level;
      if (exact.xp !== undefined) S.xp = clamp(+exact.xp, 0, 1);
    } else {
      S.xp += n / (200 + S.level * 15);
      while (S.xp >= 1) {
        S.xp -= 1;
        S.level++;
      }
    }
    var gained = S.level - from;
    if (gained > 0) {
      if (!exact) {
        S.perkPoints += gained;
        S.skillPoints += gained * S.skillsPerLevel;
      }
      /* bar fills to the top, flashes, then rolls over to the new progress */
      var bar = $("xpFill"),
        wrap = bar.parentNode,
        rest = S.xp;
      bar.style.width = "100%";
      setTimeout(function() {
        wrap.classList.add("flash");
        setTimeout(function() {
          wrap.classList.remove("flash");
          bar.style.transition = "none";
          bar.style.width = "0%";
          void bar.offsetWidth;
          bar.style.transition = "";
          renderXP();
          var lv = $("lvl");
          lv.classList.remove("up");
          void lv.offsetWidth;
          lv.classList.add("up");
        }, 260);
      }, 700);
      $("xpPct").textContent = "100%";
      if (!exact) showLevelUp(from, S.level, gained, gained * S.skillsPerLevel);
      renderPerks();
    } else renderXP();
    wake($("vitals"));
  }

  /* ---------- compass ---------- */
  var PX = 640 / 180,
    CARD = {
      0: "N",
      45: "NE",
      90: "E",
      135: "SE",
      180: "S",
      225: "SW",
      270: "W",
      315: "NW"
    };
  var lastHdg = null;

  function norm(d) {
    return ((d % 360) + 360) % 360;
  }
  var MKI = {
    quest: '<path class="f" d="M9 2.5L15.5 9 9 15.5 2.5 9z"/><path d="M9 2.5L15.5 9 9 15.5 2.5 9z"/>',
    door: '<path class="f" d="M5 16V6.5a4 4 0 0 1 8 0V16z"/><path d="M5 16V6.5a4 4 0 0 1 8 0V16M3 16h12"/><circle cx="10.8" cy="11" r=".9" fill="currentColor" stroke="none"/>',
    mine: '<path class="f" d="M9 3l7 12H2z"/><path d="M9 3l7 12H2z"/><path d="M9 7.5v3.5M9 13v.1"/>',
    loot: '<path class="f" d="M9 1.8l2.1 4.4 4.8.6-3.5 3.4.9 4.8L9 12.7 4.7 15l.9-4.8L2.1 6.8l4.8-.6z"/><path d="M9 1.8l2.1 4.4 4.8.6-3.5 3.4.9 4.8L9 12.7 4.7 15l.9-4.8L2.1 6.8l4.8-.6z"/>',
    defend: '<circle class="ring" cx="9" cy="9.5" r="8"/><path class="f" d="M4.5 8.5L9 4.5l4.5 4V15h-9z"/><path d="M2.5 9.5L9 3.5l6.5 6M4.5 8v7h9V8M9 8.5v2.6M9 12.8v.1"/>',
    settlement: '<path class="f" d="M4.5 8.5L9 4.5l4.5 4V15h-9z"/><path d="M2.5 9.5L9 3.5l6.5 6M4.5 8v7h9V8M7.8 15v-3.5h2.4V15"/>',
    city: '<path class="f" d="M3 15V7h5v8zM8 15V3h7v12z"/><path d="M3 15V7h5M8 15V3h7v12M1.5 15h15M10.5 6h2M10.5 9h2"/>',
    cave: '<path class="f" d="M2 15C3 8 5.5 5 9 5s6 3 7 10z"/><path d="M2 15C3 8 5.5 5 9 5s6 3 7 10M6.5 15c.5-2.8 1.4-4.5 2.5-4.5s2 1.7 2.5 4.5"/>',
    vault: '<circle class="f" cx="9" cy="9" r="6"/><circle cx="9" cy="9" r="6"/><circle cx="9" cy="9" r="2"/><path d="M9 1.5v1.8M9 14.7v1.8M1.5 9h1.8M14.7 9h1.8M3.7 3.7l1.2 1.2M13.1 13.1l1.2 1.2M3.7 14.3l1.2-1.2M13.1 4.9l1.2-1.2"/>',
    factory: '<path class="f" d="M2.5 15V9l4 2V9l4 2V4.5h3V15z"/><path d="M2.5 15V9l4 2V9l4 2V4.5h3V15zM1.5 15h15"/>',
    landmark: '<path class="f" d="M5 3l8 3.2L5 9.5z"/><path d="M5 16V3l8 3.2L5 9.5"/>',
    pin: '<path class="f" d="M9 16s5-5 5-8.8a5 5 0 0 0-10 0C4 11 9 16 9 16z"/><path d="M9 16s5-5 5-8.8a5 5 0 0 0-10 0C4 11 9 16 9 16z"/><circle cx="9" cy="7.2" r="1.7"/>',
    enemy: '<circle class="dot" cx="9" cy="9" r="3.4"/>',
    ally: '<circle class="dot" cx="9" cy="9" r="3.4"/>'
  };
  var mkEls = {},
    mkSeq = 0,
    focusId = null;

  function fmtDist(m) {
    return m >= 1000 ? (m / 1000).toFixed(1) + " km" : Math.round(m) + " m";
  }

  function buildTickStrip() {
    var html = "";
    for (var d = -90; d <= 450; d += 5) {
      var x = ((d + 90) * PX).toFixed(1),
        n = norm(d);
      if (CARD[n] !== undefined) html += '<span class="cd' + (n % 90 ? " minor" : "") + (n === 0 ? " n" : "") + '" style="left:' + x + 'px">' + CARD[n] + "</span>";
      else html += '<i class="tk' + (n % 15 ? "" : " mid") + '" style="left:' + x + 'px"></i>';
    }
    $("ticks").innerHTML = '<div id="tstrip">' + html + '</div>';
  }

  function renderCompass(force) {
    var h = S.heading;
    if (!force && lastHdg !== null && Math.abs(h - lastHdg) < 0.05) return;
    lastHdg = h;
    if (!$("tstrip")) buildTickStrip();
    var hn = norm(h);
    $("tstrip").style.transform = "translate3d(" + (320 - (hn + 90) * PX).toFixed(1) + "px,0,0)";
    /* markers keep their own elements so pulses and transitions survive each frame */
    var layer = $("marks"),
      seen = {},
      focus = null,
      focusScore = Infinity,
      now = Date.now(),
      rows = [];
    S.markers.forEach(function(m) {
      if (m.t === "enemy" && S.survival && S.survival.on && !S.survival.enemyMarkers) return;
      if (m.disc === false && !S.settings.showUndiscovered) return;
      if (!m.id) m.id = "mk" + (++mkSeq);
      if (m._new === undefined) m._new = now;
      var type = MKI[m.t] ? m.t : (m.t === "place" ? "landmark" : "quest");
      var el = mkEls[m.id];
      if (!el) {
        el = document.createElement("span");
        mkEls[m.id] = el;
        layer.appendChild(el);
      }
      if (el._type !== type) {
        el.innerHTML = '<svg viewBox="0 0 18 18">' + MKI[type] + '</svg><i class="el"></i>';
        el._type = type;
        el._el = el.lastChild;
      }
      seen[m.id] = 1;
      var questy = type === "quest" || type === "door" || type === "defend";
      var delta = norm(m.b - h + 180) - 180,
        ad = Math.abs(delta);
      var tracked = (questy || (type === "pin" && m.id === "player-waypoint")) && m.active !== false;
      var vis = ad <= 88 || tracked;
      var op = !vis ? 0 : ad <= 55 ? 1 : ad <= 88 ? 1 - (ad - 55) / 33 * .7 : .75;
      el.style.left = (clamp(delta, -88, 88) * PX + 320).toFixed(1) + "px";
      el.style.opacity = op;
      var cls = "mk " + type;
      if (questy && m.active === false) cls += " inactive";
      if (m.disc === false) cls += " undisc";
      if (ad > 88 && vis) cls += delta < 0 ? " edge-l" : " edge-r";
      if (questy && now - m._new < 3200) cls += " new";
      /* the plugin sends dz only beyond the game's own height threshold (fQuestMarkerZDiffThreshold, about 3.6 m) */
      var arrow = m.dz > 0 ? "▲" : m.dz < 0 ? "▼" : "";
      if (el._el.textContent !== arrow) el._el.textContent = arrow;
      /* a name takes focus within 8 degrees and keeps it out to 11, so small heading wobble cannot blink it */
      var held = m.id === focusId;
      if (m.label && vis && ad < (held ? 11 : 8) && ad - (held ? 3 : 0) < focusScore) {
        focusScore = ad - (held ? 3 : 0);
        focus = m;
      }
      rows.push([el, cls, m]);
    });
    focusId = focus ? focus.id : null;
    rows.forEach(function(r) {
      var c = r[1] + (r[2] === focus ? " focus" : "");
      if (r[0].className !== c) r[0].className = c;
    });
    Object.keys(mkEls).forEach(function(id) {
      if (!seen[id]) {
        mkEls[id].remove();
        delete mkEls[id];
      }
    });

    var cf = $("cfocus");
    if (focus) {
      var ft = MKI[focus.t] ? focus.t : "landmark";
      var name = focus.label + (ft === "door" ? " · door" : "") + (focus.disc === false ? " · undiscovered" : "");
      var meta = (focus.dist !== undefined ? fmtDist(focus.dist) : "") + (focus.dz > 0 ? " ▲" : focus.dz < 0 ? " ▼" : "");
      var html = esc(name) + (meta ? "<span>" + esc(meta) + "</span>" : "");
      if (cf._h !== html) {
        cf.innerHTML = html;
        cf._h = html;
      }
      cf.className = "cfocus show " + ft;
    } else cf.className = "cfocus";
    $("hdg").textContent = ("00" + Math.round(norm(h))).slice(-3) + "°";
  }

  /* ---------- weapon, needs, limbs ---------- */
  function renderAmmo() {
    $("clip").textContent = S.clip;
    $("res").textContent = S.reserve;
    $("wName").textContent = S.weapon;
    $("nade").textContent = ("0" + S.grenades).slice(-2);
    var nb = $("nadeName"),
      nr = nb.parentNode,
      x = S.explosive;
    /* nothing in the throwable slot: the row folds away */
    if (!x || (S.grenades <= 0 && !nr.classList.contains("out"))) {
      nr.classList.add("gone");
    } else {
      nr.classList.remove("gone");
      nb.textContent = nr.classList.contains("cycled") ? x.name : (x.short || x.name);
    }
    var a = $("ammo");
    var nomag = !(S.clipMax > 0);
    a.classList.toggle("nomag", nomag);
    a.classList.toggle("empty", !nomag && S.clip === 0);
    a.classList.toggle("low", !nomag && S.clip > 0 && S.clip <= Math.ceil(S.clipMax / 4));
  }
  var NEED_STAGES = {
    food: ["Well-fed", "Peckish", "Hungry", "Famished", "Ravenous", "Starving"],
    water: ["Hydrated", "Parched", "Thirsty", "Mildly dehydrated", "Dehydrated", "Severely dehydrated"],
    rest: ["Well-rested", "Tired", "Overtired", "Weary", "Exhausted", "Incapacitated"]
  };
  var needStageWas = {};

  function needStage(k) {
    var st = S.survival.stages ? S.survival.stages[k] : undefined;
    if (st !== undefined && st !== null) return clamp(+st, 0, 5);
    var v = clamp(S.needs[k], 0, 1);
    return v >= .84 ? 0 : v >= .67 ? 1 : v >= .5 ? 2 : v >= .33 ? 3 : v >= .17 ? 4 : 5;
  }

  function renderNeeds() {
    var anyVisible = false;
    ["food", "water", "rest"].forEach(function(k) {
      var v = clamp(S.needs[k], 0, 1),
        st = needStage(k),
        name = NEED_STAGES[k][st];
      var visible = !S.settings.hideSatisfiedNeeds || st > 0;
      cores[k].el.classList.toggle("need-hidden", !visible);
      anyVisible = anyVisible || visible;
      setCore(k, v, S.survival.stageNames ? name : Math.round(v * 100) + "%", v);
      if (needStageWas[k] !== undefined && st > needStageWas[k] && st >= 2 && S.survival.on)
        notify(name, k === "food" ? "Eat something soon" : k === "water" ? "Drink something soon" : "Find a bed", "bad", 6000, "warn");
      needStageWas[k] = st;
      cores[k].el.classList.toggle("warn", v < 0.35 && v >= 0.15);
      cores[k].el.classList.toggle("crit", v < 0.15);
      cores[k].el.classList.toggle("low", v < 0.15);
    });
    $("needsGap").classList.toggle("need-hidden", !anyVisible);
    layoutCoresSoon();
  }
  /* limb condition 0..1, from the game's six condition values (each out of 100):
     head = PerceptionCondition, torso = EnduranceCondition, arms = Left/RightAttackCondition,
     legs = Left/RightMobilityCondition. A limb is only crippled at 0, as in vanilla. */
  var LIMB_NAME = {
    head: "Head",
    torso: "Torso",
    larm: "Left arm",
    rarm: "Right arm",
    lleg: "Left leg",
    rleg: "Right leg"
  };
  var limbWas = {};
  /* ---------- limb figure skins ----------
     silhouette: built in. vaultboy / vaultgirl: the parts from Condition Boy (Nexus 56602), converted to
     figures/<who>-<layer>-<part>.png. Base layer is clothed or naked, armor pieces draw on top, and each
     part is tinted by its condition the way Condition Boy does it. */
  var FIG_PARTS = ["torso", "lleg", "rleg", "larm", "rarm", "head"];
  var FIG_TINT = {
    ok: "#cfe9d3",
    hurt: "#f2c46a",
    bad: "#ff9a52",
    crip: "#ff5a40"
  };
  S.figure = {
    type: "silhouette",
    hideHealthy: true
  };
  S.gear = {
    clothed: true,
    armor: {
      head: false,
      torso: false,
      larm: false,
      rarm: false,
      lleg: false,
      rleg: false
    }
  };
  var figImgs = {},
    figReady = {},
    figTmp = document.createElement("canvas"),
    blinkOn = true;

  function loadFigure(type) {
    if (figReady[type]) return figReady[type];
    figReady[type] = new Promise(function(res, rej) {
      var list = [],
        left = 0,
        failed = false;
      ["naked", "clothed", "armor"].forEach(function(layer) {
        FIG_PARTS.forEach(function(part) {
          list.push(type + "-" + layer + "-" + part);
        });
      });
      left = list.length;
      list.forEach(function(k) {
        var img = new Image();
        img.onload = function() {
          figImgs[k] = img;
          if (--left === 0 && !failed) res();
        };
        img.onerror = function() {
          if (!failed) {
            failed = true;
            figReady[type] = null;
            rej(new Error("missing " + k));
          }
        };
        img.src = "figures/" + k + ".png";
      });
    });
    return figReady[type];
  }

  function limbClass(v) {
    return v <= 0 ? "crip" : v < 0.35 ? "bad" : v < 0.75 ? "hurt" : "ok";
  }

  function tintPart(img, color, W, H) {
    figTmp.width = W;
    figTmp.height = H;
    var c = figTmp.getContext("2d");
    c.globalCompositeOperation = "source-over";
    c.drawImage(img, 0, 0, W, H);
    c.globalCompositeOperation = "multiply";
    c.fillStyle = color;
    c.fillRect(0, 0, W, H);
    c.globalCompositeOperation = "destination-in";
    c.drawImage(img, 0, 0, W, H);
    c.globalCompositeOperation = "source-over";
    return figTmp;
  }

  function drawFigure() {
    var type = S.figure.type,
      cv = $("limbFig");
    var useImg = type !== "silhouette" && figImgs[type + "-naked-torso"];
    document.querySelector(".limbs").toggleAttribute("hidden", !!useImg);
    cv.hidden = !useImg;
    if (!useImg) return;
    var W = cv.width,
      H = cv.height,
      ctx = cv.getContext("2d");
    ctx.clearRect(0, 0, W, H);
    var base = S.gear.clothed ? "clothed" : "naked";
    [base, "armor"].forEach(function(layer) {
      FIG_PARTS.forEach(function(part) {
        if (layer === "armor" && (!S.gear.armor[part] || !S.settings.armorPieces)) return;
        var img = figImgs[type + "-" + layer + "-" + part];
        if (!img) return;
        var cls = limbClass(clamp(S.limbs[part], 0, 1));
        ctx.globalAlpha = (cls === "crip" && !blinkOn) ? 0.35 : 1;
        ctx.drawImage(tintPart(img, FIG_TINT[cls], W, H), 0, 0);
      });
    });
    ctx.globalAlpha = 1;
  }
  setInterval(function() {
    if (S.anyCrip && S.figure.type !== "silhouette") {
      blinkOn = !blinkOn;
      drawFigure();
    } else blinkOn = true;
  }, 700);

  function setFigure(f) {
    Object.assign(S.figure, f);
    var type = S.figure.type;
    if (type === "silhouette") {
      renderLimbs();
      return Promise.resolve(true);
    }
    return loadFigure(type).then(function() {
      renderLimbs();
      return true;
    }, function() {
      S.figure.type = "silhouette";
      renderLimbs();
      return false;
    });
  }

  function setGear(g) {
    if (g.clothed !== undefined) S.gear.clothed = !!g.clothed;
    if (g.armor) Object.assign(S.gear.armor, g.armor);
    drawFigure();
  }

  function renderLimbs() {
    var anyHurt = false,
      anyCrip = false;
    Object.keys(S.limbs).forEach(function(k) {
      var el = $("l-" + k);
      if (!el) return;
      var v = clamp(S.limbs[k], 0, 1);
      var cls = v <= 0 ? "crip" : v < 0.35 ? "bad" : v < 0.75 ? "hurt" : "";
      el.setAttribute("class", cls);
      if (v < 0.98) anyHurt = true;
      if (cls === "crip") anyCrip = true;
      if (limbWas[k] !== undefined && limbWas[k] > 0 && v <= 0) notify(LIMB_NAME[k] + " crippled", "Use a Stimpak or a doctor to heal it", "bad", 6000, "warn");
      limbWas[k] = v;
    });
    /* the figure only appears when something is damaged, and stays lit while a limb is crippled */
    var fig = document.querySelector(".limbs"),
      cv = $("limbFig");
    var hide = !anyHurt && S.figure.hideHealthy;
    fig.classList.toggle("healthy", hide);
    cv.classList.toggle("healthy", hide);
    fig.parentNode.classList.toggle("pinned", anyCrip);
    S.anyCrip = anyCrip;
    drawFigure();
  }

  /* ---------- events ---------- */
  var ICON = {
    item: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linejoin="round"><path d="M4 8l8-4 8 4v8l-8 4-8-4z"/><path d="M4 8l8 4 8-4M12 12v8"/></svg>',
    weapon: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round"><circle cx="12" cy="12" r="6"/><path d="M12 2v5M12 17v5M2 12h5M17 12h5"/></svg>',
    radio: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round"><circle cx="12" cy="13" r="1.6"/><path d="M8.5 9.5a5 5 0 0 0 0 7M15.5 9.5a5 5 0 0 1 0 7M5.5 6.5a9 9 0 0 0 0 13M18.5 6.5a9 9 0 0 1 0 13"/></svg>',
    level: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"><path d="M6 14l6-6 6 6M6 19l6-6 6 6"/></svg>',
    quest: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6"><path d="M12 3l9 9-9 9-9-9z"/></svg>',
    caps: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6"><circle cx="12" cy="12" r="8"/><circle cx="12" cy="12" r="4"/></svg>'
  };

  function notify(text, sub, kind, life, iconKey) {
    kind = kind || "item";
    var feed = $("feed"),
      li = document.createElement("li");
    li.className = "fi " + kind;
    var icon = iconKey && FXI[iconKey] ? '<svg viewBox="9 9 26 26">' + FXI[iconKey] + "</svg>" : (ICON[kind] || ICON.item);
    li.innerHTML = '<div class="ft"><span>' + esc(text) + "</span>" + (sub ? "<small>" + esc(sub) + "</small>" : "") + "</div>" + icon;
    feed.insertBefore(li, feed.firstChild);
    requestAnimationFrame(function() {
      requestAnimationFrame(function() {
        li.classList.add("in");
      });
    });
    while (feed.children.length > 5) feed.removeChild(feed.lastChild);
    setTimeout(function() {
      li.classList.add("out");
      setTimeout(function() {
        li.remove();
      }, 500);
    }, life || 7000);
  }

  function showFor(el, ms) {
    el.classList.add("show");
    clearTimeout(el._h);
    if (ms) el._h = setTimeout(function() {
      el.classList.remove("show");
    }, ms);
  }
  /* banners take turns so a quest completion and a level-up never draw over each other */
  var bannerQ = [],
    bannerBusy = false,
    bannerTimer = null;
  /* after a save loads: drop everything that belonged to the old moment; the plugin then sends a snapshot */
  function resetTransient() {
    clearTimeout(calmTimer);
    calmTimer = null;
    calmSince = null;
    calmReady = false;
    calmObserved = {};
    Object.keys(calmWake).forEach(function(g) {
      clearTimeout(calmWake[g]);
    });
    calmWake = {};
    Array.prototype.forEach.call(stage.querySelectorAll('.awake'), function(el) {
      clearTimeout(el._t);
      el.classList.remove('awake');
    });
    clearTimeout($("hit")._h);
    $("hit").classList.remove("on");
    setCombat(false);
    S.locationName = null;
    locationRemaining = 0;
    locationLeft = null;
    $("locName").textContent = "";
    renderLocationVisibility();
    bannerQ = [];
    clearTimeout(bannerTimer);
    bannerTimer = null;
    bannerBusy = false;
    ["quest", "lvlup", "msg", "subs"].forEach(function(id) {
      var el = $(id);
      if (el) {
        el.classList.remove("show");
        clearTimeout(el._h);
        clearTimeout(el._t);
      }
    });
    var feed = $("feed");
    while (feed.firstChild) feed.removeChild(feed.firstChild);
    setTarget(null);
    showLoot(null);
    setMines([]);
    setAttack(null);
    S.markers = S.markers.filter(function(m) {
      return String(m.id).indexOf("loot:") !== 0;
    });
    setStealth({
      sneaking: false,
      level: 0
    });
    renderCompass(true);
  }

  function enqueueBanner(show, ms) {
    bannerQ.push([show, ms]);
    if (!bannerBusy) nextBanner();
  }

  function nextBanner() {
    var it = bannerQ.shift();
    if (!it) {
      bannerBusy = false;
      return;
    }
    bannerBusy = true;
    it[0]();
    bannerTimer = setTimeout(nextBanner, it[1] + 350);
  }

  function quest(q) {
    wakeCalm("tracker");
    enqueueBanner(function() {
      showQuestBanner(q);
    }, q.ms || 6500);
  }

  function showQuestBanner(q) {
    $("quest").classList.toggle("alert", q.tone === "alert");
    $("quest").classList.toggle("good", q.tone === "good");
    $("qEye").textContent = q.eyebrow || "Quest completed";
    $("qTitle").textContent = q.title || "";
    $("qObj").innerHTML = (q.objectives || []).map(function(o) {
      return "<li" + (o.done ? ' class="done"' : "") + ">" + esc(o.t) + "</li>";
    }).join("");
    var el = $("quest");
    el.classList.remove("show");
    void el.offsetWidth;
    showFor(el, q.ms || 6500);
  }

  function message(title, text, ms) {
    $("msgTitle").textContent = title || "Tutorial";
    $("msgText").textContent = text;
    showFor($("msg"), ms === undefined ? 9000 : ms);
  }

  function subtitle(who, line, ms) {
    $("subWho").textContent = who || "";
    $("subLine").textContent = line;
    showFor($("subs"), ms || Math.max(3500, line.length * 70));
  }

  function setTarget(t) {
    var el = $("target");
    S.target = t;
    if (!t) {
      el.classList.remove("show");
      return;
    }
    $("tName").textContent = t.name || "Unknown";
    $("tLvl").textContent = "LVL " + (t.level || 1) + (S.stealth && S.stealth.sneaking && stealthState() === "hidden" ? " · Unaware" : "");
    $("tStar").style.display = t.legendary ? "" : "none";
    el.classList.toggle("leg", !!t.legendary);
    renderMutations(t);
    setBar($("tFill"), $("tGhost"), clamp(t.pct === undefined ? 1 : t.pct, 0, 1), "width");
    el.classList.add("show");
  }
  /* ---------- legendary mutations (Immersive Legendary Mutations) and legendary gear (LEO) ----------
     The mutations mod mutes vanilla's "has mutated!" message on purpose, so by default Phosphor only marks the
     enemy bar. Feed notices are an opt-in. */
  S.mutationFeed = false;

  function renderMutations(t, fresh) {
    var box = $("tMuts"),
      list = (t && t.mutations) || [];
    box.innerHTML = list.map(function(m, i) {
      return '<span class="' + esc(m.kind || "") + (fresh && i === list.length - 1 ? " new" : "") + '">' + esc(m.name) + "</span>";
    }).join("");
    var lvl = $("tLvl"),
      mx = lvl.querySelector(".t-mx");
    if (list.length > 1) {
      if (!mx) {
        mx = document.createElement("span");
        mx.className = "t-mx";
        lvl.appendChild(mx);
      }
      mx.textContent = "×" + list.length;
    } else if (mx) mx.remove();
  }
  /* crosshair spread: the game's own crosshair radius (stage px), so the ticks sit where vanilla's would */
  function setSpread(c) {
    var g = c && c.spread !== undefined ? clamp(+c.spread, 4, 260) : 8,
      ring = $("ring");
    ring.classList.toggle("live", !!(c && c.spread !== undefined));
    /* follow vanilla: the ring only while its ticks show (a drawn gun), the dot only with them or vanilla's own dot;
       nothing with no weapon, a holstered weapon or melee. Missing fields (replay, older plugin) keep both. */
    $("cross").classList.toggle("noring", !!c && c.reticle === false);
    $("cross").classList.toggle("nodot", !!c && c.dot === false);
    if (ring._g === g) return;
    ring._g = g;
    var q = function(sel) {
      return ring.querySelector(sel);
    };
    q(".t").style.top = -(g + 9) + "px";
    q(".b").style.top = g + "px";
    q(".l").style.left = -(g + 9) + "px";
    q(".r").style.left = g + "px";
  }

  function targetMutated(m) {
    var t = S.target;
    if (!t) return;
    // The game side sends the target with its new mutation in the same message; don't list it twice.
    var known = (t.mutations || []).some(function(x) {
      return x.name === m.name;
    });
    if (!known) {
      t.mutations = (t.mutations || []).concat([m]);
      if (m.heal) t.pct = clamp(t.pct + m.heal, 0, 1);
    }
    setTarget(t);
    renderMutations(t, true);
    var el = $("target");
    el.classList.remove("mutating");
    void el.offsetWidth;
    el.classList.add("mutating");
    setTimeout(function() {
      el.classList.remove("mutating");
    }, 1200);
    if (S.mutationFeed) notify((t.name || "Enemy") + " mutated", m.name, "legendary", 5000, "star");
  }

  function legendaryLoot(x) {
    if (x.dropped) {
      if (x.b !== undefined && S.settings.legendaryDropMarkers) {
        S.markers = S.markers.filter(function(m) {
          return m.id !== "loot:" + x.id;
        });
        S.markers.push({
          id: "loot:" + x.id,
          t: "loot",
          b: x.b,
          dist: x.dist,
          label: "Legendary " + x.item
        });
        renderCompass(true);
      }
      notify("Legendary item dropped", x.item + (x.effects ? " · " + x.effects.join(", ") : ""), "legendary", 6000, "star");
    }
    if (x.picked) {
      S.markers = S.markers.filter(function(m) {
        return m.id !== "loot:" + x.id;
      });
      renderCompass(true);
      notify(x.effects ? x.effects.join(" ") + " " + x.item : x.item, "Legendary" + (x.effects && x.effects.length > 1 ? " · " + x.effects.length + " effects" : ""), "legendary", 7000, "star");
    }
  }

  function setWeaponLegendary(effects) {
    S.weaponLegendary = effects || [];
    $("wLeg").hidden = !S.weaponLegendary.length || S.settings.showWeaponLegendary === false;
    $("wLegTxt").textContent = S.weaponLegendary.join(" · ");
  }

  /* activation prompt under the crosshair: key comes from PrismaUI's GetButtonPrompt (keyboard text or
     "gp:<button>"), text is the game's activate label (Alternative Activation Prompts changes it) */
  function setPrompt(p) {
    var h = $("hint");
    if (!p) {
      h.hidden = true;
      return;
    }
    if (p.keys) {
      /* vanilla's rollover: the target's name over one or more button hints */
      h.innerHTML = (p.name ? '<b class="h-name' + (p.crime ? " crime" : "") + '">' + esc(p.name) + "</b>" : "") +
        '<span class="h-keys">' + p.keys.map(function(k) {
          return "<span>" + (k.key ? '<span class="kbd">' + esc(String(k.key).replace(/^gp:/, "")) + "</span> " : "") + esc(k.text) + "</span>";
        }).join("") + "</span>";
      h.classList.add("multi");
      h.classList.toggle("crime", !!p.crime);
    } else {
      var key = String(p.key || "E").replace(/^gp:/, "");
      h.innerHTML = '<span class="kbd">' + (p.hold ? "Hold " : "") + esc(key) + "</span> " + esc(p.text || "");
      h.classList.remove("multi", "crime");
    }
    h.hidden = false;
  }

  function showLoot(l) {
    S.loot = l;
    var el = $("loot");
    if (!l) {
      el.classList.remove("show");
      $("hint").classList.remove("off");
      return;
    }
    $("lName").textContent = l.name || "Container";
    var items = l.items || [],
      sel = Math.max(0, Math.min(l.sel || 0, items.length - 1));
    var wt = items.reduce(function(a, i) {
      return a + (i.w || 0) * (i.q || 1);
    }, 0);
    $("lWt").textContent = l.locked ? "LOCKED" : wt > 0 ? "WT " + wt.toFixed(1) : items.length > 7 ? items.length + " ITEMS" : "";
    /* long containers scroll: a window of 7 rows that follows the selection */
    var start = Math.max(0, Math.min(sel - 3, items.length - 7)),
      shown = items.slice(start, start + 7);
    $("lList").innerHTML = l.locked ? '<li><span style="color:var(--muted)">Locked</span><small></small></li>' :
      shown.map(function(i, k) {
        var cls = (start + k === sel ? "sel" : "") + (k === 0 && start > 0 ? " more-up" : "") + (k === shown.length - 1 && start + 7 < items.length ? " more-down" : "");
        return '<li class="' + cls.trim() + '"><span>' + (i.leg ? '<i class="lleg">★</i>' : "") + esc(i.n) + "</span><small>" + (i.q > 1 ? "×" + i.q : "") + "</small></li>";
      }).join("") || '<li><span style="color:var(--muted)">Empty</span><small></small></li>';
    if (l.keys) $("lKeys").innerHTML = l.keys.map(function(k) {
      return "<span>" + (k.key ? '<span class="kbd">' + esc(String(k.key).replace(/^gp:/, "")) + "</span> " : "") + esc(k.text) + "</span>";
    }).join("");
    el.classList.toggle("steal", !!l.steal);
    el.classList.add("show");
    $("hint").classList.add("off");
  }
  var locationRemaining = 0;
  S.locationName = null;

  function renderLocationVisibility() {
    $("locName").classList.toggle("arrival-hidden", S.settings.locationOnArrival && (!S.locationName || locationRemaining <= 0));
  }

  function tickLocation(dt) {
    if (!S.settings.locationOnArrival || locationRemaining <= 0 || stage.classList.contains("awaiting") || $("locName").closest(".ctx-hide")) return;
    locationRemaining = Math.max(0, locationRemaining - dt);
    renderLocationVisibility();
  }
  /* Walking along a location's edge flips between it and the bare region ("Military Checkpoint" <->
     "Commonwealth"). Stepping out into the region is not an arrival, and stepping back into the place
     you left within a minute is not one either; only real arrivals restart the name and wake vitals. */
  var locationLeft = null,
    locationLeftAt = 0;

  function setLocation(name, sub, faction) {
    if (sub) $("locSub").textContent = sub;
    var region = String($("locSub").textContent || "").toLowerCase(),
      now = Date.now();
    var generic = function(n) {
      return !!n && !!region && String(n).toLowerCase() === region;
    };
    var announce = false;
    if (name !== S.locationName) {
      var prev = S.locationName;
      var stepOut = !!prev && !generic(prev) && generic(name);
      var stepBack = !!name && name === locationLeft && generic(prev) && now - locationLeftAt < 60000;
      if (stepOut) {
        locationLeft = prev;
        locationLeftAt = now;
      } else if (!generic(name)) locationLeft = null;
      announce = !!name && !stepOut && !stepBack;
      S.locationName = name;
      locationRemaining = announce && S.settings.locationOnArrival ? 8 : 0;
    }
    if (faction !== undefined) {
      S.localFaction = faction || null;
      renderLocalRep();
    }
    $("locName").textContent = name;
    renderLocationVisibility();
    if (announce) wake($("vitals"));
  }

  /* ---------- explosives (Explosives Cycler) ---------- */
  /* reads the player's throwable slot. Vanilla equips it from the Pip-Boy or favorites,
     Explosives Cycler equips it from its lists; either way the HUD only sees what's equipped. */
  function setExplosive(x, announce) {
    var nr = $("nadeName").parentNode;
    if (!x) {
      S.explosive = null;
      renderAmmo();
      return;
    }
    var prev = S.explosive,
      prevCount = S.grenades;
    var changed = !prev || (x.name && x.name !== prev.name);
    S.explosive = prev && !changed ? prev : {
      name: x.name || (prev && prev.name) || "Grenade",
      short: ""
    };
    if (x.name) S.explosive.name = x.name;
    S.explosive.short = x.short || S.explosive.short || S.explosive.name.split(" ")[0].toUpperCase().slice(0, 5);
    if (x.count !== undefined) S.grenades = Math.max(0, +x.count);
    if (changed && announce !== false) {
      nr.classList.remove("cycled");
      void nr.offsetWidth;
      nr.classList.add("cycled");
      clearTimeout(nr._t);
      nr._t = setTimeout(function() {
        nr.classList.remove("cycled");
        renderAmmo();
      }, 2600);
    } else if (!changed && S.grenades < prevCount) {
      nr.classList.remove("thrown");
      void nr.offsetWidth;
      nr.classList.add("thrown");
      clearTimeout(nr._th);
      nr._th = setTimeout(function() {
        nr.classList.remove("thrown");
      }, 500);
    }
    if (S.grenades <= 0) {
      /* last one thrown: show 00 in red for a moment, then fold the row away */
      nr.classList.add("out");
      clearTimeout(nr._o);
      nr._o = setTimeout(function() {
        nr.classList.remove("out");
        renderAmmo();
      }, 900);
    } else nr.classList.remove("out");
    renderAmmo();
    wake($("kit"));
  }

  /* ---------- game clock (HoloTime) ---------- */
  var SUN = '<circle cx="6" cy="6" r="2.4"/><path d="M6 .8v1.3M6 9.9v1.3M.8 6h1.3M9.9 6h1.3M2.3 2.3l.9.9M8.8 8.8l.9.9M2.3 9.7l.9-.9M8.8 3.2l.9-.9"/>';
  var MOON = '<path d="M8.6 1.6a4.6 4.6 0 1 0 1.8 7.2A4 4 0 0 1 8.6 1.6z"/>';
  var lastClock = "";

  function renderClock() {
    var t = S.time,
      h = ((t.hour % 24) + 24) % 24,
      hh = Math.floor(h),
      mm = Math.floor((h - hh) * 60);
    var txt = t.h24 ? ("0" + hh).slice(-2) + ":" + ("0" + mm).slice(-2) :
      ((hh % 12) || 12) + ":" + ("0" + mm).slice(-2) + (hh < 12 ? " AM" : " PM");
    var night = h < 6 || h >= 19.5;
    if (txt === lastClock) return;
    lastClock = txt;
    $("clockTxt").textContent = txt;
    var c = $("clock");
    if (c._night !== night) {
      c._night = night;
      c.classList.toggle("night", night);
      $("clockIco").innerHTML = night ? MOON : SUN;
    }
  }

  function setTime(t) {
    if (t.hour !== undefined) S.time.hour = +t.hour;
    if (t.timescale !== undefined) S.time.timescale = +t.timescale;
    if (t.h24 !== undefined) S.time.h24 = !!t.h24;
    S.time.set = true;
    $("clock").hidden = false;
    $("clockSep").hidden = false;
    renderClock();
  }

  /* ---------- companion card (Companion Status + Visible Companion Affinity) ---------- */
  var CICON = {
    human: '<circle cx="12" cy="8.5" r="3.4"/><path d="M5.5 19.5c.8-3.8 3.4-5.8 6.5-5.8s5.7 2 6.5 5.8"/>',
    dog: '<path d="M7.6 6.5L5 9.8l2.3 1.6M16.4 6.5L19 9.8l-2.3 1.6"/><path d="M7.8 7.2c1.1-1.2 2.5-1.7 4.2-1.7s3.1.5 4.2 1.7l-.6 6.6c-.3 2.4-1.8 3.7-3.6 3.7s-3.3-1.3-3.6-3.7z"/><path d="M11 15h2l-1 1.1z"/><circle cx="10.2" cy="11" r=".5"/><circle cx="13.8" cy="11" r=".5"/>',
    robot: '<circle cx="12" cy="10.5" r="4.6"/><path d="M12 5.9V3.6M8.5 17.3L7 20M15.5 17.3L17 20M12 15.1V20"/><circle cx="10.3" cy="10" r=".9"/><circle cx="13.7" cy="10" r=".9"/>',
    mutant: '<circle cx="12" cy="7.6" r="2.6"/><path d="M4 19.5c.6-4.4 3.4-7.2 8-7.2s7.4 2.8 8 7.2M8 12.6L6.8 10.4M16 12.6l1.2-2.2"/>'
  };
  var RANKS = [
    [1000, "Idolized"],
    [750, "Admiration"],
    [500, "Confidant"],
    [0, "Neutral"],
    [-500, "Disdain"],
    [-1e9, "Hatred"]
  ];

  function rankFor(a) {
    for (var i = 0; i < RANKS.length; i++)
      if (a >= RANKS[i][0]) return RANKS[i][1];
    return "Hatred";
  }
  /* Liga of my Companions can field up to 16; the row wraps and folds the rest into +N */
  var PORTRAITS = {
    piper: "piper",
    nick: "nick",
    valentine: "nick",
    preston: "preston",
    garvey: "preston",
    maccready: "maccready",
    danse: "danse",
    cait: "cait",
    codsworth: "codsworth",
    deacon: "deacon",
    dogmeat: "dogmeat",
    dog: "dogmeat",
    hancock: "hancock",
    x6: "x6-88"
  };
  /* Candidates in order: a base companion from PORTRAITS, then the whole name ("blood-rose", "f4-111"),
     then each word of three or more letters ("gage" for Porter Gage). Add-ons just drop portraits/<name>.svg;
     missing files fail quietly and the next candidate loads, ending at the type icon. */
  function portraitCandidates(c) {
    if (c.portrait) return [c.portrait];
    if (!LOCAL) return [];
    var words = String(c.name).toLowerCase().split(/[^a-z0-9]+/).filter(Boolean),
      list = [],
      seen = {};
    for (var i = 0; i < words.length; i++)
      if (PORTRAITS[words[i]]) {
        list.push(PORTRAITS[words[i]]);
        break;
      }
    list.push(words.join("-"));
    words.forEach(function(w) {
      if (w.length >= 3) list.push(w);
    });
    return list.filter(function(n) {
        if (!n || seen[n]) return false;
        seen[n] = true;
        return true;
      })
      .map(function(n) {
        return "portraits/" + n + ".svg";
      });
  }
  var PARTY_MAX = 12,
    pmEls = {};
  S.party = [];

  function pmFallback(c) {
    return '<svg viewBox="0 0 24 24">' + (CICON[c.type] || CICON.human) + "</svg>";
  }

  function buildMember(c) {
    var el = document.createElement("div");
    el.className = "pm enter";
    el.innerHTML =
      '<div class="pm-face">' +
      '<svg class="pm-ring" viewBox="0 0 48 48">' + ringLayer("trk", 24, 22, 16, 4) + ringLayer("val", 24, 22, 16, 4) + '</svg>' +
      '<div class="pm-pic"></div><div class="pm-down"></div>' +
      '<span class="pm-badge"><svg viewBox="0 0 8 8"><rect x="1.2" y="1" width="2" height="6"/><rect x="4.8" y="1" width="2" height="6"/></svg></span>' +
      '</div><span class="pm-name"></span>';
    setRing(el.querySelector(".pm-ring .trk").parentNode, 0, 1);
    var pic = el.querySelector(".pm-pic"),
      srcs = portraitCandidates(c),
      at = 0;
    if (srcs.length) {
      var img = new Image();
      img.alt = "";
      img.onerror = function() {
        if (++at < srcs.length) img.src = srcs[at];
        else pic.innerHTML = pmFallback(c);
      };
      img.src = srcs[0];
      pic.appendChild(img);
    } else pic.innerHTML = pmFallback(c);
    setTimeout(function() {
      el.classList.remove("enter");
    }, 500);
    return el;
  }

  function renderParty() {
    var box = $("party"),
      list = S.party,
      shown = list.slice(0, list.length > PARTY_MAX ? PARTY_MAX - 1 : PARTY_MAX),
      keep = {};
    box.hidden = !list.length;
    shown.forEach(function(c, i) {
      var el = pmEls[c.name];
      if (!el) {
        el = pmEls[c.name] = buildMember(c);
      }
      if (box.children[i] !== el) box.insertBefore(el, box.children[i] || null);
      keep[c.name] = 1;
      var st = c.state || "following",
        hp = clamp(c.hp === undefined ? 1 : c.hp, 0, 1);
      el.classList.toggle("downed", st === "downed");
      el.classList.toggle("waiting", st === "waiting");
      el.classList.toggle("hurt", st !== "downed" && hp < 0.35);
      setRing(el.querySelector(".pm-ring .val").parentNode, 0, hp);
      el.querySelector(".pm-name").textContent = st === "downed" ? "Downed" : c.name.split(" ")[0];
    });
    Object.keys(pmEls).forEach(function(n) {
      if (!keep[n]) {
        var el = pmEls[n];
        delete pmEls[n];
        el.remove();
      }
    });
    var more = box.querySelector(".pm-more");
    if (list.length > shown.length) {
      if (!more) {
        more = document.createElement("span");
        more.className = "pm-more";
      }
      box.appendChild(more);
      more.textContent = "+" + (list.length - shown.length);
    } else if (more) more.remove();
    /* companion compass dots come from the plugin's markers list, at their real bearings */
    renderCompass(true);
  }

  function findMember(name) {
    return S.party.filter(function(c) {
      return c.name === name;
    })[0];
  }

  function setCompanion(c) {
    if (c === null) {
      S.party = [];
      renderParty();
      return;
    }
    var cur = c.name ? findMember(c.name) : S.party[0];
    if (cur) {
      var wasDown = cur.state === "downed";
      Object.assign(cur, c);
      if (!wasDown && cur.state === "downed") notify(cur.name + " is down", "Gets back up when the fight ends", "bad", 6000, "warn");
    } else if (c.name) S.party.push(Object.assign({
      hp: 1,
      state: "following",
      type: "human"
    }, c));
    renderParty();
    wake($("vitals"));
  }

  function setParty(list) {
    var names = list.map(function(c) {
      return c.name;
    });
    S.party = list.map(function(c) {
      var cur = findMember(c.name);
      return cur ? Object.assign(cur, c) : Object.assign({
        hp: 1,
        state: "following",
        type: "human"
      }, c);
    });
    S.party.sort(function(a, b) {
      return names.indexOf(a.name) - names.indexOf(b.name);
    });
    renderParty();
    wake($("vitals"));
  }

  function removeCompanion(name) {
    S.party = S.party.filter(function(c) {
      return c.name !== name;
    });
    renderParty();
  }

  function companionReact(r) {
    var kind = typeof r === "string" ? r : r.kind,
      c = (r && r.name) ? findMember(r.name) : S.party[0];
    if (!c) return;
    var up = kind === "liked" || kind === "loved";
    var delta = {
      loved: 60,
      liked: 15,
      disliked: -15,
      hated: -60
    } [kind] || 0;
    if (c.affinity !== undefined && c.affinity !== null) {
      var before = c.rank || rankFor(c.affinity);
      c.affinity = clamp(c.affinity + delta, -1000, 1000);
      var after = rankFor(c.affinity);
      if (!c.rank && before !== after) notify(c.name + ": " + after, "Affinity " + (up ? "increased" : "dropped"), up ? "level" : "bad");
    }
    var el = pmEls[c.name];
    if (el) {
      var p = document.createElement("span");
      p.className = "pop " + (up ? "up" : "down");
      p.textContent = (up ? "▲ " : "▼ ") + (kind === "loved" ? "Loved" : kind === "liked" ? "Liked" : kind === "hated" ? "Hated" : "Disliked");
      el.appendChild(p);
      setTimeout(function() {
        p.remove();
      }, 1700);
    }
    wake($("vitals"));
  }

  /* ---------- quest tracker (Active Quest HUD) ---------- */
  var trItems = {};

  function setTracker(tr) {
    var box = $("tracker");

    function progress(x) {
      return x ? JSON.stringify([x.title, (x.objectives || []).map(function(o) {
        return [o.t, !!o.done];
      }), x.others]) : "";
    }
    if (progress(tr) !== progress(S.tracker)) wakeCalm("tracker");
    S.tracker = tr;
    if (!tr) {
      box.classList.remove("show");
      return;
    }
    var list = $("trObj"),
      fresh = $("trTitle").textContent !== tr.title,
      keep = {};
    if (fresh) {
      $("trTitle").textContent = tr.title;
      list.innerHTML = "";
      trItems = {};
      box.classList.remove("swap");
      void box.offsetWidth;
      box.classList.add("swap");
    }
    (tr.objectives || []).forEach(function(o) {
      var li = trItems[o.t];
      if (!li) {
        li = document.createElement("li");
        li.innerHTML = '<small></small><span class="ot"></span><i class="ob"></i>';
        li.querySelector(".ot").textContent = o.t;
        list.appendChild(li);
        trItems[o.t] = li;
        if (!fresh) li.classList.add("enter");
      }
      keep[o.t] = 1;
      var was = li.classList.contains("done");
      li.classList.toggle("done", !!o.done);
      if (o.done && !was && !fresh) {
        li.classList.remove("justdone");
        void li.offsetWidth;
        li.classList.add("justdone");
      }
      li.querySelector("small").textContent = (!o.done && o.dist !== undefined && o.dist !== null) ? fmtDist(o.dist) : "";
    });
    Object.keys(trItems).forEach(function(k) {
      if (keep[k]) return;
      var li = trItems[k];
      delete trItems[k];
      li.classList.add("leave");
      setTimeout(function() {
        li.remove();
      }, 450);
    });
    var others = tr.others || [];
    $("trMore").textContent = others.length ? others.length + " more active · " + others.join(" · ") : "";
    $("trMore").hidden = !others.length;
    box.classList.add("show");
  }
  /* ---------- stealth (sneak / detection) ---------- */
  var EYES = {
    hidden: '<path d="M3 11c2.6 3 5.6 4.5 9 4.5s6.4-1.5 9-4.5"/><path d="M6.2 14.2L5 16.2M12 15.5V18M17.8 14.2L19 16.2"/>',
    caution: '<path d="M2.8 12c2.6 3.4 5.7 5 9.2 5s6.6-1.6 9.2-5"/><path d="M2.8 12h18.4"/><path class="fillc" d="M9.2 12a2.8 2.8 0 0 0 5.6 0z"/>',
    danger: '<path d="M2.5 12S6 6.5 12 6.5 21.5 12 21.5 12 18 17.5 12 17.5 2.5 12 2.5 12z"/><circle class="fillc" cx="12" cy="12" r="2.8"/>',
    detected: '<path d="M2.5 13S6 7.5 12 7.5 21.5 13 21.5 13 18 18.5 12 18.5 2.5 13 2.5 13z"/><circle class="fillc" cx="12" cy="13" r="3"/><path d="M12 2.5v2.4M5.2 4.6l1.4 1.8M18.8 4.6l-1.4 1.8"/>'
  };
  var ST_LABEL = {
    hidden: "Hidden",
    caution: "Caution",
    danger: "Danger",
    detected: "Detected"
  };
  S.stealth = {
    sneaking: false,
    level: 0,
    state: null,
    mult: null
  };

  function stealthState() {
    var st = S.stealth;
    if (st.state) return st.state;
    return st.level >= 85 ? "detected" : st.level >= 50 ? "danger" : st.level >= 20 ? "caution" : "hidden";
  }

  function renderStealth() {
    var st = S.stealth,
      el = $("stealth"),
      state = stealthState();
    stage.classList.toggle("sneaking", st.sneaking);
    el.classList.toggle("show", st.sneaking);
    if (el._state !== state) {
      var prev = el._state;
      el._state = state;
      ["hidden", "caution", "danger", "detected"].forEach(function(k) {
        el.classList.toggle(k, k === state);
      });
      $("stEye").innerHTML = EYES[state];
      $("stLabel").textContent = ST_LABEL[state];
      if (prev) {
        el.classList.remove("flip");
        void el.offsetWidth;
        el.classList.add("flip");
        clearTimeout(el._f);
        el._f = setTimeout(function() {
          el.classList.remove("flip");
        }, 400);
      }
    }
    var f = clamp(st.level / 100, 0, 1) * 100 + "%";
    $("stL").style.width = f;
    $("stR").style.width = f;
    var atk = $("sneakAtk"),
      atkText = sneakText();
    if (atk.textContent !== atkText) atk.textContent = atkText;
    atk.classList.toggle("show", st.sneaking && state === "hidden" && !!S.target);
    if (S.target) {
      $("tLvl").textContent = "LVL " + (S.target.level || 1) + (st.sneaking && state === "hidden" ? " · Unaware" : "");
      renderMutations(S.target);
    }
    var b = $("b-crouch");
    if (b) b.setAttribute("aria-pressed", st.sneaking ? "true" : "false");
  }
  /* the game's sneak attack multiplier for the equipped weapon and current target, from the plugin */
  function sneakText() {
    var m = +S.stealth.mult;
    return m > 0 ? "Sneak attack ×" + (Math.round(m * 10) / 10) : "Sneak attack";
  }

  function setStealth(x) {
    if (x.sneaking !== undefined) S.stealth.sneaking = !!x.sneaking;
    if (x.level !== undefined) S.stealth.level = clamp(+x.level, 0, 100);
    if (x.state !== undefined) S.stealth.state = x.state || null;
    if (x.mult !== undefined) S.stealth.mult = x.mult;
    renderStealth();
  }

  /* ---------- HUD context: what shows where ---------- */
  var CTX_GROUPS = ["msg", "target", "feed", "tracker", "alert", "banner", "mines", "cross", "stealth", "subs", "compass", "party", "vitals", "effects", "kit"];
  var CTX_GROUP_LABEL = {
    msg: "Messages",
    target: "Enemy",
    feed: "Feed",
    tracker: "Quests",
    alert: "Settlement",
    mines: "Mines",
    banner: "Banners",
    cross: "Crosshair",
    stealth: "Stealth",
    subs: "Subtitles",
    compass: "Compass",
    party: "Companions",
    vitals: "Vitals",
    effects: "Effects",
    kit: "Weapon"
  };
  /* listed from highest priority down; anything not named in hide/dim is shown */
  var CTX = [{
      id: "hidden",
      label: "HUD toggled off",
      hide: "*"
    },
    {
      id: "photo",
      label: "Photo mode",
      hide: "*"
    },
    {
      id: "menu",
      label: "Pip-Boy and menus",
      hide: "*"
    },
    {
      id: "scene",
      label: "Animation scene",
      hide: "*"
    },
    {
      id: "powerarmor",
      label: "Power armor",
      hide: ["msg", "target", "feed", "banner", "cross", "stealth", "subs", "compass", "vitals", "kit"]
    },
    {
      id: "vats",
      label: "VATS",
      hide: ["mines", "alert", "cross", "tracker", "feed", "compass", "msg", "banner", "target", "stealth"],
      dim: ["party"]
    },
    {
      id: "dialogue",
      label: "Dialogue",
      hide: ["mines", "subs", "cross", "target", "kit", "vitals", "compass", "tracker", "stealth"],
      dim: ["effects", "alert"]
    },
    {
      id: "workshop",
      label: "Workshop mode",
      hide: ["mines", "cross", "kit", "effects", "vitals", "party", "tracker", "target", "stealth", "compass"]
    },
    {
      id: "scope",
      label: "Scope",
      hide: ["cross", "tracker", "party", "msg"],
      dim: ["feed", "compass", "vitals", "alert"]
    },
    {
      id: "wheel",
      label: "Wheel menu",
      hide: ["cross", "target", "msg"],
      dim: ["mines", "tracker", "alert", "feed", "compass", "party", "vitals", "effects", "kit", "banner", "stealth"]
    },
    {
      id: "ads",
      label: "Iron sights",
      hide: ["cross"],
      dim: ["tracker", "alert", "party", "feed", "msg"]
    },
    {
      id: "calm",
      label: "Out of combat",
      hide: []
    },
    {
      id: "explore",
      label: "Exploring and combat",
      hide: []
    }
  ];
  /* open game menus -> situation. Vanilla names are the game's own; mod menu names are taken from their
     files and should be confirmed in game (PrismaDebugger can log open menus). */
  var MENU_CTX = {
    PipboyMenu: "menu",
    ContainerMenu: "menu",
    BarterMenu: "menu",
    ExamineMenu: "menu",
    TerminalMenu: "menu",
    LockpickingMenu: "menu",
    LevelUpMenu: "menu",
    SleepWaitMenu: "menu",
    PauseMenu: "menu",
    Console: "menu",
    LooksMenu: "menu",
    BookMenu: "menu",
    LoadingMenu: "menu",
    MainMenu: "menu",
    PowerArmorModMenu: "menu",
    SPECIALMenu: "menu",
    CreditsMenu: "menu",
    CookingMenu: "menu",
    VATSMenu: "vats",
    DialogueMenu: "dialogue",
    ScopeMenu: "scope",
    WorkshopMenu: "workshop",
    FavoritesMenu: "wheel",
    WheelMenu: "wheel",
    ScreenArcherMenu: "photo",
    PhotoMenu: "photo",
    NAFMenu: "menu",
    NAFStudioMenu: "menu",
    FalloutEmotions2Menu: "menu",
    OCM_outfitChangeMenu: "menu",
    AddItemMenu: "menu",
    TextInputMenu: "menu"
  };
  S.ctx = {
    menus: [],
    powerArmor: false,
    ads: false,
    thirdPerson: false,
    scene: false,
    photo: false,
    hudHidden: false,
    prismaFocus: false,
    rules: {}
  };
  var calmTimer = null,
    calmSince = null,
    calmReady = false,
    calmWake = {},
    calmObserved = {};

  function observeCalm(group, value) {
    var signature = JSON.stringify(value);
    if (calmObserved[group] !== signature) {
      calmObserved[group] = signature;
      wakeCalm(group);
    }
  }

  function updateCalm() {
    clearTimeout(calmTimer);
    calmTimer = null;
    if (S.combat) {
      calmSince = null;
      calmReady = false;
    } else {
      if (calmSince === null) calmSince = Date.now();
      var left = S.settings.calmDelay * 1000 - (Date.now() - calmSince);
      calmReady = left <= 0;
      if (!calmReady) calmTimer = setTimeout(updateCalm, left);
    }
    renderContext();
  }

  function wakeCalm(group) {
    clearTimeout(calmWake[group]);
    calmWake[group] = setTimeout(function() {
      delete calmWake[group];
      renderContext();
    }, 5000);
    if (S.settings) renderContext();
  }

  function ctxActive() {
    var c = S.ctx,
      on = {
        explore: true
      };
    if (!S.combat && calmReady) on.calm = true;
    c.menus.forEach(function(m) {
      if (MENU_CTX[m]) on[MENU_CTX[m]] = true;
    });
    if (c.prismaFocus) on.menu = true;
    if (c.powerArmor) on.powerarmor = true;
    if (c.ads) on.ads = true;
    if (c.scene) on.scene = true;
    if (c.photo) on.photo = true;
    if (c.hudHidden) on.hidden = true;
    for (var i = 0; i < CTX.length; i++)
      if (on[CTX[i].id]) return CTX[i];
    return CTX[CTX.length - 1];
  }

  function ctxRule(cx, g) {
    if (cx.id === "calm") return {
      show: "s",
      dim: "d",
      hide: "h"
    } [S.settings.calmRules[g]] || "s";
    var o = S.ctx.rules[cx.id];
    if (o === "show") return "s";
    if (o === "hide") return "h";
    if (cx.hide === "*" || (cx.hide || []).indexOf(g) >= 0) return "h";
    if ((cx.dim || []).indexOf(g) >= 0) return "d";
    return "s";
  }

  function renderContext() {
    var cx = ctxActive();
    stage.classList.toggle("wide-layout", IN_GAME && !S.ctx.powerArmor);
    Array.prototype.forEach.call(stage.querySelectorAll("[data-g]"), function(el) {
      var g = el.getAttribute("data-g"),
        r = ctxRule(cx, g);
      if (cx.id === "calm") {
        if (g === "cross" && !el.hasAttribute("data-crosshair")) r = "s";
        if (calmWake[g] || el.closest(".awake") || el.querySelector(".awake") ||
          (g === "vitals" && S.hpMax > 0 && S.hp / S.hpMax < .35)) r = "s";
      }
      // Only the reticle is exempt; prompt, quick loot and the other cross-group widgets retain their rules.
      if ((el.hasAttribute("data-crosshair") || el.hasAttribute("data-hitmarker")) && cx.id === "ads" && S.ctx.thirdPerson &&
        !S.settings.hideThirdPersonAimCrosshair && S.ctx.rules.ads !== "hide") r = "s";
      if (el.hasAttribute("data-crosshair") && !S.settings.showCrosshair) r = "h";
      if (el.hasAttribute("data-hitmarker") && !S.settings.showHitMarkers) r = "h";
      el.classList.toggle("ctx-hide", r === "h");
      el.classList.toggle("ctx-dim", r === "d");
    });
    stage.setAttribute("data-ctx", cx.id);
    S.ctxId = cx.id;
    renderRulesTable();
    if (window.__ctxMock) window.__ctxMock(cx);
  }

  function renderRulesTable() {
    var t = $("rulesTable");
    if (!t) return;
    var cur = S.ctxId,
      html = "<tr><th>Situation</th>" + CTX_GROUPS.map(function(g) {
        return "<th>" + CTX_GROUP_LABEL[g] + "</th>";
      }).join("") + "</tr>";
    CTX.forEach(function(cx) {
      html += '<tr' + (cx.id === cur ? ' class="cur"' : "") + "><td>" + cx.label + "</td>" + CTX_GROUPS.map(function(g) {
        var r = ctxRule(cx, g);
        return '<td class="' + r + '">' + (r === "s" ? "●" : r === "d" ? "◐" : "·") + "</td>";
      }).join("") + "</tr>";
    });
    if (t._h !== html) {
      t.innerHTML = html;
      t._h = html;
    }
  }

  function setContext(c) {
    ["menus", "powerArmor", "ads", "thirdPerson", "scene", "photo", "hudHidden", "prismaFocus"].forEach(function(k) {
      if (c[k] !== undefined) S.ctx[k] = k === "menus" ? (c.menus || []).slice() : !!c[k];
    });
    if (c.rules) Object.assign(S.ctx.rules, c.rules);
    renderContext();
  }

  /* ---------- karma (KARMA, Nexus 80425) ----------
     alignment -2..2 from KARM_PlayerAlignmentValue (breakpoints +/-250 and +/-750); title is the mod's
     level-dependent karma title. */
  var ALIGN = {
    "-2": "Very evil",
    "-1": "Evil",
    "0": "Neutral",
    "1": "Good",
    "2": "Very good"
  };
  S.karma = null;

  function karmaAlignFor(v) {
    return v >= 750 ? 2 : v >= 250 ? 1 : v <= -750 ? -2 : v <= -250 ? -1 : 0;
  }

  function setKarma(k) {
    var prev = S.karma,
      cur = Object.assign({}, prev || {}, k);
    if (cur.value !== undefined && k.alignment === undefined) cur.alignment = karmaAlignFor(cur.value);
    S.karma = cur;
    var el = $("karma");
    el.hidden = false;
    el.className = "karma k" + (cur.alignment || 0);
    el.classList.toggle("off", !S.settings.karmaReadout);
    $("karmaAlign").textContent = ALIGN[String(cur.alignment || 0)];
    $("karmaTitle").textContent = cur.title || "";
    if (k.change) {
      var up = k.change > 0;
      notify((up ? "+" : "") + k.change + " Karma", k.reason || (up ? "Good deed" : "Bad deed"), up ? "item" : "bad", 4500, "karma");
    }
    if (prev && prev.alignment !== undefined && prev.alignment !== cur.alignment) {
      notify("Karma: " + ALIGN[String(cur.alignment)], cur.title ? "You're now known as " + cur.title : "", cur.alignment > prev.alignment ? "level" : "bad", 7000, "karma");
      el.classList.remove("flash");
      void el.offsetWidth;
      el.classList.add("flash");
      setTimeout(function() {
        el.classList.remove("flash");
      }, 3000);
    }
    wake($("vitals"));
  }

  /* ---------- reputation (REPUTATION, Nexus 79394) ----------
     New Vegas style fame and infamy per faction; title is one of the mod's 12 (Idolized ... Vilified, Wild Child...). */
  var REP_GOOD = {
      "Idolized": 1,
      "Liked": 1,
      "Accepted": 1
    },
    REP_BAD = {
      "Shunned": 1,
      "Hated": 1,
      "Vilified": 1
    };

  function capFirst(t) {
    t = String(t || "");
    return t.charAt(0).toUpperCase() + t.slice(1);
  }

  function repTone(title) {
    return REP_GOOD[title] ? "fame" : REP_BAD[title] ? "infamy" : title === "Neutral" ? "" : "mixed";
  }
  S.rep = {};

  function setReputation(r) {
    var prev = S.rep[r.faction] || {};
    var cur = S.rep[r.faction] = Object.assign({}, prev, r);
    var ev = r.event;
    if (ev === "discovered") notify("Discovered: " + r.faction, "Your reputation with them starts now", "radio", 5000, "star");
    else if (ev === "fameGained" || ev === "fameLost" || ev === "infamyGained" || ev === "infamyLost") {
      var good = ev === "fameGained" || ev === "infamyLost";
      notify(capFirst(r.faction) + ": " + {
        fameGained: "Fame gained",
        fameLost: "Fame lost",
        infamyGained: "Infamy gained",
        infamyLost: "Infamy lost"
      } [ev], cur.title ? cur.title : "", good ? "item" : "bad", 5000, "star");
    } else if (ev === "hitSquad") notify("A " + r.faction + " hit squad is hunting you", "Your infamy caught up with you", "bad", 8000, "hitsquad");
    else if (ev === "gift") notify(capFirst(r.faction) + " sent you a gift", r.detail || "Check your inventory", "item", 6000, "gift");
    if (prev.title && cur.title && prev.title !== cur.title && ev !== "discovered")
      notify(capFirst(r.faction) + ": you're now " + cur.title, "Your reputation with them changed", repTone(cur.title) === "infamy" ? "bad" : "level", 7000, "star");
    renderLocalRep();
  }
  S.localFaction = null;

  function renderLocalRep() {
    var f = S.localFaction,
      r = f && S.rep[f],
      el = $("lrep");
    var show = !!(r && r.title) && S.settings.localReputation;
    el.hidden = !show;
    $("repSep").hidden = !show;
    if (show) {
      el.className = "lrep " + repTone(r.title);
      $("lrepTxt").textContent = r.title;
    }
  }

  /* ---------- tracked challenges (CHALLENGES, Nexus 76347) ---------- */
  S.challenges = [];
  var chEls = {};

  function setChallenges(list) {
    var box = $("chal"),
      ul = $("chList"),
      keep = {};
    S.challenges = list || [];
    S.challenges.forEach(function(c) {
      var li = chEls[c.id];
      if (!li) {
        li = chEls[c.id] = document.createElement("li");
        li.innerHTML = '<div class="ch-row"><span></span><small></small></div><div class="ch-bar"><i></i></div>';
        ul.appendChild(li);
      }
      keep[c.id] = 1;
      var prevCur = li._cur;
      li.querySelector(".ch-row span").textContent = c.name;
      li.querySelector(".ch-row small").textContent = c.cur + " / " + c.max;
      li.querySelector(".ch-bar i").style.width = (clamp(c.cur / c.max, 0, 1) * 100) + "%";
      if (prevCur !== undefined && c.cur > prevCur) {
        li.classList.remove("tick");
        void li.offsetWidth;
        li.classList.add("tick");
        setTimeout(function() {
          li.classList.remove("tick");
        }, 700);
      }
      li._cur = c.cur;
    });
    Object.keys(chEls).forEach(function(id) {
      if (!keep[id]) {
        var li = chEls[id];
        delete chEls[id];
        li.classList.add("leave");
        setTimeout(function() {
          li.remove();
        }, 400);
      }
    });
    box.classList.toggle("show", S.challenges.length > 0);
  }

  function challengeComplete(c) {
    var known = S.challenges.filter(function(x) {
      return x.id === c.id;
    })[0];
    if (known) {
      if (known.done) return;
      known.done = true;
    }
    var li = chEls[c.id];
    if (li) {
      li.classList.add("done");
      li.querySelector(".ch-row small").textContent = "Complete";
      li.querySelector(".ch-bar i").style.width = "100%";
    }
    quest({
      tone: "good",
      eyebrow: "Challenge complete",
      title: c.name,
      objectives: [{
        t: c.reward || "Reward granted",
        done: true
      }],
      ms: 6000
    });
    notify("Challenge complete: " + c.name, c.reward || "", "level", 6000, "medal");
    if (c.xp) addXP(c.xp);
    setTimeout(function() {
      setChallenges(S.challenges.filter(function(x) {
        return x.id !== c.id;
      }));
    }, 2500);
  }

  /* ---------- mines ----------
     The plugin finds armed mines that are counting down at the player (what Mine Detector HUD detects) and sends
     their screen position each frame. Off-screen mines send a bearing relative to the camera instead. */
  S.mines = [];
  var mineEls = {};
  var MINE_TRI = '<svg viewBox="0 0 44 44"><circle class="trk" cx="22" cy="22" r="18"/><circle class="val" cx="22" cy="22" r="18" pathLength="100" transform="rotate(-90 22 22)"/><path class="tri" d="M22 11.5l9.5 16.5h-19z"/><path class="ex" d="M22 17.5v5M22 25.2v.1"/></svg><small></small>';

  function renderMines() {
    var layer = $("mineLayer"),
      dir = $("mineDir"),
      keep = {},
      nearest = null,
      dirHtml = "";
    S.mines.forEach(function(m) {
      var frac = m.total ? clamp(m.left / m.total, 0, 1) : 1,
        hot = frac < 0.35;
      if (!nearest || m.dist < nearest.dist) nearest = m;
      if (m.onScreen === false) {
        dirHtml += '<i style="transform:rotate(' + (+m.bearing || 0).toFixed(1) + 'deg)"></i>';
        return;
      }
      var el = mineEls[m.id];
      if (!el) {
        el = mineEls[m.id] = document.createElement("div");
        el.className = "mn";
        el.innerHTML = MINE_TRI;
        layer.appendChild(el);
      }
      keep[m.id] = 1;
      var sc = clamp(1.45 - m.dist / 35, 0.55, 1.45);
      el.style.left = m.x.toFixed(1) + "px";
      el.style.top = m.y.toFixed(1) + "px";
      el.style.transform = "scale(" + sc.toFixed(2) + ")";
      el.classList.toggle("hot", hot);
      el.style.setProperty("--beep", (0.12 + 0.55 * frac).toFixed(2) + "s");
      el.querySelector(".val").setAttribute("stroke-dasharray", (frac * 100).toFixed(1) + " 100");
      el.querySelector("small").textContent = Math.round(m.dist) + " m";
    });
    Object.keys(mineEls).forEach(function(id) {
      if (!keep[id]) {
        mineEls[id].remove();
        delete mineEls[id];
      }
    });
    if (dir._h !== dirHtml) {
      dir.innerHTML = dirHtml;
      dir._h = dirHtml;
    }
    var chip = $("mineChip");
    /* The game's own prompt already names the mine with its real keys (Take / Equip), so the extra name chip stays
       hidden; the marker on the mine carries the distance and the fuse. */
    if (false && nearest) {
      var hotN = nearest.total && nearest.left / nearest.total < 0.35;
      chip.className = "w mchip show" + (hotN ? " hot" : "");
      var html = '<svg viewBox="0 0 14 14"><path d="M7 1.5l5.5 10h-11z"/></svg>' + esc(nearest.name || "Mine") + " <b>" + Math.round(nearest.dist) + " m</b>" +
        (nearest.left !== undefined ? " <b>" + Math.max(0, nearest.left).toFixed(1) + "s</b>" : "") +
        (nearest.disarm ? '<span class="kbd">E</span> Disarm' : "");
      if (chip._h !== html) {
        chip.innerHTML = html;
        chip._h = html;
      }
    } else chip.className = "w mchip";
    /* compass: one marker per mine that has an absolute bearing */
    var ids = {};
    S.mines.forEach(function(m) {
      if (m.b === undefined || !S.settings.mineCompass) return;
      var id = "mine:" + m.id;
      ids[id] = 1;
      var mk = S.markers.filter(function(x) {
        return x.id === id;
      })[0];
      if (!mk) {
        mk = {
          id: id,
          t: "mine",
          label: m.name || "Mine"
        };
        S.markers.push(mk);
      }
      mk.b = m.b;
      mk.dist = m.dist;
    });
    var before = S.markers.length;
    S.markers = S.markers.filter(function(x) {
      return String(x.id).indexOf("mine:") !== 0 || ids[x.id];
    });
    if (before !== S.markers.length || Object.keys(ids).length) renderCompass(true);
  }

  function mineBoom(m) {
    if (m.onScreen === false || m.x === undefined) return;
    var b = document.createElement("div");
    b.className = "mboom";
    b.style.left = m.x + "px";
    b.style.top = m.y + "px";
    $("mineLayer").appendChild(b);
    setTimeout(function() {
      b.remove();
    }, 650);
  }

  function setMines(list, exploded) {
    if (!S.settings.mineWarnings) list = [];
    if (exploded)(Array.isArray(exploded) ? exploded : [exploded]).forEach(function(id) {
      var m = S.mines.filter(function(x) {
        return x.id === id;
      })[0];
      if (m) mineBoom(m);
    });
    S.mines = (list || []).map(function(m) {
      return Object.assign({}, m);
    });
    renderMines();
  }

  /* ---------- settlement attacks ----------
     SKK Settlement Attack System holds the attack until you arrive, then spawns 2-64 attackers.
     States: pending (on the way), active (fighting), won, lost (vanilla: you didn't make it), ended (called off). */
  S.attack = null;

  function attackMarker(a) {
    if (a.b === undefined && !S.markers.some(function(m) {
        return m.id === "atk";
      })) return;
    var m = S.markers.filter(function(x) {
      return x.id === "atk";
    })[0];
    if (!m) {
      m = {
        id: "atk",
        t: "defend",
        active: true
      };
      S.markers.push(m);
      m._new = Date.now();
    }
    if (a.b !== undefined) m.b = a.b;
    if (a.dist !== undefined) m.dist = a.dist;
    m.label = a.name;
    renderCompass(true);
  }

  function renderAttack() {
    var a = S.attack,
      el = $("salert");
    if (!a) {
      el.className = "w salert";
      return;
    }
    // A pending attack only gets the card once you're close (100 m); until then the compass marker carries it.
    var mk = S.markers.filter(function(x) {
        return x.id === "atk";
      })[0],
      far = a.dist !== undefined ? a.dist : (mk && mk.dist);
    if (a.state === "pending" && far !== undefined && far > 100) {
      el.className = "w salert";
      return;
    }
    el.className = "w salert show " + a.state;
    $("saEye").textContent = {
      pending: "Under attack",
      active: "Defend",
      won: "Defended",
      lost: "Settlement hit",
      ended: "Attack over"
    } [a.state] || "Under attack";
    $("saName").textContent = a.name;
    var meta = [];
    if (a.faction) meta.push(a.faction);
    if (a.total) meta.push(a.total + " attackers");
    if (a.defense !== undefined) meta.push("Defense " + a.defense);
    $("saMeta").textContent = meta.join(" · ");
    var m = S.markers.filter(function(x) {
        return x.id === "atk";
      })[0],
      dist = a.dist !== undefined ? a.dist : (m && m.dist);
    var line = $("saLine");
    if (a.state === "pending") {
      line.hidden = false;
      line.innerHTML = (dist !== undefined ? "<b>" + esc(fmtDist(dist)) + "</b>" : "") + esc(a.hoursLeft !== undefined ? fmtGame(a.hoursLeft * 3600) + " until it resolves without you" : "Waits for you to arrive");
    } else if (a.state === "won") {
      line.hidden = false;
      line.textContent = "All attackers down";
    } else if (a.state === "lost") {
      line.hidden = false;
      line.textContent = "You didn't make it in time. Check for damage.";
    } else if (a.state === "ended") {
      line.hidden = false;
      line.textContent = "The attackers pulled back";
    } else line.hidden = true;
    var bar = $("saBar");
    bar.hidden = a.state !== "active";
    if (a.state === "active" && a.total) {
      var n = Math.min(a.total, 20),
        on = Math.ceil(clamp(a.remaining / a.total, 0, 1) * n),
        segs = $("saSegs");
      if (segs.children.length !== n) segs.innerHTML = new Array(n + 1).join("<i></i>");
      Array.prototype.forEach.call(segs.children, function(x, i) {
        x.className = i < on ? "on" : "";
      });
      $("saCount").textContent = a.remaining + " / " + a.total + " left";
    }
    $("saVb").hidden = !(a.vertibird && (a.state === "pending" || a.state === "active"));
  }

  function setAttack(x) {
    clearTimeout(setAttack._t);
    if (x === null) {
      S.attack = null;
      S.markers = S.markers.filter(function(m) {
        return m.id !== "atk";
      });
      renderAttack();
      renderCompass(true);
      return;
    }
    var prev = S.attack,
      same = prev && (x.id === undefined || prev.id === x.id);
    var a = same ? Object.assign(prev, x) : Object.assign({
      id: x.id || x.name,
      state: "pending"
    }, x);
    if (a.remaining === undefined && a.total) a.remaining = a.total;
    S.attack = a;
    attackMarker(a);
    var was = same ? prev._shown : null;
    if (!same) {
      quest({
        tone: "alert",
        eyebrow: "Settlement under attack",
        title: a.name,
        objectives: [{
          t: (a.faction ? a.faction + " · " : "") + (a.total ? a.total + " attackers" : "Attackers incoming")
        }, {
          t: "Defend the settlement"
        }],
        ms: 6000
      });
      notify(a.name + " is under attack", (a.faction || "Attackers") + (a.dist !== undefined ? " · " + fmtDist(a.dist) : ""), "bad", 7000, "warn");
    } else if (was !== a.state) {
      if (a.state === "active") notify("Defend " + a.name, a.total ? a.total + " attackers" : "", "bad", 5000, "warn");
      if (a.state === "won") {
        quest({
          tone: "good",
          eyebrow: "Settlement defended",
          title: a.name,
          objectives: [{
            t: "All attackers down",
            done: true
          }],
          ms: 6000
        });
        notify(a.name + " defended", "", "level");
      }
      if (a.state === "lost") notify(a.name + " was hit", "You didn't make it in time", "bad", 6000, "warn");
      if (a.state === "ended") notify("Attack on " + a.name + " is over", "", "item");
    }
    a._shown = a.state;
    if (a.state === "won" || a.state === "lost" || a.state === "ended") {
      S.markers = S.markers.filter(function(m) {
        return m.id !== "atk";
      });
      renderCompass(true);
      setAttack._t = setTimeout(function() {
        if (S.attack === a) {
          S.attack = null;
          renderAttack();
        }
      }, 6000);
    }
    renderAttack();
  }

  /* ---------- survival ---------- */
  S.survival = {
    on: false,
    fatigue: 0,
    enemyMarkers: false,
    stageNames: true,
    stages: null,
    adrenaline: {
      rank: 0,
      kills: 0,
      perRank: 5,
      bonus: ""
    },
    showAdrenaline: true
  };
  S.weight = {
    cur: 0,
    max: 300,
    show: "near"
  };
  var ILLNESS = {
    fatigue: {
      name: "Fatigue",
      icon: "sleepy",
      hours: 144,
      desc: "Sleep wears off faster"
    },
    infection: {
      name: "Infection",
      icon: "infection",
      hours: 72,
      desc: "Slow bleed, can't drop you below 1 HP"
    },
    insomnia: {
      name: "Insomnia",
      icon: "insomnia",
      hours: 60,
      desc: "Sleep only does half as much"
    },
    lethargy: {
      name: "Lethargy",
      icon: "lethargy",
      hours: 96,
      desc: "AP regenerates 50% slower"
    },
    parasites: {
      name: "Parasites",
      icon: "parasites",
      hours: 168,
      desc: "Food only does half as much"
    },
    weakness: {
      name: "Weakness",
      icon: "weakness",
      hours: 48,
      desc: "You take 20% more damage"
    }
  };

  function catchIllness(id, hoursLeft, quiet) {
    var d = ILLNESS[id];
    if (!d) return;
    var fresh = !hasEffect("ill_" + id);
    addEffect({
      id: "ill_" + id,
      name: d.name,
      kind: "bad",
      icon: d.icon,
      hours: d.hours,
      hoursLeft: hoursLeft
    });
    if (fresh && !quiet) notify("You caught " + d.name, d.desc + " · antibiotics cure it", "bad", 7000, d.icon);
  }

  function cureIllness(id) {
    var ids = id ? [id] : Object.keys(ILLNESS),
      cured = [];
    ids.forEach(function(k) {
      if (hasEffect("ill_" + k)) {
        removeEffect("ill_" + k, false);
        cured.push(ILLNESS[k].name);
      }
    });
    return cured;
  }

  function renderAdrenaline() {
    var a = S.survival.adrenaline,
      el = $("adren");
    el.hidden = !(S.survival.on && S.survival.showAdrenaline && (a.rank > 0 || a.kills > 0));
    var segs = $("adSegs");
    if (segs.children.length !== 10) {
      segs.innerHTML = new Array(11).join("<i></i>");
    }
    Array.prototype.forEach.call(segs.children, function(seg, i) {
      seg.className = i < a.rank ? "on" : (i === a.rank && a.kills > 0 ? "part" : "");
      seg.style.setProperty("--p", (a.kills / a.perRank * 100) + "%");
    });
    $("adRank").textContent = a.rank + (a.bonus ? " · " + a.bonus : "");
  }
  var wasOver = false;

  function renderWeight() {
    var w = S.weight,
      f = w.max > 0 ? w.cur / w.max : 0,
      el = cores.wt.el;
    setCore("wt", Math.min(f, 1), Math.round(w.cur) + "/" + Math.round(w.max), Math.min(f, 1));
    var show = w.show === "always" || (w.show === "near" && f >= 0.85);
    if (el.classList.contains("away") === show) {
      el.classList.toggle("away", !show);
      layoutCoresSoon();
    }
    el.classList.toggle("warn", f >= 0.85 && f <= 1);
    el.classList.toggle("over", f > 1);
    var lbl = el.querySelector("small");
    if (lbl.firstChild.nodeType === 3) lbl.firstChild.nodeValue = f > 1 ? "Overweight" : "Weight";
    if ((f > 1) !== wasOver) {
      wasOver = f > 1;
      layoutCoresSoon();
    }
  }

  function setSurvival(x) {
    var wasOver = S.weight.cur > S.weight.max;
    if (x.on !== undefined) {
      S.survival.on = !!x.on;
      stage.classList.toggle("nosurvival", !S.survival.on);
      layoutCoresSoon();
    }
    ["fatigue", "enemyMarkers", "stageNames", "stages", "showAdrenaline"].forEach(function(k) {
      if (x[k] !== undefined) S.survival[k] = x[k];
    });
    if (x.adrenaline) {
      var a = S.survival.adrenaline,
        before = a.rank;
      Object.assign(a, x.adrenaline);
      if (a.rank > before) pop($("adren"), "Adrenaline " + a.rank);
      wake($("kit"));
    }
    renderVitals();
    renderNeeds();
    renderAdrenaline();
    renderCompass(true);
  }

  function setWeight(x) {
    var wasOver = S.weight.cur > S.weight.max;
    Object.assign(S.weight, x);
    var over = S.weight.cur > S.weight.max;
    if (over && !wasOver) notify("Overencumbered", S.survival.on ? "Endurance and Agility drop, and your legs take damage" : "You can't run or fast travel", "bad", 6000, "warn");
    renderWeight();
    wake($("vitals"));
  }

  function setCombat(on) {
    S.combat = !!on;
    stage.classList.toggle("combat", S.combat);
    var b = $("b-combat");
    if (b) b.setAttribute("aria-pressed", S.combat ? "true" : "false");
    if (S.settings) updateCalm();
  }

  function hurtFlash() {
    var f = $("fxHurt");
    f.classList.add("on");
    setTimeout(function() {
      f.classList.remove("on");
    }, 160);
  }

  function kick() {
    wake($("kit"));
    wakeCalm("cross");
    var r = $("ring");
    r.classList.add("kick");
    setTimeout(function() {
      r.classList.remove("kick");
    }, 70);
  }

  function hitmark(kill) {
    if (!S.settings.showHitMarkers) return;
    var h = $("hit");
    h.classList.toggle("kill", !!kill);
    h.classList.add("on");
    clearTimeout(h._h);
    h._h = setTimeout(function() {
      h.classList.remove("on");
    }, 90);
  }

  /* ---------- active effects (chems, meds, drinks, addictions) ---------- */
  var FXI = {
    bolt: '<path class="ic" d="M24.5 12l-8 12h6l-2 8 8-12h-6z"/>',
    shield: '<path class="ic" d="M22 12.5l8 3v5.5c0 5-3.4 8.6-8 10.5-4.6-1.9-8-5.5-8-10.5v-5.5z"/><path class="ic" d="M18.5 22l2.5 2.5 4.5-5"/>',
    hourglass: '<path class="ic" d="M16 13h12M16 31h12M17.5 13c0 5.5 9 5.5 9 9s-9 3.5-9 9M26.5 13c0 5.5-9 5.5-9 9s9 3.5 9 9"/>',
    dumbbell: '<path class="ic" d="M14 22h16M15 18v8M18 16v12M29 18v8M26 16v12"/>',
    capsule: '<g transform="rotate(-45 22 22)"><rect class="ic" x="13" y="18.5" width="18" height="7" rx="3.5"/><path class="ic" d="M22 18.5v7"/></g>',
    syringe: '<path class="ic" d="M27.5 12.5l4 4M29.5 14.5L19 25l-3-3 10.5-10.5M17.5 23.5L13 28M20.5 20.5l1.5 1.5M23.5 17.5l1.5 1.5"/>',
    ivbag: '<path class="ic" d="M16 12.5h12V25a6 6 0 0 1-12 0z"/><path class="ic" d="M19.5 12.5V10h5v2.5M22 31v3.5M19 18h6M19 21.5h4"/>',
    bottle: '<path class="ic" d="M20 12h4v4.5l2.5 3.5V32h-9V20l2.5-3.5z"/><path class="ic" d="M17.5 23h9"/>',
    food: '<path class="ic" d="M16 12v7.5a2.5 2.5 0 0 0 5 0V12M18.5 12v20M27.5 12c-2.4 0-3.5 3-3.5 7s1.2 4.5 3.5 4.5V32"/>',
    warn: '<path class="ic" d="M22 13l10 17H12z"/><path class="ic" d="M22 19.5v5M22 27.3v.1"/>',
    infection: '<path class="ic" d="M22 11.5c3.6 4.6 6 7.6 6 10.6a6 6 0 0 1-12 0c0-3 2.4-6 6-10.6z"/><path class="ic" d="M22 19.5v6M19 22.5h6"/>',
    parasites: '<path class="ic" d="M13 25c2.2-4 4.3-4 6.5 0s4.3 4 6.5 0 4.3-4 4.5-1"/><circle class="ic" cx="31.5" cy="21" r="1.6"/>',
    insomnia: '<path class="ic" d="M12.5 23s3.6-5.5 9.5-5.5 9.5 5.5 9.5 5.5-3.6 5.5-9.5 5.5-9.5-5.5-9.5-5.5z"/><circle class="ic" cx="22" cy="23" r="2.3"/><path class="ic" d="M28 10.5a3.4 3.4 0 1 0 3.5 4.6 3 3 0 0 1-3.5-4.6z"/>',
    lethargy: '<rect class="ic" x="13" y="17" width="16" height="10" rx="1.5"/><path class="ic" d="M29 20.5h2v3h-2M15.5 19.5v5"/>',
    weakness: '<path class="ic" d="M22 12.5l8 3v5.5c0 5-3.4 8.6-8 10.5-4.6-1.9-8-5.5-8-10.5v-5.5z"/><path class="ic" d="M23 14l-2.5 5.5 3 2-3 6"/>',
    karma: '<circle class="ic" cx="22" cy="22" r="9.5"/><path class="ic" d="M22 12.5a4.75 4.75 0 0 1 0 9.5 4.75 4.75 0 0 0 0 9.5"/>',
    star: '<path class="ic" d="M22 12l2.7 5.4 6 .9-4.3 4.2 1 5.9L22 25.6l-5.4 2.8 1-5.9-4.3-4.2 6-.9z"/>',
    medal: '<circle class="ic" cx="22" cy="25.5" r="6"/><path class="ic" d="M18 12l4 7.5 4-7.5M22 23v3l1.4.9"/>',
    hitsquad: '<circle class="ic" cx="22" cy="22" r="8"/><path class="ic" d="M22 11v5M22 28v5M11 22h5M28 22h5"/><circle class="ic" cx="22" cy="22" r="1.2"/>',
    gift: '<rect class="ic" x="14" y="19" width="16" height="12" rx="1"/><path class="ic" d="M13 15.5h18V19H13zM22 15.5V31M22 15.5c-2-3.5-6-3.5-5 0M22 15.5c2-3.5 6-3.5 5 0"/>',
    sleepy: '<path class="ic" d="M15 15h6l-6 7h6M24 21h5l-5 6h5"/>',
    bed: '<path class="ic" d="M12 28V15M12 23h20v5M12 19.5h6v3.5M18 19.5h11a3 3 0 0 1 3 3V23"/>'
  };
  var CIRC_FX = 2 * Math.PI * 19;
  S.effects = [];
  var fxPaused = false;
  /* game-time effects (illnesses, Well Rested) count down at the game's timescale and read in days/hours */
  function fmtGame(sec) {
    var h = Math.max(0, sec / 3600);
    if (h >= 24) return Math.floor(h / 24) + "d " + Math.floor(h % 24) + "h";
    if (h >= 1) return Math.floor(h) + "h " + ("0" + Math.floor((h % 1) * 60)).slice(-2) + "m";
    return Math.ceil(h * 60) + "m";
  }

  function fmt(s) {
    s = Math.max(0, Math.ceil(s));
    var m = Math.floor(s / 60),
      r = s % 60;
    return m >= 60 ? Math.floor(m / 60) + "h" + ("0" + (m % 60)).slice(-2) : m + ":" + ("0" + r).slice(-2);
  }

  function drawEffect(e) {
    var el = e.el;
    var frac = e.duration > 0 ? clamp(e.remaining / e.duration, 0, 1) : 1;
    el._val.style.strokeDashoffset = CIRC_FX * (1 - frac);
    el._time.textContent = e.duration > 0 ? (e.game ? fmtGame(e.remaining) : fmt(e.remaining)) : "∞";
    el.classList.toggle("ending", e.duration > 0 && e.remaining <= (e.game ? 3600 : 10));
  }

  function addEffect(e) {
    if (e.hours !== undefined) {
      e = Object.assign({}, e, {
        game: true,
        duration: e.hours * 3600,
        remaining: (e.hoursLeft !== undefined ? e.hoursLeft : e.hours) * 3600
      });
    }
    var dur = +(e.duration || 0);
    var cur = S.effects.filter(function(x) {
      return x.id === e.id;
    })[0];
    if (cur) {
      cur.duration = dur;
      cur.remaining = e.remaining !== undefined ? +e.remaining : dur;
      cur.el.classList.remove("flash");
      void cur.el.offsetWidth;
      cur.el.classList.add("flash");
      drawEffect(cur);
      return cur;
    }
    var fx = {
      id: e.id,
      name: e.name || e.id,
      kind: e.kind || "chem",
      icon: e.icon || "bolt",
      game: !!e.game,
      duration: dur,
      remaining: e.remaining !== undefined ? +e.remaining : dur
    };
    var el = document.createElement("div");
    el.className = "fx " + fx.kind;
    el.innerHTML = '<svg viewBox="0 0 44 44"><circle class="trk" cx="22" cy="22" r="19"/><circle class="val" cx="22" cy="22" r="19" transform="rotate(-90 22 22)"/>' + (FXI[fx.icon] || FXI.bolt) + "</svg><b>" + esc(fx.name) + "</b><small></small>";
    el._val = el.querySelector(".val");
    el._time = el.querySelector("small");
    el._val.style.strokeDasharray = CIRC_FX;
    fx.el = el;
    var box = $("effects");
    box.insertBefore(el, box.firstChild);
    S.effects.push(fx);
    drawEffect(fx);
    requestAnimationFrame(function() {
      requestAnimationFrame(function() {
        el.classList.add("in");
      });
    });
    return fx;
  }

  function removeEffect(id, announce) {
    S.effects = S.effects.filter(function(e) {
      if (e.id !== id) return true;
      e.el.classList.remove("in");
      e.el.classList.add("out");
      setTimeout(function() {
        e.el.remove();
      }, 400);
      if (announce) notify(e.name + " wore off", "", (e.kind === "med" || e.kind === "food") ? "item" : e.kind, 4500, e.icon);
      return false;
    });
  }

  function syncEffects(list) {
    var ids = list.map(function(e) {
      return e.id;
    });
    S.effects.slice().forEach(function(e) {
      if (ids.indexOf(e.id) < 0) removeEffect(e.id, false);
    });
    list.forEach(function(e) {
      var f = addEffect(e);
      f.el.classList.remove("flash");
    });
  }

  function hasEffect(id) {
    return S.effects.some(function(e) {
      return e.id === id;
    });
  }
  var fxLast = performance.now();
  setInterval(function() {
    var now = performance.now(),
      dt = (now - fxLast) / 1000;
    fxLast = now;
    renderFortify();
    if (fxPaused) return;
    tickLocation(dt);
    S.time.hour += dt * S.time.timescale / 3600;
    renderClock();
    S.effects.slice().forEach(function(e) {
      if (e.duration <= 0) return;
      e.remaining -= dt * (e.game ? S.time.timescale : 1);
      if (e.remaining <= 0) removeEffect(e.id, true);
      else drawEffect(e);
    });
    if (window.__fxTick) window.__fxTick(dt);
    $("fxJet").classList.toggle("on", hasEffect("jet"));
  }, 200);

  /* ---------- single entry point for the F4SE plugin ---------- */
  /* settings: mirrors Phosphor's MCM page; the plugin forwards the saved values in one message */
  S.settings = {
    showCrosshair: true,
    showHitMarkers: true,
    calmDelay: 5,
    calmRules: {
      vitals: "show",
      compass: "show",
      kit: "show",
      party: "show",
      tracker: "show",
      effects: "show",
      cross: "show"
    },
    hideThirdPersonAimCrosshair: false,
    showLevelBar: true,
    clock24: false,
    textReadability: "off",
    levelUpBanner: true,
    perkReminder: true,
    showArmorWheel: true,
    karmaReadout: true,
    localReputation: true,
    challengeTracker: true,
    mineWarnings: true,
    mineCompass: true,
    legendaryDropMarkers: true,
    showWeaponLegendary: true,
    armorPieces: true,
    hideSatisfiedNeeds: false,
    hideFullVitals: false,
    locationOnArrival: false,
    showUndiscovered: true,
    textScale: 1,
    iconScale: 1,
    ringScale: 1
  };

  function applySettings(x) {
    var wasArrival = S.settings.locationOnArrival;
    var rules = Object.assign({}, S.settings.calmRules);
    Object.assign(S.settings, x);
    S.settings.calmRules = rules;
    Object.keys(x.calmRules || {}).forEach(function(g) {
      if (g in rules && ["show", "dim", "hide"].indexOf(x.calmRules[g]) >= 0) rules[g] = x.calmRules[g];
    });
    S.settings.calmDelay = isFinite(+S.settings.calmDelay) ? clamp(+S.settings.calmDelay, 0, 15) : 5;
    if (!S.settings.showHitMarkers) {
      clearTimeout($("hit")._h);
      $("hit").classList.remove("on");
    }
    if (x.showWeaponLegendary !== undefined) setWeaponLegendary(S.weaponLegendary);
    updateCalm();
    if (x.textReadability !== undefined) {
      var readability = ["off", "subtle", "strong"].indexOf(x.textReadability) >= 0 ? x.textReadability : "off";
      S.settings.textReadability = readability;
      if (readability === "off") stage.removeAttribute("data-readability");
      else stage.setAttribute("data-readability", readability);
    }
    if (wasArrival !== S.settings.locationOnArrival) locationRemaining = S.settings.locationOnArrival && S.locationName ? 8 : 0;
    renderLocationVisibility();
    renderVitalVisibility();
    setArmor(armorLast);
    $("xp").classList.toggle("off", !S.settings.showLevelBar);
    if (x.clock24 !== undefined) {
      S.time.h24 = !!x.clock24;
      renderClock();
    }
    if (x.hideHealthyFigure !== undefined) S.figure.hideHealthy = !!x.hideHealthyFigure;
    if (x.limbFigure !== undefined) setFigure({
      type: x.limbFigure
    });
    if (x.showWeight !== undefined) S.weight.show = x.showWeight;
    if (x.showAdrenaline !== undefined) S.survival.showAdrenaline = !!x.showAdrenaline;
    if (x.needStageNames !== undefined) S.survival.stageNames = !!x.needStageNames;
    if (x.survivalEnemyMarkers !== undefined) S.survival.enemyMarkers = !!x.survivalEnemyMarkers;
    if (x.mutationNotices !== undefined) S.mutationFeed = x.mutationNotices === "feed";
    if (x.contextRules) Object.assign(S.ctx.rules, x.contextRules);
    if (x.idleOpacity !== undefined) stage.style.setProperty("--idle", clamp(+x.idleOpacity, .2, 1));
    /* accessibility: text (ring captions cap at 130% to keep the vitals row clear of the compass) and compass icon size multipliers (MCM Look > Text size / Compass icon size) */
    if (x.textScale !== undefined) {
      S.settings.textScale = clamp(+x.textScale || 1, .8, 1.5);
      stage.style.setProperty("--ts", S.settings.textScale);
    }
    if (x.iconScale !== undefined) {
      S.settings.iconScale = clamp(+x.iconScale || 1, .8, 2);
      stage.style.setProperty("--is", S.settings.iconScale);
    }
    if (x.ringScale !== undefined) {
      S.settings.ringScale = clamp(+x.ringScale || 1, .8, 1.5);
      stage.style.setProperty("--rs", S.settings.ringScale);
    }
    layoutCoresSoon();
    if (!S.settings.mineWarnings) setMines([]);
    renderPerks();
    renderLimbs();
    renderWeight();
    renderAdrenaline();
    renderNeeds();
    renderLocalRep();
    renderContext();
    $("karma").classList.toggle("off", !S.settings.karmaReadout);
    $("chal").classList.toggle("off", !S.settings.challengeTracker);
    renderCompass(true);
  }

  function hudUpdate(data) {
    try {
      applyUpdate(data);
    } catch (e) {
      console.error("hudUpdate", e);
    }
  }

  function applyUpdate(data) {
    var d = typeof data === "string" ? JSON.parse(data) : (data || {});
    /* in game the stage stays invisible until the first full snapshot, so preview sample values never show */
    if (d.snapshot || d.hp !== undefined) {
      if (stage.classList.contains("awaiting")) {
        calmSince = null;
        updateCalm();
      }
      stage.classList.remove("awaiting");
    }
    if (d.reset) resetTransient();
    if (d.markers) observeCalm("compass", d.markers.map(function(m) {
      return [m.id, m.t, m.label, m.active];
    }));
    if (d.marker || d.markerRemove) wakeCalm("compass");
    if (d.effects) observeCalm("effects", d.effects.map(function(e) {
      return [e.id, e.name, e.kind, e.duration, e.hours];
    }));
    if (d.effect || d.effectRemove) wakeCalm("effects");
    if (d.illness || d.cure) wakeCalm("effects");
    if (d.party) observeCalm("party", d.party.map(function(c) {
      return [c.name, c.state || "following"];
    }));
    if (d.companion !== undefined || d.companionRemove || d.companionReact) wakeCalm("party");
    var vit = false,
      amm = false;
    ["hp", "hpMax", "ap", "apMax", "rads"].forEach(function(k) {
      if (d[k] !== undefined) {
        S[k] = +d[k];
        vit = true;
      }
    });
    if (d.radsGain) {
      if (d.rads === undefined) S.rads += +d.radsGain;
      vit = true;
      pop($("k-rad"), "+" + d.radsGain + " RADS");
    }
    /* in game the plugin sends xpGain together with the exact xp/level after the gain; the gain is then only
       the "+N XP" pop and the bar animates to the real values. The levelUp message does the banner. */
    if (d.xpGain) addXP(+d.xpGain, (d.xp !== undefined || d.level !== undefined) ? {
      xp: d.xp,
      level: d.level
    } : null);
    else {
      if (d.level !== undefined) {
        S.level = +d.level;
        renderXP();
      }
      if (d.xp !== undefined) {
        S.xp = clamp(+d.xp, 0, 1);
        renderXP();
      }
    }
    if (d.perkPoints !== undefined) {
      S.perkPoints = +d.perkPoints;
      renderPerks();
    }
    if (d.skillPoints !== undefined) {
      S.skillPoints = +d.skillPoints;
      renderPerks();
    }
    if (d.skillsPerLevel !== undefined) S.skillsPerLevel = +d.skillsPerLevel;
    if (d.perkKey !== undefined) S.perkKey = d.perkKey;
    if (d.levelUp) showLevelUp(d.levelUp.from, d.levelUp.to, d.levelUp.perks || (d.levelUp.to - d.levelUp.from), d.levelUp.skills || 0);
    if (d.heading !== undefined) S.heading = +d.heading;
    // The game side owns the list it sends; the HUD keeps its own settlement-attack and legendary-drop markers.
    if (d.markers) S.markers = d.markers.concat(S.markers.filter(function(m) {
      return m.id === "atk" || String(m.id).indexOf("loot:") === 0;
    }));
    if (d.marker) {
      var mi = S.markers.findIndex(function(m) {
        return m.id === d.marker.id;
      });
      if (mi < 0) S.markers.push(d.marker);
      else Object.assign(S.markers[mi], d.marker);
    }
    if (d.markerRemove) S.markers = S.markers.filter(function(m) {
      return m.id !== d.markerRemove;
    });
    ["clip", "clipMax", "reserve", "grenades"].forEach(function(k) {
      if (d[k] !== undefined) {
        S[k] = +d[k];
        amm = true;
      }
    });
    if (d.weapon !== undefined) {
      S.weapon = d.weapon;
      amm = true;
    }
    if (d.needs) {
      Object.assign(S.needs, d.needs);
      renderNeeds();
    }
    if (d.limbs) {
      Object.assign(S.limbs, d.limbs);
      renderLimbs();
      wake($("kit"));
    }
    if (d.figure) setFigure(d.figure);
    if (d.gear) setGear(d.gear);
    if (d.combat !== undefined) setCombat(d.combat);
    if (d.location !== undefined) setLocation(d.location, d.region, d.faction);
    else if (d.faction !== undefined) {
      S.localFaction = d.faction || null;
      renderLocalRep();
    }
    if (d.karma) setKarma(d.karma);
    if (d.armor !== undefined) setArmor(d.armor);
    if (d.legendaryLoot) legendaryLoot(d.legendaryLoot);
    if (d.weaponLegendary !== undefined) setWeaponLegendary(d.weaponLegendary);
    if (d.mutationFeed !== undefined) S.mutationFeed = !!d.mutationFeed;
    if (d.reputation) setReputation(d.reputation);
    if (d.challenges) setChallenges(d.challenges);
    if (d.challengeComplete) challengeComplete(d.challengeComplete);
    if (d.settings) applySettings(d.settings);
    if (d.notify) notify(d.notify.text, d.notify.sub, d.notify.kind, d.notify.ms, d.notify.icon);
    if (d.quest) quest(d.quest);
    /* null hides: in game the plugin mirrors vanilla's own show/hide timing */
    if (d.message === null) {
      clearTimeout($("msg")._h);
      $("msg").classList.remove("show");
    } else if (d.message) message(d.message.title, d.message.text, d.message.ms);
    if (d.subtitle === null) {
      clearTimeout($("subs")._h);
      $("subs").classList.remove("show");
    } else if (d.subtitle) subtitle(d.subtitle.who, d.subtitle.line, d.subtitle.ms);
    if (d.target !== undefined) setTarget(d.target);
    if (d.crosshair !== undefined) setSpread(d.crosshair);
    if (d.targetMutated) targetMutated(d.targetMutated);
    if (d.loot !== undefined) showLoot(d.loot);
    if (d.prompt !== undefined) setPrompt(d.prompt);
    if (d.stealth) setStealth(d.stealth);
    if (d.context) setContext(d.context);
    if (d.survival) setSurvival(d.survival);
    if (d.settlementAttack !== undefined) setAttack(d.settlementAttack);
    if (d.mines !== undefined || d.mineExploded) setMines(d.mines !== undefined ? d.mines : S.mines, d.mineExploded);
    if (d.weight) setWeight(d.weight);
    if (d.illness) catchIllness(d.illness.id, d.illness.hoursLeft, d.illness.quiet);
    if (d.cure) cureIllness(d.cure === true ? null : d.cure);
    if (d.paused !== undefined) fxPaused = !!d.paused;
    if (d.effects) syncEffects(d.effects);
    if (d.effect) addEffect(d.effect);
    if (d.effectRemove) removeEffect(d.effectRemove, !!d.announce);
    if (d.explosive !== undefined) setExplosive(d.explosive, d.explosive && d.explosive.announce);
    if (d.flashlight !== undefined) $("lamp").classList.toggle("gone", !d.flashlight);
    if (d.time) setTime(d.time);
    if (d.party) setParty(d.party);
    if (d.companion !== undefined) setCompanion(d.companion);
    if (d.companionRemove) removeCompanion(d.companionRemove);
    if (d.companionReact) companionReact(d.companionReact);
    if (d.tracker !== undefined) setTracker(d.tracker);
    if (vit) {
      renderVitals();
      wake($("vitals"));
    }
    if (amm) {
      renderAmmo();
      wake($("kit"));
      wakeCalm("cross");
    }
    renderCompass(!!(d.markers || d.marker || d.markerRemove));
  }
  window.hudUpdate = hudUpdate;
  window.hud = {
    update: hudUpdate,
    notify: notify,
    quest: quest,
    message: message,
    subtitle: subtitle,
    setTarget: setTarget,
    showLoot: showLoot,
    setLocation: setLocation,
    setCombat: setCombat,
    hitmark: hitmark,
    kick: kick,
    hurt: hurtFlash,
    addEffect: addEffect,
    removeEffect: removeEffect,
    setStealth: setStealth,
    setFigure: setFigure,
    setGear: setGear,
    setContext: setContext,
    setSurvival: setSurvival,
    setAttack: setAttack,
    setMines: setMines,
    setKarma: setKarma,
    targetMutated: targetMutated,
    showLevelUp: showLevelUp,
    legendaryLoot: legendaryLoot,
    setWeaponLegendary: setWeaponLegendary,
    setReputation: setReputation,
    setChallenges: setChallenges,
    challengeComplete: challengeComplete,
    setWeight: setWeight,
    catchIllness: catchIllness,
    cureIllness: cureIllness,
    setTracker: setTracker,
    setCompanion: setCompanion,
    setParty: setParty,
    removeCompanion: removeCompanion,
    companionReact: companionReact,
    setTime: setTime,
    setExplosive: setExplosive,
    state: S
  };

  /* ---------- preview scene (hidden in game) ---------- */
  function paintScene() {
    var c = $("scene"),
      x = c.getContext("2d"),
      seed = 7;

    function rnd() {
      seed = (seed * 16807) % 2147483647;
      return seed / 2147483647;
    }
    var sky = x.createLinearGradient(0, 0, 0, 700);
    sky.addColorStop(0, "#34413d");
    sky.addColorStop(.5, "#7f8166");
    sky.addColorStop(1, "#c6aa76");
    x.fillStyle = sky;
    x.fillRect(0, 0, 1920, 700);
    var sun = x.createRadialGradient(1360, 560, 10, 1360, 560, 520);
    sun.addColorStop(0, "rgba(255,230,180,.55)");
    sun.addColorStop(1, "rgba(255,230,180,0)");
    x.fillStyle = sun;
    x.fillRect(0, 0, 1920, 800);

    function skyline(base, minH, maxH, col) {
      x.fillStyle = col;
      x.beginPath();
      x.moveTo(0, base);
      var px = 0;
      while (px < 1920) {
        var w = 40 + rnd() * 110,
          h = minH + rnd() * (maxH - minH);
        x.lineTo(px, base - h);
        if (rnd() < .45) {
          x.lineTo(px + w * .3, base - h - 14 * rnd());
          x.lineTo(px + w * .55, base - h + 18 * rnd());
          x.lineTo(px + w * .8, base - h - 8);
        }
        x.lineTo(px + w, base - h);
        x.lineTo(px + w, base - rnd() * 20);
        px += w + rnd() * 30;
      }
      x.lineTo(1920, base);
      x.closePath();
      x.fill();
    }
    skyline(650, 20, 150, "rgba(84,86,72,.55)");
    skyline(675, 10, 90, "rgba(58,58,48,.85)");
    var gr = x.createLinearGradient(0, 660, 0, 1080);
    gr.addColorStop(0, "#6d6350");
    gr.addColorStop(1, "#26231d");
    x.fillStyle = gr;
    x.fillRect(0, 662, 1920, 418);
    x.fillStyle = "rgba(36,34,30,.6)";
    x.beginPath();
    x.moveTo(930, 664);
    x.lineTo(990, 664);
    x.lineTo(1420, 1080);
    x.lineTo(500, 1080);
    x.closePath();
    x.fill();
    x.strokeStyle = "rgba(210,190,140,.25)";
    x.lineWidth = 4;
    x.setLineDash([40, 50]);
    x.beginPath();
    x.moveTo(960, 668);
    x.lineTo(960, 1080);
    x.stroke();
    x.setLineDash([]);
    x.strokeStyle = "rgba(30,28,24,.9)";
    x.lineCap = "round";
    [
      [300, 720, 1.2],
      [1610, 735, 1],
      [1760, 700, 1.5],
      [180, 760, .9]
    ].forEach(function(t) {
      var tx = t[0],
        ty = t[1],
        s = t[2];
      x.lineWidth = 6 * s;
      x.beginPath();
      x.moveTo(tx, ty);
      x.lineTo(tx + 4, ty - 160 * s);
      x.stroke();
      x.lineWidth = 3 * s;
      for (var i = 0; i < 4; i++) {
        var by = ty - 60 * s - i * 28 * s,
          dir = i % 2 ? 1 : -1;
        x.beginPath();
        x.moveTo(tx + 2, by);
        x.lineTo(tx + dir * (40 + rnd() * 30) * s, by - 30 * s);
        x.stroke();
      }
    });
    for (var f = 0; f < 4; f++) {
      var fy = 600 + f * 45,
        fog = x.createLinearGradient(0, fy - 40, 0, fy + 40);
      fog.addColorStop(0, "rgba(210,200,170,0)");
      fog.addColorStop(.5, "rgba(210,200,170,.13)");
      fog.addColorStop(1, "rgba(210,200,170,0)");
      x.fillStyle = fog;
      x.fillRect(0, fy - 40, 1920, 80);
    }
    var v = x.createRadialGradient(960, 540, 380, 960, 540, 1150);
    v.addColorStop(0, "rgba(0,0,0,0)");
    v.addColorStop(1, "rgba(0,0,0,.6)");
    x.fillStyle = v;
    x.fillRect(0, 0, 1920, 1080);
  }

  /* ---------- initial state ---------- */
  renderVitals();
  renderXP();
  renderAmmo();
  renderNeeds();
  renderLimbs();
  renderCompass();
  renderClock();
  renderStealth();
  updateCalm();
  setSurvival({
    on: false
  });
  renderWeight();
  if (IN_GAME) {
    $("clock").hidden = true;
    $("clockSep").hidden = true;
    S.markers = [];
    renderCompass(true);
    S.explosive = null;
    renderAmmo();
    Object.keys(S.limbs).forEach(function(k) {
      S.limbs[k] = 1;
    });
    renderLimbs();
  }
  if (IN_GAME) {
    stage.classList.add("awaiting");
    setPrompt(null);
  } else message("Tutorial", "Messages and tutorials appear here. The panel grows to fit and fades on its own after a few seconds.", 16000);

  if (IN_GAME) return;

  paintScene();
  setTarget({
    name: "Raider Scavver",
    level: 14,
    pct: 0.64,
    legendary: true
  });
  showLoot({
    name: "Wooden Crate",
    sel: 0,
    items: [{
      n: "Duct Tape",
      q: 2,
      w: .5
    }, {
      n: "Purified Water",
      q: 1,
      w: 1
    }, {
      n: "10mm Round",
      q: 24,
      w: 0
    }, {
      n: "Pre-War Money",
      q: 3,
      w: 0
    }]
  });
  notify("Radio signal detected", "Tune in on the Pip-Boy", "radio", 24000);
  notify("Pipe Bolt-Action Pistol", "Recon Sights · DMG 24", "weapon", 24000);
  notify("Radstag Meat", "+1 · WT 1.0", "item", 24000);
  subtitle("Settler", "Pump's running again. First clean water we've had in a week.", 9000);
  setTimeout(function() {
    quest({
      eyebrow: "Quest updated",
      title: "Signal in the Static",
      objectives: [{
        t: "Reach the relay tower",
        done: true
      }, {
        t: "Get inside the relay tower"
      }],
      ms: 7000
    });
  }, 500);
  wake($("vitals"));
  wake($("kit"));
  wake($("compass"));
  addEffect({
    id: "radx",
    name: "Rad-X",
    kind: "med",
    icon: "capsule",
    duration: 60,
    remaining: 41
  });
  addEffect({
    id: "psycho",
    name: "Psycho",
    kind: "chem",
    icon: "bolt",
    duration: 90,
    remaining: 64
  });
  setTime({
    hour: 21.7,
    timescale: 20
  });

  /* compass drift */
  /* compass: slow look-around drift plus Turn buttons that ease the view */
  var baseTarget = 20,
    baseCur = 20,
    t0 = performance.now();

  function turnTo(deg) {
    baseTarget = deg;
    if (reduce) {
      baseCur = deg;
      S.heading = deg;
      renderCompass(true);
    }
  }
  if (!reduce) {
    (function loop(t) {
      baseCur += (baseTarget - baseCur) * 0.06;
      S.heading = baseCur + 5 * Math.sin((t - t0) / 6000) + 1.5 * Math.sin((t - t0) / 2100);
      renderCompass();
      requestAnimationFrame(loop);
    })(t0);
  }

  /* demo controls */
  var ITEMS = [
    ["Purified Water", "+1 · WT 1.0", "item"],
    ["Mutfruit", "+2 · WT 0.2", "item"],
    ["Stimpak", "+1", "item"],
    ["Bottle Caps", "+38", "caps"],
    ["Combat Rifle", "Long Barrel · DMG 33", "weapon"],
    ["Adhesive", "+3 · WT 0.1", "item"],
    ["Holotape", "Field Notes, Day 4", "quest"]
  ];
  var LINES = [
    ["Settler", "Crops took a beating last night. Mole rats again."],
    ["Trader", "Caps first, questions later. That's how this works."],
    ["Raider", "You hear that? Somebody's poking around."],
    ["Radio", "...repeat, the bridge is out. Take the long way around."]
  ];
  var li = 0,
    ii = 0,
    lastUser = 0;

  /* demo quest log: drives the tracker, the compass markers and the banner together */
  var QS = {
    sig: {
      title: "Signal in the Static",
      marker: "q1",
      obj: [{
        t: "Reach the relay tower",
        done: true
      }, {
        t: "Get inside the relay tower"
      }]
    },
    salt: {
      title: "Salt and Rust",
      marker: "q2",
      obj: [{
        t: "Clear the dockyard of ghouls"
      }]
    }
  };
  var qOrder = ["sig", "salt"],
    tracked = "sig",
    poolI = 0;
  var POOL = [{
      id: "lights",
      title: "When the Lights Came Back",
      obj: "Restore power to the substation",
      t: "quest",
      label: "Substation"
    },
    {
      id: "dark",
      title: "The Long Dark",
      obj: "Search the flooded maintenance tunnels",
      t: "door",
      label: "Maintenance Hatch"
    },
    {
      id: "debts",
      title: "Debts in Caps",
      obj: "Collect what the market traders owe",
      t: "quest",
      label: "Diamond City Market"
    }
  ];

  function mk(id) {
    return S.markers.filter(function(m) {
      return m.id === id;
    })[0];
  }

  function pushTracker() {
    var q = QS[tracked];
    if (!q) {
      setTracker(null);
      return;
    }
    var m = mk(q.marker),
      open = true;
    setTracker({
      title: q.title,
      objectives: q.obj.map(function(o) {
        var r = {
          t: o.t,
          done: !!o.done
        };
        if (!o.done && open) {
          r.dist = m ? m.dist : undefined;
          open = false;
        }
        return r;
      }),
      others: qOrder.filter(function(k) {
        return k !== tracked;
      }).map(function(k) {
        return QS[k].title;
      })
    });
  }

  function syncQuestMarkers() {
    qOrder.forEach(function(k) {
      var m = mk(QS[k].marker);
      if (m) {
        var on = k === tracked;
        if (on && m.active === false) m._new = Date.now();
        m.active = on;
      }
    });
    renderCompass(true);
  }

  var EXPL = [{
      name: "Frag Grenade",
      short: "FRAG",
      count: 8
    },
    {
      name: "Molotov Cocktail",
      short: "MOLO",
      count: 3
    },
    {
      name: "Plasma Grenade",
      short: "PLSM",
      count: 2
    },
    {
      name: "Pulse Mine",
      short: "MINE",
      count: 4
    },
    {
      name: "Baseball Grenade",
      short: "BASE",
      count: 5
    }
  ];
  var ei = 0,
    cyclerOn = true;

  function equipNext(from) {
    for (var k = 1; k <= EXPL.length; k++) {
      var j = (from + k) % EXPL.length;
      if (EXPL[j].count > 0) return j;
    }
    return -1;
  }

  var ROSTER = [{
      name: "Piper",
      type: "human",
      affinity: 780
    },
    {
      name: "Nick Valentine",
      type: "human",
      affinity: 910
    },
    {
      name: "Dogmeat",
      type: "dog",
      affinity: null
    },
    {
      name: "Codsworth",
      type: "robot",
      affinity: 520
    },
    {
      name: "Cait",
      type: "human",
      affinity: 300
    },
    {
      name: "Hancock",
      type: "human",
      affinity: 640
    },
    {
      name: "MacCready",
      type: "human",
      affinity: 880
    },
    {
      name: "Preston Garvey",
      type: "human",
      affinity: 260
    },
    {
      name: "Paladin Danse",
      type: "human",
      affinity: 505
    },
    {
      name: "Deacon",
      type: "human",
      affinity: 760
    },
    {
      name: "X6-88",
      type: "human",
      affinity: 120
    },
    {
      name: "Curie",
      type: "robot",
      affinity: 820
    },
    {
      name: "Strong",
      type: "mutant",
      affinity: -120
    },
    {
      name: "Heather Casdin",
      type: "human",
      affinity: 450
    }
  ];

  function nextRecruit() {
    return ROSTER.filter(function(c) {
      return !findMember(c.name);
    })[0];
  }

  function pickMember(alive) {
    var l = S.party.filter(function(c) {
      return !alive || c.state !== "downed";
    });
    return l[Math.floor(Math.random() * l.length)];
  }

  /* preview stand-ins for the game screens */
  var MOCK_LABEL = {
    ads: "Game: aiming down iron sights",
    scope: "Game: scope view (Sniper Scope Overlay)",
    vats: "Game: VATS",
    dialogue: "Game: dialogue options (Extended Dialogue Interface)",
    wheel: "Game: wheel menu",
    workshop: "Game: workshop mode",
    menu: "Game: Pip-Boy",
    scene: "Game: animation scene",
    photo: "Game: photo mode",
    hidden: "Game: HUD toggled off",
    powerarmor: "Game: vanilla power armor HUD"
  };
  window.__ctxMock = function(cx) {
    var m = $("ctxMock"),
      id = cx.id,
      pa = S.ctx.powerArmor;
    m.className = "ctxmock" + ((id !== "explore" && id !== "calm") || pa ? " on" : "") + " " + (id === "powerarmor" ? "pa" : id) + (pa && id !== "powerarmor" ? " pa" : "");
    $("cmLabel").textContent = MOCK_LABEL[id] || "";
    $("cmLabel").hidden = !MOCK_LABEL[id];
    $("cmPa").textContent = pa ? "Vanilla power armor HUD stays here" : "";
  };
  window.__ctxMock(ctxActive());

  /* demo durations are shortened so you can watch them run out */
  var FX_DEF = {
    stim: {
      name: "Stimpak",
      kind: "med",
      icon: "syringe",
      duration: 8,
      desc: "Restoring health · limbs mended"
    },
    psycho: {
      name: "Psycho",
      kind: "chem",
      icon: "bolt",
      duration: 90,
      desc: "+25% damage · +50 damage resist",
      addictive: true
    },
    medx: {
      name: "Med-X",
      kind: "chem",
      icon: "shield",
      duration: 90,
      desc: "+25% damage resist",
      addictive: true
    },
    jet: {
      name: "Jet",
      kind: "chem",
      icon: "hourglass",
      duration: 12,
      desc: "Time slows down",
      addictive: true
    },
    buffout: {
      name: "Buffout",
      kind: "chem",
      icon: "dumbbell",
      duration: 90,
      desc: "+2 STR · +3 END · +50 HP",
      addictive: true
    },
    radx: {
      name: "Rad-X",
      kind: "med",
      icon: "capsule",
      duration: 60,
      desc: "+100 radiation resist"
    },
    nuka: {
      name: "Nuka-Cola",
      kind: "drink",
      icon: "bottle",
      duration: 30,
      desc: "+AP regen · quenches thirst"
    },
    radaway: {
      name: "RadAway",
      kind: "med",
      icon: "ivbag",
      duration: 10,
      desc: "-300 rads over 10 s"
    }
  };
  var uses = {};

  function survivalKill() {
    if (!S.survival.on) return;
    var a = S.survival.adrenaline,
      k = a.kills + 1,
      r = a.rank;
    if (k >= a.perRank && r < 10) {
      r++;
      k = 0;
    } else if (r >= 10) k = 0;
    setSurvival({
      adrenaline: {
        rank: r,
        kills: k
      }
    });
  }
  var demoMines = [];
  setInterval(function() {
    if (!demoMines.length) return;
    var gone = [];
    demoMines.forEach(function(m) {
      m.left -= 0.1;
      m.disarm = m.dist <= 6;
      if (m.left <= 0) {
        gone.push(m.id);
        var hit = m.dist < 9 ? 1 : m.dist < 18 ? 0.35 : 0;
        if (hit) {
          hurtFlash();
          S.hp -= 55 * hit * (hasEffect("ill_weakness") ? 1.2 : 1);
          if (hit === 1) {
            S.limbs.lleg = clamp(S.limbs.lleg - .45, 0, 1);
            S.limbs.rleg = clamp(S.limbs.rleg - .45, 0, 1);
            renderLimbs();
            wake($("kit"));
          }
          renderVitals();
          wake($("vitals"));
        }
      }
    });
    if (gone.length) {
      demoMines = demoMines.filter(function(m) {
        return gone.indexOf(m.id) < 0;
      });
      setMines(demoMines, gone);
    } else setMines(demoMines);
  }, 100);
  var overT = 0;
  window.__fxTick = function(dt) {
    if (S.survival.on) {
      var n = S.needs,
        fat = clamp((1 - n.rest) * 0.45 + (1 - n.food) * 0.15 + (1 - n.water) * 0.15 - 0.1, 0, 0.7);
      if (Math.abs(fat - S.survival.fatigue) > 0.004) {
        S.survival.fatigue = fat;
        renderVitals();
      }
      if (hasEffect("ill_infection") && S.hp > 1) {
        S.hp = Math.max(1, S.hp - 0.6 * dt);
        renderVitals();
      }
      if (S.weight.cur > S.weight.max) {
        overT += dt;
        if (overT > 5) {
          overT = 0;
          S.limbs.lleg = clamp(S.limbs.lleg - .06, 0, 1);
          S.limbs.rleg = clamp(S.limbs.rleg - .06, 0, 1);
          S.hp -= 3;
          renderLimbs();
          renderVitals();
          wake($("vitals"));
          wake($("kit"));
        }
      } else overT = 0;
    }
    var st = S.stealth;
    if (st.sneaking && !S.combat && st.level > 0) {
      setStealth({
        level: st.level - 7 * dt
      });
    }
    var purging = hasEffect("radaway") && S.rads > 0;
    if (purging) {
      S.rads = Math.max(0, S.rads - 30 * dt);
      renderVitals();
      wake($("vitals"));
    }
    cores.rad.el.classList.toggle("purging", purging);
    if (hasEffect("stim") && S.hp < S.hpMax) {
      S.hp += 90 / 8 * dt;
      renderVitals();
      wake($("vitals"));
    }
    if (hasEffect("nuka") && S.ap < S.apMax) {
      S.ap = clamp(S.ap + 3 * dt, 0, S.apMax);
      renderVitals();
    }
  };

  function useFx(key) {
    var f = FX_DEF[key];
    var refreshed = hasEffect(key);
    addEffect({
      id: key,
      name: f.name,
      kind: f.kind,
      icon: f.icon,
      duration: f.duration
    });
    notify(f.name + (refreshed ? " refreshed" : ""), f.desc + " · " + fmt(f.duration), f.kind === "med" ? "item" : f.kind, 6000, f.icon);
    if (key === "stim") {
      Object.keys(S.limbs).forEach(function(k) {
        S.limbs[k] = clamp(S.limbs[k] + .5, 0, 1);
      });
      renderLimbs();
      wake($("kit"));
    }
    if (key === "radaway") {
      pop($("k-rad"), "-300 RADS", "good");
      wake($("vitals"));
    }
    if (key === "nuka") {
      S.needs.water = clamp(S.needs.water + .25, 0, 1);
      renderNeeds();
      wake($("kit"));
    }
    if (f.addictive) {
      uses[key] = (uses[key] || 0) + 1;
      if (uses[key] >= 3 && !hasEffect("addict_" + key) && Math.random() < .5) {
        setTimeout(function() {
          addEffect({
            id: "addict_" + key,
            name: "Addicted",
            kind: "bad",
            icon: "warn",
            duration: 0
          });
          notify(f.name + " addiction", "Withdrawal penalties until cured", "bad", 7000, "warn");
        }, 900);
      }
    }
  }

  var ACT = {
    damage: function() {
      S.hp -= (25 + Math.random() * 40) * (hasEffect("medx") ? .75 : 1) * (hasEffect("psycho") ? .8 : 1) * (hasEffect("ill_weakness") ? 1.2 : 1);
      hurtFlash();
      setCombat(true);
      var keys = Object.keys(S.limbs),
        k = keys[Math.floor(Math.random() * keys.length)];
      S.limbs[k] = clamp(S.limbs[k] - .4, 0, 1);
      renderLimbs();
      S.ap = clamp(S.ap - 6, 0, S.apMax);
      renderVitals();
      wake($("vitals"));
      wake($("kit"));
    },
    heal: function() {
      S.hp += 90;
      Object.keys(S.limbs).forEach(function(k) {
        S.limbs[k] = clamp(S.limbs[k] + .5, 0, 1);
      });
      renderVitals();
      renderLimbs();
      notify("Stimpak used", "+90 HP · limbs restored", "item");
      wake($("vitals"));
      wake($("kit"));
    },
    rad: function() {
      hudUpdate({
        radsGain: 20 + Math.round(Math.random() * 30)
      });
    },
    fire: function() {
      if (S.clip === 0) {
        wake($("kit"));
        return;
      }
      S.clip--;
      kick();
      renderAmmo();
      wake($("kit"));
      S.ap = clamp(S.ap + 2, 0, S.apMax);
      renderVitals();
      var sneakHit = S.stealth.sneaking && stealthState() === "hidden" && S.target;
      if (S.stealth.sneaking) setStealth({
        level: S.stealth.level + (sneakHit ? 30 : 22)
      });
      if (S.target && !S.target.dead) {
        var t = S.target;
        t.pct = clamp(t.pct - (.18 + Math.random() * .16) * (hasEffect("psycho") ? 1.25 : 1) * (sneakHit ? (+S.stealth.mult || 2) : 1), 0, 1);
        if (sneakHit) pop($("cross"), sneakText());
        var kill = t.pct === 0;
        if (kill) t.dead = true;
        hitmark(kill);
        setTarget(t);
        if (!kill && t.legendary && t.mutateAt !== undefined && t.pct <= t.mutateAt) {
          t.mutateAt = Math.random() < .45 ? t.pct * .5 : undefined; /* small chance it mutates again */
          var pool = MUT_POOL.filter(function(m) {
            return !(t.mutations || []).some(function(x) {
              return x.name === m.name;
            });
          });
          var mut = pool[Math.floor(Math.random() * pool.length)];
          setTimeout(function() {
            targetMutated(mut);
          }, 250);
        }
        if (kill && t.legendary) {
          var dropN = ++legDrops,
            item = LEG_ITEMS[dropN % LEG_ITEMS.length];
          setTimeout(function() {
            legendaryLoot({
              id: "d" + dropN,
              dropped: true,
              item: item[0],
              effects: item[1],
              b: norm(S.heading + 6),
              dist: 12
            });
            pendingDrop = {
              id: "d" + dropN,
              item: item[0],
              effects: item[1]
            };
          }, 700);
        }
        if (kill) {
          survivalKill();
          if (S.attack && S.attack.state === "active") ACT.atkkill();
          setTimeout(function() {
            setTarget(null);
            addXP(t.legendary ? 60 : 24);
            S.markers = S.markers.filter(function(m) {
              return m.t !== "enemy";
            });
            renderCompass(true);
          }, 500);
        }
      }
    },
    reload: function() {
      var take = Math.min(S.clipMax - S.clip, S.reserve);
      S.clip += take;
      S.reserve -= take;
      renderAmmo();
      wake($("kit"));
    },
    xp: function() {
      addXP(150);
    },
    xpbig: function() {
      var need = 0,
        lv = S.level,
        x = S.xp;
      for (var i = 0; i < 3; i++) {
        need += (1 - (i === 0 ? x : 0)) * (200 + (lv + i) * 15);
      }
      addXP(Math.ceil(need) + 40);
    },
    perkspend: function() {
      if (!S.perkPoints && !S.skillPoints) return;
      S.perkPoints = 0;
      S.skillPoints = 0;
      renderPerks();
      notify("Perks chosen", "Perk menu closed", "level");
    },
    skills: function(b) {
      S.skillsPerLevel = S.skillsPerLevel ? 0 : 15;
      b.setAttribute("aria-pressed", S.skillsPerLevel ? "true" : "false");
      b.textContent = "Skill points: " + (S.skillsPerLevel ? "15 per level" : "off");
    },
    legspot: function() {
      setTarget({
        name: "Legendary " + ["Raider Veteran", "Feral Ghoul Reaver", "Mirelurk Hunter", "Assaultron"][Math.floor(Math.random() * 4)],
        level: 22 + Math.floor(Math.random() * 10),
        pct: 1,
        legendary: true,
        mutations: [],
        mutateAt: .35 + Math.random() * .3
      });
      S.markers = S.markers.filter(function(m) {
        return m.t !== "enemy";
      }).concat([{
        b: S.heading + 4,
        t: "enemy"
      }]);
      if (S.stealth.sneaking) setStealth({
        level: S.stealth.level + 12
      });
      else setCombat(true);
      renderCompass(true);
      renderStealth();
    },
    legpick: function() {
      if (!pendingDrop) {
        notify("Nothing to pick up", "Kill a legendary first", "item");
        return;
      }
      var d = pendingDrop;
      pendingDrop = null;
      legendaryLoot({
        id: d.id,
        picked: true,
        item: d.item,
        effects: d.effects
      });
      S.weapon = d.item;
      renderAmmo();
      setWeaponLegendary(d.effects);
      wake($("kit"));
    },
    mutfeed: function(b) {
      S.mutationFeed = !S.mutationFeed;
      b.setAttribute("aria-pressed", S.mutationFeed ? "true" : "false");
      b.textContent = "Mutation notices: " + (S.mutationFeed ? "feed too" : "subtle");
    },
    target: function() {
      var leg = Math.random() < .4;
      setTarget({
        name: leg ? "Legendary Raider Psycho" : "Raider Psycho",
        level: 10 + Math.floor(Math.random() * 10),
        pct: 1,
        legendary: leg
      });
      S.markers = S.markers.filter(function(m) {
        return m.t !== "enemy";
      }).concat([{
        b: S.heading + 8,
        t: "enemy"
      }, {
        b: S.heading - 50,
        t: "enemy"
      }]);
      if (S.stealth.sneaking) setStealth({
        level: S.stealth.level + 12
      });
      else setCombat(true);
      renderCompass(true);
      renderStealth();
    },
    loot: function() {
      if (S.loot && $("loot").classList.contains("show")) {
        var l = S.loot;
        l.sel = ((l.sel || 0) + 1) % l.items.length;
        showLoot(l);
      } else showLoot({
        name: "Toolbox",
        sel: 0,
        items: [{
          n: "Wrench",
          q: 1,
          w: 2
        }, {
          n: "Screws",
          q: 6,
          w: 0
        }, {
          n: "Oil",
          q: 2,
          w: .2
        }]
      });
    },
    item: function() {
      var it = ITEMS[ii++ % ITEMS.length];
      notify(it[0], it[1], it[2]);
    },
    quest: function() {
      var id = tracked,
        q = QS[id];
      if (!q || q.closing) return;
      q.closing = true;
      q.obj.forEach(function(o) {
        o.done = true;
      });
      pushTracker();
      setTimeout(function() {
        quest({
          eyebrow: "Quest completed",
          title: q.title,
          objectives: q.obj.slice(-1)
        });
        notify(q.title, "Quest completed", "quest");
        addXP(150);
        S.markers = S.markers.filter(function(m) {
          return m.id !== q.marker;
        });
        delete QS[id];
        qOrder = qOrder.filter(function(k) {
          return k !== id;
        });
        var p = POOL[poolI++ % POOL.length],
          nid = p.id + poolI;
        QS[nid] = {
          title: p.title,
          marker: nid,
          obj: [{
            t: p.obj
          }]
        };
        qOrder.push(nid);
        tracked = nid;
        S.markers.push({
          id: nid,
          t: p.t,
          b: norm(S.heading + 35 + Math.random() * 90),
          dist: 300 + Math.round(Math.random() * 900),
          label: p.label,
          active: true
        });
        syncQuestMarkers();
        setTimeout(function() {
          notify(p.title, "Quest started", "quest");
          pushTracker();
        }, 1600);
      }, 1300);
    },
    talk: function() {
      var l = LINES[li++ % LINES.length];
      subtitle(l[0], l[1]);
    },
    fx: function(b) {
      useFx(b.dataset.fx);
    },
    left: function() {
      turnTo(baseTarget - 45);
    },
    right: function() {
      turnTo(baseTarget + 45);
    },
    face: function() {
      var q = S.markers.filter(function(m) {
        return (m.t === "quest" || m.t === "door") && m.active !== false;
      })[0];
      if (q) turnTo(baseTarget + (norm(q.b - baseTarget + 180) - 180));
    },
    indoors: function() {
      var q = mk("q1"),
        sq = QS.sig;
      if (!q || !sq) return;
      if (q.t === "door") {
        Object.assign(q, {
          t: "quest",
          label: "Broadcast source",
          dist: 38,
          dz: -9
        });
        sq.obj = [{
          t: "Reach the relay tower",
          done: true
        }, {
          t: "Get inside the relay tower",
          done: true
        }, {
          t: "Find the source of the broadcast"
        }];
      } else {
        Object.assign(q, {
          t: "door",
          label: "Relay Tower",
          dist: 142,
          dz: 12
        });
        sq.obj = [{
          t: "Reach the relay tower",
          done: true
        }, {
          t: "Get inside the relay tower"
        }];
      }
      q._new = Date.now();
      renderCompass(true);
      if (tracked !== "sig") {
        tracked = "sig";
        syncQuestMarkers();
      }
      pushTracker();
    },
    pin: function() {
      S.markers = S.markers.filter(function(m) {
        return m.id !== "pin";
      });
      S.markers.push({
        id: "pin",
        t: "pin",
        b: norm(S.heading + 20 + Math.random() * 40),
        dist: 200 + Math.round(Math.random() * 900),
        label: "Custom marker"
      });
      renderCompass(true);
      wake($("compass"));
    },
    swap: function() {
      if (qOrder.length < 2) return;
      tracked = qOrder[(qOrder.indexOf(tracked) + 1) % qOrder.length];
      syncQuestMarkers();
      pushTracker();
      wake($("compass"));
    },
    cycle: function() {
      /* Cycler: next in its list. Vanilla: pick another throwable from favorites. Same result on the HUD. */
      var j = equipNext(S.explosive ? ei : EXPL.length - 1);
      if (j < 0) return;
      ei = j;
      setExplosive(EXPL[ei]);
      $("b-unequip").textContent = "Unequip grenade";
    },
    throw: function() {
      if (!S.explosive || S.grenades <= 0) return;
      var item = EXPL[ei];
      item.count = Math.max(0, item.count - 1);
      setExplosive({
        name: item.name,
        count: item.count
      });
      if (S.stealth.sneaking) setStealth({
        level: S.stealth.level + 35
      });
      if (item.count === 0) {
        if (cyclerOn) {
          var j = equipNext(ei);
          setTimeout(function() {
            if (j >= 0) {
              ei = j;
              setExplosive(EXPL[ei]);
            } else {
              setExplosive(null);
              $("b-unequip").textContent = "Equip grenade";
              notify("Explosives Cycler", "Nothing left in this list", "bad", 5000, "warn");
            }
          }, 1000);
        } else setTimeout(function() {
          setExplosive(null);
          $("b-unequip").textContent = "Equip grenade";
        }, 950);
      }
    },
    unequip: function() {
      if (S.explosive) {
        setExplosive(null);
      } else {
        var j = equipNext(ei - 1 < 0 ? EXPL.length - 1 : ei - 1);
        if (j >= 0) {
          ei = j;
          setExplosive(EXPL[ei]);
        }
      }
      var b = $("b-unequip");
      b.textContent = S.explosive ? "Unequip grenade" : "Equip grenade";
    },
    cycler: function(b) {
      cyclerOn = !cyclerOn;
      b.setAttribute("aria-pressed", cyclerOn ? "true" : "false");
      b.textContent = "Explosives Cycler: " + (cyclerOn ? "on" : "off");
      $("b-cycle").textContent = cyclerOn ? "Cycle explosive" : "Equip from favorites";
    },
    cadd: function() {
      var r = nextRecruit();
      if (!r) return;
      setCompanion({
        name: r.name,
        type: r.type,
        affinity: r.affinity,
        hp: 1,
        state: "following"
      });
      notify(r.name + " joined the team", S.party.length + " of 16 · Liga of my Companions", "item");
    },
    cdismiss: function() {
      var c = S.party[S.party.length - 1];
      if (!c) return;
      removeCompanion(c.name);
      notify(c.name + " dismissed", "Heading back to Sanctuary Hills", "item");
    },
    cwait: function() {
      if (!S.party.length) return;
      var waiting = !S.party.some(function(c) {
        return c.state === "waiting";
      });
      S.party.forEach(function(c) {
        if (c.state !== "downed") c.state = waiting ? "waiting" : "following";
      });
      renderParty();
      wake($("vitals"));
      notify(waiting ? "Team: wait here" : "Team: follow me", "Order sent to " + S.party.length + " companions", "radio");
    },
    chit: function() {
      var c = pickMember(true);
      if (!c) return;
      setCombat(true);
      var hp = clamp(c.hp - (.25 + Math.random() * .2), 0, 1);
      if (hp <= 0) {
        setCompanion({
          name: c.name,
          hp: 0,
          state: "downed"
        });
        setTimeout(function() {
          if (findMember(c.name) && c.state === "downed") setCompanion({
            name: c.name,
            hp: .3,
            state: "following"
          });
        }, 7000);
      } else setCompanion({
        name: c.name,
        hp: hp
      });
    },
    clike: function() {
      var c = pickMember();
      if (c) companionReact({
        name: c.name,
        kind: Math.random() < .25 ? "loved" : "liked"
      });
    },
    cdislike: function() {
      var c = pickMember();
      if (c) companionReact({
        name: c.name,
        kind: Math.random() < .25 ? "hated" : "disliked"
      });
    },
    time: function() {
      S.time.hour += 1;
      renderClock();
      wake($("vitals"));
    },
    combat: function() {
      setCombat(!S.combat);
    },
    figure: function(b) {
      var want = b.dataset.fig;
      setFigure({
        type: want
      }).then(function(ok) {
        if (!ok) notify("Figure files not found", "Open hud.html#preview from the PhosphorHUD folder", "bad", 6000, "warn");
        ["silhouette", "vaultboy", "vaultgirl"].forEach(function(k) {
          $("b-fig-" + k).setAttribute("aria-pressed", S.figure.type === k ? "true" : "false");
        });
        wake($("kit"));
      });
    },
    clothes: function(b) {
      setGear({
        clothed: !S.gear.clothed
      });
      b.setAttribute("aria-pressed", S.gear.clothed ? "true" : "false");
      b.textContent = "Clothing: " + (S.gear.clothed ? "on" : "off");
      wake($("kit"));
    },
    armor: function(b) {
      var on = !S.gear.armor.torso;
      setGear({
        armor: {
          head: on,
          torso: on,
          larm: on,
          rarm: on,
          lleg: on,
          rleg: on
        }
      });
      b.setAttribute("aria-pressed", on ? "true" : "false");
      b.textContent = "Armor pieces: " + (on ? "on" : "off");
      wake($("kit"));
    },
    crouch: function() {
      var on = !S.stealth.sneaking;
      setStealth({
        sneaking: on,
        level: on ? (S.combat ? 70 : 8) : 0
      });
    },
    noise: function() {
      if (!S.stealth.sneaking) ACT.crouch();
      setStealth({
        level: S.stealth.level + 30
      });
    },
    spotted: function() {
      if (!S.stealth.sneaking) ACT.crouch();
      setStealth({
        level: 100
      });
      setCombat(true);
      if (!S.target) ACT.target();
    },
    lose: function() {
      setCombat(false);
      setStealth({
        level: Math.min(S.stealth.level, 30)
      });
    },
    kgood: function() {
      var r = [
        ["Gave water to a beggar", 15],
        ["Freed a captive", 40],
        ["Healed a settler", 10],
        ["Returned a lost item", 25]
      ][Math.floor(Math.random() * 4)];
      var v = (S.karma.value || 0) + r[1] * 6;
      setKarma({
        value: v,
        change: r[1],
        reason: r[0],
        title: KTITLE[String(karmaAlignFor(v))]
      });
    },
    kbad: function() {
      var r = [
        ["Stole from a vendor", -20],
        ["Picked a lock", -5],
        ["Pickpocketed a settler", -25],
        ["Murdered an innocent", -100]
      ][Math.floor(Math.random() * 4)];
      var v = (S.karma.value || 0) + r[1] * 6;
      setKarma({
        value: v,
        change: r[1],
        reason: r[0],
        title: KTITLE[String(karmaAlignFor(v))]
      });
    },
    rfame: function() {
      var r = S.rep["Diamond City"] || {
        fame: 0,
        infamy: 0
      };
      var f = (r.fame || 0) + 1;
      setReputation({
        faction: "Diamond City",
        fame: f,
        infamy: r.infamy || 0,
        title: f >= 4 ? "Idolized" : f >= 2 ? "Liked" : "Accepted",
        event: "fameGained"
      });
    },
    rinf: function() {
      var r = S.rep["the Gunners"] || {
        fame: 0,
        infamy: 0
      };
      var i = (r.infamy || 0) + 1;
      setReputation({
        faction: "the Gunners",
        fame: 0,
        infamy: i,
        title: i >= 3 ? "Vilified" : i >= 2 ? "Hated" : "Shunned",
        event: "infamyGained"
      });
      if (i === 3) setTimeout(function() {
        setReputation({
          faction: "Gunner",
          event: "hitSquad"
        });
      }, 1200);
    },
    rdisc: function() {
      var f = ["the Atom Cats", "Covenant", "Bunker Hill", "the Hubologists", "Goodneighbor"][Math.floor(Math.random() * 5)];
      setReputation({
        faction: f,
        event: "discovered",
        title: "Neutral"
      });
    },
    rtown: function() {
      if (!S.rep["Diamond City"]) setReputation({
        faction: "Diamond City",
        fame: 2,
        infamy: 0,
        title: "Liked"
      });
      setLocation("Diamond City", "Commonwealth", "Diamond City");
      if (Math.random() < .5) setTimeout(function() {
        setReputation({
          faction: "Diamond City",
          event: "gift",
          detail: "Supply crate: 3 Stimpaks, 150 caps"
        });
      }, 1500);
    },
    chprog: function() {
      var open = S.challenges.filter(function(x) {
        return !x.done && x.cur < x.max;
      });
      var c = open[Math.floor(Math.random() * open.length)];
      if (!c) return;
      c.cur = Math.min(c.max, c.cur + Math.ceil(c.max / 6));
      setChallenges(S.challenges.slice());
      if (c.cur >= c.max) setTimeout(function() {
        challengeComplete(c);
        setTimeout(function() {
          var n = CH_POOL.shift();
          if (n) setChallenges(S.challenges.concat([n]));
        }, 3500);
      }, 400);
    },
    minefield: function() {
      var now = [{
          id: "m1",
          name: "Frag Mine",
          x: 905,
          y: 792,
          dist: 5,
          total: 4.5
        },
        {
          id: "m2",
          name: "Frag Mine",
          x: 1180,
          y: 712,
          dist: 13,
          total: 6.5
        },
        {
          id: "m3",
          name: "Pulse Mine",
          x: 620,
          y: 735,
          dist: 16,
          total: 8
        },
        {
          id: "m4",
          name: "Frag Mine",
          onScreen: false,
          bearing: 150,
          dist: 7,
          total: 7
        }
      ];
      demoMines = now.map(function(m) {
        m.left = m.total;
        m.b = norm(S.heading + (m.onScreen === false ? m.bearing : (m.x - 960) / 1920 * 90));
        return m;
      });
      setMines(demoMines);
    },
    disarm: function() {
      if (!demoMines.length) return;
      var n = demoMines.slice().sort(function(a, b) {
        return a.dist - b.dist;
      })[0];
      if (n.dist > 6) {
        notify("Too far to disarm", "Get within reach of the " + n.name, "item");
        return;
      }
      demoMines = demoMines.filter(function(m) {
        return m !== n;
      });
      setMines(demoMines);
      notify("Disarmed " + n.name, "+1 " + n.name, "item");
      if (S.stealth.sneaking) setStealth({
        level: S.stealth.level + 5
      });
    },
    backoff: function() {
      demoMines.forEach(function(m) {
        m.dist += 9;
        if (m.onScreen !== false) {
          m.y -= 40;
          m.x = 960 + (m.x - 960) * 0.8;
        }
      });
      setMines(demoMines);
    },
    atk: function() {
      var f = ["Raiders", "Gunners", "Super Mutants", "Feral Ghouls", "Rust Devils"][Math.floor(Math.random() * 5)];
      var where = [
        ["Abernathy Farm", 1320],
        ["Red Rocket Truck Stop", 640],
        ["Sunshine Tidings Co-op", 1870]
      ][Math.floor(Math.random() * 3)];
      var total = 6 + Math.floor(Math.random() * 14);
      setAttack(null);
      setAttack({
        id: where[0],
        name: where[0],
        faction: f,
        total: total,
        defense: 40 + Math.floor(Math.random() * 80),
        state: "pending",
        b: norm(S.heading + 60 + Math.random() * 120),
        dist: where[1]
      });
      wake($("compass"));
    },
    arrive: function() {
      if (!S.attack || S.attack.state !== "pending") return;
      setAttack({
        state: "active",
        dist: 30,
        b: S.heading + 4
      });
      setCombat(true);
      if (!S.target) ACT.target();
    },
    atkkill: function() {
      var a = S.attack;
      if (!a || a.state !== "active") return;
      var r = Math.max(0, a.remaining - 1);
      setAttack({
        remaining: r,
        state: r === 0 ? "won" : "active"
      });
      if (r === 0) setCombat(false);
    },
    vb: function() {
      if (S.attack) setAttack({
        vertibird: !S.attack.vertibird
      });
    },
    atkleave: function() {
      if (S.attack && (S.attack.state === "pending" || S.attack.state === "active")) {
        setAttack({
          state: S.attack.state === "active" ? "ended" : "lost"
        });
        setCombat(false);
      }
    },
    patrol: function() {
      var p = [
        ["Minutemen patrol", "arrived at Abernathy Farm"],
        ["Brotherhood patrol", "passing Red Rocket"],
        ["Settler patrol", "back at Sanctuary Hills"]
      ][Math.floor(Math.random() * 3)];
      notify(p[0], p[1], "radio");
    },
    surv: function(b) {
      var on = !S.survival.on;
      setSurvival({
        on: on
      });
      b.setAttribute("aria-pressed", on ? "true" : "false");
      b.textContent = "Survival mode: " + (on ? "on" : "off");
    },
    kill: function() {
      survivalKill();
      addXP(18);
    },
    sleep: function() {
      var n = S.needs,
        ins = hasEffect("ill_insomnia");
      n.rest = ins ? clamp(n.rest + (1 - n.rest) / 2, 0, 1) : 1;
      n.food = clamp(n.food - .12, 0, 1);
      n.water = clamp(n.water - .18, 0, 1);
      S.time.hour += 8;
      renderClock();
      S.effects.forEach(function(e) {
        if (e.game) e.remaining -= 8 * 3600;
      });
      var a = S.survival.adrenaline;
      var before = a.rank;
      setSurvival({
        adrenaline: {
          rank: Math.max(0, a.rank - 3),
          kills: 0
        }
      });
      renderNeeds();
      renderVitals();
      wake($("vitals"));
      notify("Slept 8 hours", (ins ? "Insomnia: rest only half restored" : "Rest restored") + (before ? " · adrenaline " + before + " → " + S.survival.adrenaline.rank : ""), "item", 6000, "bed");
    },
    meat: function() {
      var par = hasEffect("ill_parasites");
      S.needs.food = clamp(S.needs.food + (par ? .15 : .3), 0, 1);
      renderNeeds();
      wake($("vitals"));
      notify("Ate Radstag Meat (raw)", par ? "Parasites: only half as filling" : "Raw meat can carry disease", "item");
      if (!par && S.survival.on) setTimeout(function() {
        catchIllness("parasites");
      }, 900);
    },
    dirty: function() {
      S.needs.water = clamp(S.needs.water + .3, 0, 1);
      renderNeeds();
      hudUpdate({
        radsGain: 12
      });
      notify("Drank Dirty Water", "+12 rads · unpurified water can carry disease", "item", 5000, "bottle");
      if (S.survival.on) setTimeout(function() {
        var pool = ["infection", "insomnia", "lethargy", "weakness", "fatigue"].filter(function(k) {
          return !hasEffect("ill_" + k);
        });
        if (pool.length) catchIllness(pool[0]);
      }, 900);
    },
    anti: function() {
      var cured = cureIllness();
      notify("Antibiotics", cured.length ? "Cured: " + cured.join(", ") : "Nothing to cure", "item", 6000, "capsule");
    },
    heavy: function() {
      var loot = [
        ["Heavy Incinerator", 34],
        ["Industrial Oil x6", 18],
        ["Typewriter x3", 27],
        ["Fusion Core x2", 8]
      ][Math.floor(Math.random() * 4)];
      setWeight({
        cur: S.weight.cur + loot[1]
      });
      notify(loot[0], "+" + loot[1] + " lb · " + Math.round(S.weight.cur) + "/" + S.weight.max, "item");
    },
    drop: function() {
      setWeight({
        cur: Math.max(120, S.weight.cur - 70)
      });
      notify("Dropped junk", "-70 lb · " + Math.round(S.weight.cur) + "/" + S.weight.max, "item");
    },
    ctx: function(b) {
      var k = b.dataset.ctx;
      var MENU_FOR = {
        scope: "ScopeMenu",
        vats: "VATSMenu",
        dialogue: "DialogueMenu",
        wheel: "WheelMenu",
        workshop: "WorkshopMenu",
        menu: "PipboyMenu"
      };
      setContext({
        menus: MENU_FOR[k] ? [MENU_FOR[k]] : [],
        ads: k === "ads",
        scene: k === "scene",
        photo: k === "photo",
        hudHidden: k === "hidden"
      });
      Array.prototype.forEach.call(document.querySelectorAll('[data-act="ctx"]'), function(x) {
        x.setAttribute("aria-pressed", x === b ? "true" : "false");
      });
      if (k === "dialogue") subtitle("Piper", "Got a minute? I've been chasing a lead on the relay tower.", 6000);
      if (k === "vats" && !S.target) ACT.target();
    },
    pa: function(b) {
      var on = !S.ctx.powerArmor;
      setContext({
        powerArmor: on
      });
      b.setAttribute("aria-pressed", on ? "true" : "false");
      b.textContent = "Power armor: " + (on ? "on" : "off");
    }
  };
  /* sample mutation names in the spirit of the mod's lists; the plugin sends the real effect names */
  var MUT_POOL = [{
      name: "Ballistic hide",
      kind: "defense"
    }, {
      name: "Reflective scales",
      kind: "defense"
    }, {
      name: "Healing factor",
      kind: "heal",
      heal: .25
    },
    {
      name: "Adrenal surge",
      kind: "offense"
    }, {
      name: "Enlarged",
      kind: "offense"
    }, {
      name: "Shield emitter",
      kind: "defense"
    },
    {
      name: "Contaminated Psycho",
      kind: "offense"
    }, {
      name: "Self-repair",
      kind: "heal",
      heal: .2
    }
  ];
  var LEG_ITEMS = [
    ["Combat Rifle", ["Two Shot"]],
    ["Combat Shotgun", ["Bloodied", "Explosive"]],
    ["Hunting Rifle", ["Instigating", "Faster Fire Rate", "Limb Damage"]],
    ["10mm Pistol", ["Quad"]]
  ];
  var legDrops = 0,
    pendingDrop = null;
  S.perkKey = "K"; /* sample: LevelUpMenuEx's hotkey is whatever you bind in its settings */
  var KTITLE = {
    "-2": "Devil",
    "-1": "Marauder",
    "0": "Wanderer",
    "1": "Defender",
    "2": "Paladin"
  };
  var CH_POOL = [{
    id: "c3",
    name: "Ain't That a Kick in the Head",
    cur: 0,
    max: 10,
    reward: "+500 XP",
    xp: 500
  }];
  setKarma({
    value: 380,
    title: KTITLE["1"]
  });
  setChallenges([{
    id: "c1",
    name: "Hit the Deck",
    cur: 31,
    max: 50,
    reward: "Perk: Hit the Deck",
    xp: 0
  }, {
    id: "c2",
    name: "Tough Guy",
    cur: 9,
    max: 10,
    reward: "+1000 XP · Perk rank",
    xp: 1000
  }]);
  pushTracker();
  setSurvival({
    on: true,
    adrenaline: {
      rank: 2,
      kills: 3
    }
  });
  setWeight({
    cur: 262,
    max: 300
  });
  if (LOCAL) ACT.figure($("b-fig-vaultboy"));
  setParty(ROSTER.slice(0, 3).map(function(c) {
    return {
      name: c.name,
      type: c.type,
      affinity: c.affinity
    };
  }));
  setCompanion({
    name: "Nick Valentine",
    hp: .62
  });
  setExplosive(EXPL[0], false);
  $("controls").addEventListener("click", function(e) {
    var b = e.target.closest("button[data-act]");
    if (!b) return;
    lastUser = Date.now();
    ACT[b.dataset.act](b);
  });

  /* gentle ambient activity while nobody is clicking */
  setInterval(function() {
    if (Date.now() - lastUser < 20000) return;
    var r = Math.random();
    if (r < .35) ACT.item();
    else if (r < .6) ACT.talk();
    else if (r < .72) ACT.clike();
    else {
      S.needs.rest = clamp(S.needs.rest - .04, 0, 1);
      renderNeeds();
      if (S.needs.rest < .2) pop($("k-rest"), "+Fatigue");
      wake($("vitals"));
    }
  }, 9000);
})();
