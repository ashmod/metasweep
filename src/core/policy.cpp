#include "policy.hpp"
#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace core {

static Policy builtin_aggressive() {
  Policy p; p.name = "aggressive";
  p.keep = { "EXIF.Orientation", "Image.ColorProfile", "Image.DPI" };
  p.drop = { "*" }; // default deny
  return p;
}

static Policy builtin_safe() {
  Policy p; p.name = "safe";
  p.keep = { "EXIF.Orientation", "Image.ColorProfile", "Image.DPI" };
  p.drop = {
    "EXIF.GPS*",
    "EXIF.SerialNumber",
    "XMP.CreatorTool",
    "XMP.History*",
    "PDF.Author","PDF.Creator","PDF.Producer","PDF.CreationDate","PDF.ModDate",
    "ID3.TPE1","ID3.TALB","ID3.TDRC",
    "ZIP.Comment"
  };
  return p;
}

Policy load_policy(bool safe_flag,
                   const std::string& custom_path,
                   const std::vector<std::string>& keep_cli,
                   const std::vector<std::string>& drop_cli) {
  Policy base = safe_flag ? builtin_safe() : builtin_aggressive();
  if (!custom_path.empty()) {
    // Placeholder: later parse YAML/JSON. For now, treat as safe fallback.
    base.name = "custom(UNPARSED)";
  }
  // Overlay CLI
  base.keep.insert(base.keep.end(), keep_cli.begin(), keep_cli.end());
  base.drop.insert(base.drop.end(), drop_cli.begin(), drop_cli.end());
  return base;
}

bool glob_match(const std::string& pat, const std::string& txt) {
  // Very small '*' only glob.
  size_t pi=0, ti=0, star=std::string::npos, match=0;
  while (ti < txt.size()) {
    if (pi < pat.size() && (pat[pi]==txt[ti] || pat[pi]=='?')) { ++pi; ++ti; }
    else if (pi < pat.size() && pat[pi]=='*') { star = pi++; match = ti; }
    else if (star != std::string::npos) { pi = star+1; ti = ++match; }
    else return false;
  }
  while (pi < pat.size() && pat[pi]=='*') ++pi;
  return pi == pat.size();
}

std::string risk_for(const std::string& f) {
  // Simplified mapping. Expand as needed.
  if (f.rfind("EXIF.GPS",0)==0 || f=="PDF.CreationDate" || f=="PDF.ModDate") return "HIGH";
  if (f=="EXIF.SerialNumber" || f=="EXIF.Make" || f=="EXIF.Model" || f=="ID3.TPE1") return "MEDIUM";
  if (f=="EXIF.Orientation" || f=="Image.ColorProfile" || f=="Image.DPI") return "SAFE";
  if (f.rfind("PDF.",0)==0) return "MEDIUM";
  if (f=="ID3.TPE1" || f=="ID3.TALB") return "MEDIUM";
  if (f=="ID3.TDRC") return "LOW";
  if (f=="ZIP.Comment") return "LOW";
  return "LOW";
}

bool policy_keep(const Policy& p, const std::string& canonical) {
  // If any keep matches -> keep
  for (auto& k : p.keep) if (glob_match(k, canonical)) return true;
  // If any drop matches -> drop
  for (auto& d : p.drop) if (glob_match(d, canonical)) return false;
  // Default deny unless whitelisted
  return false;
}

} // namespace core
