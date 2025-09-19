#include "pdf_info.hpp"
#include <fstream>
#include <string>
#include <string_view>
#include <vector>
#include <filesystem>

using namespace std::literals;
using core::Detected;
using core::InspectResult;
using core::FileType;
using core::Policy;
using core::risk_for;

namespace {

// Read whole file (small PDFs are fine for MVP)
static bool read_all(const std::string& path, std::string& out) {
  std::ifstream f(path, std::ios::binary);
  if (!f) return false;
  f.seekg(0, std::ios::end);
  std::streamoff len = f.tellg();
  if (len <= 0 || len > (1ll<<28)) return false; // 256MB guard
  out.resize(static_cast<size_t>(len));
  f.seekg(0, std::ios::beg);
  f.read(out.data(), len);
  return f.good() || f.eof();
}

// crude finder
static size_t find_last(const std::string& s, std::string_view needle) {
  return s.rfind(needle.data(), std::string::npos, needle.size());
}

static std::string trim_parens(const std::string& v) {
  if (v.size() >= 2 && v.front()=='(' && v.back()==')') {
    return v.substr(1, v.size()-2);
  }
  return v;
}

// Minimal /Info dict parse: extract common keys of form /Key (value)
static void parse_info_dict(std::string_view dict,
                            std::vector<core::Field>& out_fields,
                            size_t& meta_bytes) {
  auto scan_key = [&](std::string_view key, const char* canon){
    auto pos = dict.find(key);
    if (pos == std::string_view::npos) return;
    // skip key
    pos += key.size();
    // skip whitespace
    while (pos < dict.size() && isspace(static_cast<unsigned char>(dict[pos]))) ++pos;
    // expect literal string "(...)"
    if (pos < dict.size() && dict[pos]=='(') {
      size_t end = dict.find(')', pos+1);
      if (end != std::string_view::npos) {
        std::string val(dict.substr(pos, end-pos+1));
        std::string clean = trim_parens(val);
        meta_bytes += key.size() + val.size();
        out_fields.push_back({canon, clean, risk_for(canon), "PDF.Info", key.size()+val.size()});
      }
    }
  };

  scan_key("/Title"sv,         "PDF.Title");
  scan_key("/Author"sv,        "PDF.Author");
  scan_key("/Creator"sv,       "PDF.Creator");
  scan_key("/Producer"sv,      "PDF.Producer");
  scan_key("/CreationDate"sv,  "PDF.CreationDate");
  scan_key("/ModDate"sv,       "PDF.ModDate");
}

struct InfoLoc {
  bool present=false;
  int obj_num=-1;
  size_t obj_start=std::string::npos;
  size_t obj_end=std::string::npos;
  size_t trailer_info_pos=std::string::npos; // position of "/Info"
};

// Find trailer, /Info ref, and the referenced object span "N 0 obj ... endobj"
static InfoLoc locate_info(const std::string& pdf) {
  InfoLoc loc{};
  // quick check
  if (pdf.rfind("%PDF-", 0) != 0) return loc;

  // Search for last "trailer" then the dict following it
  size_t trailer_pos = find_last(pdf, "trailer");
  if (trailer_pos == std::string::npos) return loc;
  size_t dict_start = pdf.find("<<", trailer_pos);
  if (dict_start == std::string::npos) return loc;
  size_t dict_end = pdf.find(">>", dict_start);
  if (dict_end == std::string::npos) return loc;

  // Find /Info N 0 R inside trailer dict
  size_t info_pos = pdf.find("/Info", dict_start);
  if (info_pos == std::string::npos || info_pos > dict_end) return loc;

  size_t num_start = pdf.find_first_of("0123456789", info_pos+5);
  if (num_start == std::string::npos) return loc;
  size_t num_end = pdf.find_first_not_of("0123456789", num_start);
  if (num_end == std::string::npos) return loc;
  int objnum = std::stoi(pdf.substr(num_start, num_end-num_start));

  // Find "obj" header for that object
  std::string needle = std::to_string(objnum) + " 0 obj";
  size_t ostart = pdf.find(needle);
  if (ostart == std::string::npos) return loc;
  size_t ocontent = pdf.find("<<", ostart);
  if (ocontent == std::string::npos) return loc;
  size_t oend = pdf.find("endobj", ocontent);
  if (oend == std::string::npos) return loc;

  loc.present = true;
  loc.obj_num = objnum;
  loc.obj_start = ocontent;
  loc.obj_end = oend;
  loc.trailer_info_pos = info_pos;
  return loc;
}

} // anon

namespace backends {

bool pdf_can_handle(const Detected& d) { return d.type == FileType::PDF; }

core::InspectResult pdf_inspect(const Detected& d) {
  InspectResult ir; ir.file = d.path; ir.type = FileType::PDF;
  std::string buf;
  if (!read_all(d.path, buf)) return ir;

  // locate info object
  auto loc = locate_info(buf);
  if (!loc.present) return ir;

  ir.detected_blocks.push_back("Info");

  // Extract dictionary body between "<<" and ">>" inside Info object
  size_t dict_s = buf.find("<<", loc.obj_start);
  size_t dict_e = buf.find(">>", dict_s);
  if (dict_s != std::string::npos && dict_e != std::string::npos && dict_e > dict_s) {
    size_t meta_bytes = 0;
    parse_info_dict(std::string_view(buf.data()+dict_s, dict_e - dict_s + 2),
                    ir.fields, meta_bytes);
    ir.meta_bytes = meta_bytes;
  }
  return ir;
}

core::InspectResult pdf_strip_to(const std::string& in_path,
                                 const std::string& out_path,
                                 const Policy& p) {
  (void)p; // we strip all common /Info keys in MVP
  std::string buf;
  if (!read_all(in_path, buf)) {
    core::Detected dd{in_path, FileType::PDF, {}};
    return pdf_inspect(dd);
  }

  auto loc = locate_info(buf);
  if (!loc.present) {
    // nothing to strip; just copy through
    std::filesystem::copy_file(in_path, out_path,
      std::filesystem::copy_options::overwrite_existing);
    core::Detected d2{out_path, FileType::PDF, {}};
    return pdf_inspect(d2);
  }

  // Replace the Info object dictionary with empty dict <<>>
  size_t dict_s = buf.find("<<", loc.obj_start);
  size_t dict_e = buf.find(">>", dict_s);
  if (dict_s != std::string::npos && dict_e != std::string::npos && dict_e > dict_s) {
    std::string replacement = "<<>>";
    buf.replace(dict_s, (dict_e - dict_s + 2), replacement);
  }

  // (Optionally) also neutralize the trailer /Info reference → keep it pointing to the now-empty dict
  // For MVP we leave /Info N 0 R intact; safer for readers.

  // write out
  {
    std::filesystem::create_directories(std::filesystem::path(out_path).parent_path());
    std::ofstream o(out_path, std::ios::binary | std::ios::trunc);
    o.write(buf.data(), static_cast<std::streamsize>(buf.size()));
  }

  core::Detected d2{out_path, FileType::PDF, {}};
  return pdf_inspect(d2);
}

} // namespace backends
