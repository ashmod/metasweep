#pragma once
#include <string>
#include "core/detect.hpp"
#include "core/policy.hpp"

namespace backends {
bool audio_can_handle(const core::Detected& d);
core::InspectResult audio_inspect(const core::Detected& d);
core::InspectResult audio_strip_to(const std::string& in_path,
                                   const std::string& out_path,
                                   const core::Policy& p);
}
