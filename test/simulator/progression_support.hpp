#pragma once
#include "driver.hpp"

namespace simulator {
namespace e2e {
namespace progression_support {
bool expect(Driver&, bool, const std::string&);
bool wait(Driver&, const std::function<bool()>&, uint32_t, const std::string&);
bool page(Driver&, Page, const std::string&, uint32_t maxMs = 8000);
bool toHome(Driver&, const std::string&);
bool openViewer(Driver&, const std::string&);
bool openPartners(Driver&, const std::string&);
bool carry(Driver&, unsigned row, uint16_t species, const std::string&);
bool homeEvolution(Driver&, uint16_t from, uint16_t to, const std::string&);
bool finishEvolution(Driver&, const std::string&);
bool receipt(Driver&, uint16_t from, uint16_t to, uint64_t cost, const std::string&);
bool onlyProgress(const Save& before, const Save& after, uint16_t id, uint64_t total);
bool sameAccounts(Driver&, const Save& before, const std::string&);
bool quiescentWrites(Driver&, const std::string&);
void viewerKeys(Driver&, SDL_Scancode, unsigned count, const std::string&);
bool openPreviewTargets(Driver&, const std::string&);
bool skipPreviewToViewer(Driver&, const std::string&);
bool practiceRound(Driver&, const std::string&);
}
}
}
