"""Browser regressions for save boundaries and replay; run with unittest."""
import json
import os
import pathlib
import unittest
from playwright.sync_api import sync_playwright

ROOT = pathlib.Path(__file__).resolve().parents[1]
HUD = pathlib.Path(os.environ.get("PHOSPHOR_TEST_MOD", ROOT / "mod/Phosphor HUD")) / "PrismaUI_F4/views/PhosphorHUD/hud.html"


class HudTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.runtime = sync_playwright().start()
        cls.browser = cls.runtime.chromium.launch()

    @classmethod
    def tearDownClass(cls):
        cls.browser.close()
        cls.runtime.stop()

    def setUp(self):
        self.page = self.browser.new_page(viewport={"width": 1920, "height": 1080})
        self.errors = []
        self.page.on("pageerror", lambda error: self.errors.append(str(error)))
        # Game rendering must work offline, without Google's font service.
        self.page.route("https://**/*", lambda route: route.abort())
        self.page.goto(HUD.as_uri(), wait_until="domcontentloaded")
        self.page.wait_for_function("typeof window.hudUpdate === 'function'")

    def tearDown(self):
        self.page.close()
        self.assertEqual(self.errors, [], "No asynchronous JavaScript exceptions")

    def send(self, message):
        self.page.evaluate("data => window.hudUpdate(data)", json.dumps(message))

    def test_crosshair_master_and_independent_hit_markers(self):
        self.page.clock.install()
        self.send({"snapshot": True, "settings": {"showCrosshair": False},
                   "prompt": {"name": "Door", "key": "E", "verb": "Open"},
                   "loot": {"name": "Crate", "items": [{"name": "Ammo", "count": 2}]}})
        hidden = lambda selector: self.page.locator(selector).evaluate("e => !!e.closest('.ctx-hide')")
        self.assertTrue(hidden("#cross"))
        self.assertFalse(hidden("#hint"))
        self.assertFalse(hidden("#loot"))
        for context in ({"ads": True, "thirdPerson": True}, {"menus": ["ScopeMenu"]}):
            self.send({"context": context, "settings": {"contextRules": {"ads": "show", "scope": "show"}}})
            self.assertTrue(hidden("#cross"), "Master off wins over context Show and third-person exemption")
        self.send({"context": {"menus": [], "ads": False}, "settings": {"calmDelay": 0, "calmRules": {"cross": "hide"}}})
        self.page.evaluate("window.hud.hitmark(true)")
        self.assertFalse(hidden("#hit"), "Hit marker is outside both reticle hiding paths")
        self.assertTrue(self.page.locator("#hit").evaluate("e => e.classList.contains('on')"))
        self.send({"settings": {"showHitMarkers": False}})
        self.page.evaluate("window.hud.hitmark(false)")
        self.assertFalse(self.page.locator("#hit").evaluate("e => e.classList.contains('on')"))
        self.send({"reset": True, "snapshot": True})
        self.assertTrue(hidden("#cross"), "Preferences persist through save reset")

    def test_calm_delay_repeated_false_combat_exit_and_reset(self):
        self.page.clock.install()
        self.send({"snapshot": True, "combat": True, "settings": {"calmRules": {"cross": "hide"}}})
        self.send({"combat": False})
        self.page.clock.fast_forward(4000)
        self.send({"combat": False})
        self.assertEqual(self.page.locator("#stage").get_attribute("data-ctx"), "explore")
        self.page.clock.fast_forward(1100)
        self.assertEqual(self.page.locator("#stage").get_attribute("data-ctx"), "calm")
        self.send({"combat": True})
        self.assertEqual(self.page.locator("#stage").get_attribute("data-ctx"), "explore")
        self.send({"combat": False})
        self.page.clock.fast_forward(2000)
        self.send({"combat": True})
        self.send({"combat": False})
        self.page.clock.fast_forward(4000)
        self.assertEqual(self.page.locator("#stage").get_attribute("data-ctx"), "explore")
        self.page.clock.fast_forward(1100)
        self.assertEqual(self.page.locator("#stage").get_attribute("data-ctx"), "calm")
        self.send({"reset": True, "snapshot": True, "combat": False})
        self.assertEqual(self.page.locator("#stage").get_attribute("data-ctx"), "explore")
        self.page.clock.fast_forward(5100)
        self.assertEqual(self.page.locator("#stage").get_attribute("data-ctx"), "calm")

    def test_calm_each_rule_and_awake_override(self):
        self.page.clock.install()
        self.send({"snapshot": True, "hp": 100, "hpMax": 100, "rads": 0, "combat": False})
        self.page.clock.fast_forward(6000)
        selectors = {"vitals": "#cores", "compass": "#compass", "kit": ".kit-row",
                     "party": "#party", "tracker": "#tracker", "effects": "#effects", "cross": "#cross"}
        for group, selector in selectors.items():
            for rule in ("hide", "dim"):
                with self.subTest(group=group, rule=rule):
                    self.send({"settings": {"calmRules": {group: rule}}})
                    node = self.page.locator(selector)
                    self.assertTrue(node.evaluate("(e, cls) => !!e.closest('.'+cls)", "ctx-"+rule))
                    node.evaluate("e => e.classList.add('awake')")
                    self.page.evaluate("window.hud.setContext({})")
                    self.assertFalse(node.evaluate("e => !!e.closest('.ctx-hide,.ctx-dim')"))
                    node.evaluate("e => e.classList.remove('awake')")
                    self.send({"settings": {"calmRules": {group: "show"}}})
        for selector in ("#hint", "#loot", "#sneakAtk", "#hit"):
            self.send({"settings": {"calmRules": {"cross": "hide"}}})
            self.assertFalse(self.page.locator(selector).evaluate("e => !!e.closest('.ctx-hide')"), selector)

    def test_calm_real_wakes_low_health_and_context_priority(self):
        self.page.clock.install()
        rules = {key: "hide" for key in ("vitals", "compass", "kit", "party", "tracker", "effects", "cross")}
        self.send({"snapshot": True, "hp": 100, "hpMax": 100, "rads": 0, "combat": False,
                   "settings": {"calmDelay": 0, "calmRules": rules}})
        self.page.clock.fast_forward(6000)
        hidden = lambda s: self.page.locator(s).evaluate("e => !!e.closest('.ctx-hide')")
        self.assertTrue(hidden("#cores"))
        self.send({"hp": 34})
        self.page.clock.fast_forward(6000)
        self.assertFalse(hidden("#cores"), "Low HP keeps vitals awake after the damage wake expires")
        self.send({"hp": 35})
        self.page.clock.fast_forward(6000)
        self.assertTrue(hidden("#cores"), "Low-health boundary is strictly below 35 percent")
        self.send({"clip": 4})
        self.assertFalse(hidden(".kit-row"))
        self.assertFalse(hidden("#cross"))
        self.page.clock.fast_forward(5100)
        self.assertTrue(hidden("#cross"))
        self.send({"tracker": {"title": "Test quest", "objectives": [{"t": "Find the door", "done": False}]}})
        self.assertFalse(hidden("#tracker"))
        self.page.clock.fast_forward(5100)
        self.assertTrue(hidden("#tracker"))
        self.send({"tracker": {"title": "Test quest", "objectives": [{"t": "Find the door", "done": False, "dist": 20}]}})
        self.assertTrue(hidden("#tracker"), "Distance refresh is not a quest update")
        self.send({"quest": {"title": "Objective completed"}})
        self.assertFalse(hidden("#tracker"))
        for context, expected in (({"powerArmor": True}, "powerarmor"), ({"powerArmor": False, "menus": ["VATSMenu"]}, "vats"),
                                  ({"menus": ["PauseMenu"]}, "menu"), ({"menus": ["ScopeMenu"]}, "scope"),
                                  ({"menus": [], "ads": True, "thirdPerson": True}, "ads")):
            self.send({"context": context})
            self.assertEqual(self.page.locator("#stage").get_attribute("data-ctx"), expected)
        self.assertFalse(hidden("#cross"), "Third-person ADS keeps its 0.9.18 exemption over calm")

    def test_calm_preview_combat_button_and_rules_row(self):
        self.page.clock.install()
        self.page.goto(HUD.as_uri() + "#preview", wait_until="domcontentloaded")
        self.page.reload(wait_until="domcontentloaded")
        self.assertIn("Out of combat", self.page.locator("#rulesTable").text_content())
        self.page.locator("#b-combat").evaluate("e => e.click()")
        self.page.locator("#b-combat").evaluate("e => e.click()")
        self.page.clock.fast_forward(4000)
        self.assertEqual(self.page.locator("#stage").get_attribute("data-ctx"), "explore")
        self.page.clock.fast_forward(1100)
        self.assertEqual(self.page.locator("#stage").get_attribute("data-ctx"), "calm")

    def test_calm_effect_illness_compass_and_companion_wakes(self):
        self.page.clock.install()
        self.send({"snapshot": True, "combat": False, "settings": {"calmDelay": 0,
                   "calmRules": {"effects": "hide", "compass": "hide", "party": "hide"}}})
        self.page.clock.fast_forward(6000)
        hidden = lambda s: self.page.locator(s).evaluate("e => !!e.closest('.ctx-hide')")
        self.assertTrue(hidden("#effects"))
        self.send({"illness": {"id": "infection", "hoursLeft": 12, "quiet": True}})
        self.assertFalse(hidden("#effects"), "Illness updates use the same effects wake path")
        self.page.clock.fast_forward(5100)
        self.assertTrue(hidden("#effects"))
        self.send({"cure": "infection"})
        self.assertFalse(hidden("#effects"))
        self.send({"markers": [{"id": "custom", "t": "pin", "b": 30, "dist": 50}]})
        self.assertFalse(hidden("#compass"))
        self.page.clock.fast_forward(5100)
        self.send({"markers": [{"id": "custom", "t": "pin", "b": 40, "dist": 40}]})
        self.assertTrue(hidden("#compass"), "Continuous bearings/distances must not keep compass awake")
        self.send({"party": [{"name": "Porter Gage", "type": "human", "hp": 1}]})
        self.assertFalse(hidden("#party"))
        self.send({"reset": True, "snapshot": True, "combat": False})
        self.assertTrue(hidden("#party"), "Reset discards previous wake states")

    def test_third_person_aim_crosshair_default_and_optional_hiding(self):
        self.send({"snapshot": True, "context": {"menus": [], "ads": True, "thirdPerson": True}})
        hidden = lambda: self.page.locator("#cross").evaluate("e => !!e.closest('.ctx-hide')")
        self.assertFalse(hidden(), "Third-person aiming keeps its aiming reference by default")
        self.assertTrue(self.page.locator("#hint").evaluate("e => !!e.closest('.ctx-hide')"), "Interaction prompt retains existing ADS hiding")
        self.page.clock.install()
        self.page.clock.fast_forward(10000)
        self.assertFalse(hidden(), "Sustained aiming must not hide the crosshair")
        self.send({"settings": {"hideThirdPersonAimCrosshair": True}})
        self.assertTrue(hidden(), "Optional hiding preserves the existing ADS rule")
        self.send({"context": {"ads": False}})
        self.assertFalse(hidden())
        self.send({"context": {"ads": True}, "settings": {"hideThirdPersonAimCrosshair": False}})
        self.assertFalse(hidden())
        self.send({"context": {"thirdPerson": False}})
        self.assertTrue(hidden(), "First-person iron sights retain their default hide rule")
        self.send({"context": {"thirdPerson": True}})
        self.assertFalse(hidden())

    def test_third_person_crosshair_respects_context_overrides_and_reset(self):
        hidden = lambda: self.page.locator("#cross").evaluate("e => !!e.closest('.ctx-hide')")
        self.send({"snapshot": True, "context": {"menus": [], "ads": True, "thirdPerson": True}})
        for menu in ("ScopeMenu", "VATSMenu", "PipboyMenu", "PauseMenu", "DialogueMenu", "WorkshopMenu"):
            self.send({"context": {"menus": [menu]}})
            self.assertTrue(hidden(), menu)
        self.send({"context": {"menus": [], "powerArmor": True}})
        self.assertTrue(hidden())
        self.send({"context": {"powerArmor": False}, "settings": {"contextRules": {"ads": "hide"}}})
        self.assertTrue(hidden(), "Hide everything still wins")
        self.send({"settings": {"hideThirdPersonAimCrosshair": True, "contextRules": {"ads": "show"}}})
        self.assertFalse(hidden(), "Show everything still wins")
        self.send({"settings": {"contextRules": {"ads": "recommended"}}})
        self.assertTrue(hidden())
        self.send({"reset": True, "snapshot": True, "context": {"ads": True, "thirdPerson": True}})
        self.assertTrue(hidden(), "Saved preference survives reload")
        self.send({"settings": {"hideThirdPersonAimCrosshair": False}})
        self.assertFalse(hidden())

    def test_custom_waypoint_moves_removes_and_stays_at_compass_edge(self):
        self.send({"snapshot": True, "heading": 0, "markers": [
            {"id": "player-waypoint", "t": "pin", "b": 180, "dist": 1200, "label": "Custom waypoint", "active": True}]})
        marker = self.page.locator("#marks .mk.pin")
        self.assertEqual(marker.count(), 1)
        self.assertGreater(float(marker.evaluate("e => e.style.opacity")), 0, "Waypoint behind player remains at compass edge")
        self.send({"markers": [{"id": "player-waypoint", "t": "pin", "b": 0, "dist": 10, "label": "Custom waypoint", "active": True}]})
        self.assertEqual(marker.count(), 1)
        self.assertFalse(marker.evaluate("e => e.classList.contains('edge-l') || e.classList.contains('edge-r')"))
        self.send({"markers": []})
        self.assertEqual(marker.count(), 0)

    def test_companion_compass_dot_uses_the_plugin_bearing(self):
        self.send({"snapshot": True, "heading": 0, "markers": [], "party": [{"name": "Dogmeat", "type": "dog"}]})
        dots = self.page.locator("#marks .mk.ally")
        self.assertEqual(dots.count(), 0, "The party list alone adds no guessed compass dot")
        left = lambda: float(dots.evaluate("e => parseFloat(e.style.left)"))
        self.send({"markers": [{"id": "ally:Dogmeat", "t": "ally", "b": 0, "dist": 4, "label": "Dogmeat"}]})
        self.assertEqual(dots.count(), 1)
        self.assertAlmostEqual(left(), 320, places=0, msg="A companion straight ahead sits at the compass centre")
        self.send({"markers": [{"id": "ally:Dogmeat", "t": "ally", "b": 30, "dist": 4, "label": "Dogmeat"}]})
        self.assertGreater(left(), 330, "A companion to the right is drawn right of centre")
        self.send({"heading": 30})
        self.assertAlmostEqual(left(), 320, places=0, msg="Turning to face the companion centres it")
        self.send({"party": [{"name": "Dogmeat", "type": "dog", "hp": 0.5}]})
        self.assertEqual(dots.count(), 1, "Party updates neither add nor move the dot")
        self.assertAlmostEqual(left(), 320, places=0)

    def test_undiscovered_locations_follow_the_mcm_toggle(self):
        marks = [{"id": "pl1", "t": "factory", "b": 5, "dist": 60, "label": "Old Mill", "disc": False},
                 {"id": "pl2", "t": "vault", "b": -5, "dist": 80, "label": "Vault 111", "disc": True}]
        self.send({"snapshot": True, "heading": 0, "markers": marks})
        hollow = self.page.locator("#marks .mk.undisc")
        self.assertEqual(hollow.count(), 1, "Undiscovered locations show hollow by default")
        self.assertEqual(self.page.locator("#marks .mk.vault").count(), 1)
        self.send({"settings": {"showUndiscovered": False}})
        self.assertEqual(hollow.count(), 0, "Turning the option off removes undiscovered locations at once")
        self.assertEqual(self.page.locator("#marks .mk.vault").count(), 1, "Discovered locations stay")
        self.send({"markers": marks})
        self.assertEqual(hollow.count(), 0, "New marker lists respect the option")
        self.send({"settings": {"showUndiscovered": True}})
        self.assertEqual(hollow.count(), 1, "Turning it back on restores them")

    def test_compass_focus_name_holds_through_small_heading_wobble(self):
        self.send({"snapshot": True, "heading": 0, "markers": [
            {"id": "pl7", "t": "landmark", "b": 9, "dist": 50, "label": "Nahant Chapel", "disc": True}]})
        shown = lambda: self.page.locator("#cfocus").evaluate("e => e.classList.contains('show')")
        self.assertFalse(shown(), "A marker 9 degrees off centre does not take focus")
        self.send({"heading": 3})
        self.assertTrue(shown(), "Facing the marker shows its name")
        for heading in (-1.5, 0, -1, 0.5):
            self.send({"heading": heading})
            self.assertTrue(shown(), "Wobbling around the 8 degree edge keeps the name up")
        self.send({"heading": -3})
        self.assertFalse(shown(), "Turning clearly away drops the name")
        self.send({"heading": 0})
        self.assertFalse(shown(), "Once dropped, the name needs the marker back within 8 degrees")

    def test_compass_icons_and_lines_keep_a_dark_outline_on_bright_ground(self):
        self.send({"snapshot": True, "heading": 0, "markers": [
            {"id": "pl7", "t": "landmark", "b": 2, "dist": 50, "label": "Nahant Chapel", "disc": True}]})
        def looks():
            return self.page.evaluate("""() => [document.querySelector('#marks .mk svg'), document.querySelector('#ticks .tk'),
                document.querySelector('#compass .needle')].map(n => { const s = getComputedStyle(n); return [s.filter, s.boxShadow]; })""")
        def box():
            return self.page.locator("#compass").evaluate("""e => Array.from(e.querySelectorAll('*')).map(n => {
                const r = n.getBoundingClientRect(); return [r.x, r.y, r.width, r.height]; })""")
        off, layout = looks(), box()
        self.assertIn("rgba(0, 0, 0", off[0][0], "Icons carry a dark edge even with readability off")
        self.assertNotEqual(off[1][1], "none", "Ticks carry a dark edge even with readability off")
        seen = []
        for mode in ("subtle", "strong"):
            self.send({"settings": {"textReadability": mode}})
            now = looks()
            self.assertNotEqual(now, off, mode + " readability strengthens the compass outline")
            self.assertEqual(box(), layout, "Outlines must not move anything")
            seen.append(now)
        self.assertNotEqual(*seen)
        self.send({"settings": {"textReadability": "off"}})
        self.assertEqual(looks(), off)

    def test_text_readability_changes_shadows_without_layout_or_hide_changes(self):
        self.send({"snapshot": True, "location": "Sanctuary Hills", "hp": 100, "hpMax": 100,
                   "ap": 100, "apMax": 100, "rads": 0,
                   "settings": {"hideFullVitals": True, "showLevelBar": False}})
        self.page.evaluate("document.querySelectorAll('*').forEach(e => e.getAnimations().forEach(a => a.finish()))")
        def appearance():
            return self.page.locator("#stage").evaluate("""e => Array.from(e.querySelectorAll('*')).map(n => {
                const r=n.getBoundingClientRect(), s=getComputedStyle(n);
                return [r.x,r.y,r.width,r.height,s.display,s.visibility,s.opacity];
            })""")
        def shadows():
            return self.page.locator("#stage").evaluate("""e => [e,e.querySelector('#locName'),e.querySelector('.kbd'),
                e.querySelector('.subs p'),e.querySelector('.lu-num')].map(n => getComputedStyle(n).textShadow)""")
        baseline, original = appearance(), shadows()
        modes = []
        for mode in ("subtle", "strong"):
            self.send({"settings": {"textReadability": mode}})
            current = shadows()
            for previous, updated in zip(original, current):
                self.assertNotEqual(previous, updated, "Readability must affect ordinary and specially styled text")
            self.assertEqual(appearance(), baseline, "Shadow must not move or reveal widgets")
            modes.append(current)
        self.assertNotEqual(*modes)
        self.send({"settings": {"textReadability": "off"}})
        self.assertEqual(shadows(), original, "Off restores all original text styles")
        self.assertEqual(appearance(), baseline)

    def test_text_readability_survives_partial_settings_and_save_reset(self):
        self.send({"settings": {"textReadability": "strong"}})
        shadow = self.page.locator("#locName").evaluate("e => getComputedStyle(e).textShadow")
        self.send({"reset": True, "settings": {"clock24": True}})
        self.send({"snapshot": True, "location": "Red Rocket"})
        self.assertEqual(self.page.locator("#locName").evaluate("e => getComputedStyle(e).textShadow"), shadow)
        self.send({"settings": {"textReadability": "off"}})
        self.assertNotEqual(self.page.locator("#locName").evaluate("e => getComputedStyle(e).textShadow"), shadow)

    def test_hidden_until_snapshot_and_settings_do_not_reveal_demo(self):
        self.assertTrue(self.page.locator("#stage").evaluate("e => e.classList.contains('awaiting')"))
        self.send({"settings": {"limbFigure": "silhouette"}})
        self.assertTrue(self.page.locator("#stage").evaluate("e => e.classList.contains('awaiting')"))
        self.send({"snapshot": True, "hp": 88, "hpMax": 100, "rads": 0})
        self.assertFalse(self.page.locator("#stage").evaluate("e => e.classList.contains('awaiting')"))
        self.assertEqual(self.page.evaluate("window.hud.state.hp"), 88)

    def test_reset_cancels_banner_queue_and_new_banner_starts_immediately(self):
        self.send({"snapshot": True, "quest": {"title": "Old save", "ms": 60000}})
        self.send({"quest": {"title": "Must never appear", "ms": 60000}})
        self.send({"reset": True})
        self.send({"snapshot": True, "quest": {"title": "New save", "ms": 60000}})
        self.assertEqual(self.page.locator("#qTitle").text_content(), "New save")
        self.assertTrue(self.page.locator("#quest").evaluate("e => e.classList.contains('show')"))

    def test_canceled_banner_timer_cannot_advance_new_queue(self):
        self.page.clock.install()
        self.send({"snapshot": True, "quest": {"title": "Old", "ms": 500}})
        self.send({"reset": True})
        self.send({"quest": {"title": "New first", "ms": 60000}})
        self.send({"quest": {"title": "New second", "ms": 60000}})
        self.assertEqual(self.page.locator("#qTitle").text_content(), "New first")
        self.page.clock.run_for(1000)
        self.assertEqual(self.page.locator("#qTitle").text_content(), "New first")

    def test_reset_clears_feed_and_enemy(self):
        self.send({"snapshot": True, "notify": {"text": "Old pickup"},
                   "target": {"name": "Old enemy", "pct": .5, "level": 2}})
        self.send({"reset": True})
        self.assertEqual(self.page.locator("#feed").inner_text(), "")
        self.assertIsNone(self.page.evaluate("window.hud.state.target"))

    def test_radiation_gain_with_absolute_value_does_not_double_count(self):
        self.send({"snapshot": True, "hp": 90, "hpMax": 100, "rads": 4})
        self.send({"rads": 9, "radsGain": 5})
        self.assertEqual(self.page.evaluate("window.hud.state.rads"), 9)
        self.assertEqual(self.page.locator("#k-rad .pop").last.text_content(), "+5 RADS")
        # A batched popup can arrive after the absolute value was already sent.
        self.send({"rads": 9, "radsGain": 2})
        self.assertEqual(self.page.evaluate("window.hud.state.rads"), 9)
        self.send({"radsGain": 1})
        self.assertEqual(self.page.evaluate("window.hud.state.rads"), 10)
        self.send({"rads": 0, "radsGain": 1})
        self.assertEqual(self.page.evaluate("window.hud.state.rads"), 0)

    def test_small_radiation_has_visible_arc_without_inflating_hp_bite(self):
        self.send({"snapshot": True, "hp": 100, "hpMax": 100, "rads": 1})
        rad_arc = self.page.locator("#k-rad .val").evaluate_all(
            "paths => paths.reduce((n, p) => n + p.getTotalLength(), 0)")
        self.assertGreater(rad_arc, 1, "Even one rad must draw a visible arc")
        self.assertAlmostEqual(self.page.evaluate("window.hud.state.hp"), 99.9)
        self.assertEqual(self.page.locator("#k-rad small b").text_content(), "1")
        self.send({"rads": 0})
        self.assertTrue(self.page.locator("#k-rad .val").evaluate_all(
            "paths => paths.every(p => p.getAttribute('d') === '')"))

    def test_radiation_total_updates_while_consuming_in_hidden_pipboy(self):
        # Synthetic engine readings: this checks display behavior, not item-effect calculation.
        self.send({"snapshot": True, "settings": {"hideFullVitals": True},
                   "hp": 450, "hpMax": 500, "ap": 100, "apMax": 100, "rads": 0})
        self.assertFalse(self.page.locator("#k-rad").is_visible())
        self.send({"paused": True, "context": {"menus": ["PipboyMenu"]}})
        # Healing and radiation can arrive together after consuming an item.
        self.send({"hp": 480, "rads": 6})
        self.send({"rads": 6, "radsGain": 6})
        self.send({"paused": False, "context": {"menus": []}})
        self.assertEqual(self.page.locator("#k-rad small b").text_content(), "6")
        self.assertEqual(self.page.evaluate("window.hud.state.hp"), 480)
        self.assertTrue(self.page.locator("#k-rad").is_visible())
        self.assertFalse(self.page.locator("#cores").evaluate("e => e.classList.contains('ctx-hide')"))
        # A second item increases the total; the delayed popup must not double it.
        self.send({"hp": 500, "rads": 10})
        self.send({"rads": 10, "radsGain": 4})
        self.assertEqual(self.page.locator("#k-rad small b").text_content(), "10")
        self.assertEqual(self.page.evaluate("window.hud.state.hp"), 495)
        self.send({"rads": 0})
        self.assertFalse(self.page.locator("#k-rad").is_visible())

    def test_level_bar_setting_hides_row_and_preserves_updates(self):
        self.send({"snapshot": True, "level": 23, "xp": .5, "time": {"hour": 13, "timescale": 0}})
        self.assertTrue(self.page.locator("#xp").is_visible())
        self.send({"settings": {"showLevelBar": False}})
        self.assertFalse(self.page.locator("#xp").is_visible())
        self.assertTrue(self.page.locator("#k-hp").is_visible())
        self.assertTrue(self.page.locator("#clockTxt").is_visible())
        self.send({"reset": True})
        self.send({"snapshot": True, "level": 24, "xp": .25,
                   "context": {"menus": ["VATSMenu"], "rules": {"vats": "show"}}})
        self.assertFalse(self.page.locator("#xp").is_visible())
        self.send({"settings": {"showLevelBar": True}})
        self.assertTrue(self.page.locator("#xp").is_visible())
        self.assertEqual(self.page.locator("#lvl").text_content(), "24")
        self.assertEqual(self.page.locator("#xpPct").text_content(), "25%")

    def test_clock_format_setting_switches_immediately_and_survives_time_updates(self):
        self.send({"settings": {"clock24": True}})
        self.assertTrue(self.page.locator("#clock").evaluate("e => e.hidden"))
        self.send({"snapshot": True, "time": {"hour": 13.5, "timescale": 0}})
        self.assertEqual(self.page.locator("#clockTxt").text_content(), "13:30")
        for hour, h12, h24 in [(0, "12:00 AM", "00:00"), (12, "12:00 PM", "12:00"),
                               (23.5, "11:30 PM", "23:30"), (24, "12:00 AM", "00:00")]:
            self.send({"settings": {"clock24": False}, "time": {"hour": hour, "timescale": 0}})
            self.assertEqual(self.page.locator("#clockTxt").text_content(), h12)
            self.send({"settings": {"clock24": True}})
            self.assertEqual(self.page.locator("#clockTxt").text_content(), h24)
        self.send({"reset": True})
        self.send({"snapshot": True, "time": {"hour": 9.5, "timescale": 0}})
        self.assertEqual(self.page.locator("#clockTxt").text_content(), "09:30")

    def test_stealth_without_multiplier_keeps_detection_and_clears_old_number(self):
        self.send({"snapshot": True, "target": {"name": "Raider", "level": 5, "pct": .8},
                   "stealth": {"sneaking": True, "level": 0, "mult": 3.5}})
        self.assertEqual(self.page.locator("#sneakAtk").text_content(), "Sneak attack ×3.5")
        self.send({"stealth": {"sneaking": True, "level": 0, "mult": None}})
        self.assertEqual(self.page.locator("#sneakAtk").text_content(), "Sneak attack")
        self.assertTrue(self.page.locator("#sneakAtk").evaluate("e => e.classList.contains('show')"))
        for level, label in [(35, "Caution"), (70, "Danger"), (95, "Detected")]:
            self.send({"stealth": {"sneaking": True, "level": level, "mult": None}})
            self.assertEqual(self.page.locator("#stLabel").text_content(), label)
            self.assertFalse(self.page.locator("#sneakAtk").evaluate("e => e.classList.contains('show')"))
        self.send({"stealth": {"sneaking": False, "mult": None}})
        self.assertFalse(self.page.locator("#stealth").evaluate("e => e.classList.contains('show')"))

    def test_dynamic_needs_show_independently_from_first_stage_until_satisfied(self):
        stages = {"food": 0, "water": 0, "rest": 0}
        self.send({"snapshot": True, "survival": {"on": True, "stages": stages}})
        for need in stages:
            self.assertTrue(self.page.locator("#k-" + need).is_visible(), "Default stays always visible")
        self.send({"settings": {"hideSatisfiedNeeds": True}})
        for need in stages:
            self.assertFalse(self.page.locator("#k-" + need).is_visible(), "Satisfied needs hide immediately")
        self.assertEqual(self.box(".cores .gap.survival-only")["width"], 0)
        for need, label in [("food", "Peckish"), ("water", "Parched"), ("rest", "Tired")]:
            for stage in range(1, 6):
                stages[need] = stage
                self.send({"survival": {"stages": stages}})
                for other in stages:
                    self.assertEqual(self.page.locator("#k-" + other).is_visible(), other == need)
                self.assertGreater(self.box(".cores .gap.survival-only")["width"], 0)
                if stage == 1:
                    self.assertIn(label.upper(), self.page.locator("#k-" + need).inner_text())
            stages[need] = 0
            self.send({"survival": {"stages": stages}})
            self.assertFalse(self.page.locator("#k-" + need).is_visible())
        for core in ("hp", "ap", "rad"):
            self.assertTrue(self.page.locator("#k-" + core).is_visible())

    def test_dynamic_needs_recovery_settings_and_save_snapshot(self):
        self.send({"snapshot": True, "settings": {"hideSatisfiedNeeds": True},
                   "survival": {"on": True, "stages": {"food": 2, "water": 1, "rest": 1}}})
        self.send({"survival": {"stages": {"food": 0, "water": 1, "rest": 0}}})
        self.assertFalse(self.page.locator("#k-food").is_visible())
        self.assertTrue(self.page.locator("#k-water").is_visible())
        self.assertFalse(self.page.locator("#k-rest").is_visible())
        self.send({"settings": {"hideSatisfiedNeeds": False}})
        for need in ("food", "water", "rest"):
            self.assertTrue(self.page.locator("#k-" + need).is_visible())
        self.send({"settings": {"hideSatisfiedNeeds": True}})
        self.send({"reset": True})
        self.send({"snapshot": True, "survival": {"on": True, "stages": {"food": 1, "water": 0, "rest": 0}}})
        self.assertTrue(self.page.locator("#k-food").is_visible())
        self.assertFalse(self.page.locator("#k-water").is_visible())
        self.assertFalse(self.page.locator("#k-rest").is_visible())
        self.send({"survival": {"on": False}})
        self.send({"settings": {"hideSatisfiedNeeds": False}})
        for need in ("food", "water", "rest"):
            self.assertFalse(self.page.locator("#k-" + need).is_visible(), "Non-survival stays hidden")

    def test_dynamic_needs_preserve_context_hiding_and_fraction_fallback(self):
        self.send({"snapshot": True, "settings": {"hideSatisfiedNeeds": True},
                   "survival": {"on": True}, "needs": {"food": 1, "water": .8, "rest": 1}})
        self.assertFalse(self.page.locator("#k-food").is_visible())
        self.assertTrue(self.page.locator("#k-water").is_visible())
        self.send({"needs": {"water": 1}})
        self.assertFalse(self.page.locator("#k-water").is_visible())
        # The authoritative stage wins over a conflicting percentage.
        self.send({"survival": {"stages": {"food": 1, "water": 0, "rest": 0}}})
        self.assertTrue(self.page.locator("#k-food").is_visible())
        self.send({"context": {"powerArmor": True}})
        self.page.wait_for_function("getComputedStyle(document.getElementById('cores')).opacity === '0'")
        self.assertEqual(self.page.locator("#cores").evaluate("e => getComputedStyle(e).opacity"), "0")
        self.send({"context": {"powerArmor": False}})
        self.assertTrue(self.page.locator("#k-food").is_visible())
        self.assertFalse(self.page.locator("#k-water").is_visible())

    def test_thirst_recovery_without_new_consumable_effect_respects_dynamic_setting(self):
        for dynamic in (False, True):
            with self.subTest(dynamic=dynamic):
                self.send({"snapshot": True, "settings": {"hideSatisfiedNeeds": dynamic, "needStageNames": True},
                           "survival": {"on": True, "stages": {"food": 1, "water": 1, "rest": 1}},
                           "needs": {"food": .8, "water": .8, "rest": .8}})
                self.assertTrue(self.page.locator("#k-water").is_visible())
                self.assertEqual(self.page.locator("#k-water small b").text_content(), "Parched")
                # World drinking has no new consumable effect; only the recovered need changes.
                self.send({"survival": {"stages": {"food": 1, "water": 0, "rest": 1}}, "needs": {"water": 1}})
                self.assertEqual(self.page.locator("#k-water").is_visible(), not dynamic)
                self.assertEqual(self.page.locator("#k-water small b").text_content(), "Hydrated")
                self.assertTrue(self.page.locator("#k-food").is_visible())
                self.assertTrue(self.page.locator("#k-rest").is_visible())
                self.send({"survival": {"stages": {"food": 1, "water": 1, "rest": 1}}, "needs": {"water": .8}})
                self.assertTrue(self.page.locator("#k-water").is_visible(), "A later thirst warning shows the wheel again")

    def test_hunger_recovery_without_new_food_effect_preserves_other_needs(self):
        for dynamic in (False, True):
            with self.subTest(dynamic=dynamic):
                self.send({"snapshot": True, "settings": {"hideSatisfiedNeeds": dynamic, "needStageNames": True},
                           "survival": {"on": True, "stages": {"food": 2, "water": 1, "rest": 1}},
                           "needs": {"food": .6, "water": .8, "rest": .8}})
                # A partial meal helps, but must not clear the remaining hunger stage.
                self.send({"survival": {"stages": {"food": 1, "water": 1, "rest": 1}}, "needs": {"food": .8}})
                self.assertTrue(self.page.locator("#k-food").is_visible())
                self.assertEqual(self.page.locator("#k-food small b").text_content(), "Peckish")
                self.send({"survival": {"stages": {"food": 0, "water": 1, "rest": 1}}, "needs": {"food": 1}})
                self.assertEqual(self.page.locator("#k-food").is_visible(), not dynamic)
                self.assertEqual(self.page.locator("#k-food small b").text_content(), "Well-fed")
                for other in ("water", "rest"):
                    self.assertTrue(self.page.locator("#k-" + other).is_visible())
                self.send({"survival": {"stages": {"food": 1, "water": 1, "rest": 1}}, "needs": {"food": .8}})
                self.assertTrue(self.page.locator("#k-food").is_visible())
                self.assertEqual(self.page.locator("#k-food small b").text_content(), "Peckish")

    def test_dynamic_vitals_toggle_and_independent_recovery(self):
        self.send({"snapshot": True, "hp": 100, "hpMax": 100, "ap": 100, "apMax": 100, "rads": 0})
        for key in ("hp", "ap", "rad"):
            self.assertTrue(self.page.locator("#k-" + key).is_visible())
        self.send({"settings": {"hideFullVitals": True}})
        for key in ("hp", "ap", "rad"):
            self.assertFalse(self.page.locator("#k-" + key).is_visible())
        for message, visible in [({"hp": 99}, {"hp"}), ({"hp": 100}, set()),
                                 ({"ap": 99}, {"ap"}), ({"ap": 100}, set()),
                                 ({"rads": 1}, {"rad"}), ({"rads": 0, "hp": 100}, set()),
                                 ({"hp": 60, "ap": 70, "rads": 100}, {"hp", "ap", "rad"})]:
            self.send(message)
            for key in ("hp", "ap", "rad"):
                self.assertEqual(self.page.locator("#k-" + key).is_visible(), key in visible)
        self.send({"hp": 100, "ap": 100, "rads": 0})
        self.send({"settings": {"hideFullVitals": False}})
        for key in ("hp", "ap", "rad"):
            self.assertTrue(self.page.locator("#k-" + key).is_visible())

    def test_dynamic_vitals_use_available_caps_and_native_rounding(self):
        # Native fatigue must retain its fraction: 100 AP lost out of 300 is not .33.
        self.send({"snapshot": True, "settings": {"hideFullVitals": True},
                   "hp": 100, "hpMax": 100, "ap": 200, "apMax": 300,
                   "survival": {"on": True, "fatigue": 100 / 300}})
        self.assertFalse(self.page.locator("#k-ap").is_visible())
        self.send({"ap": 199})
        self.assertTrue(self.page.locator("#k-ap").is_visible())
        self.send({"snapshot": True, "settings": {"hideFullVitals": True},
                   "hpMax": 101, "hp": 81, "rads": 200, "apMax": 101, "ap": 81,
                   "survival": {"on": True, "fatigue": .2}})
        for key in ("hp", "ap"):
            self.assertFalse(self.page.locator("#k-" + key).is_visible())
        self.assertTrue(self.page.locator("#k-rad").is_visible())
        self.send({"hp": 80, "ap": 80})
        for key in ("hp", "ap"):
            self.assertTrue(self.page.locator("#k-" + key).is_visible())
        # Non-integer available caps can round down in the native message.
        self.send({"hpMax": 102, "apMax": 102, "hp": 71, "ap": 71, "rads": 300,
                   "survival": {"fatigue": .3}})
        for key in ("hp", "ap"):
            self.assertFalse(self.page.locator("#k-" + key).is_visible())
        self.send({"hp": 70, "ap": 70})
        for key in ("hp", "ap"):
            self.assertTrue(self.page.locator("#k-" + key).is_visible())
        self.send({"hp": 0, "rads": 1000})
        self.assertTrue(self.page.locator("#k-hp").is_visible(), "Zero available health is not healthy")
        self.send({"survival": {"on": False}, "ap": 71})
        self.assertTrue(self.page.locator("#k-ap").is_visible(), "Fatigue cap only applies in survival")
        self.send({"ap": 0})
        self.assertTrue(self.page.locator("#k-ap").is_visible())
        self.send({"survival": {"on": True, "fatigue": .3}, "ap": 71})
        self.send({"settings": {"hideFullVitals": True}, "survival": {"on": True, "fatigue": 0},
                   "hp": 102, "ap": 102, "rads": 0})
        for key in ("hp", "ap", "rad"):
            self.assertFalse(self.page.locator("#k-" + key).is_visible(), "Batched settings must not clamp a new snapshot to the previous fatigue cap")

    def test_dynamic_vitals_and_needs_settings_are_independent_and_layout_compacts(self):
        self.send({"snapshot": True, "hp": 100, "hpMax": 100, "ap": 100, "apMax": 100, "rads": 0,
                   "survival": {"on": True, "stages": {"food": 0, "water": 1, "rest": 0}},
                   "settings": {"hideFullVitals": True, "hideSatisfiedNeeds": True, "showWeight": "never"}})
        self.assertTrue(self.page.locator("#k-water").is_visible())
        for key in ("hp", "ap", "rad", "food", "rest"):
            self.assertFalse(self.page.locator("#k-" + key).is_visible())
        self.assertEqual(self.box("#needsGap")["width"], 0)
        self.assertAlmostEqual(self.box("#k-water")["left"], self.box("#cores")["left"], delta=.1)
        self.send({"hp": 90})
        self.assertGreater(self.box("#needsGap")["width"], 0)
        self.send({"hp": 100, "settings": {"showWeight": "always"}})
        self.assertGreater(self.box("#needsGap")["width"], 0)
        self.send({"settings": {"hideSatisfiedNeeds": False}})
        for key in ("food", "water", "rest"):
            self.assertTrue(self.page.locator("#k-" + key).is_visible())
        self.assertFalse(self.page.locator("#k-hp").is_visible())
        self.send({"settings": {"hideSatisfiedNeeds": True, "hideFullVitals": False}})
        self.assertTrue(self.page.locator("#k-hp").is_visible())
        self.assertFalse(self.page.locator("#k-food").is_visible())

    def test_dynamic_vitals_survive_save_snapshot_and_context_hiding(self):
        self.send({"snapshot": True, "hp": 50, "hpMax": 100, "ap": 50, "apMax": 100, "rads": 10,
                   "settings": {"hideFullVitals": True}})
        self.send({"reset": True})
        self.send({"snapshot": True, "hp": 100, "hpMax": 100, "ap": 100, "apMax": 100, "rads": 0})
        for key in ("hp", "ap", "rad"):
            self.assertFalse(self.page.locator("#k-" + key).is_visible())
        self.send({"hp": 50, "context": {"powerArmor": True}})
        self.page.wait_for_function("getComputedStyle(document.getElementById('cores')).opacity === '0'")
        self.send({"context": {"powerArmor": False}})
        self.assertTrue(self.page.locator("#k-hp").is_visible())
        self.assertFalse(self.page.locator("#k-ap").is_visible())

    def location_opacity(self):
        # Playwright's simulated JS clock does not advance CSS transitions.
        return self.page.locator("#locName").evaluate("""e => {
            e.getAnimations().forEach(a => a.finish());
            return getComputedStyle(e).opacity;
        }""")

    def test_location_arrival_default_and_duplicate_updates(self):
        self.page.clock.install()
        self.page.reload(wait_until="domcontentloaded")  # Register the HUD interval on the simulated clock.
        self.send({"snapshot": True, "location": "Sanctuary Hills", "region": "Commonwealth",
                   "time": {"hour": 13, "timescale": 0}})
        self.page.clock.run_for(10000)
        self.assertEqual(self.location_opacity(), "1", "Default keeps location visible")
        self.send({"settings": {"locationOnArrival": True}})
        self.page.clock.run_for(7000)
        self.assertEqual(self.location_opacity(), "1")
        self.send({"location": "Sanctuary Hills", "settings": {"locationOnArrival": True}})
        self.page.clock.run_for(1600)
        self.assertEqual(self.location_opacity(), "0", "Duplicate location/settings do not restart the arrival")
        self.assertEqual(self.page.locator("#locName").text_content(), "Sanctuary Hills")
        self.assertTrue(self.page.locator("#clockTxt").is_visible())
        self.assertTrue(self.page.locator("#locSub").is_visible())
        self.assertTrue(self.page.locator("#xp").is_visible())
        self.send({"location": "Diamond City"})
        self.page.clock.run_for(500)
        self.assertEqual(self.location_opacity(), "1")
        self.assertEqual(self.page.locator("#locName").text_content(), "Diamond City")
        self.page.clock.run_for(8100)
        self.assertEqual(self.location_opacity(), "0")

    def test_location_arrival_new_location_and_save_reset_replace_old_countdown(self):
        self.page.clock.install()
        self.page.reload(wait_until="domcontentloaded")
        self.send({"settings": {"locationOnArrival": True}, "snapshot": True, "location": "Sanctuary Hills"})
        self.page.clock.run_for(7000)
        self.send({"location": "Red Rocket"})
        self.page.clock.run_for(2000)
        self.assertEqual(self.location_opacity(), "1", "Old location expiry cannot hide the new name")
        self.send({"reset": True})
        self.page.clock.run_for(10000)
        self.assertEqual(self.location_opacity(), "0")
        self.send({"snapshot": True, "location": "Red Rocket"})
        self.page.clock.run_for(7000)
        self.assertEqual(self.location_opacity(), "1", "Loading the same location starts a fresh arrival")
        self.page.clock.run_for(1600)
        self.assertEqual(self.location_opacity(), "0")
        self.send({"location": ""})
        self.send({"location": "Red Rocket"})
        self.page.clock.run_for(500)
        self.assertEqual(self.location_opacity(), "1", "Returning from an unnamed location counts as arrival")

    def test_weapon_legendary_line_follows_mcm_toggle(self):
        wleg = self.page.locator("#wLeg")
        self.send({"snapshot": True, "weapon": "Combat Shotgun", "clip": 3, "clipMax": 8, "reserve": 20,
                   "weaponLegendary": ["Bloodied", "Explosive"]})
        self.assertTrue(wleg.is_visible(), "Shown by default")
        self.assertEqual(self.page.locator("#wLegTxt").text_content(), "Bloodied \u00b7 Explosive")
        self.send({"settings": {"showWeaponLegendary": False}})
        self.assertFalse(wleg.is_visible(), "Toggle off hides the line immediately")
        self.assertTrue(self.page.locator("#wName").is_visible(), "Weapon name stays")
        self.send({"weaponLegendary": ["Two Shot"]})
        self.assertFalse(wleg.is_visible(), "New effects stay hidden while off")
        self.send({"settings": {"karmaReadout": True}})
        self.assertFalse(wleg.is_visible(), "Unrelated settings do not bring it back")
        self.send({"reset": True, "snapshot": True})
        self.assertFalse(wleg.is_visible(), "Preference persists through save reset")
        self.send({"settings": {"showWeaponLegendary": True}})
        self.assertTrue(wleg.is_visible(), "Toggle on restores the current effects")
        self.assertEqual(self.page.locator("#wLegTxt").text_content(), "Two Shot")
        self.send({"weaponLegendary": []})
        self.assertFalse(wleg.is_visible(), "An unlisted weapon still hides the line")

    def test_location_edge_flapping_does_not_reannounce_or_wake_vitals(self):
        # Seen in game: walking the edge of Military Checkpoint flipped to "Commonwealth" and back every few
        # seconds, restarting the arrival name and keeping out-of-combat-hidden vitals on screen.
        self.page.clock.install()
        self.page.reload(wait_until="domcontentloaded")
        vitals_hidden = lambda: self.page.locator("#cores").evaluate("e => !!e.closest('.ctx-hide')")
        self.send({"snapshot": True, "combat": False, "hp": 100, "hpMax": 100,
                   "location": "Military Checkpoint", "region": "Commonwealth",
                   "settings": {"locationOnArrival": True, "calmDelay": 0}})
        self.page.clock.run_for(9000)
        self.assertEqual(self.location_opacity(), "0")
        for _ in range(3):
            self.send({"location": "Commonwealth"})
            self.page.clock.run_for(1500)
            self.assertEqual(self.location_opacity(), "0", "Stepping out into the bare region is not an arrival")
            self.send({"location": "Military Checkpoint"})
            self.page.clock.run_for(1500)
            self.assertEqual(self.location_opacity(), "0", "Stepping back within a minute is not an arrival")
        self.send({"settings": {"calmRules": {"vitals": "hide"}}})
        self.page.clock.run_for(6000)
        self.assertTrue(vitals_hidden())
        for _ in range(2):
            self.send({"location": "Commonwealth"})
            self.page.clock.run_for(1500)
            self.assertTrue(vitals_hidden(), "Stepping out does not wake hidden vitals")
            self.send({"location": "Military Checkpoint"})
            self.page.clock.run_for(1500)
            self.assertTrue(vitals_hidden(), "Stepping back does not wake hidden vitals")
        self.send({"location": "Commonwealth"})
        self.page.clock.run_for(61000)
        self.send({"location": "Military Checkpoint"})
        self.page.clock.run_for(500)
        self.assertEqual(self.location_opacity(), "1", "Coming back after a minute away is an arrival again")
        self.assertFalse(vitals_hidden(), "A real arrival wakes vitals so the name can be seen")
        self.page.clock.run_for(9000)
        self.send({"location": "Starlight Drive-In"})
        self.page.clock.run_for(500)
        self.assertEqual(self.location_opacity(), "1", "A different place is always an arrival")
        self.page.clock.run_for(9000)
        self.send({"reset": True})
        self.send({"snapshot": True, "combat": False, "location": "Commonwealth", "region": "Commonwealth"})
        self.page.clock.run_for(500)
        self.assertEqual(self.location_opacity(), "1", "Loading a save in the open still names the region")

    def test_location_arrival_pauses_when_menu_hides_hud_and_toggle_off_restores_name(self):
        self.page.clock.install()
        self.page.reload(wait_until="domcontentloaded")
        self.send({"settings": {"locationOnArrival": True}, "snapshot": True, "location": "Sanctuary Hills",
                   "paused": True, "context": {"menus": ["PauseMenu"]}})
        self.page.clock.run_for(12000)
        self.send({"paused": False, "context": {"menus": []}})
        self.page.clock.run_for(7000)
        self.assertEqual(self.location_opacity(), "1", "Load-time pause does not consume arrival display")
        self.send({"context": {"menus": ["PipboyMenu"]}})
        self.page.clock.run_for(12000)
        self.send({"context": {"menus": []}})
        self.page.clock.run_for(1600)
        self.assertEqual(self.location_opacity(), "0")
        self.send({"settings": {"locationOnArrival": False}})
        self.page.clock.run_for(12000)
        self.assertEqual(self.location_opacity(), "1", "Disabling restores persistent location")

    def resize_hud(self, width, height):
        self.page.set_viewport_size({"width": width, "height": height})
        # Deliver the resize deterministically; do not sample before fit() runs.
        self.page.evaluate("window.dispatchEvent(new Event('resize'))")

    def box(self, selector):
        return self.page.locator(selector).evaluate("e => e.getBoundingClientRect().toJSON()")

    def font_px(self, selector):
        return float(self.page.locator(selector).first.evaluate("e => parseFloat(getComputedStyle(e).fontSize)"))

    def test_text_and_icon_size_settings_scale_the_hud_within_limits(self):
        self.send({"snapshot": True, "heading": 0, "hp": 90, "hpMax": 100, "location": "Sanctuary Hills",
                   "subtitle": {"who": "Settler", "line": "Pump's running again. First clean water we've had in a week."},
                   "markers": [{"id": "q1", "t": "quest", "b": 0, "dist": 40, "label": "Relay Tower", "active": True}]})
        base = {s: self.font_px(s) for s in (".subs p", ".cd", ".core small", ".clip")}
        layout = lambda: self.page.locator("#marks .mk").evaluate(
            "e => ({height: e.offsetHeight, bottom: e.offsetTop + e.offsetHeight, centre: e.offsetLeft + e.offsetWidth / 2})")
        mark = layout()
        self.assertAlmostEqual(mark["height"], 18, delta=.1)
        self.send({"settings": {"textScale": 1.5, "iconScale": 2}})
        self.assertAlmostEqual(self.font_px(".subs p"), base[".subs p"] * 1.5, delta=.1)
        self.assertAlmostEqual(self.font_px(".cd"), base[".cd"] * 1.5, delta=.1)
        self.assertAlmostEqual(self.font_px(".core small"), base[".core small"] * 1.3, delta=.1, msg="Ring captions cap at 130%")
        self.assertAlmostEqual(self.font_px(".clip"), base[".clip"], delta=.1, msg="Large numerals keep their size")
        big = layout()
        self.assertAlmostEqual(big["height"], 36, delta=.1)
        self.assertAlmostEqual(big["bottom"], mark["bottom"], delta=.5, msg="Icons grow upward from the same baseline")
        self.assertAlmostEqual(big["centre"], mark["centre"], delta=.5, msg="Icons stay centred on their bearing")
        # Layout guards at the largest sizes.
        self.assertLess(self.box("#vitals")["right"], self.box(".compass")["left"], "Vitals row stays clear of the compass")
        self.assertGreater(self.box(".subs")["left"], 56 + 312 * 1.5, "Subtitles narrow to stay clear of the location name")
        for span in self.page.locator(".l-keys span").all():
            self.assertLess(span.evaluate("e => e.getBoundingClientRect().height"), 40, "Loot key hints never split across lines")
        # Out-of-range values clamp; 1 restores the original sizes.
        self.send({"settings": {"textScale": 9, "iconScale": 0.1}})
        self.assertAlmostEqual(self.font_px(".subs p"), base[".subs p"] * 1.5, delta=.1)
        self.assertAlmostEqual(layout()["height"], 18 * .8, delta=.6)
        self.send({"settings": {"textScale": 1, "iconScale": 1}})
        self.assertEqual({s: self.font_px(s) for s in base}, base)
        self.assertAlmostEqual(layout()["height"], 18, delta=.1)

    def test_ring_size_scales_rings_and_keeps_vitals_clear_of_the_compass(self):
        self.page.clock.install()
        self.send({"snapshot": True, "hp": 90, "hpMax": 100, "ap": 50, "apMax": 90, "rads": 20,
                   "survival": {"on": True, "stages": {"food": 2, "water": 3, "rest": 4}},
                   "needs": {"food": .6, "water": .4, "rest": .2}, "weight": {"cur": 280, "max": 300},
                   "settings": {"showWeight": "always", "needStageNames": True},
                   "party": [{"name": "Piper", "type": "human", "hp": 1}],
                   "effects": [{"id": "psycho", "name": "Psycho", "kind": "chem", "icon": "bolt", "duration": 60, "remaining": 50}]})
        size = lambda q: self.page.locator(q).first.evaluate("e => parseFloat(getComputedStyle(e).width)")  # layout size, not mid-animation
        rows = lambda: self.page.locator("#cores").evaluate("e => e.classList.contains('two-rows')")
        def settle():
            self.page.clock.fast_forward(700)
        settle()
        base = {q: size(q) for q in ("#k-hp svg", "#k-food svg", "#effects .fx svg", "#party .pm-face")}
        self.assertFalse(rows(), "Default rings fit on one row")
        for scale in (1.1, 1.2, 1.3, 1.4, 1.5):
            with self.subTest(scale=scale):
                self.send({"settings": {"ringScale": scale}})
                settle()
                for q, w in base.items():
                    self.assertAlmostEqual(size(q), w * scale, delta=.2, msg=q)
                self.assertLess(self.box("#cores")["right"], self.box(".compass")["left"], "Vitals stay clear of the compass")
                for q in ("#k-hp", "#k-ap", "#k-rad", "#k-wt", "#k-food", "#k-water", "#k-rest"):
                    self.assertLess(self.box(q)["right"], self.box(".compass")["left"], q)
        self.assertTrue(rows(), "At 150% food, water and rest move to their own row")
        self.assertLess(self.box("#k-food")["bottom"], self.box("#k-hp")["top"] + 1, "The needs row sits above the main rings")
        self.send({"survival": {"on": False}})
        settle()
        self.assertFalse(rows(), "Without survival the main rings fit on one row again")
        self.send({"survival": {"on": True}, "settings": {"ringScale": 1}})
        settle()
        self.assertFalse(rows())
        self.assertEqual({q: size(q) for q in base}, base, "100% restores the original sizes")
        self.send({"settings": {"ringScale": 7}})
        settle()
        self.assertAlmostEqual(size("#k-hp svg"), base["#k-hp svg"] * 1.5, delta=.2, msg="Ring size clamps at 150%")

    def test_ultrawide_edges_use_screen_margin_without_stretching_widgets(self):
        self.send({"snapshot": True, "hp": 90, "hpMax": 100, "context": {"powerArmor": False}})
        # 56 design pixels become 74 2/3 pixels at 1440p, regardless of screen width.
        for width in (3440, 5120, 2560):
            with self.subTest(width=width):
                self.resize_hud(width, 1440)
                for selector in (".msg", "#vitals"):
                    self.assertAlmostEqual(self.box(selector)["left"], 74.6667, delta=.1)
                for selector in ("#feed", ".rcol", ".kit"):
                    self.assertAlmostEqual(width - self.box(selector)["right"], 74.6667, delta=.1)
                core = self.box("#k-hp > svg")
                self.assertAlmostEqual(core["width"], core["height"], delta=.1)

    def test_ultrawide_preserves_centered_widgets_and_native_mine_coordinates(self):
        self.send({"snapshot": True, "hp": 90, "hpMax": 100,
                   "prompt": {"key": "E", "text": "Activate"},
                   "mines": [{"id": "test", "x": 1200, "y": 600, "dist": 10}]})
        for width in (3440, 5120, 2560):
            with self.subTest(width=width):
                self.resize_hud(width, 1440)
                for selector in ("#cross", "#hint", "#sneakAtk", "#mineDir", ".compass"):
                    rect = self.box(selector)
                    self.assertAlmostEqual(rect["left"] + rect["width"] / 2, width / 2, delta=.1)
                self.assertAlmostEqual(self.box("#cross")["top"], 720, delta=.1)
                layer = self.box("#mineLayer")
                self.assertAlmostEqual(layer["left"], (width - 2560) / 2, delta=.1)
                self.assertAlmostEqual(layer["width"], 2560, delta=.1)
                mine = self.page.locator("#mineLayer .mn").evaluate("""e => {
                    const r = e.parentNode.getBoundingClientRect();
                    return {x:r.left + parseFloat(e.style.left)*r.width/1920,
                            y:r.top + parseFloat(e.style.top)*r.height/1080};
                }""")
                self.assertAlmostEqual(mine["x"], width / 2 + 320, delta=.1)
                self.assertAlmostEqual(mine["y"], 800, delta=.1)

    def test_ultrawide_power_armor_keeps_original_widget_placement(self):
        self.resize_hud(3440, 1440)
        self.send({"snapshot": True, "hp": 90, "hpMax": 100,
                   "context": {"powerArmor": True}})
        for menus in ([], ["PipboyMenu"]):
            self.send({"context": {"menus": menus}})
            self.assertAlmostEqual(self.box("#vitals")["left"], 514.6667, delta=.1)
            self.assertAlmostEqual(3440 - self.box(".kit")["right"], 514.6667, delta=.1)
        self.send({"context": {"powerArmor": False, "menus": []}})
        self.assertAlmostEqual(self.box("#vitals")["left"], 74.6667, delta=.1)

    def test_standard_and_narrow_screens_keep_original_layout(self):
        for width, height, left, top, bottom in (
                (1920, 1080, 56, 540, 56),
                (3840, 2160, 112, 1080, 112),
                (1280, 1024, 37.3333, 512, 189.3333)):
            with self.subTest(width=width, height=height):
                self.resize_hud(width, height)
                self.assertAlmostEqual(self.box("#vitals")["left"], left, delta=.1)
                self.assertAlmostEqual(self.box("#cross")["top"], top, delta=.1)
                self.assertAlmostEqual(height - self.box(".kit")["bottom"], bottom, delta=.1)


    def test_armor_plates_core_is_not_built_without_the_plugin_message(self):
        self.send({"snapshot": True, "hp": 300, "hpMax": 300, "ap": 90, "apMax": 90, "rads": 0})
        self.assertEqual(self.page.locator("#k-armor").count(), 0, "No armor core without armor data")
        cores = self.page.locator("#cores .core").count()
        self.send({"armor": {"current": 110, "max": 150, "perPlate": 50, "stock": 3}})
        self.assertEqual(self.page.locator("#cores .core").count(), cores + 1, "Armor adds exactly one core")
        self.send({"armor": None})

    def test_armor_plates_core_draws_one_arc_per_plate_and_reads_armor_and_stock(self):
        self.page.clock.install()
        self.send({"snapshot": True, "armor": {"current": 110, "max": 150, "perPlate": 50, "stock": 3}})
        self.assertEqual(self.page.locator("#k-armor").count(), 1)
        self.assertEqual(self.page.locator("#k-armor .pa").count(), 3, "150 max at 50 per plate is three plates")
        self.assertEqual(self.page.locator("#k-armor small b").inner_text(), "110 \u00b7 \u00d73")
        fills = self.page.locator("#k-armor .pa").evaluate_all(
            "els => els.map(e => parseFloat(e.getAttribute('stroke-dasharray')))")
        seg = 2 * 3.141592653589793 * 26 / 3
        self.assertAlmostEqual(fills[0], seg - 3, places=1, msg="first plate is full")
        self.assertAlmostEqual(fills[1], seg - 3, places=1, msg="second plate is full")
        self.assertAlmostEqual(fills[2], 0.2 * (seg - 3), places=1, msg="third plate holds the last 10 of 50")
        self.assertEqual(self.page.locator("#k-armor").evaluate("e => e.classList.contains('broken')"), False)
        self.send({"armor": {"current": 0, "max": 150, "perPlate": 50, "stock": 0}})
        self.assertEqual(self.page.locator("#k-armor").evaluate("e => e.classList.contains('broken')"), True)
        self.assertEqual(self.page.locator("#k-armor small .lbl").inner_text(), "BROKEN", "Broken relabels the core")
        self.assertEqual(self.page.locator("#k-armor .crack").evaluate("e => getComputedStyle(e).display"), "inline",
                         "Broken shows the crack in the plate")
        self.assertEqual(self.page.locator("#k-armor .pa").count(), 3, "Arc count stays with the pool size")
        self.send({"armor": {"current": 40, "max": 150, "perPlate": 50, "stock": 0}})
        self.assertEqual(self.page.locator("#k-armor small .lbl").inner_text(), "ARMOR")
        self.assertEqual(self.page.locator("#k-armor .crack").evaluate("e => getComputedStyle(e).display"), "none")

    def test_armor_wheel_setting_hides_and_restores_the_core(self):
        self.send({"snapshot": True, "armor": {"current": 110, "max": 150, "perPlate": 50, "stock": 3}})
        self.assertEqual(self.page.locator("#k-armor").evaluate("e => e.classList.contains('off')"), False)
        self.send({"settings": {"showArmorWheel": False}})
        self.assertEqual(self.page.locator("#k-armor").evaluate("e => e.classList.contains('off')"), True,
                         "Turning the armor wheel off hides it")
        self.send({"armor": {"current": 60, "max": 150, "perPlate": 50, "stock": 3}})
        self.assertEqual(self.page.locator("#k-armor").evaluate("e => e.classList.contains('off')"), True,
                         "Armor updates keep it hidden while the setting is off")
        self.send({"settings": {"showArmorWheel": True}})
        self.assertEqual(self.page.locator("#k-armor").evaluate("e => e.classList.contains('off')"), False,
                         "Turning it back on shows the latest armor")
        self.assertEqual(self.page.locator("#k-armor small b").inner_text(), "60 \u00b7 \u00d73")

    def test_armor_wheel_setting_defaults_to_on(self):
        self.send({"snapshot": True, "armor": {"current": 110, "max": 150, "perPlate": 50, "stock": 3}})
        self.assertEqual(self.page.locator("#k-armor").evaluate("e => e.classList.contains('off')"), False,
                         "No showArmorWheel setting means the wheel is shown")

    def test_crosshair_follows_vanilla_reticle_and_dot(self):
        self.send({"snapshot": True, "crosshair": {"spread": 20}})
        shown = lambda sel: self.page.locator(sel).evaluate("e => getComputedStyle(e).display") != "none"
        self.assertTrue(shown("#ring") and shown("#cross .dot"), "Old messages without the fields keep both")
        self.send({"crosshair": {"spread": 20, "reticle": False, "dot": False}})
        self.assertFalse(shown("#ring"), "No weapon, holstered or melee: no ring")
        self.assertFalse(shown("#cross .dot"), "and no dot")
        self.send({"crosshair": {"spread": 20, "reticle": False, "dot": True}})
        self.assertFalse(shown("#ring"))
        self.assertTrue(shown("#cross .dot"), "Vanilla's dot (looking at something usable) brings the dot back")
        self.send({"crosshair": {"spread": 20, "reticle": True, "dot": True}})
        self.assertTrue(shown("#ring") and shown("#cross .dot"), "A drawn gun shows the full crosshair")

    def test_armor_plates_core_hides_on_null_and_keeps_stage_order(self):
        self.send({"snapshot": True, "armor": {"current": 150, "max": 150, "perPlate": 50, "stock": 12}})
        self.send({"armor": None})
        self.assertTrue(self.page.locator("#k-armor").evaluate("e => e.classList.contains('off')"))
        before_weight = self.page.locator("#cores .core").evaluate_all("els => els.map(e => e.id)")
        self.assertLess(before_weight.index("k-armor"), before_weight.index("k-wt"))


if __name__ == "__main__":
    unittest.main()
