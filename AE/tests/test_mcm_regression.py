"""MCM integration matrix; set PHOSPHOR_TEST_MOD to test extracted release assets."""
import copy
import itertools
import json
import os
import pathlib
import subprocess
import unittest

from playwright.sync_api import sync_playwright

ROOT = pathlib.Path(__file__).resolve().parents[1]
MOD = pathlib.Path(os.environ.get("PHOSPHOR_TEST_MOD", ROOT / "mod/Phosphor HUD"))
HUD = MOD / "PrismaUI_F4/views/PhosphorHUD/hud.html"
MCM = MOD / "MCM/Config/PhosphorHUD"
ALL_CONTROLS = [c for c in json.loads((MCM / "config.json").read_text(encoding="utf-8"))["content"] if "id" in c]
# Plugin-side controls: they change game behavior, not the HUD page, and are checked separately below.
PLUGIN_ONLY = {"bQuickLootFirst", "PerkChoicesKey"}
CONTROLS = [c for c in ALL_CONTROLS if c["id"].split(":")[0] not in PLUGIN_ONLY]
BOOLS = dict(zip(
    "bHideThirdPersonAimCrosshair bShowLevelBar bHideFullVitals bLocationOnArrival bHideHealthyFigure bShowArmorPieces bShowAdrenaline bNeedStageNames bHideSatisfiedNeeds bSurvivalEnemyMarkers bMineWarnings bMineCompass bKarmaReadout bLocalReputation bChallengeTracker bLegendaryDropMarkers bLevelUpBanner bPerkReminder iClockFormat".split(),
    "hideThirdPersonAimCrosshair showLevelBar hideFullVitals locationOnArrival hideHealthyFigure armorPieces showAdrenaline needStageNames hideSatisfiedNeeds survivalEnemyMarkers mineWarnings mineCompass karmaReadout localReputation challengeTracker legendaryDropMarkers levelUpBanner perkReminder clock24".split()))
CONTEXTS = dict(zip(
    "iCtxAds iCtxScope iCtxVats iCtxDialogue iCtxWheel iCtxWorkshop iCtxPowerArmor iCtxScene".split(),
    "ads scope vats dialogue wheel workshop powerarmor scene".split()))
BOOLS.update(bShowCrosshair="showCrosshair", bShowHitMarkers="showHitMarkers", bShowWeaponLegendary="showWeaponLegendary",
             bShowArmorWheel="showArmorWheel", bShowUndiscovered="showUndiscovered")
CALM = dict(zip("iCalmVitals iCalmCompass iCalmWeapon iCalmCompanions iCalmQuests iCalmEffects iCalmCrosshair".split(),
                "vitals compass kit party tracker effects cross".split()))
ENUMS = {"iLimbFigure": ("limbFigure", ["silhouette", "silhouette", "vaultboy", "vaultgirl"]),
         "iTextReadability": ("textReadability", ["off", "subtle", "strong"]),
         "iShowWeight": ("showWeight", ["near", "always", "never"]),
         "iMutationNotices": ("mutationNotices", ["bar", "feed", "off"])}
ACTIVATE = {"ads": {"ads": True}, "scope": {"menus": ["ScopeMenu"]},
            "vats": {"menus": ["VATSMenu"]}, "dialogue": {"menus": ["DialogueMenu"]},
            "wheel": {"menus": ["FavoritesMenu"]}, "workshop": {"menus": ["WorkshopMenu"]},
            "powerarmor": {"powerArmor": True}, "scene": {"scene": True}}
EXPLORE = dict(menus=[], powerArmor=False, ads=False, scene=False, photo=False, hudHidden=False, prismaFocus=False)
RECOMMENDED = dict(ads="s", scope="d", vats="s", dialogue="h", wheel="d", workshop="h", powerarmor="h", scene="h")


def native(overrides):
    result = subprocess.run([str(ROOT / "build/tests/phosphor-tests.exe"), "--mcm-settings", str(MCM / "settings.ini")],
                            input=json.dumps(overrides), capture_output=True, text=True, timeout=10)
    if result.returncode:
        raise AssertionError(result.stderr)
    return json.loads(result.stdout)


