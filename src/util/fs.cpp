#include "fs.hpp"
#include <filesystem>
#include <string>
namespace fs = std::filesystem;
namespace util {
std::string derive_output_path(const std::string& in, const std::string& out_dir, bool in_place){
  if (in_place) return in;
  fs::path p(in);
  fs::path dir = out_dir.empty()? p.parent_path() : fs::path(out_dir);
  auto stem = p.stem().string();
  auto ext  = p.extension().string();
  return (dir / (stem + ".cleaned" + ext)).string();
}
}
