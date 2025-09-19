#include <CLI/CLI.hpp>
#include <fmt/format.h>
#include "cli/commands.hpp"
#include "core/detect.hpp"
#include "core/policy.hpp"

int main(int argc, char** argv) {
  CLI::App app{"privacy-friendly metadata inspector & cleaner (local-only)"};

  bool no_color = false;
  int vcount = 0;
  app.add_flag("--no-color", no_color, "disable ANSI colors");
  app.add_flag("-v,--verbose", vcount, "increase verbosity (repeatable)");

  // shared policy flags (subcommand-local via helper)
  bool safe=false; std::string custom_policy;
  auto add_policy_opts = [&](CLI::App* sub){
    sub->add_flag("--safe", safe, "use safe policy");
    sub->add_option("--custom", custom_policy, "policy file (YAML/JSON)");
  };

  // ===== inspect =====
  std::vector<std::string> inspect_targets;
  auto* inspect = app.add_subcommand("inspect", "show metadata summary (no changes)");
  bool recursive_inspect=false;
  std::string format_inspect="auto", report_inspect;
  inspect->add_option("targets", inspect_targets)->required();
  inspect->add_flag("-r,--recursive", recursive_inspect, "recurse into directories");
  inspect->add_option("--format", format_inspect, "output format: auto|json|pretty|html");
  inspect->add_option("--report", report_inspect, "write JSON/HTML report");
  add_policy_opts(inspect);

  // ===== strip =====
  std::vector<std::string> strip_targets;
  auto* strip = app.add_subcommand("strip", "remove metadata according to policy");
  bool recursive_strip=false, in_place=false, yes=false, dry_run=false;
  std::string out_dir, format_strip="auto", report_strip;
  strip->add_option("targets", strip_targets)->required();
  strip->add_flag("-r,--recursive", recursive_strip, "recurse into directories");
  strip->add_option("-o,--out-dir", out_dir, "write cleaned files to DIR");
  strip->add_flag("--in-place", in_place, "overwrite originals (prompt unless --yes)");
  strip->add_flag("--yes", yes, "skip confirmation prompts");
  strip->add_flag("--dry-run", dry_run, "show what would happen");
  strip->add_option("--format", format_strip, "output format: auto|json|pretty|html");
  strip->add_option("--report", report_strip, "write JSON/HTML report");
  add_policy_opts(strip);

  // ===== explain =====
  std::vector<std::string> explain_target;
  auto* explain = app.add_subcommand("explain", "describe risks & recommendations");
  explain->add_option("target", explain_target)->required()->expected(1);

  // parse
  try { app.parse(argc, argv); }
  catch(const CLI::ParseError& e) { return app.exit(e); }

  // dispatch
  if (*inspect) {
    cmd::InspectOpts io;
    io.recursive = recursive_inspect;
    io.format = format_inspect;
    io.report = report_inspect;
    io.verbose = vcount;
    io.no_color = no_color;
    return cmd::run_inspect(inspect_targets, io);
  }

  if (*strip) {
    core::Policy pol = core::load_policy(safe, custom_policy, {}, {});
    cmd::StripOpts so;
    so.recursive = recursive_strip;
    so.out_dir = out_dir;
    so.in_place = in_place;
    so.yes = yes;
    so.format = format_strip;
    so.report = report_strip;
    so.dry_run = dry_run;
    so.verbose = vcount;
    so.no_color = no_color;
    return cmd::run_strip(strip_targets, pol, so);
  }

  if (*explain) {
    cmd::ExplainOpts eo; eo.verbose = vcount; eo.no_color = no_color;
    return cmd::run_explain(explain_target.front(), eo);
  }

  fmt::print("{}\n", app.help());
  return 0;
}
