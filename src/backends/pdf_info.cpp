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

// --- IO ---
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

static inline bool is_space(char c){
  unsigned char u = static_cast<unsigned char>(c);
  return u==' ' || u=='\t' || u=='\r' || u=='\n' || u=='\f' || u=='\v';
}

static std::string trim_parens(std::string_view v) {
  if (v.size()>=2 && v.front()=='(' && v.back()==')') {
    return std::string(v.substr(1, v.size()-2));
  }
  return std::string(v);
}

// Extract value “( … )” starting at pos (which points to '('). Very simple,
// handles nested escaped parens minimally.
static std::pair<size_t,size_t> paren_span(const std::string& s, size_t pos){
  if (pos>=s.size() || s[pos]!='(') return {std::string::npos,std::string::npos};
  int depth = 0; size_t i = pos;
  for (; i<s.size(); ++i) {
    char c = s[i];
    if (c=='\\') { ++i; continue; } // skip escaped char
    if (c=='(') ++depth;
    else if (c==')') { --depth; if (depth==0) { return {pos, i}; }
    }
  }
  return {std::string::npos,std::string::npos};
}

// --- Primary strategy: trailer → /Info N 0 R → object span ---
struct InfoLoc {
  bool present=false;
  int obj_num=-1;
  size_t obj_start=std::string::npos;
  size_t obj_end=std::string::npos;
};

static size_t rfind_simple(const std::string& s, std::string_view needle) {
  return s.rfind(needle.data());
}

static InfoLoc locate_info_via_trailer(const std::string& pdf) {
  InfoLoc loc{};
  if (pdf.rfind("%PDF-", 0) != 0) return loc;

  // find the LAST "trailer"
  size_t trailer_pos = rfind_simple(pdf, "trailer");
  if (trailer_pos == std::string::npos) return loc;
  size_t dict_start = pdf.find("<<", trailer_pos);
  if (dict_start == std::string::npos) return loc;
  size_t dict_end = pdf.find(">>", dict_start);
  if (dict_end == std::string::npos) return loc;

  // /Info N 0 R
  size_t info_pos = pdf.find("/Info", dict_start);
  if (info_pos == std::string::npos || info_pos > dict_end) return loc;

  size_t num_start = pdf.find_first_of("0123456789", info_pos+5);
  if (num_start == std::string::npos) return loc;
  size_t num_end = pdf.find_first_not_of("0123456789", num_start);
  if (num_end == std::string::npos) return loc;
  int objnum = std::stoi(pdf.substr(num_start, num_end-num_start));

  // find "obj" for that number
  std::string needle = std::to_string(objnum) + " 0 obj";
  size_t ostart = pdf.find(needle);
  if (ostart == std::string::npos) return loc;
  size_t dict_s = pdf.find("<<", ostart);
  if (dict_s == std::string::npos) return loc;
  size_t oend = pdf.find("endobj", dict_s);
  if (oend == std::string::npos) return loc;

  loc.present = true;
  loc.obj_num = objnum;
  loc.obj_start = dict_s;
  loc.obj_end = oend;
  return loc;
}

// --- Fallback: scan for any object with Info-like keys in a dict ---
// returns dict spans [dict_s, dict_e] inside "N 0 obj ... endobj"
struct DictSpan { size_t dict_s, dict_e; };
static bool looks_info_dict(std::string_view dict) {
  return (dict.find("/Title")!=std::string_view::npos)
      || (dict.find("/Author")!=std::string_view::npos)
      || (dict.find("/Creator")!=std::string_view::npos)
      || (dict.find("/Producer")!=std::string_view::npos)
      || (dict.find("/CreationDate")!=std::string_view::npos)
      || (dict.find("/ModDate")!=std::string_view::npos);
}

static DictSpan find_first_info_like_object(const std::string& pdf){
  size_t pos = 0;
  while (true) {
    // find next "obj"
    size_t obj = pdf.find(" obj", pos);
    if (obj == std::string::npos) break;

    // backtrack to beginning of the line to include object header
    size_t line_start = pdf.rfind('\n', obj);
    size_t hdr_start = (line_start==std::string::npos? 0 : line_start+1);
    // quick sanity: header looks like "N 0 obj"
    // not strictly required here

    // find dict inside this object
    size_t dict_s = pdf.find("<<", obj);
    if (dict_s == std::string::npos) break;
    size_t dict_e = pdf.find(">>", dict_s);
    if (dict_e == std::string::npos) break;

    std::string_view dv(pdf.data()+dict_s, dict_e - dict_s + 2);
    if (looks_info_dict(dv)) return {dict_s, dict_e};

    // move past endobj
    size_t endobj = pdf.find("endobj", dict_e);
    if (endobj == std::string::npos) break;
    pos = endobj + 6;
  }
  return {std::string::npos, std::string::npos};
}

