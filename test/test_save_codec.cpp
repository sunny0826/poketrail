#include "support/codec_test/harness.hpp"
#include "support/codec_test/activity_cases.hpp"
#include "support/codec_test/framing_cases.hpp"
#include "support/codec_test/schema_cases.hpp"

int main() {
  activityState();
  Save s;
  std::vector<uint8_t> data;
  Save decoded;
  framingChecks(s, data, decoded);
  schemaChecks(s, data, decoded);
}
