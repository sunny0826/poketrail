#pragma once
#include "ruins_hub_cases.hpp"
#include "ruins_replay_cases.hpp"
#include "ruins_telegraph_cases.hpp"
#include "ruins_clue_cases.hpp"
#include "ruins_regression_cases.hpp"
#include <cstring>

// Runs every legendary ruins Runtime case, or one selected by MYM5_RUINS_CASE.
namespace {
void ruinsCases() {
  const char* only = std::getenv("MYM5_RUINS_CASE");
  const auto run = [only](const char* name, void (*fn)()) {
    if (!only || !*only || std::strstr(name, only)) fn();
  };
  run("entry-and-clue", ruinsEntryAndClue);
  run("order-and-capture", ruinsOrderAndCapture);
  run("boss-retreat-retry", ruinsBossRetreatRetry);
  run("captured-replay-zero-balls", ruinsCapturedReplayAndZeroBalls);
  run("target-states", ruinsTargetStates);
  run("dodge-first-bosses", ruinsDodgeFirstBosses);
  run("clue-order-visibility", ruinsClueOrderVisibility);
  run("captured-failure-menu", ruinsCapturedFailureMenu);
  run("deferred-throw-resume", ruinsDeferredThrowResume);
}
}
