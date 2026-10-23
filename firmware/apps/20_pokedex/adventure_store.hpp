#pragma once
#include "adventure_model.hpp"
namespace pokedex { namespace adventure {
enum class LoadStatus : uint8_t {
  Empty, Loaded, Corrupt, UnsupportedVersion, IoError, NeedsProvisioning
};
struct LoadResult {
  LoadStatus status;
  bool degraded;
  LoadResult(LoadStatus s = LoadStatus::Empty, bool d = false) : status(s), degraded(d) {}
};
enum class CommitStatus : uint8_t { Committed, IoError, Conflict };
class Store {
 public:
  virtual ~Store() {}
  virtual LoadResult load(Save& out) = 0;
  virtual CommitStatus commit(const Save& candidate) = 0;
};
} }
