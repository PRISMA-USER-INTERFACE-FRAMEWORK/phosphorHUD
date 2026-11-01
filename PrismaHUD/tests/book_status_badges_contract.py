from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "src/QuickLootController.cpp").read_text(encoding="utf-8")
view = (ROOT / "assets/views/hud_quickloot.js").read_text(encoding="utf-8")
css = (ROOT / "assets/views/hud_quickloot.css").read_text(encoding="utf-8")

checks = {
    "book status is limited to BOOK forms": "RE::ENUM_FORM_ID::kBOOK" in source,
    "book flags come from TESObjectBOOK data": "book->data.flags" in source,
    "read bit matches verified HUD++ runtime flag": "0x08u" in source,
    "skill-book bit matches verified AddPerk flag": "0x10u" in source,
    "read state is sent to the view": 'extras["bookRead"]' in source,
    "skill-book state is sent to the view": 'extras["skillBook"]' in source,
    "view renders read state": "item.bookRead" in view and "book-read-badge" in view,
    "view renders skill-book state": "item.skillBook" in view and "skill-book-badge" in view,
    "book badges have dedicated styling": ".book-read-badge" in css and ".skill-book-badge" in css,
}

failed = [name for name, ok in checks.items() if not ok]
if failed:
    for name in failed:
        print(f"FAIL: {name}")
    raise SystemExit(1)

print("book_status_badges_contract: PASS")
