#pragma once
#include <string>
namespace util {
std::string derive_output_path(const std::string& in, const std::string& out_dir, bool in_place);
}
