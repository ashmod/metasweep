#include "report.hpp"
#include "policy.hpp"
#include <fmt/format.h>

namespace core {

void print_pretty(const InspectResult& r, int, bool) {
  fmt::print("{} [{}]\n", r.file, (r.type==FileType::Image?"image": r.type==FileType::PDF?"pdf": r.type==FileType::Audio?"audio": r.type==FileType::ZIP?"zip":"unknown"));
  for (auto& f : r.fields) {
    fmt::print("  • {} = {} ({}) [{}]\n", f.canonical, f.value, f.risk, f.block);
  }
  if (r.fields.empty()) fmt::print("  (no metadata detected)\n");
}

void print_plan(const InspectResult& r, const Policy& p) {
  fmt::print("Plan for {} (policy: {}):\n", r.file, p.name);
  for (auto& f : r.fields) {
    bool keep = policy_keep(p, f.canonical);
    fmt::print("  {}  {}\n", keep? "KEEP":"DROP", f.canonical);
  }
}

void print_summary(const InspectResult& before, const InspectResult& after, const std::string& out_path) {
  fmt::print("Stripped {} → {}\n", before.file, out_path);
  fmt::print("  before fields: {} | after fields: {}\n", before.fields.size(), after.fields.size());
}

void print_risks(const InspectResult& r, int) {
  fmt::print("Risks for {}:\n", r.file);
  for (auto& f : r.fields) {
    if (f.risk=="HIGH" || f.risk=="MEDIUM") fmt::print("  {} ({})\n", f.canonical, f.risk);
  }
}

std::string to_json(const InspectResult&) { return "{}"; }
std::string to_html(const InspectResult&) { return "<!-- TODO -->"; }

}
