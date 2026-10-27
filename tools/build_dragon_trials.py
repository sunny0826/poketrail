#!/usr/bin/env python3
"""Generate the fixed dragon trial catalog from assets/dragon-trials.json.

The two trials, their stages, the shared battle parameters and the upgrade caps
are contract data. Everything is validated against the frozen shape before any
firmware file is written, so a damaged source cannot silently change the game.
"""
import argparse
import hashlib
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets/dragon-trials.json"
HEADER = ROOT / "firmware/apps/20_pokedex/dragon_trial_catalog_generated.hpp"
IMPLEMENTATION = ROOT / "firmware/apps/20_pokedex/dragon_trial_catalog_generated.cpp"

# Frozen shape of issue #2. The JSON is the reviewed fixed source; these
# constants make drift impossible without editing this generator.
TICKET_STEP = 6
PLAYER_HP = 3
CUES_PER_STAGE = 12
NORMAL_WINDOW_MS = 180
PERFECT_WINDOW_MS = 70
TELEGRAPH = "Alternate"
NORMAL_BONUS_STEP_MS = 30
PERFECT_BONUS_STEP_MS = 10
MAX_NORMAL_BONUS_MS = 180
MAX_PERFECT_BONUS_MS = 60
MAX_GUARD = 1
TRIALS = [
    {
        "id": 1,
        "species": 147,
        "stages": [(3, 1500), (4, 1450)],
    },
    {
        "id": 2,
        "species": 246,
        "stages": [(3, 1500), (4, 1450), (5, 1350)],
    },
]


def require(condition, message):
    if not condition:
        raise ValueError(message)


def _int(value, label, low, high):
    require(type(value) is int, f"{label} must be an integer")
    require(low <= value <= high, f"{label} out of range")
    return value


def _exact_map(data, keys, label):
    require(isinstance(data, dict), f"{label} must be an object")
    require(set(data) == set(keys), f"{label} must contain exactly {sorted(keys)}")


def validate(data):
    _exact_map(data, {"ruleVersion", "note", "ticketStep", "playerHp", "cuesPerStage",
                      "normalWindowMs", "perfectWindowMs", "telegraph", "upgrades",
                      "trials"}, "catalog")
    require(type(data["note"]) is str and data["note"], "note must be non-empty text")
    require(_int(data["ruleVersion"], "ruleVersion", 1, 1) == 1, "unsupported rule version")
    require(_int(data["ticketStep"], "ticketStep", TICKET_STEP, TICKET_STEP) == TICKET_STEP,
            "ticket step must stay 6")
    require(_int(data["playerHp"], "playerHp", PLAYER_HP, PLAYER_HP) == PLAYER_HP,
            "player HP must stay 3")
    require(_int(data["cuesPerStage"], "cuesPerStage", CUES_PER_STAGE, CUES_PER_STAGE)
            == CUES_PER_STAGE, "each stage must keep 12 cues")
    require(_int(data["normalWindowMs"], "normalWindowMs", NORMAL_WINDOW_MS, NORMAL_WINDOW_MS)
            == NORMAL_WINDOW_MS, "normal window must stay 180ms")
    require(_int(data["perfectWindowMs"], "perfectWindowMs", PERFECT_WINDOW_MS, PERFECT_WINDOW_MS)
            == PERFECT_WINDOW_MS, "perfect window must stay 70ms")
    require(data["telegraph"] == TELEGRAPH, "telegraph must stay Alternate")

    upgrades = data["upgrades"]
    _exact_map(upgrades, {"normalStepMs", "perfectStepMs", "maxNormalBonusMs",
                          "maxPerfectBonusMs", "maxGuard"}, "upgrades")
    checks = [
        ("normalStepMs", NORMAL_BONUS_STEP_MS),
        ("perfectStepMs", PERFECT_BONUS_STEP_MS),
        ("maxNormalBonusMs", MAX_NORMAL_BONUS_MS),
        ("maxPerfectBonusMs", MAX_PERFECT_BONUS_MS),
        ("maxGuard", MAX_GUARD),
    ]
    for key, expected in checks:
        require(_int(upgrades[key], f"upgrades.{key}", expected, expected) == expected,
                f"upgrades.{key} must stay {expected}")
    require(upgrades["maxNormalBonusMs"] % upgrades["normalStepMs"] == 0,
            "normal bonus cap must be a whole number of 30ms steps")
    require(upgrades["maxPerfectBonusMs"] % upgrades["perfectStepMs"] == 0,
            "perfect bonus cap must be a whole number of 10ms steps")

    trials = data["trials"]
    require(isinstance(trials, list) and len(trials) == len(TRIALS),
            "the catalog holds exactly two trials")
    seen_ids, seen_species = set(), set()
    stages = []
    for entry, expected in zip(trials, TRIALS):
        _exact_map(entry, {"id", "species", "stages"}, "trial")
        require(_int(entry["id"], "trial id", 1, 2) == expected["id"], "trial ids are fixed 1/2")
        require(_int(entry["species"], "trial species", 1, 251) == expected["species"],
                "trial species is fixed")
        require(entry["id"] not in seen_ids and entry["species"] not in seen_species,
                "trial ids and species must be unique")
        seen_ids.add(entry["id"])
        seen_species.add(entry["species"])
        raw = entry["stages"]
        require(isinstance(raw, list), "trial stages must be a list")
        require(len(raw) == len(expected["stages"]), "trial stage count is fixed")
        for stage, (wanted_hp, wanted_beat) in zip(raw, expected["stages"]):
            _exact_map(stage, {"enemyHp", "beatMs"}, "stage")
            require(_int(stage["enemyHp"], "stage enemyHp", 1, 9) == wanted_hp,
                    "stage enemy HP is fixed")
            require(_int(stage["beatMs"], "stage beatMs", 800, 3000) == wanted_beat,
                    "stage beat is fixed")
            stages.append((wanted_hp, wanted_beat))
    return stages


