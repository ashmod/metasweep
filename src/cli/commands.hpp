#pragma once
#include <string>
#include <vector>
#include "core/policy.hpp"

namespace cmd {

struct InspectOpts {
  bool recursive = false;
  std::string format = "auto";
  std::string report;
  int verbose = 0;
  bool no_color = false;
};

struct StripOpts {
  bool recursive = false;
  std::string out_dir;
  bool in_place = false;
  bool yes = false;
  std::string format = "auto";
  std::string report;
  bool dry_run = false;
  int verbose = 0;
  bool no_color = false;
};

struct ExplainOpts {
  int verbose = 0;
  bool no_color = false;
};

int run_inspect(const std::vector<std::string>& targets, const InspectOpts&);
int run_strip(const std::vector<std::string>& targets, const core::Policy&, const StripOpts&);
int run_explain(const std::string& target, const ExplainOpts&);
int run_policy(const std::string& action, const std::string& file);

} // namespace cmd
