#pragma once
#include <cstdio>
#include <cstdlib>
#include <string>

inline std::string verificationOutput(const char* name) {
  const char* root = std::getenv("MYM5_UI_OUTPUT_DIR");
  if (!root || !*root) {
    std::fprintf(stderr, "MYM5_UI_OUTPUT_DIR is required; use mise run test-render\n");
    std::exit(1);
  }
  return std::string(root) + "/" + name;
}