def _sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _trial_rows(data):
    rows = []
    first = 0
    for entry in data["trials"]:
        rows.append((entry["species"], first, len(entry["stages"])))
        first += len(entry["stages"])
    return rows


def generate(data, source_sha256):
    stages = validate(data)
    rows = _trial_rows(data)
    lines = [
        "// Generated by tools/build_dragon_trials.py; edit assets/dragon-trials.json.",
        "#pragma once",
        "#include <cstdint>",
        "namespace pokedex { namespace adventure { namespace dragongen {",
        "struct StageEntry { uint16_t enemyHp; uint16_t beatMs; };",
        "struct TrialEntry { uint16_t species; uint8_t firstStage; uint8_t stageCount; };",
        f"constexpr uint8_t kTrialCount = {len(rows)};",
        f"constexpr uint8_t kStageCount = {len(stages)};",
        f"constexpr uint8_t kTicketStep = {data['ticketStep']};",
        f"constexpr uint8_t kPlayerHp = {data['playerHp']};",
        f"constexpr uint8_t kCueLimit = {data['cuesPerStage']};",
        f"constexpr uint16_t kNormalWindowMs = {data['normalWindowMs']};",
        f"constexpr uint16_t kPerfectWindowMs = {data['perfectWindowMs']};",
        "constexpr uint8_t kTelegraphAlternate = 0;  // TelegraphPattern::Alternate",
        f"constexpr uint8_t kNormalBonusStepMs = {data['upgrades']['normalStepMs']};",
        f"constexpr uint8_t kPerfectBonusStepMs = {data['upgrades']['perfectStepMs']};",
        f"constexpr uint8_t kMaxNormalBonusMs = {data['upgrades']['maxNormalBonusMs']};",
        f"constexpr uint8_t kMaxPerfectBonusMs = {data['upgrades']['maxPerfectBonusMs']};",
        f"constexpr uint8_t kMaxGuard = {data['upgrades']['maxGuard']};",
        "extern const StageEntry kStages[kStageCount];",
        "extern const TrialEntry kTrials[kTrialCount];",
        "extern const char kSourceSha256[65];",
        "} } }",
        "",
    ]
    header = "\n".join(lines)
    implementation = "\n".join([
        "// Generated by tools/build_dragon_trials.py; edit assets/dragon-trials.json.",
        "#include \"dragon_trial_catalog_generated.hpp\"",
        "namespace pokedex { namespace adventure { namespace dragongen {",
        "const StageEntry kStages[kStageCount] = {",
        *[f"  {{{hp}, {beat}}},  // stage {index}"
          for index, (hp, beat) in enumerate(stages)],
        "};",
        "const TrialEntry kTrials[kTrialCount] = {",
        *[f"  {{{species}, {first}, {count}}},  // trial {index + 1}"
          for index, (species, first, count) in enumerate(rows)],
        "};",
        f"const char kSourceSha256[65] = \"{source_sha256}\";",
        "} } }",
        "",
    ])
    return header, implementation


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    raw = SOURCE.read_bytes()
    data = json.loads(raw)
    source_sha256 = hashlib.sha256(raw).hexdigest()
    header, implementation = generate(data, source_sha256)
    if args.check:
        if not HEADER.exists() or HEADER.read_text() != header:
            raise SystemExit("dragon trial header stale; regenerate tools/build_dragon_trials.py")
        if not IMPLEMENTATION.exists() or IMPLEMENTATION.read_text() != implementation:
            raise SystemExit("dragon trial implementation stale; regenerate tools/build_dragon_trials.py")
        print(f"dragon trial check: 2 trials, 5 stages, 12 cues, 180/70ms, "
              f"bonus 30/10 caps 180/60, guard {data['upgrades']['maxGuard']}, "
              f"source {source_sha256[:12]}")
        return
    HEADER.write_text(header)
    IMPLEMENTATION.write_text(implementation)
    print(f"generated {HEADER.relative_to(ROOT)} and {IMPLEMENTATION.relative_to(ROOT)}")


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, KeyError) as error:
        sys.exit(f"龙穴试炼目录生成失败：{error}")
