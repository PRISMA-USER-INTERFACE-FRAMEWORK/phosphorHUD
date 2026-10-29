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
        plugin = (ROOT / "src/Plugin.cpp").read_text(encoding="utf-8")
        for call in ("Live::ReadHudText(", "Live::ReadCrosshair(", "Live::ReadFlashlight(",
                     "Live::ShrinkVanillaText(", "Live::HideVanillaCrosshair(",
                     "Live::ShrinkVanillaFlashlight("):
            self.assertFalse(call in plugin, "Game-task code must not enter the Flash movie: " + call)

    def test_flash_movie_access_requires_ui_task_scope(self):
        live = (ROOT / "src/Live.cpp").read_text(encoding="utf-8")
        self.assertIn("AddUITask(", live, "Flash work needs the engine's UI queue")
        self.assertIn("activeHudMovie", live, "A movie must only be available within UI capture scope")


if __name__ == "__main__":
    unittest.main()
