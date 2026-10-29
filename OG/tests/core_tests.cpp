#include "Core.h"
#include "ModData.h"
#include "EventInbox.h"
#include "UiMailbox.h"
#include <limits>
#include <memory>
#include <cmath>
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
using namespace Phosphor;
using namespace std::chrono_literals;
int failures = 0;
void Check(bool value, const char* name) {
    if (!value) { ++failures; std::cerr << "FAIL: " << name << '\n'; }
}
int main(int argc, char** argv) {
    // Test-only bridge: exercise the real INI reader and settings conversion from Python.
    if (argc > 2 && std::string_view(argv[1]) == "--mcm-settings") {
        std::ifstream defaultsFile(argv[2]);
        const auto defaults = ReadMainIni(defaultsFile);
        const auto request = Json::parse(std::cin);
        Ini overrides;
        for (const auto& [key, value] : request.items()) overrides[key] = value.get<int>();
        std::cout << Json{{"settings", MakeSettings(defaults, overrides)},
                          {"effectHourLimit", EffectHourLimit(defaults, overrides)},
                          {"quickLootFirst", QuickLootFirstEnabled(defaults, overrides)}}.dump();
        return 0;
    }
    const auto schemaPath = argc > 2 ? argv[2] : "schema/hud-messages.schema.json";
    std::ifstream schemaFile(schemaPath);
    const auto schema = Json::parse(schemaFile);
    if (argc > 1) {
        std::ifstream messages(argv[1], std::ios::binary);
        const auto result = ReadReplay(messages, schema);
        for (const auto& error : result.errors) std::cerr << error << '\n';
        return result.errors.empty() ? 0 : 1;
    }
    std::istringstream ini("\xef\xbb\xbf[Main]\r\nbHideHealthyFigure=0\r\niCtxAds=2 ; comment\ninvalid=2oops\n[Other]\niCtxAds=1\n");
    auto parsed = ReadMainIni(ini);
    for (const auto& expected : kNeedStages) {
        const auto message = ParseSurvivalMessage("You are " + std::string(expected.name) + ".");
        Check(message && std::string_view(message->need) == expected.need && message->stage == expected.stage,
              "All existing hunger/thirst/rest warning stages remain recognized");
    }
    const auto parched = ParseSurvivalMessage("You are Parched from lack of water.");
    Check(parched && std::string_view(parched->need) == "water" && parched->stage == 1,
          "Logged parched warning sets thirst stage one");
    const auto hydrated = ParseSurvivalMessage("You are properly hydrated.");
    Check(hydrated && std::string_view(hydrated->need) == "water" && hydrated->stage == 0,
          "Logged hydration recovery clears the remembered thirst stage without a drink effect");
    const auto mixedCaseHydrated = ParseSurvivalMessage("  YOU ARE PROPERLY HYDRATED  ");
    Check(mixedCaseHydrated && mixedCaseHydrated->stage == 0,
          "Recovery recognizes capitalization, whitespace and optional punctuation");
    const auto stillPeckish = ParseSurvivalMessage("You feel better, but still Peckish.");
    Check(stillPeckish && std::string_view(stillPeckish->need) == "food" && stillPeckish->stage == 1,
          "Partial meal keeps the logged peckish warning");
    for (const auto* text : {"You are properly fed.", "  YOU ARE PROPERLY FED  "}) {
        const auto fed = ParseSurvivalMessage(text);
        Check(fed && std::string_view(fed->need) == "food" && fed->stage == 0,
              "Properly fed recovery clears hunger without requiring a new food effect");
    }
    Check(!ParseSurvivalMessage("You are not properly fed.") && !ParseSurvivalMessage("Properly fed settler"),
          "Negated recovery and unrelated food text do not clear hunger");
    Check(!ParseSurvivalMessage("Properly hydrated water bottle") &&
          !ParseSurvivalMessage("You are not properly hydrated.") &&
          !ParseSurvivalMessage("You are carrying too much."),
          "Unrelated text and negated recovery cannot clear a need");
    Check(parsed.contains("bHideHealthyFigure") && parsed.at("bHideHealthyFigure") == 0,
          "UTF-8 BOM and Windows line endings preserve settings");
    Check(parsed.contains("iCtxAds") && parsed.at("iCtxAds") == 2 && !parsed.contains("invalid"),
          "Only Main section and complete integers accepted");
    auto settings = MakeSettings({{"bMineWarnings", 0}, {"iCtxAds", 1}},
                                 {{"bMineWarnings", 1}, {"iCtxAds", 2}});
    Check(settings.value("mineWarnings", false) && settings.contains("contextRules") &&
          settings["contextRules"].value("ads", "") == "hide", "MCM overrides default file");
    Check(settings.value("limbFigure", "") == "silhouette", "Auto without Condition Boy falls back");
    Check(MakeSettings({}, {}, 1).value("limbFigure", "") == "vaultgirl", "Auto respects Condition Girl");
    Check(MakeSettings({}, {{"iLimbFigure", 2}}, 1).value("limbFigure", "") == "vaultboy", "Explicit figure overrides Auto");
    auto invalid = MakeSettings({{"iShowWeight", 1}, {"bMineWarnings", 0}},
                                {{"iShowWeight", 99}, {"bMineWarnings", -1}});
    Check(invalid.value("showWeight", "") == "always" && !invalid.value("mineWarnings", true),
          "Out-of-range override preserves valid default");
    Check(settings.size() == 35 && settings.value("idleOpacity", 0.0) == 0.85 && settings.value("survivalEnemyMarkers", true) == false &&
          settings.value("showWeaponLegendary", false) == true && settings.value("showArmorWheel", false) == true,
          "All HUD settings supplied with safe defaults");
    Check(!MakeSettings({}, {}).value("hideThirdPersonAimCrosshair", true), "Third-person aim crosshair is visible by default");
    Check(MakeSettings({}, {}).value("showUndiscovered", false) && !MakeSettings({}, {{"bShowUndiscovered", 0}}).value("showUndiscovered", true),
          "Undiscovered locations show by default and the MCM can hide them");
    const auto calmDefaults = MakeSettings({}, {});
    Check(calmDefaults.value("showCrosshair", false) && calmDefaults.value("showHitMarkers", false) &&
          calmDefaults.value("calmDelay", -1) == 5, "Calm/crosshair defaults preserve existing visuals");
    Check(calmDefaults["calmRules"] == Json{{"vitals","show"},{"compass","show"},{"kit","show"},
          {"party","show"},{"tracker","show"},{"effects","show"},{"cross","show"}}, "All calm groups default to Show");
    const auto calmCustom = MakeSettings({}, {{"bShowCrosshair",0},{"bShowHitMarkers",0},{"iCalmDelay",15},
          {"iCalmVitals",2},{"iCalmCompass",1},{"iCalmWeapon",2},{"iCalmCompanions",1},
          {"iCalmQuests",2},{"iCalmEffects",1},{"iCalmCrosshair",2}});
    Check(!calmCustom["showCrosshair"].get<bool>() && !calmCustom["showHitMarkers"].get<bool>() &&
          calmCustom["calmDelay"] == 15 && calmCustom["calmRules"] == Json{{"vitals","hide"},{"compass","dim"},
          {"kit","hide"},{"party","dim"},{"tracker","hide"},{"effects","dim"},{"cross","hide"}}, "Calm MCM mapping");
    const auto calmInvalid = MakeSettings({{"iCalmVitals",1},{"iCalmDelay",0}},
          {{"iCalmVitals",3},{"iCalmDelay",16},{"bShowCrosshair",-1},{"bShowHitMarkers",2}});
    Check(calmInvalid["calmRules"]["vitals"] == "dim" && calmInvalid["calmDelay"] == 0 &&
          calmInvalid["showCrosshair"].get<bool>() && calmInvalid["showHitMarkers"].get<bool>(), "Invalid calm settings retain valid defaults");
    Check(MakeSettings({}, {{"bHideThirdPersonAimCrosshair", 1}}).value("hideThirdPersonAimCrosshair", false), "MCM can preserve ADS crosshair hiding");
    auto waypoint = ReadCustomWaypoint(0.0, 700.0, true);
    auto pin = CustomWaypointMarker(waypoint, 0, 0);
    Check(pin.value("t", "") == "pin" && pin.value("b", -1.0) == 0 && pin.value("dist", -1L) == 10,
          "Visible custom waypoint becomes a compass pin with north bearing and metre distance");
    pin = CustomWaypointMarker(ReadCustomWaypoint(700, 0, true), 0, 0);
    Check(pin.value("b", -1.0) == 90 && pin.value("id", "") == "player-waypoint", "Moved waypoint keeps stable ID and updates bearing");
    Check(CustomWaypointMarker(ReadCustomWaypoint(0, 0, false), 0, 0).is_null(), "Removed/invisible waypoint emits no stale marker");
    Check(ReadCustomWaypoint(0, 0, true).has_value(), "World origin is a valid custom waypoint");
    Check(!ReadCustomWaypoint(std::nullopt, 5, true) && !ReadCustomWaypoint(5, std::nullopt, true), "Incomplete waypoint is ignored");
    Check(!ReadCustomWaypoint(std::numeric_limits<double>::infinity(), 1, true), "Invalid waypoint coordinates are ignored");
    Check(CustomWaypointMarker(waypoint, std::numeric_limits<double>::quiet_NaN(), 0).is_null(), "Invalid player position is ignored");
    {
        // Four quests routed through one exit door, a second door 400 units away, and a quest marker on the same spot.
        const auto lead = MergeDoorMarkers({{640, 0, true}, {650, 10, true}, {640, 0, false}, {1040, 0, true}, {660, -20, true}, {645, 5, true}});
        Check(lead.size() == 6 && lead[0] == 0 && lead[1] == 0 && lead[4] == 0 && lead[5] == 0, "Door markers at one door merge into the first");
        Check(lead[2] == 2, "A quest marker that is not on a door never merges");
        Check(lead[3] == 3, "A different door keeps its own marker");
        Check(DoorGroupLabel("Investigate Saugus Ironworks", 1) == "Investigate Saugus Ironworks", "A single door marker keeps its objective");
        Check(DoorGroupLabel("x", 4) == "Door \u00b7 4 objectives", "A merged door marker counts its objectives");
        Check(MergeDoorMarkers({}).empty(), "No markers, no groups");
    }
    Check(settings.value("showLevelBar", false) && !settings.value("clock24", true),
          "Display defaults preserve level bar and 12-hour clock");
    Check(settings.value("textReadability", "missing") == "off", "Text readability preserves existing appearance by default");
    Check(MakeSettings({}, {{"iTextReadability", 1}}).value("textReadability", "missing") == "subtle",
          "Subtle text readability maps from MCM");
    std::istringstream textIni("[Main]\niTextReadability=2\n");
    Check(MakeSettings({{"iTextReadability", 1}}, ReadMainIni(textIni)).value("textReadability", "missing") == "strong",
          "Saved strong text readability overrides defaults");
    Check(MakeSettings({{"iTextReadability", 1}}, {{"iTextReadability", 99}}).value("textReadability", "missing") == "subtle",
          "Invalid readability override preserves valid default");
    Check(MakeSettings({}, {{"iTextReadability", -1}}).value("textReadability", "missing") == "off",
          "Invalid readability uses safe default");
    std::istringstream displayIni("[Main]\nbShowLevelBar=0\niClockFormat=1\n");
    auto display = MakeSettings({{"bShowLevelBar", 1}, {"iClockFormat", 0}}, ReadMainIni(displayIni));
    Check(!display.value("showLevelBar", true) && display.value("clock24", false),
          "Saved MCM display choices override defaults");
    auto badDisplay = MakeSettings({}, {{"bShowLevelBar", -1}, {"iClockFormat", 24}});
    Check(badDisplay.value("showLevelBar", false) && !badDisplay.value("clock24", true),
          "Invalid display settings preserve safe defaults");
    Check(!MakeSettings({}, {}).value("hideSatisfiedNeeds", true), "Needs remain visible by default");
    std::istringstream needsIni("[Main]\nbHideSatisfiedNeeds=1\n");
    Check(MakeSettings({{"bHideSatisfiedNeeds", 0}}, ReadMainIni(needsIni)).value("hideSatisfiedNeeds", false),
          "Saved dynamic needs choice overrides the default");
    Check(!MakeSettings({{"bHideSatisfiedNeeds", 1}}, {{"bHideSatisfiedNeeds", 0}}).value("hideSatisfiedNeeds", true),
          "Saved always-visible choice overrides dynamic needs");
    Check(!MakeSettings({}, {{"bHideSatisfiedNeeds", 2}}).value("hideSatisfiedNeeds", true),
          "Invalid dynamic needs choice preserves the default");
    Check(!MakeSettings({}, {}).value("hideFullVitals", true), "Vitals remain visible by default");
    std::istringstream vitalsIni("[Main]\nbHideFullVitals=1\n");
    Check(MakeSettings({{"bHideFullVitals", 0}}, ReadMainIni(vitalsIni)).value("hideFullVitals", false),
          "Saved dynamic vitals choice overrides default");
    Check(!MakeSettings({{"bHideFullVitals", 1}}, {{"bHideFullVitals", 0}}).value("hideFullVitals", true),
          "Saved always-visible vitals choice overrides default");
    Check(!MakeSettings({}, {{"bHideFullVitals", -1}}).value("hideFullVitals", true),
          "Invalid vitals setting preserves safe default");
    Check(!MakeSettings({}, {}).value("locationOnArrival", true), "Location remains visible by default");
    std::istringstream locationIni("[Main]\nbLocationOnArrival=1\n");
    Check(MakeSettings({{"bLocationOnArrival", 0}}, ReadMainIni(locationIni)).value("locationOnArrival", false),
          "Saved arrival-only location choice overrides default");
    Check(!MakeSettings({{"bLocationOnArrival", 1}}, {{"bLocationOnArrival", 0}}).value("locationOnArrival", true),
          "Saved persistent-location choice overrides default");
    Check(!MakeSettings({}, {{"bLocationOnArrival", 2}}).value("locationOnArrival", true),
          "Invalid location setting preserves safe default");

    std::istringstream good("\xef\xbb\xbf// replay\n\n{\"snapshot\":true,\"hp\":123}\n{\"notify\":{\"text\":\"Quote \\\" & \\u2605\"}}\n");
    auto file = ReadReplay(good, schema);
    Check(file.errors.empty() && file.messages.size() == 2, "Replay handles comments, BOM, Unicode and escaping");
    std::istringstream bad("{\"hp\":10}\n[]\n{bad}\n");
    auto rejected = ReadReplay(bad, schema);
    Check(rejected.messages.empty() && rejected.errors.size() == 2, "Invalid replay rejected atomically with line errors");
    std::istringstream empty("// none\n");
    Check(!ReadReplay(empty, schema).errors.empty(), "Empty replay reports a useful error");
    std::istringstream wrongType("{\"hp\":10}\n{\"markers\":{}}\n");
    auto invalidTypes = ReadReplay(wrongType, schema);
    Check(invalidTypes.messages.empty() && !invalidTypes.errors.empty(), "Schema-invalid records prevent any replay output");
    std::ifstream examples("examples/messages.jsonl");
    auto allExamples = ReadReplay(examples, schema);
    Check(allExamples.errors.empty() && allExamples.messages.size() == 95, "Canonical examples pass native replay validation");
    std::istringstream unicode("{\"explosive\":{\"short\":\"★☢★☢★☢\"}}\n");
    Check(ReadReplay(unicode, schema).errors.empty(), "Schema lengths count Unicode characters, not UTF-8 bytes");

    Replay replay;
    auto start = Replay::Clock::time_point{};
    replay.Start({Json{{"hp", 10}}, Json{{"hp", 20}}, Json{{"hp", 30}}}, start);
    Check(replay.Active(), "Replay starts");
    Check(replay.Next(start) == std::optional<Json>(Json{{"hp", 10}}), "First line immediately ready");
    Check(!replay.Next(start + 999ms), "No early delivery");
    Check(replay.Next(start + 10s) == std::optional<Json>(Json{{"hp", 20}}), "Stall preserves order");
    Check(!replay.Next(start + 10s), "No catch-up burst after game-thread stall");
    Check(replay.Next(start + 11s) == std::optional<Json>(Json{{"hp", 30}}) && !replay.Active(), "Replay completes exactly once");
    replay.Start({Json{{"settings", Json::object()}}, Json{{"reset", true}}, Json{{"snapshot", true}, {"hp", 1}}, Json{{"hp", 2}}}, start);
    Check(replay.Next(start).has_value() && replay.Next(start).has_value() &&
          replay.Next(start) == std::optional<Json>(Json{{"snapshot", true}, {"hp", 1}}),
          "Lead-in lines and the first snapshot go out without delay");
    Check(!replay.Next(start + 999ms) && replay.Next(start + 1s) == std::optional<Json>(Json{{"hp", 2}}),
          "Pacing starts after the first snapshot");
    replay.Start({Json{{"hp", 99}}}, start);
    replay.Stop();
    Check(!replay.Active() && !replay.Next(start + 20s), "Cancellation drops stale save data");
    Check(XpForLevel(1) == 0 && XpForLevel(2) == 200 && XpForLevel(3) == 475 && XpForLevel(270) == 2757250,
          "XP curve matches FO4 (200 for level 2; checked against a level-270 save)");
    Check(std::abs(XpFraction(270, 2761030) - 0.1855F) < 0.001F, "Real save: level 270 with 2,761,030 XP is 18.6% in");
    Check(std::abs(XpFraction(2, 337.5F) - 0.5F) < 0.001F, "Running total: halfway from level 2 to 3");
    Check(XpFraction(23, 0) == 0 && XpFraction(2, 5000) == 1.0F, "Fraction clamps");
    Check(std::abs(XpFraction(23, 0.5F * (75 * 23 + 125)) - 0.5F) < 0.001F, "Below the threshold reads as XP within the level");
    auto gain = ProgressEvents(2, 300, 2, 450);
    Check(gain.value("xpGain", 0) == 150 && !gain.contains("levelUp"), "XP gain without a level-up");
    auto up = ProgressEvents(2, 400, 3, 500);
    Check(up.value("xpGain", 0) == 100 && up["levelUp"].value("from", 0) == 2 && up["levelUp"].value("to", 0) == 3,
          "Level-up carries the gain and from/to");
    Check(ProgressEvents(2, 300, 2, 300).empty() && ProgressEvents(0, 0, 5, 3000).empty(),
          "No event without a gain or before the first read");
    RadiationEvents radiation;
    radiation.Reset(4, start);
    Check(radiation.Update(4, start + 250ms).empty(), "Existing radiation at load is not exposure");
    Check(radiation.Update(6, start + 500ms).empty(), "Radiation popup waits for the one-second batch");
    Check(radiation.Update(9, start + 1s) == Json{{"rads", 9}, {"radsGain", 5}},
          "Radiation gains accumulate and carry the exact absolute value");
    Check(radiation.Update(11, start + 1250ms).empty(), "Subsequent exposure is throttled");
    Check(radiation.Update(11, start + 2s) == Json{{"rads", 11}, {"radsGain", 2}},
          "Pending gain flushes even after exposure stops");
    Check(radiation.Update(2, start + 2250ms).empty(), "Radiation removal does not emit a gain");
    Check(radiation.Update(4, start + 3s) == Json{{"rads", 4}, {"radsGain", 2}},
          "New exposure uses the lowered radiation baseline");
    Check(radiation.Update(5, start + 3250ms).empty(), "An unsent gain is pending before a load");
    radiation.Reset(800, start + 3500ms);
    Check(radiation.Update(800, start + 5s).empty(), "Save reset discards pending radiation and rebaselines");
    Check(radiation.Update(802, start + 5250ms) == Json{{"rads", 802}, {"radsGain", 2}},
          "Exposure resumes after a save reset");
    radiation.Reset(0, start);
    radiation.Update(3, start + 250ms);
    Check(radiation.Update(0, start + 1s) == Json{{"rads", 0}, {"radsGain", 3}},
          "Exposure followed by removal keeps the popup and authoritative zero");
    Delta delta;
    auto first = delta.Changes(Json{{"hp", 10}, {"weapon", "Pipe Gun"}, {"explosive", nullptr}});
    Check(first.size() == 3, "First read sends every field");
    Check(delta.Changes(Json{{"hp", 10}, {"weapon", "Pipe Gun"}, {"explosive", nullptr}}).empty(), "Unchanged read sends nothing");
    auto changed = delta.Changes(Json{{"hp", 9}, {"weapon", "Pipe Gun"}, {"explosive", Json{{"name", "Frag"}, {"count", 2}}}});
    Check(changed.size() == 2 && changed["hp"] == 9 && changed["explosive"]["count"] == 2, "Only changed fields, objects compared whole");
    delta.Clear();
    Check(delta.Changes(Json{{"hp", 9}}).size() == 1, "Clear forgets what was sent");
    Check(MakeSettings({}, {{"iIdleOpacity", 5}}).value("idleOpacity", 0.0) == 0.2, "Idle opacity never below 20%");
    Check(MakeSettings({}, {}).value("textScale", 0.0) == 1.0 && MakeSettings({}, {}).value("iconScale", 0.0) == 1.0,
          "Text and icon size default to 100%");
    Check(MakeSettings({}, {}).value("ringScale", 0.0) == 1.0 && MakeSettings({}, {{"iRingScale", 150}}).value("ringScale", 0.0) == 1.5 &&
          MakeSettings({}, {{"iRingScale", 30}}).value("ringScale", 0.0) == 0.8 && MakeSettings({}, {{"iRingScale", 151}}).value("ringScale", 0.0) == 1.0,
          "Ring size defaults to 100% and stays within 80-150%");
    Check(MakeSettings({}, {{"iTextScale", 150}, {"iIconScale", 200}}).value("textScale", 0.0) == 1.5 &&
          MakeSettings({}, {{"iTextScale", 150}, {"iIconScale", 200}}).value("iconScale", 0.0) == 2.0, "Size sliders reach their maximum");
    Check(MakeSettings({}, {{"iTextScale", 40}, {"iIconScale", 10}}).value("textScale", 0.0) == 0.8 &&
          MakeSettings({}, {{"iIconScale", 10}}).value("iconScale", 0.0) == 0.8, "Sizes never below 80%");
    Check(MakeSettings({}, {{"iTextScale", 400}, {"iIconScale", 999}}).value("textScale", 0.0) == 1.0 &&
          MakeSettings({}, {{"iIconScale", 999}}).value("iconScale", 0.0) == 1.0, "Out-of-range sizes fall back to 100%");
    Check(EffectHourLimit({}, {}) == 2 && EffectHourLimit({{"iMaxEffectHours", 2}}, {{"iMaxEffectHours", 0}}) == 0,
          "Effect limit defaults to 2 hours; 0 shows everything");
    Check(ModData::Slug("  Hit the Deck!!! ") == "hit-the-deck", "Challenge identifiers normalize punctuation and whitespace");
    EventInbox<std::shared_ptr<int>> inbox(2);
    auto reference = std::make_shared<int>(7);
    inbox.Push(reference);
    Check(reference.use_count() == 1, "Inactive event inbox does not retain game references");
    inbox.SetEnabled(true);
    inbox.Push(reference);
    inbox.Push(std::make_shared<int>(8));
    inbox.Push(std::make_shared<int>(9));
    auto batch = inbox.Take();
    Check(batch.events.size() == 2 && *batch.events[0] == 7 && *batch.events[1] == 8 && batch.dropped == 1,
          "Event bursts preserve accepted order and report bounded overflow");
    batch.events.clear();
    Check(reference.use_count() == 1 && inbox.Take().dropped == 0, "Drained events release references and overflow is reported once");
    inbox.Push(reference);
    inbox.SetEnabled(false);
    Check(reference.use_count() == 1 && inbox.Take().events.empty(), "Disabling capture drops stale queued references");
    inbox.SetEnabled(true);
    inbox.Push(reference);
    Check(inbox.Take().events.size() == 1, "Event capture resumes after reset");
    Check(ModData::Slug("Vault 81") == "vault-81" && ModData::Slug("---") == "x", "Slugs retain digits and handle empty names");
    Check(ModData::KarmaTitle(0) == "Kindhearted" && ModData::KarmaTitle(23) == "Devil" &&
          !ModData::KarmaTitle(-1) && !ModData::KarmaTitle(24), "Karma AV indices resolve at both bounds and reject invalid indices");
    Check(ModData::ReputationTitle(0) == "Neutral" && ModData::ReputationTitle(18) == "Drag" &&
          !ModData::ReputationTitle(-1) && !ModData::ReputationTitle(19), "Reputation AV indices resolve at both bounds and reject invalid indices");
    Check(ModData::ParseChallenge(" Abominable: Kill abominations ( 12 / 42 ) ") ==
          std::optional<Json>(Json{{"id", "ch-abominable"}, {"name", "Abominable"}, {"cur", 12}, {"max", 42}}),
          "Challenge parser extracts name and resolved progress");
    Check(ModData::ParseChallenge("Hit the Deck (75/50)")->at("cur") == 50 &&
          ModData::ParseChallenge("Hit the Deck (-2/50)")->at("cur") == 0, "Challenge progress clamps to its range");
    const ModData::GlobalResolver globals = [](std::string_view id) -> std::optional<double> {
        if (id == "CHAL_PBT_FIRST_X") return 12;
        if (id == "BAD") return std::numeric_limits<double>::quiet_NaN();
        return std::nullopt;
    };
    Check(ModData::ParseChallenge("Abominable (<Global=CHAL_PBT_FIRST_X>/42)", globals)->at("cur") == 12,
          "Challenge global tokens resolve by their exact editor ID");
    Check(!ModData::ParseChallenge("Abominable (<Global=MISSING>/42)", globals), "Missing globals do not fabricate zero progress");
    for (const auto* text : {"No progress", "(1/2)", "Empty (1/0)", "Bad (12junk/42)", "Bad (12/42junk)",
                             "Bad (junk/42)", "Bad (nan/42)", "Bad (1/inf)", "Bad (<Global=BAD>/42)"}) {
        Check(!ModData::ParseChallenge(text, globals), text);
    }
    UiMailbox<int, std::string> uiMailbox;
    Check(!uiMailbox.Latest(), "No Flash snapshot is available before UI capture");
    Check(uiMailbox.Submit(1) && !uiMailbox.Submit(2), "UI requests coalesce into one queued callback");
    auto uiWork = uiMailbox.Begin();
    Check(uiWork && uiWork->request == 2, "UI callback uses the latest requested settings");
    Check(!uiMailbox.Complete(*uiWork, "old prompt") && uiMailbox.Latest() == "old prompt",
          "UI capture publishes an owned copy and releases its queue reservation");
    Check(uiMailbox.Submit(3), "A later frame can schedule another UI capture");
    uiWork = uiMailbox.Begin();
    uiMailbox.Reset();
    Check(!uiMailbox.Latest(), "Save transition immediately discards the old prompt snapshot");
    Check(!uiMailbox.Submit(4), "Save transition does not accumulate callbacks behind an outstanding one");
    Check(uiMailbox.Complete(*uiWork, "stale prompt") && !uiMailbox.Latest(),
          "In-flight old-session results are rejected and new-session work is retained");
    uiWork = uiMailbox.Begin();
    Check(uiWork && uiWork->request == 4, "Next UI capture uses the new session request");
    Check(!uiMailbox.Submit(0), "Restore requested during capture coalesces without a second callback");
    Check(uiMailbox.Complete(*uiWork, "new prompt"), "Settings/restore changes during capture request another pass");
    uiWork = uiMailbox.Begin();
    Check(uiWork && uiWork->request == 0, "Latest restore request is not lost");
    Check(!uiMailbox.Complete(*uiWork, "restored"), "Restore completes without an endless retry");
    Check(uiMailbox.Submit(5), "UI queue can restart after restore");
    uiMailbox.Reset();
    Check(!uiMailbox.Begin() && !uiMailbox.Latest(), "Reset before callback execution prevents stale work entirely");
    Check(uiMailbox.Submit(6), "A canceled old callback releases the reservation");
    uiMailbox.CancelQueue();
    Check(uiMailbox.Submit(7), "Failed scheduling remains retryable");
    uiWork = uiMailbox.Begin();
    Check(!uiMailbox.Complete(*uiWork, std::nullopt), "Capture failure releases the queue reservation");

    Check(!KeepGameSubtitles(false, "recommended") && !KeepGameSubtitles(false, "show"), "Outside dialogue the game's subtitle stays hidden");
    Check(KeepGameSubtitles(true, "recommended") && KeepGameSubtitles(true, "hide"),
          "In dialogue the game's subtitle shows while Phosphor's stands down");
    Check(!KeepGameSubtitles(true, "show"), "Dialogue set to Show everything: Phosphor's subtitle only, never doubled");
    {
        const auto rules = MakeSettings({}, {{"iCtxDialogue", 1}})["contextRules"];
        Check(rules.value("dialogue", "") == "show", "iCtxDialogue 1 maps to the rule the subtitle decision reads");
    }
    Check(QuickLootFirstEnabled({}, {}), "Quick loot first is on by default");
    Check(!QuickLootFirstEnabled({{"bQuickLootFirst", 1}}, {{"bQuickLootFirst", 0}}), "The MCM switch turns quick loot first off");
    Check(QuickLootFirstEnabled({{"bQuickLootFirst", 1}}, {{"bQuickLootFirst", 7}}), "Out-of-range values keep the default");
    {
        const auto bind = [](const char* text) {
            std::istringstream input(text);
            return FindKeybind(input, "PhosphorHUD", "PerkChoicesKey");
        };
        const auto g = bind(R"({"keybinds":[{"id":"Respond","keycode":67,"modName":"PressXtoRespond","modifiers":0},
                                {"id":"PerkChoicesKey","keycode":71,"modName":"PhosphorHUD","modifiers":3}],"version":1})");
        Check(g && g->code == 71 && g->modifiers == 3, "The perk-choices key and its modifiers are read from Keybinds.json");
        Check(!bind(R"({"keybinds":[{"id":"Respond","keycode":67,"modName":"PressXtoRespond","modifiers":0}]})"),
              "No entry means no key: the feature waits for the player to pick one");
        Check(!bind(R"({"keybinds":[{"id":"PerkChoicesKey","keycode":0,"modName":"PhosphorHUD","modifiers":0}]})"),
              "An unbound key is ignored");
        Check(!bind("not json") && !bind("{}") && !bind(R"({"keybinds":[{"id":"PerkChoicesKey","keycode":"G","modName":"PhosphorHUD"}]})"),
              "Broken or unexpected Keybinds.json content is ignored");
        const auto noMods = bind(R"({"keybinds":[{"id":"PerkChoicesKey","keycode":20,"modName":"PhosphorHUD"}]})");
        Check(noMods && noMods->code == 20 && noMods->modifiers == 0, "Missing modifiers mean none");
    }
    if (failures) { std::cerr << failures << " failures\n"; return 1; }
    std::cout << "All core tests passed\n";
}
