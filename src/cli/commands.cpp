#include "commands.hpp"
#include <fmt/format.h>
#include <filesystem>
#include "core/detect.hpp"
#include "core/report.hpp"
#include "core/sanitize.hpp"
#include "util/fs.hpp"

using namespace std;

namespace cmd {
int run_inspect(const vector<string>& targets, const InspectOpts& o) {
  for (auto& t : targets) {
    auto d = core::detect_file(t);
    auto r = core::inspect(d);
    core::print_pretty(r, o.verbose, !o.no_color);
  }
  return 0;
}

int run_strip(const vector<string>& targets, const core::Policy& policy, const StripOpts& o) {
  for (auto& t : targets) {
    auto out = util::derive_output_path(t, o.out_dir, o.in_place);
    auto d_before = core::detect_file(t);
    auto r_before = core::inspect(d_before);
    if (o.dry_run) {
      core::print_plan(r_before, policy);
      continue;
    }
    auto r_after  = core::strip_to(t, out, policy);
    core::print_summary(r_before, r_after, out);
  }
  return 0;
}

int run_explain(const string& target, const ExplainOpts& o) {
  auto d = core::detect_file(target);
  auto r = core::inspect(d);
  core::print_risks(r, o.verbose);
  return 0;
}

int run_policy(const string& action, const string& file) {
  (void)action; (void)file;
  fmt::print("Built-in policies: aggressive (default), safe. Use --safe or --keep/--drop.\n");
  return 0;
}
}
