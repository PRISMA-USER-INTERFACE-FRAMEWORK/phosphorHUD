"""Browser checks for the HUD layout API, the native save hook and the layout editor; run with unittest."""
import json
import os
import pathlib
import unittest
from playwright.sync_api import sync_playwright

ROOT = pathlib.Path(__file__).resolve().parents[1]
HUD = pathlib.Path(os.environ.get("PHOSPHOR_TEST_MOD", ROOT / "mod/Phosphor HUD")) / "PrismaUI_F4/views/PhosphorHUD/hud.html"


class LayoutTests(unittest.TestCase):
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
        self.page.route("https://**/*", lambda route: route.abort())
        self.page.goto(HUD.as_uri(), wait_until="domcontentloaded")
        self.page.wait_for_function("typeof window.hudUpdate === 'function' && typeof window.hudLayout === 'object'")

    def tearDown(self):
        self.page.close()
        self.assertEqual(self.errors, [], "No asynchronous JavaScript exceptions")

    def test_layout_apply_sets_translate_scale_and_round_trips(self):
        layout = {"version": 1, "groups": {"vitals": {"dx": 30, "dy": -20, "s": 1.2, "hidden": False}}}
        self.assertTrue(self.page.evaluate("data => window.hudLayout.apply(data)", layout))
        vitals = self.page.locator("#vitals")
        self.assertEqual(vitals.evaluate("e => e.style.translate"), "30px -20px")
        self.assertEqual(vitals.evaluate("e => e.style.scale"), "1.2")
        self.assertEqual(self.page.evaluate("window.hudLayout.get()"), layout)
        self.assertTrue(self.page.evaluate("data => window.hudLayout.apply(data)", json.dumps(layout)), "Saved layouts arrive as JSON text")
        self.assertTrue(self.page.evaluate("data => window.hudLayout.apply(data)",
                                           {"version": 1, "groups": {"party": {"dx": 0, "dy": 0, "s": 1, "hidden": False}}}))
        self.assertEqual(self.page.evaluate("window.hudLayout.get()"), {"version": 1, "groups": {}},
                         "Defaults are not stored and unspecified groups reset")
        self.assertEqual(vitals.evaluate("e => [e.style.translate, e.style.scale]"), ["", ""])

    def test_layout_rejects_bad_input_and_keeps_state(self):
        self.assertTrue(self.page.evaluate("data => window.hudLayout.apply(data)", {"version": 1, "groups": {"vitals": {"dx": 30}}}))
        before = self.page.evaluate("window.hudLayout.get()")
        for bad in ("{not json", {"version": 2, "groups": {}}, {"version": 1}, {"version": 1, "groups": "vitals"}, None):
            with self.subTest(bad=bad):
                self.assertFalse(self.page.evaluate("data => window.hudLayout.apply(data)", bad))
                self.assertEqual(self.page.evaluate("window.hudLayout.get()"), before)

    def test_layout_clamps_values_and_drops_unknown_or_bad_fields(self):
        self.assertTrue(self.page.evaluate("data => window.hudLayout.apply(data)", json.dumps({"version": 1, "groups": {
            "vitals": {"dx": 5000, "dy": -5000, "s": 9, "hidden": "yes"},
            "compass": {"dx": -5000, "dy": 5000, "s": 0.1, "hidden": True},
            "nope": {"dx": 5}}})))
        self.assertEqual(self.page.evaluate("window.hudLayout.get().groups"),
                         {"vitals": {"dx": 1920, "dy": -1080, "s": 1.5, "hidden": False},
                          "compass": {"dx": -1920, "dy": 1080, "s": 0.5, "hidden": True}})

    def test_layout_reset_clears_one_group_or_everything(self):
        self.page.evaluate("() => { window.hudLayout.setGroup('vitals', {dx: 30, dy: 10, s: 1.1}); window.hudLayout.setGroup('kit', {dx: -40}); }")
        self.assertFalse(self.page.evaluate("() => window.hudLayout.reset('nope')"))
        self.assertTrue(self.page.evaluate("() => window.hudLayout.reset('vitals')"))
        self.assertEqual(self.page.locator("#vitals").evaluate("e => [e.style.translate, e.style.scale]"), ["", ""])
        self.assertEqual(list(self.page.evaluate("window.hudLayout.get().groups")), ["kit"])
        self.assertTrue(self.page.evaluate("() => window.hudLayout.reset()"))
        self.assertEqual(self.page.evaluate("window.hudLayout.get()"), {"version": 1, "groups": {}})
        self.assertEqual(self.page.locator("#kit").evaluate("e => e.style.translate"), "")

    def test_hidden_settlement_alert_stays_hidden_through_attack_redraw(self):
        self.page.evaluate("() => window.hudLayout.setGroup('salert', {hidden: true})")
        self.page.evaluate("() => window.hud.setAttack({id: 'atk-test', name: 'Sanctuary Hills', state: 'active', total: 5, remaining: 3, dist: 20})")
        alert = self.page.locator("#salert")
        self.assertIn("show", alert.get_attribute("class"), "The attack redraw ran")
        self.assertEqual(alert.evaluate("e => getComputedStyle(e).display"), "none", "renderAttack rewrites the class but must not unhide the alert")
        self.page.evaluate("() => window.hud.setAttack({id: 'atk-test', state: 'won'})")
        self.assertIn("won", alert.get_attribute("class"))
        self.assertEqual(alert.evaluate("e => getComputedStyle(e).display"), "none")
        self.page.evaluate("() => window.hudLayout.setGroup('salert', {hidden: false})")
        self.assertNotEqual(alert.evaluate("e => getComputedStyle(e).display"), "none", "Unhiding brings the alert back")

    def test_send_to_game_reaches_the_native_save_hook(self):
        self.page.evaluate("() => { window.savedLayouts = []; window.saveLayout = json => window.savedLayouts.push(json); }")
        payload = json.dumps({"version": 1, "groups": {"vitals": {"dx": 12, "dy": 0, "s": 1, "hidden": False}}})
        self.page.evaluate("data => window.hudSendToGame('saveLayout', data)", payload)
        self.assertEqual(self.page.evaluate("window.savedLayouts"), [payload])
        self.page.evaluate("() => window.hudLayout.setGroup('vitals', {dx: 12})")
        self.page.evaluate("() => window.hudLayout.save()")
        self.assertEqual(self.page.evaluate("window.savedLayouts.length"), 2)
        self.assertEqual(json.loads(self.page.evaluate("window.savedLayouts[1]")), self.page.evaluate("window.hudLayout.get()"))

    def test_editor_open_twice_keeps_one_overlay_and_close_reports_once(self):
        self.page.evaluate("() => { window.closedCount = 0; window.editorClosed = () => { window.closedCount += 1; }; }")
        self.page.evaluate("() => { window.hudEditorOpen(); window.hudEditorOpen(); }")
        self.assertEqual(self.page.locator("#hedOverlay").count(), 1)
        self.assertEqual(self.page.locator("#hedRoot").count(), 1)
        self.assertEqual(self.page.locator(".hed-box").count(), len(self.page.evaluate("window.hudLayout.groups")))
        self.assertTrue(self.page.locator("#stage").evaluate("e => e.classList.contains('editing')"))
        self.assertTrue(self.page.evaluate("window.hudEditor.isOpen()"))
        self.page.evaluate("() => { window.hudEditorClose(); window.hudEditorClose(); }")
        self.assertEqual(self.page.evaluate("window.closedCount"), 1, "Closing twice reports the editor once")
        self.assertFalse(self.page.locator("#stage").evaluate("e => e.classList.contains('editing')"))
        self.assertFalse(self.page.evaluate("window.hudEditor.isOpen()"))

    def test_editor_group_drag_moves_by_stage_scaled_amount(self):
        self.page.evaluate("() => window.hudEditorOpen()")
        self.page.locator("#hedSnap").select_option("0")
        hed = self.page.locator('.hed-box[data-hed="vitals"]').evaluate("e => e.getBoundingClientRect().toJSON()")
        scale = self.page.locator("#stage").evaluate("e => e.getBoundingClientRect().width / 1920")
        x, y = hed["x"] + hed["width"] / 2, hed["y"] + hed["height"] / 2
        self.page.mouse.move(x, y)
        self.page.mouse.down()
        self.page.mouse.move(x + 100, y + 40, steps=5)
        self.page.mouse.up()
        vitals = self.page.evaluate("window.hudLayout.get().groups.vitals")
        self.assertAlmostEqual(vitals["dx"], 100 / scale, delta=.5)
        self.assertAlmostEqual(vitals["dy"], 40 / scale, delta=.5)

    def test_layout_and_editor_scripts_load_without_page_errors(self):
        self.assertEqual(self.page.evaluate("[typeof window.hudLayout, typeof window.hudEditor]"), ["object", "object"])
        self.page.evaluate("() => { window.hudLayout.apply({version: 1, groups: {vitals: {dx: 4}}}); window.hudEditorOpen(); window.hudEditorClose(); }")
        self.assertEqual(self.errors, [])


if __name__ == "__main__":
    unittest.main()