def choices(control):
    opt = control["valueOptions"]
    if control["type"] == "switcher":
        return [0, 1]
    if control["type"] == "dropdown":
        return range(len(opt["options"]))
    return range(opt["min"], opt["max"] + 1, opt["step"])


class McmRegressionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.defaults = native({})["settings"]
        cls.runtime = sync_playwright().start()
        cls.browser = cls.runtime.chromium.launch()

    @classmethod
    def tearDownClass(cls):
        cls.browser.close()
        cls.runtime.stop()

    def setUp(self):
        self.errors = []
        self.page = self.browser.new_page(viewport={"width": 3440, "height": 1440})
        self.page.on("pageerror", lambda error: self.errors.append(str(error)))
        self.page.on("console", lambda msg: self.errors.append(msg.text) if msg.type == "error" and msg.text.startswith("hudUpdate") else None)
        self.page.route("https://**/*", lambda route: route.abort())
        self.page.clock.install(time=1700000000000)
        self.page.goto(HUD.as_uri(), wait_until="domcontentloaded")
        self.page.clock.pause_at(1700000000100)

    def tearDown(self):
        self.page.close()
        self.assertEqual(self.errors, [])

    def send(self, message):
        self.page.evaluate("d => window.hudUpdate(d)", json.dumps(message))

    def seed(self, settings):
        self.send({"reset": True})
        self.send({"settings": settings, "snapshot": True, "paused": False, "combat": True, "context": EXPLORE,
                   "location": "Sanctuary Hills", "region": "Commonwealth", "faction": "minutemen",
                   "reputation": {"faction": "minutemen", "title": "Liked"},
                   "time": {"hour": 13.5, "timescale": 0}, "level": 23, "xp": .5, "perkPoints": 2,
                   "hp": 100, "hpMax": 100, "ap": 100, "apMax": 100, "rads": 0,
                   "survival": {"on": True, "fatigue": 0, "stages": {"food": 0, "water": 1, "rest": 0},
                                "adrenaline": {"rank": 2, "kills": 1}},
                   "weight": {"cur": 90, "max": 100}, "target": {"name": "Raider", "pct": .5, "level": 5}})

    def opacity(self):
        return self.page.locator("#locName").evaluate("e => {e.getAnimations().forEach(a => a.finish()); return getComputedStyle(e).opacity}")

    def other_hud(self):
        # Compare every widget, including markers, limbs, effects and banners.
        # Exclude only the intentionally changing arrival-name class.
        return self.page.locator("#stage").evaluate("""e => {
            const clone = e.cloneNode(true);
            clone.querySelector('#locName').classList.remove('arrival-hidden');
            return clone.outerHTML;
        }""")

    def test_every_mcm_value_maps_through_native_settings_without_arrival_coupling(self):
        supported = set(BOOLS) | set(CONTEXTS) | set(ENUMS) | set(CALM) | {"iIdleOpacity", "iMaxEffectHours", "iCalmDelay", "iTextScale", "iIconScale", "iRingScale"}
        self.assertEqual({c["id"].split(":")[0] for c in CONTROLS}, supported)
        for control in CONTROLS:
            key = control["id"].split(":")[0]
            for value in choices(control):
                with self.subTest(control=key, value=value):
                    result = native({key: value})
                    settings = result["settings"]
                    expected = copy.deepcopy(self.defaults)
                    if key in BOOLS:
                        expected[BOOLS[key]] = bool(value)
                    elif key in CALM:
                        expected["calmRules"][CALM[key]] = ["show", "dim", "hide"][value]
                    elif key == "iCalmDelay":
                        expected["calmDelay"] = value
                    elif key in CONTEXTS:
                        expected["contextRules"][CONTEXTS[key]] = ["recommended", "show", "hide"][value]
                    elif key in ENUMS:
                        name, values = ENUMS[key]
                        expected[name] = values[value]
                    elif key == "iIdleOpacity":
                        expected["idleOpacity"] = value / 100
                    elif key == "iTextScale":
                        expected["textScale"] = value / 100
                    elif key == "iIconScale":
                        expected["iconScale"] = value / 100
                    elif key == "iRingScale":
                        expected["ringScale"] = value / 100
                    self.assertEqual(settings, expected)
                    self.assertEqual(result["effectHourLimit"], value if key == "iMaxEffectHours" else 2)
                    self.assertTrue(result["quickLootFirst"], "HUD options never change quick loot first")
                    if key != "bLocationOnArrival":
                        paired = native({key: value, "bLocationOnArrival": 1})
                        paired["settings"]["locationOnArrival"] = False
                        self.assertEqual(paired, result, "Arrival must not change any other native setting")

    def test_quick_loot_first_switch_and_unbound_key(self):
        ids = {c["id"].split(":")[0]: c for c in ALL_CONTROLS}
        self.assertEqual(set(ids) & PLUGIN_ONLY, PLUGIN_ONLY)
        self.assertEqual(ids["bQuickLootFirst"]["type"], "switcher")
        self.assertEqual(ids["PerkChoicesKey"]["type"], "hotkey")
        self.assertTrue(native({})["quickLootFirst"], "On by default (it does nothing until a key is picked)")
        for value in (0, 1):
            result = native({"bQuickLootFirst": value})
            self.assertEqual(result["quickLootFirst"], bool(value))
            self.assertEqual(result["settings"], self.defaults, "The switch never changes the HUD page")
        binds = json.loads((MCM / "keybinds.json").read_text(encoding="utf-8"))
        self.assertEqual(binds["modName"], "PhosphorHUD")
        self.assertEqual([b["id"] for b in binds["keybinds"]], ["PerkChoicesKey"])
        self.assertNotIn("PerkChoicesKey", (MCM / "settings.ini").read_text(encoding="utf-8"), "No default key: the player picks it")

    def test_calm_defaults_and_each_native_rule_in_browser(self):
        self.assertTrue(self.defaults["showCrosshair"])
        self.assertTrue(self.defaults["showHitMarkers"])
        self.assertEqual(self.defaults["calmDelay"], 5)
        self.assertEqual(self.defaults["calmRules"], dict.fromkeys(CALM.values(), "show"))
        for key, group in CALM.items():
            for value, css in enumerate(("", "ctx-dim", "ctx-hide")):
                settings = native({key: value, "iCalmDelay": 0})["settings"]
                self.seed(settings)
                self.page.clock.fast_forward(6000)
                self.send({"combat": False})
                selector = '[data-crosshair]' if group == "cross" else '[data-g="'+group+'"]'
                classes = self.page.locator(selector).first.get_attribute("class")
                if css:
                    self.assertIn(css, classes, key)
                else:
                    self.assertNotIn("ctx-hide", classes)
                    self.assertNotIn("ctx-dim", classes)

    def test_every_mcm_value_preserves_expiry_and_neighbouring_hud(self):
        count = 0
        for control in CONTROLS:
            key = control["id"].split(":")[0]
            if key == "bLocationOnArrival":
                continue
            for value in choices(control):
                with self.subTest(control=key, value=value):
                    settings = native({key: value, "bLocationOnArrival": 1})["settings"]
                    self.seed(settings)
                    self.page.clock.fast_forward(6000)
                    self.assertEqual(self.opacity(), "1")
                    # Real MCM close sends the entire settings object, even if just one choice changed.
                    self.send({"settings": settings, "location": "Sanctuary Hills"})
                    self.page.clock.fast_forward(2600)
                    self.assertEqual(self.opacity(), "0", "Settings reload must not restart the timer")
                    self.assertEqual(self.page.locator("#clockTxt").text_content(), "13:30" if settings["clock24"] else "1:30 PM")
                    self.assertEqual(self.page.locator("#locSub").text_content(), "Commonwealth")
                    self.assertEqual(self.page.locator("#xp").is_visible(), settings["showLevelBar"])
                    self.assertEqual(self.page.locator("#lrep").is_visible(), settings["localReputation"])
                    before = self.other_hud()
                    self.send({"settings": {**settings, "locationOnArrival": False}})
                    self.assertEqual(self.other_hud(), before, "Arrival toggle must leave every other widget unchanged")
                    self.assertEqual(self.opacity(), "1")
                    self.page.clock.fast_forward(8600)
                    self.assertEqual(self.opacity(), "1")
                    self.send({"settings": settings})
                    self.assertEqual(self.opacity(), "1")
                    self.send({"location": "Diamond City"})
                    self.page.clock.fast_forward(8600)
                    self.assertEqual(self.opacity(), "0")
                    count += 1
        print(f"MCM browser values checked: {count}")

    def test_all_context_choices_suspend_or_continue_arrival_as_rendered(self):
        for context, active in ACTIVATE.items():
            for rule in ("recommended", "show", "hide"):
                with self.subTest(context=context, rule=rule):
                    settings = copy.deepcopy(self.defaults)
                    settings["locationOnArrival"] = True
                    settings["contextRules"][context] = rule
                    self.seed(settings)
                    self.page.clock.run_for(6000)
                    self.send({"context": {**EXPLORE, **active}})
                    expected = RECOMMENDED[context] if rule == "recommended" else {"show": "s", "hide": "h"}[rule]
                    loc = self.page.locator(".loc[data-g=vitals]")
                    self.assertEqual(loc.evaluate("e => e.classList.contains('ctx-hide')"), expected == "h")
                    self.assertEqual(loc.evaluate("e => e.classList.contains('ctx-dim')"), expected == "d")
                    self.page.clock.run_for(12000)
                    self.assertEqual(self.opacity(), "1" if expected == "h" else "0")
                    self.send({"context": EXPLORE})
                    self.page.clock.run_for(1000)
                    self.assertEqual(self.opacity(), "1" if expected == "h" else "0")
                    self.page.clock.run_for(2000)
                    self.assertEqual(self.opacity(), "0")
                    self.assertTrue(self.page.locator("#clockTxt").is_visible())

    def test_dynamic_options_weight_level_and_clock_combinations(self):
        count = 0
        for needs, vitals, level, military, weight in itertools.product([False, True], [False, True], [False, True], [False, True], ["near", "always", "never"]):
            with self.subTest(needs=needs, vitals=vitals, level=level, military=military, weight=weight):
                settings = {**self.defaults, "locationOnArrival": True, "hideSatisfiedNeeds": needs,
                            "hideFullVitals": vitals, "showLevelBar": level, "clock24": military, "showWeight": weight}
                self.seed(settings)
                self.page.clock.fast_forward(8600)
                self.assertEqual(self.opacity(), "0")
                for selector, visible in {"#k-hp": not vitals, "#k-ap": not vitals, "#k-rad": not vitals,
                                          "#k-food": not needs, "#k-water": True, "#k-rest": not needs,
                                          "#k-wt": weight != "never", "#xp": level}.items():
                    self.assertEqual(self.page.locator(selector).is_visible(), visible, selector)
                self.assertEqual(self.page.locator("#clockTxt").text_content(), "13:30" if military else "1:30 PM")
                self.send({"hp": 60, "ap": 70, "rads": 100, "survival": {"stages": {"food": 1, "water": 0, "rest": 1}}})
                for key in ("hp", "ap", "rad", "food", "rest"):
                    self.assertTrue(self.page.locator("#k-" + key).is_visible())
                self.assertEqual(self.page.locator("#k-water").is_visible(), not needs)
                self.assertEqual(self.opacity(), "0", "Needs and vitals updates must not reshow location")
                self.send({"location": "Red Rocket"})
                self.assertEqual(self.opacity(), "1")
                count += 1
        print(f"Dynamic/clock/level/weight combinations checked: {count}")

    def test_higher_priority_hiding_pause_and_settings_changes(self):
        settings = copy.deepcopy(self.defaults)
        settings.update(locationOnArrival=True, idleOpacity=.2)
        settings["contextRules"] = {k: "show" for k in ACTIVATE}
        for hidden in ({"menus": ["PauseMenu", "VATSMenu"]}, {"hudHidden": True}, {"photo": True}, {"prismaFocus": True}):
            with self.subTest(hidden=hidden):
                self.seed(settings)
                self.page.clock.run_for(6000)
                self.send({"context": {**EXPLORE, **hidden}})
                self.page.clock.run_for(12000)
                self.assertEqual(self.opacity(), "1")
                self.assertTrue(self.page.locator(".loc[data-g=vitals]").evaluate("e => e.classList.contains('ctx-hide')"))
                self.send({"settings": {**settings, "clock24": True, "showLevelBar": False}})
                self.send({"context": EXPLORE, "paused": True})
                self.page.clock.run_for(12000)
                self.assertEqual(self.opacity(), "1")
                self.send({"paused": False})
                self.page.clock.run_for(2600)
                self.assertEqual(self.opacity(), "0")
                self.assertFalse(self.page.locator("#xp").is_visible())
                self.assertEqual(self.page.locator("#clockTxt").text_content(), "13:30")

    def test_other_widgets_respond_with_arrival_enabled_and_expired(self):
        for arrival in (False, True):
            settings = {**self.defaults, "locationOnArrival": arrival}
            self.seed(settings)
            self.page.clock.run_for(8600)
            for enabled in (False, True):
                with self.subTest(arrival=arrival, enabled=enabled):
                    self.send({"settings": {"perkReminder": enabled, "karmaReadout": enabled,
                               "challengeTracker": enabled, "localReputation": enabled,
                               "showAdrenaline": enabled, "needStageNames": enabled,
                               "hideHealthyFigure": enabled, "armorPieces": enabled,
                               "survivalEnemyMarkers": enabled, "mineWarnings": enabled,
                               "mineCompass": enabled, "legendaryDropMarkers": enabled,
                               "levelUpBanner": enabled}})
                    self.send({"karma": {"value": 380, "title": "Defender"},
                               "challenges": [{"id": "check", "name": "Test challenge", "cur": 1, "max": 10}],
                               "limbs": {"head": 1, "torso": 1, "larm": 1, "rarm": 1, "lleg": 1, "rleg": 1},
                               "heading": 0, "markers": [{"id": "enemy-check", "t": "enemy", "b": 0, "dist": 10}],
                               "mines": [{"id": "mine-check", "x": 900, "y": 600, "b": 20, "dist": 5}],
                               "legendaryLoot": {"id": "drop-check", "dropped": True, "item": "Rifle", "b": 30, "dist": 12}})
                    for selector in ("#karma", "#chal", "#lrep", "#adren", "#perkChip"):
                        self.assertEqual(self.page.locator(selector).is_visible(), enabled, selector)
                    self.assertEqual(self.page.evaluate("window.hud.state.figure.hideHealthy"), enabled)
                    self.assertEqual(self.page.locator(".limbs").evaluate("e => e.classList.contains('healthy')"), enabled)
                    self.assertEqual(self.page.evaluate("window.hud.state.settings.armorPieces"), enabled)
                    self.assertEqual(self.page.locator("#mineLayer .mn").count(), int(enabled))
                    self.assertEqual(self.page.locator("#marks .enemy").count(), int(enabled))
                    self.assertEqual(self.page.evaluate("window.hud.state.markers.some(m => m.id === 'loot:drop-check')"), enabled)
                    need_text = self.page.locator("#k-water small b").text_content()
                    self.assertEqual("Parched" in need_text, enabled)
                    self.assertEqual(self.opacity(), "0" if arrival else "1")
                    self.send({"levelUp": {"from": 23, "to": 24, "perks": 1}})
                    self.assertEqual(self.page.locator("#lvlup").evaluate("e => e.classList.contains('show')"), enabled)
                    self.page.clock.run_for(12000)
                    self.send({"legendaryLoot": {"id": "drop-check", "picked": True, "item": "Rifle"}})


if __name__ == "__main__":
    unittest.main()
