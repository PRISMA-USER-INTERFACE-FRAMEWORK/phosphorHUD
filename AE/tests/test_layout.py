"""Browser checks for the HUD layout API, the native save hook and the in-game layout editor; run with unittest."""
import json
import os
import pathlib
import unittest
from playwright.sync_api import sync_playwright

ROOT = pathlib.Path(__file__).resolve().parents[1]
HUD = pathlib.Path(os.environ.get("PHOSPHOR_TEST_MOD", ROOT / "mod/Phosphor HUD")) / "PrismaUI_F4/views/PhosphorHUD/hud.html"
FIT = "() => Math.abs(document.getElementById('stage').getBoundingClientRect().width - 1920 * Math.min(innerWidth / 1920, innerHeight / 1080)) < 1"


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

    def spy(self):
        self.page.evaluate("() => { window.savedLayouts = []; window.closedCount = 0; window.saveLayout = json => window.savedLayouts.push(json); window.editorClosed = () => { window.closedCount += 1; }; }")

    def test_layout_apply_sets_translate_scale_and_round_trips(self):
        """Catches a saved layout that is applied with the wrong CSS or loses fields when the plugin hands it back as JSON text."""
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
        """Catches a corrupt layout.json wiping the layout that is already showing."""
        self.assertTrue(self.page.evaluate("data => window.hudLayout.apply(data)", {"version": 1, "groups": {"vitals": {"dx": 30}}}))
        before = self.page.evaluate("window.hudLayout.get()")
        for bad in ("{not json", {"version": 2, "groups": {}}, {"version": 1}, {"version": 1, "groups": "vitals"}, None):
            with self.subTest(bad=bad):
                self.assertFalse(self.page.evaluate("data => window.hudLayout.apply(data)", bad))
                self.assertEqual(self.page.evaluate("window.hudLayout.get()"), before)

    def test_layout_clamps_values_and_drops_unknown_or_bad_fields(self):
        """Catches a hand-edited layout.json throwing a group off screen or smuggling in unknown groups and fields."""
        self.assertTrue(self.page.evaluate("data => window.hudLayout.apply(data)", json.dumps({"version": 1, "groups": {
            "vitals": {"dx": 5000, "dy": -5000, "s": 9, "hidden": "yes"},
            "compass": {"dx": -5000, "dy": 5000, "s": 0.1, "hidden": True},
            "nope": {"dx": 5}}})))
        self.assertEqual(self.page.evaluate("window.hudLayout.get().groups"),
                         {"vitals": {"dx": 1920, "dy": -1080, "s": 1.5, "hidden": False},
                          "compass": {"dx": -1920, "dy": 1080, "s": 0.5, "hidden": True}})

    def test_layout_reset_clears_one_group_or_everything(self):
        """Catches a per-group reset that clears the wrong group, or a global reset that leaves inline styles behind."""
        self.page.evaluate("() => { window.hudLayout.setGroup('vitals', {dx: 30, dy: 10, s: 1.1}); window.hudLayout.setGroup('kit', {dx: -40}); }")
        self.assertFalse(self.page.evaluate("() => window.hudLayout.reset('nope')"))
        self.assertTrue(self.page.evaluate("() => window.hudLayout.reset('vitals')"))
        self.assertEqual(self.page.locator("#vitals").evaluate("e => [e.style.translate, e.style.scale]"), ["", ""])
        self.assertEqual(list(self.page.evaluate("window.hudLayout.get().groups")), ["kit"])
        self.assertTrue(self.page.evaluate("() => window.hudLayout.reset()"))
        self.assertEqual(self.page.evaluate("window.hudLayout.get()"), {"version": 1, "groups": {}})
        self.assertEqual(self.page.locator("#kit").evaluate("e => e.style.translate"), "")

    def test_every_layout_group_matches_exactly_one_page_element(self):
        """Catches a HUD markup edit that drops or duplicates a data-edit attribute, which would leave a group unmovable or throw when the editor measures it."""
        counts = self.page.evaluate("() => window.hudLayout.groups.map(g => [g.id, document.querySelectorAll('[data-edit=\"' + g.id + '\"]').length])")
        self.assertEqual([c for c in counts if c[1] != 1], [])
        self.assertEqual(self.page.evaluate("document.querySelectorAll('[data-edit]').length"), len(counts), "No data-edit element is missing from the group list")

    def test_layout_offset_is_stage_relative_at_any_resolution_and_aspect(self):
        """Catches layout offsets drifting with the window size: the saved layout.json must place a group the same stage distance from its default at every resolution and aspect ratio."""
        for width, height in ((1920, 1080), (1280, 720), (3840, 2160), (2560, 1080), (1000, 1000)):
            with self.subTest(size=(width, height)):
                self.page.set_viewport_size({"width": width, "height": height})
                self.page.wait_for_function(FIT)
                self.page.evaluate("() => window.hudLayout.reset()")
                probe = "() => { const r = document.getElementById('kit').getBoundingClientRect(), k = document.getElementById('stage').getBoundingClientRect().width / 1920; return {x: r.left, y: r.top, k}; }"
                before = self.page.evaluate(probe)
                self.page.evaluate("() => window.hudLayout.setGroup('kit', {dx: -120, dy: -60})")
                after = self.page.evaluate(probe)
                self.assertAlmostEqual((after["x"] - before["x"]) / before["k"], -120, delta=.5)
                self.assertAlmostEqual((after["y"] - before["y"]) / before["k"], -60, delta=.5)

    def test_scaled_centred_group_keeps_its_centre_and_scales_its_width(self):
        """Catches the compass drifting off the crosshair axis when scaled: centred groups use a left transform origin so their translateX(-50%) stays centred."""
        probe = "() => { const r = document.getElementById('compass').getBoundingClientRect(); return {cx: r.left + r.width / 2, w: r.width}; }"
        before = self.page.evaluate(probe)
        self.page.evaluate("() => window.hudLayout.setGroup('compass', {s: 1.5})")
        after = self.page.evaluate(probe)
        self.assertAlmostEqual(after["cx"], before["cx"], delta=.5)
        self.assertAlmostEqual(after["w"] / before["w"], 1.5, delta=.01)

    def test_hidden_settlement_alert_stays_hidden_through_attack_redraw(self):
        """Catches the attack redraw rewriting the class list and un-hiding a group the player hid."""
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
        """Catches the page calling a differently named native hook than the plugin binds (saveLayout), so nothing would ever be written to layout.json."""
        self.spy()
        payload = json.dumps({"version": 1, "groups": {"vitals": {"dx": 12, "dy": 0, "s": 1, "hidden": False}}})
        self.page.evaluate("data => window.hudSendToGame('saveLayout', data)", payload)
        self.assertEqual(self.page.evaluate("window.savedLayouts"), [payload])
        self.page.evaluate("() => window.hudLayout.setGroup('vitals', {dx: 12})")
        self.page.evaluate("() => window.hudLayout.save()")
        self.assertEqual(self.page.evaluate("window.savedLayouts.length"), 2)
        self.assertEqual(json.loads(self.page.evaluate("window.savedLayouts[1]")), self.page.evaluate("window.hudLayout.get()"))

    def test_editor_open_twice_keeps_one_overlay_and_close_reports_once(self):
        """Catches a double open stacking overlays, or a double close telling the plugin twice and releasing focus it no longer owns."""
        self.spy()
        self.page.evaluate("() => { window.hudEditorOpen(); window.hudEditorOpen(); }")
        self.assertEqual(self.page.locator("#hedOverlay").count(), 1)
        self.assertEqual(self.page.locator("#hedRoot").count(), 1)
        self.assertEqual(self.page.locator(".hed-box").count(), len(self.page.evaluate("window.hudLayout.groups")))
        self.assertTrue(self.page.locator("#stage").evaluate("e => e.classList.contains('editing')"))
        self.assertTrue(self.page.evaluate("window.hudEditor.isOpen()"))
        self.page.evaluate("() => { window.hudEditorClose(''); window.hudEditorClose(''); }")
        self.assertEqual(self.page.evaluate("window.closedCount"), 1, "Closing twice reports the editor once")
        self.assertFalse(self.page.locator("#stage").evaluate("e => e.classList.contains('editing')"))
        self.assertFalse(self.page.evaluate("window.hudEditor.isOpen()"))

    def test_editor_shows_the_full_hud_over_context_rules_and_restores_them_on_close(self):
        """Catches the editor opening on an invisible HUD (menu or calm rules hiding every group, stage still awaiting data), or leaving the hide rules suspended after it closes."""
        self.page.evaluate("() => window.hudUpdate({context: {prismaFocus: true}})")
        cores = self.page.locator("#cores")
        self.assertIn("ctx-hide", cores.get_attribute("class"), "The menu context hides the vitals")
        self.page.wait_for_function("() => getComputedStyle(document.getElementById('cores')).opacity === '0'")
        self.assertTrue(self.page.locator("#stage").evaluate("e => e.classList.contains('awaiting')"), "No live data has arrived yet")
        self.page.evaluate("() => window.hudEditorOpen()")
        self.page.wait_for_function("() => ['stage', 'cores'].every(id => getComputedStyle(document.getElementById(id)).opacity === '1')")
        self.assertEqual(self.page.locator("#stage").evaluate("e => getComputedStyle(e).opacity"), "1", "The awaiting stage is shown while editing")
        self.page.evaluate("() => window.hudEditorClose('')")
        self.page.wait_for_function("() => getComputedStyle(document.getElementById('cores')).opacity === '0'")
        self.assertIn("ctx-hide", cores.get_attribute("class"), "Closing restores the context rules")

    def test_escape_closes_and_done_saves_only_a_changed_layout(self):
        """Catches Escape not releasing the editor (the player is stuck with a captured cursor), Done losing edits, and Done rewriting an unchanged layout.json."""
        self.spy()
        self.page.evaluate("() => window.hudEditorOpen()")
        self.page.keyboard.press("Escape")
        self.assertFalse(self.page.evaluate("window.hudEditor.isOpen()"))
        self.assertEqual(self.page.evaluate("window.closedCount"), 1)
        self.assertEqual(self.page.evaluate("window.savedLayouts"), [], "Nothing changed, nothing written")
        self.page.evaluate("() => { window.hudEditorOpen(); window.hudLayout.setGroup('vitals', {dx: 30}); }")
        self.page.locator("#hedDone").click()
        self.assertFalse(self.page.evaluate("window.hudEditor.isOpen()"))
        self.assertEqual(self.page.evaluate("window.closedCount"), 2)
        self.assertEqual([json.loads(s)["groups"]["vitals"]["dx"] for s in self.page.evaluate("window.savedLayouts")], [30])

    def test_cancel_restores_the_layout_from_before_opening_and_writes_nothing(self):
        """Catches Cancel keeping the player's experiments on screen, or saving them."""
        self.spy()
        self.page.evaluate("() => window.hudLayout.setGroup('kit', {dx: -10})")
        self.page.evaluate("() => { window.hudEditorOpen(); window.hudLayout.setGroup('vitals', {dx: 30}); window.hudLayout.setGroup('kit', {dx: -80}); }")
        self.page.locator("#hedCancel").click()
        self.assertFalse(self.page.evaluate("window.hudEditor.isOpen()"))
        self.assertEqual(self.page.evaluate("window.hudLayout.get().groups"), {"kit": {"dx": -10, "dy": 0, "s": 1, "hidden": False}})
        self.assertEqual(self.page.evaluate("window.savedLayouts"), [])
        self.assertEqual(self.page.evaluate("window.closedCount"), 1)

    def test_editor_hide_and_resets_work_per_group_and_globally(self):
        """Catches the Hide checkbox, Reset sel and Reset all disagreeing with the layout the HUD is rendering."""
        self.page.evaluate("() => window.hudEditorOpen()")
        self.page.locator('.hed-row[data-row="compass"] input').check()
        self.assertTrue(self.page.evaluate("window.hudLayout.get().groups.compass.hidden"))
        self.assertEqual(self.page.locator("#compass").evaluate("e => getComputedStyle(e).display"), "none")
        self.page.evaluate("() => window.hudLayout.setGroup('kit', {dx: -40})")
        self.page.locator('.hed-row[data-row="compass"] span').click()
        self.page.locator("#hedResetSel").click()
        self.assertEqual(list(self.page.evaluate("window.hudLayout.get().groups")), ["kit"], "Reset sel clears only the selected group")
        self.assertNotEqual(self.page.locator("#compass").evaluate("e => getComputedStyle(e).display"), "none")
        self.page.locator("#hedResetAll").click()
        self.assertEqual(self.page.evaluate("window.hudLayout.get()"), {"version": 1, "groups": {}})
        self.page.locator("#hedUndo").click()
        self.assertEqual(list(self.page.evaluate("window.hudLayout.get().groups")), ["kit"], "Reset all can be undone")

    def test_editor_group_drag_moves_by_stage_scaled_amount(self):
        """Catches drag distances ignoring the stage scale, so groups would not follow the cursor at other resolutions."""
        self.page.set_viewport_size({"width": 1280, "height": 720})
        self.page.wait_for_function(FIT)
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

    def test_editor_refuses_to_open_when_the_renderer_lacks_css_translate(self):
        """Catches an editor that opens on a renderer without CSS translate and scale, where every drag and scale is a silent no-op and the player is stuck in a dead overlay."""
        page = self.browser.new_page(viewport={"width": 1920, "height": 1080})
        logs = []
        page.on("console", lambda message: logs.append(message.text) if message.type == "error" else None)
        page.add_init_script("const supports = CSS.supports.bind(CSS); CSS.supports = (name, value) => name === 'translate' ? false : supports(name, value);")
        page.route("https://**/*", lambda route: route.abort())
        page.goto(HUD.as_uri(), wait_until="domcontentloaded")
        page.wait_for_function("typeof window.hudLayout === 'object' && typeof window.hudEditorOpen === 'function'")
        page.evaluate("() => { window.closedCount = 0; window.editorClosed = () => { window.closedCount += 1; }; window.hudEditorOpen(); }")
        self.assertEqual(page.evaluate("window.closedCount"), 1, "The plugin is told to run EndEditor")
        self.assertFalse(page.evaluate("window.hudEditor.isOpen()"))
        self.assertEqual(page.locator("#hedOverlay.open").count(), 0)
        self.assertTrue(any("translate" in text for text in logs), "A clear console error names the missing feature")
        page.close()


if __name__ == "__main__":
    unittest.main()
