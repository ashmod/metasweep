#include <CLI/CLI.hpp>
#include <fmt/format.h>
#include "cli/commands.hpp"
#include "core/policy.hpp"

int main(int argc, char** argv) {
  CLI::App app{"metasweep — privacy-friendly metadata inspector/stripper"};

  // Global flags (apply to subcommands via the opts we pass)
  bool no_color = false;
  app.add_flag("--no-color", no_color, "Disable colored output");

  // ----- inspect -----
  auto* inspect = app.add_subcommand("inspect", "Inspect metadata");
  std::vector<std::string> inspect_targets;
  cmd::InspectOpts inspect_opts; // has: recursive, format, report, verbose, no_color
  inspect->add_option("files", inspect_targets, "Files to inspect")->required();
  inspect->add_flag("-v,--verbose", inspect_opts.verbose, "Verbose field listing");
  inspect->add_flag("-r,--recursive", inspect_opts.recursive, "Recurse into directories");
  inspect->add_option("--report", inspect_opts.report, "Write JSON report to file");
  inspect->add_option("--format", inspect_opts.format, "Output format: auto|json|pretty");

  // ----- strip -----
  auto* strip = app.add_subcommand("strip", "Strip metadata");
  std::vector<std::string> strip_targets;
  cmd::StripOpts strip_opts; // has: recursive, out_dir, in_place, yes, format, report, dry_run, verbose, no_color
  bool safe_flag = false;
  std::string custom_policy;
  strip->add_option("files", strip_targets, "Files to strip")->required();
  strip->add_flag("--dry-run", strip_opts.dry_run, "Show plan without writing");
  strip->add_flag("--in-place", strip_opts.in_place, "Overwrite original files (no backup)");
  strip->add_option("-o,--out-dir", strip_opts.out_dir, "Output directory");
  strip->add_flag("-r,--recursive", strip_opts.recursive, "Recurse into directories");
  strip->add_flag("--yes", strip_opts.yes, "Skip confirmation prompts");
  strip->add_option("--report", strip_opts.report, "Write JSON report to file");
  strip->add_option("--format", strip_opts.format, "Output format: auto|json|pretty");
  strip->add_flag("--safe", safe_flag, "Use built-in safe policy");
  strip->add_option("--custom", custom_policy, "Policy file (YAML/JSON)");
  std::vector<std::string> keep_cli, drop_cli;
  strip->add_option("--keep", keep_cli, "Keep specific field(s) (repeatable)")->expected(-1);
  strip->add_option("--drop", drop_cli, "Drop specific field(s) (repeatable)")->expected(-1);

  // ----- explain -----
  auto* explain = app.add_subcommand("explain", "Explain risks for a file");
  std::string explain_target;
  cmd::ExplainOpts explain_opts; // has: verbose, no_color
  explain->add_option("file", explain_target, "File to explain")->required();
  explain->add_flag("-v,--verbose", explain_opts.verbose, "Verbose field listing");

  // Parse
  try { app.parse(argc, argv); }
  catch (const CLI::ParseError& e) { return app.exit(e); }

  // Apply global to subcommand opts
  inspect_opts.no_color = no_color;
  strip_opts.no_color   = no_color;
  explain_opts.no_color = no_color;

  // Dispatch
  if (inspect->parsed()) {
    return cmd::run_inspect(inspect_targets, inspect_opts);
  }
  if (strip->parsed()) {
    // Build policy from safe/custom and CLI keep/drop
    core::Policy pol = core::load_policy(safe_flag, custom_policy, keep_cli, drop_cli);
    return cmd::run_strip(strip_targets, pol, strip_opts);
  }
  if (explain->parsed()) {
    return cmd::run_explain(explain_target, explain_opts);
  }

  // No subcommand → help
  fmt::print("{}\n", app.help());
  return 0;
}
