#pragma once
#include <string>
#include "core/detect.hpp"
#include "core/policy.hpp"

namespace backends {
bool pdf_can_handle(const core::Detected& d);
core::InspectResult pdf_inspect(const core::Detected& d);
core::InspectResult pdf_strip_to(const std::string& in_path,
                                 const std::string& out_path,
                                 const core::Policy& p);
}
