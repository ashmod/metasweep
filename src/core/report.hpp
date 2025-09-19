#pragma once
#include <string>
#include "detect.hpp"
#include "policy.hpp"

namespace core {
void print_pretty(const InspectResult&, int verbose, bool color);
void print_plan(const InspectResult&, const Policy&);
void print_summary(const InspectResult&, const InspectResult&, const std::string& out_path);
void print_risks(const InspectResult&, int verbose);

// stubs for later JSON/HTML
std::string to_json(const InspectResult&);
std::string to_html(const InspectResult&);
}
