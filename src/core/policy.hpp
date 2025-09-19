#pragma once
#include <string>
#include <vector>
#include <unordered_set>

namespace core {

struct Policy {
  std::string name; // "aggressive" | "safe" | "custom"
  std::vector<std::string> keep; // glob patterns
  std::vector<std::string> drop; // glob patterns
};

Policy load_policy(bool safe_flag,
                   const std::string& custom_path,
                   const std::vector<std::string>& keep_cli,
                   const std::vector<std::string>& drop_cli);

// simple glob matcher: '*' wildcard, case-sensitive
bool glob_match(const std::string& pattern, const std::string& text);

// risk: HIGH/MEDIUM/LOW/SAFE
std::string risk_for(const std::string& canonical);

// convenience: decide if a field should be kept
bool policy_keep(const Policy& p, const std::string& canonical);

} // namespace core
