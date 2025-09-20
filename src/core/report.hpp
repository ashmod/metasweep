#pragma once
#include <string>
#include <ostream>
#include <vector>
#include "detect.hpp"
#include "policy.hpp"

namespace core {

// Pretty batch table like the sample, then per-file details if verbose > 0
void print_inspection_batch(const std::vector<InspectResult>& results, int verbose, bool color);

// Dry-run plan
void print_plan(const InspectResult&, const Policy&);

// Strip summary
void print_summary(const InspectResult& before, const InspectResult& after, const std::string& out_path);

// Risks for a single file (used by `explain`)
void print_risks(const InspectResult&, int verbose);

// JSON report helpers
void write_json_report(const std::vector<InspectResult>& results, const std::string& path);
void write_json_report_stream(std::ostream& os, const std::vector<InspectResult>& results);

// (stubs for later)
std::string to_json(const InspectResult&);
std::string to_html(const InspectResult&);

}
