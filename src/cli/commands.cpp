#include "commands.hpp"
#include <fmt/format.h>
#include <filesystem>
#include <iostream>
#include "core/detect.hpp"
#include "core/report.hpp"
#include "core/sanitize.hpp"
#include "core/policy.hpp"
#include "util/fs.hpp"

using namespace std;

namespace cmd {
namespace {
  bool has_wildcards(const std::string& s){
    return s.find('*') != std::string::npos || s.find('?') != std::string::npos;
  }
  void expand_pattern_into(const std::filesystem::path& base_dir,
                           const std::string& pattern,
                           bool recursive,
                           std::vector<std::string>& out) {
    namespace fs = std::filesystem;
    auto match = [&](const std::string& name){ return core::glob_match(pattern, name); };
    if (recursive) {
      for (auto it = fs::recursive_directory_iterator(base_dir, fs::directory_options::skip_permission_denied);
           it != fs::recursive_directory_iterator(); ++it) {
        if (!it->is_regular_file()) continue;
        auto name = it->path().filename().string();
        if (match(name)) out.push_back(it->path().string());
      }
    } else {
      if (!fs::exists(base_dir)) return;
      for (auto& e : fs::directory_iterator(base_dir, fs::directory_options::skip_permission_denied)) {
        if (!e.is_regular_file()) continue;
        auto name = e.path().filename().string();
        if (match(name)) out.push_back(e.path().string());
      }
    }
  }
  std::vector<std::string> collect_files(const std::vector<std::string>& targets, bool recursive){
    namespace fs = std::filesystem;
    std::vector<std::string> files;
    for (const auto& t : targets) {
      fs::path p(t);
      std::error_code ec;
      if (fs::exists(p, ec)) {
        if (fs::is_regular_file(p, ec)) { files.push_back(p.string()); continue; }
        if (fs::is_directory(p, ec)) {
          if (recursive) {
            for (auto it = fs::recursive_directory_iterator(p, fs::directory_options::skip_permission_denied);
                 it != fs::recursive_directory_iterator(); ++it) {
              if (it->is_regular_file()) files.push_back(it->path().string());
            }
          } else {
            for (auto& e : fs::directory_iterator(p, fs::directory_options::skip_permission_denied)) {
              if (e.is_regular_file()) files.push_back(e.path().string());
            }
          }
          continue;
        }
      }
      if (has_wildcards(t)) {
        auto slash = t.find_last_of("/\\");
        fs::path dir = (slash==std::string::npos) ? fs::path(".") : fs::path(t.substr(0, slash));
        std::string pat = (slash==std::string::npos) ? t : t.substr(slash+1);
        expand_pattern_into(dir, pat, recursive, files);
      }
    }
    return files;
  }
}
int run_inspect(const std::vector<std::string>& targets, const InspectOpts& o) {
  auto files = collect_files(targets, o.recursive);
  if (files.empty()) { fmt::print("No files matched.\n"); return 1; }
  std::vector<core::InspectResult> all;
  all.reserve(files.size());
  for (auto& f : files) {
    auto info = core::detect_file(f);
    auto r = core::inspect(info);
    all.push_back(std::move(r));
  }
  if (o.format == std::string("json")) {
    core::write_json_report_stream(std::cout, all);
    std::cout << std::endl;
  } else {
    core::print_inspection_batch(all, o.verbose, !o.no_color);
  }
  if (!o.report.empty()) {
    core::write_json_report(all, o.report);
    fmt::print("Wrote report: {}\n", o.report);
  }
  return 0;
}


int run_strip(const vector<string>& targets, const core::Policy& policy, const StripOpts& o) {
  auto files = collect_files(targets, o.recursive);
  if (files.empty()) { fmt::print("No files matched.\n"); return 1; }
  if (o.in_place && !o.yes) {
    fmt::print("About to overwrite {} file(s) in-place. Type 'yes' to continue: ", files.size());
    std::string line; std::getline(std::cin, line);
    if (!(line == "yes" || line == "y")) {
      fmt::print("Aborted.\n");
      return 1;
    }
  }
  for (auto& f : files) {
    auto out = util::derive_output_path(f, o.out_dir, o.in_place);
    auto d_before = core::detect_file(f);
    auto r_before = core::inspect(d_before);
    if (o.dry_run) {
      core::print_plan(r_before, policy);
      continue;
    }
    auto r_after  = core::strip_to(f, out, policy);
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
