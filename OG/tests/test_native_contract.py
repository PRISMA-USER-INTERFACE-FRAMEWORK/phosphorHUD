"""Check the native validator against the canonical Draft 2020-12 contract."""
import json
import pathlib
import subprocess
import tempfile
import unittest
import jsonschema

ROOT = pathlib.Path(__file__).resolve().parents[1]
SCHEMA = ROOT / "schema/hud-messages.schema.json"
NATIVE = ROOT / "build/tests/phosphor-tests.exe"


class NativeContractTests(unittest.TestCase):
    def test_native_validation_matches_canonical_schema(self):
        validator = jsonschema.Draft202012Validator(json.loads(SCHEMA.read_text(encoding="utf-8")))
        # Covers the constraints used by this schema, including $defs references,
        # oneOf, strict scalar types, Unicode length, nulls and nested arrays.
        cases = [
            {"markers": {}}, {"markers": [{"id": ""}]}, {"markers": [{"id": "q1", "dist": -1}]},
            {"markers": [{"id": "q1", "dist": 0}]}, {"paused": 1}, {"hp": "100"},
            {"hpMax": 0}, {"level": 1.5}, {"level": 1.0}, {"xp": 1.1}, {"xp": 0},
            {"reset": False}, {"unknownField": 1}, {"context": {"menus": [False]}},
            {"party": [{"name": "Piper", "affinity": None}]}, {"party": [{"name": "Piper", "hp": -1}]},
            {"target": None}, {"target": {}}, {"target": {"name": "Raider", "pct": .5}},
            {"weaponLegendary": ["a"] * 6}, {"explosive": {"short": "★☢★☢★☢"}},
            {"explosive": {"short": "1234567"}}, {"cure": True}, {"cure": False},
            {"effect": {"id": "x", "kind": "invalid"}}, {"notify": {"text": "quote \" & <tag>"}},
            {"settings": {"contextRules": {"ads": "hide"}}}, {"settings": {"mineWarnings": "false"}},
            {"settings": {"hideSatisfiedNeeds": True}}, {"settings": {"hideSatisfiedNeeds": "true"}},
            {"settings": {"hideFullVitals": True}}, {"settings": {"hideFullVitals": 1}},
            {"settings": {"locationOnArrival": True}}, {"settings": {"locationOnArrival": "true"}},
            {"settings": {"textReadability": "off"}}, {"settings": {"textReadability": "subtle"}},
            {"settings": {"textReadability": "strong"}}, {"settings": {"textReadability": 2}},
            {"settings": {"textReadability": "invalid"}},
            {"settings": {"showCrosshair": False, "showHitMarkers": True}},
            {"settings": {"showCrosshair": 0}}, {"settings": {"showHitMarkers": "true"}},
            {"settings": {"calmDelay": 0}}, {"settings": {"calmDelay": 15}},
            {"settings": {"calmDelay": -1}}, {"settings": {"calmDelay": 16}}, {"settings": {"calmDelay": 2.5}},
            {"settings": {"calmRules": {"cross": "hide", "vitals": "dim"}}},
            {"settings": {"calmRules": {"mines": "hide"}}}, {"settings": {"calmRules": {"cross": "invalid"}}},
        ]
        with tempfile.TemporaryDirectory(dir=ROOT / "build") as directory:
            path = pathlib.Path(directory) / "input.jsonl"
            for message in cases:
                with self.subTest(message=message):
                    path.write_text(json.dumps(message, ensure_ascii=False) + "\n", encoding="utf-8")
                    result = subprocess.run([str(NATIVE), str(path), str(SCHEMA)],
                                            capture_output=True, text=True, timeout=10)
                    self.assertEqual(result.returncode == 0, validator.is_valid(message), result.stderr)


if __name__ == "__main__":
    unittest.main()
