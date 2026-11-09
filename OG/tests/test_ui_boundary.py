"""Guard the native Flash boundary; engine execution still needs an in-game test."""
import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class UiBoundaryTests(unittest.TestCase):
    def test_inactive_hud_only_retries_a_pending_restore(self):
        plugin = (ROOT / "src/Plugin.cpp").read_text(encoding="utf-8")
        self.assertTrue("nativeRestorePending" in plugin,
                        "Inactive ticks must not continually normalize vanilla widgets")
        live = (ROOT / "src/Live.cpp").read_text(encoding="utf-8")
        self.assertTrue("appliedHiding" in live,
                        "Restoration must be limited to widgets hidden by this bridge")

    def test_game_tick_consumes_copies_instead_of_calling_flash_readers(self):
        plugin = "".join(p.read_text(encoding="utf-8") for p in sorted(ROOT.glob("src/Plugin*.cpp")))
        for call in ("Live::ReadHudText(", "Live::ReadCrosshair(", "Live::ReadFlashlight(",
                     "Live::ShrinkVanillaText(", "Live::HideVanillaCrosshair(",
                     "Live::ShrinkVanillaFlashlight("):
            self.assertFalse(call in plugin, "Game-task code must not enter the Flash movie: " + call)

    def test_flash_movie_access_requires_ui_task_scope(self):
        live = (ROOT / "src/Live.cpp").read_text(encoding="utf-8")
        self.assertIn("AddUITask(", live, "Flash work needs the engine's UI queue")
        self.assertIn("activeHudMovie", live, "A movie must only be available within UI capture scope")

    def test_layout_editor_cannot_leave_focus_or_the_hud_hidden_behind(self):
        """Catches an editor exit path that skips releasing Prisma focus or Escape ownership (stuck cursor, dead Esc key), and an editor that stays open under a menu or after the view is lost."""
        view = (ROOT / "src/PluginView.cpp").read_text(encoding="utf-8")
        end = view[view.index("void EndEditor()"):view.index("void HideReplay()")]
        for call in ("api->Unfocus(g_view)", "api->SetViewOwnsEscape(g_view, false)", "editorOpen = false", "liveShown = false"):
            self.assertIn(call, end, "Every exit path goes through EndEditor and must undo the open: " + call)
        create = view[view.index("void CreateView()"):view.index("bool BlockingMenu()")]
        self.assertIn("if (editorOpen) EndEditor();", create, "A replaced view must not leave the editor flag set")
        self.assertIn("EndEditor();", view[view.index("void HideReplay()"):view.index("void RestoreNativeHud()")], "Ending the session closes the editor")
        plugin = (ROOT / "src/Plugin.cpp").read_text(encoding="utf-8")
        self.assertIn("editorOpen && (BlockingMenu() || !api->HasFocus(g_view))) CloseEditor()", plugin,
                      "A menu opening or focus being lost must close the editor")
        for bound in (saveLayout, editorClosed, hudLayoutApply):
            self.assertIn(bound, view, "The page and plugin must agree on " + bound)


if __name__ == "__main__":
    unittest.main()
