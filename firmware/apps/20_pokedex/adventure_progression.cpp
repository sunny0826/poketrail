#include "adventure.hpp"
#include "encounter_catalog.hpp"
namespace pokedex { namespace adventure {
EvolutionView Engine::evolutionView(uint16_t species) const {
  EvolutionView view; view.from = species; view.revision = state_.rosterRevision;
  if (species < 1 || species > kSpeciesCount) return view;
  const SpeciesProgress& progress = state_.progress[species - 1];
  view.walked = progress.walked; view.available = progress.walked - progress.spent;
  CommandResult status = guard();
  if (status == CommandResult::Applied &&
      ((state_.phase != Phase::Walking && state_.phase != Phase::EncounterReady) ||
       state_.clock.pending.active || throw_.active ||
       (battle_.view().active && battle_.view().result == Result::None))) status = CommandResult::InvalidPhase;
  if (status == CommandResult::Applied && (!owns(state_, species) || state_.partner != species)) status = CommandResult::InvalidInput;
  size_t count = 0; const EvolutionRule* rules = evolutionRules(count);
  for (size_t i = 0; i < count && view.count < kEvolutionOptions; ++i) {
    if (rules[i].from != species) continue;
    EvolutionOption& option = view.options[view.count++]; option.target = rules[i].to; option.cost = rules[i].steps;
    option.status = status;
    if (option.status == CommandResult::Applied) {
      if (owns(state_, option.target)) option.status = CommandResult::TargetOwned;
      else if (view.available < option.cost) option.status = CommandResult::InsufficientProgress;
      else if (state_.rosterRevision == UINT64_MAX) option.status = CommandResult::InvalidInput;
    }
  }
  return view;
}
CommandResult Engine::evolve(EvolveRequest request, const ClockReading& clock) {
  const CommandResult g = guard(); if (g != CommandResult::Applied) return g;
  const EvolveRequest& last = state_.lastEvolution.request;
  if (last.from && request.from == last.from && request.to == last.to && request.rosterRevision == last.rosterRevision)
    return CommandResult::Duplicate;
  if (request.rosterRevision != state_.rosterRevision) return CommandResult::Stale;
  if (!validReading(clock) || clock.bootGeneration != state_.bootGeneration) return CommandResult::InvalidInput;
  if (queueSize_ || overflow_) {
    if ((state_.phase != Phase::Walking && state_.phase != Phase::EncounterReady) || throw_.active)
      return CommandResult::InvalidPhase;
    const CommandResult r = flushSteps();
    if (r != CommandResult::Applied && r != CommandResult::Duplicate) return r;
  }
  const EvolutionView view = evolutionView(request.from);
  const EvolutionOption* selected = nullptr;
  for (size_t i = 0; i < view.count; ++i) if (view.options[i].target == request.to) selected = &view.options[i];
  if (!selected) return CommandResult::InvalidInput;
  if (selected->status != CommandResult::Applied) return selected->status;
  stage(); candidate_.progress[request.from - 1].spent += selected->cost;
  candidate_.owned[(request.from - 1) / 8] &= uint8_t(~(1u << ((request.from - 1) % 8)));
  setOwned(candidate_, request.to); setRegistered(candidate_, request.to);
  candidate_.partner = request.to; ++candidate_.rosterRevision;
  if (candidate_.phase == Phase::EncounterReady) candidate_.encounter.partner = request.to;
  candidate_.lastEvolution.request = request; candidate_.lastEvolution.cost = selected->cost;
  candidate_.lastEvolution.at = makeStamp(candidate_, clock);
  return submit();
}
CommandResult Engine::giveUpCapture(uint64_t id) {
  const CommandResult g = guard(); if (g != CommandResult::Applied) return g;
  if (!id || id != state_.encounter.id) return CommandResult::InvalidInput;
  if (state_.phase == Phase::Resolved && state_.result == Result::Released) return CommandResult::Duplicate;
  if (state_.phase != Phase::ThrowReady || throw_.active) return CommandResult::InvalidPhase;
  stage(); candidate_.phase = Phase::Resolved; candidate_.result = Result::Released;
  return submit();
}
} }