// --- Parse key/value from a dict span ---
static void parse_info_dict(std::string_view dict,
                            std::vector<core::Field>& out_fields,
                            size_t& meta_bytes) {
  auto grab = [&](std::string_view key, const char* canon){
    size_t pos = dict.find(key);
    if (pos == std::string::npos) return;
    pos += key.size();
    while (pos < dict.size() && is_space(dict[pos])) ++pos;
    if (pos >= dict.size() || dict[pos] != '(') return;
    size_t global_pos = pos; // relative to dict start
    int depth = 0; size_t i = pos;
    for (; i<dict.size(); ++i) {
      char c = dict[i];
      if (c=='\\') { ++i; continue; }
      if (c=='(') ++depth;
      else if (c==')') { --depth; if (depth==0) break; }
    }
    if (i<dict.size()) {
      std::string val(dict.substr(pos, i-pos+1)); // include parens
      std::string clean = trim_parens(val);
      meta_bytes += key.size() + val.size();
      out_fields.push_back({canon, clean, risk_for(canon), "PDF.Info", key.size()+val.size()});
    }
  };

  grab("/Title"sv,        "PDF.Title");
  grab("/Author"sv,       "PDF.Author");
  grab("/Creator"sv,      "PDF.Creator");
  grab("/Producer"sv,     "PDF.Producer");
  grab("/CreationDate"sv, "PDF.CreationDate");
  grab("/ModDate"sv,      "PDF.ModDate");
}

// Replace the value for a key within [dict_s, dict_e] with empty "()"
static void clear_key_inplace(std::string& buf, size_t dict_s, size_t dict_e, std::string_view key){
  size_t pos = buf.find(key.data(), dict_s);
  if (pos == std::string::npos || pos > dict_e) return;
  pos += key.size();
  while (pos < dict_e && is_space(buf[pos])) ++pos;
  if (pos >= dict_e || buf[pos] != '(') return;
  auto [start,end] = paren_span(buf, pos);
  if (start==std::string::npos || end==std::string::npos || end>dict_e) return;
  // replace "(...)" with "()"
  buf.replace(start, (end-start+1), "()");
}

} // anon

namespace backends {

bool pdf_can_handle(const Detected& d) { return d.type == FileType::PDF; }

core::InspectResult pdf_inspect(const Detected& d) {
  InspectResult ir; ir.file = d.path; ir.type = FileType::PDF;
  std::string buf;
  if (!read_all(d.path, buf)) return ir;

  // Strategy A: via trailer
  auto loc = locate_info_via_trailer(buf);
  size_t dict_s = std::string::npos, dict_e = std::string::npos;

  if (loc.present) {
    dict_s = buf.find("<<", loc.obj_start);
    dict_e = (dict_s==std::string::npos)? std::string::npos : buf.find(">>", dict_s);
  }

  // Strategy B: fallback scan
  if (dict_s == std::string::npos || dict_e == std::string::npos || dict_e <= dict_s) {
    auto ds = find_first_info_like_object(buf);
    dict_s = ds.dict_s; dict_e = ds.dict_e;
  }

  if (dict_s == std::string::npos || dict_e == std::string::npos || dict_e <= dict_s)
    return ir; // no info-like dict found

  ir.detected_blocks.push_back("Info");
  size_t meta_bytes = 0;
  parse_info_dict(std::string_view(buf.data()+dict_s, dict_e - dict_s + 2),
                  ir.fields, meta_bytes);
  ir.meta_bytes = meta_bytes;
  return ir;
}

core::InspectResult pdf_strip_to(const std::string& in_path,
                                 const Policy& /*p*/, // MVP: clear all common keys
                                 size_t dict_s, size_t dict_e,
                                 std::string& buf) {
  // List of keys to clear
  static constexpr std::string_view keys[] = {
    "/Title","/Author","/Creator","/Producer","/CreationDate","/ModDate"
  };
  for (auto k : keys) clear_key_inplace(buf, dict_s, dict_e, k);
  // Done. Caller will write out.
  Detected d2{in_path, FileType::PDF, {}};
  return pdf_inspect(d2);
}

core::InspectResult pdf_strip_to(const std::string& in_path,
                                 const std::string& out_path,
                                 const Policy& p) {
  (void)p; // MVP: strip the common /Info keys unconditionally
  std::string buf;
  if (!read_all(in_path, buf)) {
    Detected d{in_path, FileType::PDF, {}}; return pdf_inspect(d);
  }

  // Find dict span via trailer or fallback
  auto loc = locate_info_via_trailer(buf);
  size_t dict_s = std::string::npos, dict_e = std::string::npos;
  if (loc.present) {
    dict_s = buf.find("<<", loc.obj_start);
    dict_e = (dict_s==std::string::npos)? std::string::npos : buf.find(">>", dict_s);
  }
  if (dict_s == std::string::npos || dict_e == std::string::npos || dict_e <= dict_s) {
    auto ds = find_first_info_like_object(buf);
    dict_s = ds.dict_s; dict_e = ds.dict_e;
  }

  if (dict_s != std::string::npos && dict_e != std::string::npos && dict_e > dict_s) {
    // Clear common keys in-place
    static constexpr std::string_view keys[] = {
      "/Title","/Author","/Creator","/Producer","/CreationDate","/ModDate"
    };
    for (auto k : keys) clear_key_inplace(buf, dict_s, dict_e, k);
  }
  // Write output
  std::filesystem::create_directories(std::filesystem::path(out_path).parent_path());
  std::ofstream o(out_path, std::ios::binary|std::ios::trunc);
  o.write(buf.data(), static_cast<std::streamsize>(buf.size()));

  Detected d2{out_path, FileType::PDF, {}}; return pdf_inspect(d2);
}

} // namespace backends
