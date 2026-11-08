"""Check observed plugin lifecycle milestones in an actual game log."""
import argparse
import pathlib
import sys


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=pathlib.Path)
    parser.add_argument("--replay", action="store_true", help="also require a completed replay")
    args = parser.parse_args()
    text = args.log.read_text(encoding="utf-8", errors="replace")
    # Rotation can retain older sessions; check only the latest load.
    start = text.rfind("Phosphor HUD 0.")
    if start >= 0:
        text = text[start:]
    required = ["Connected to PrismaUI V12 (ABI 139)",
                "HUD view created", "HUD DOM ready", "MCM settings refreshed"]
    if "loaded for OG 1.10.163" not in text and "loaded for NG 1.10.984" not in text:
        required.insert(0, "loaded for OG 1.10.163 or NG 1.10.984")
    if args.replay:
        required.extend(["Starting replay: 58 messages", "Replay complete"])
    problems = [f"missing milestone: {marker}" for marker in required if marker not in text]
    problems.extend(line for line in text.splitlines() if "[error]" in line or "[critical]" in line)
    if problems:
        print("\n".join(problems))
        return 1
    print("Observed game log milestones passed" + (" (including replay)" if args.replay else ""))
    print("This does not verify visual placement, main-menu invisibility, or MCM interactions.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
