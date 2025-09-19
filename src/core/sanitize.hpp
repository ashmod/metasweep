#pragma once
#include <string>
#include "detect.hpp"
#include "policy.hpp"

namespace core {
// strip_to dispatches by type; for unimplemented types, returns InspectResult of input with note
InspectResult strip_to(const std::string& in_path,
                       const std::string& out_path,
                       const Policy& policy);
}
