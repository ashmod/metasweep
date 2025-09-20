#include "report.hpp"
#include <fmt/format.h>
#include <algorithm>
#include <unordered_set>
#include <cctype>
#include <cstdlib>
#include <string>
#include <vector>
#include <fstream>

#ifdef __unix__
#include <sys/ioctl.h>
#include <unistd.h>
#endif

static const std::string H  = "─";
static const std::string V  = "│";
static const std::string TL = "┌";
static const std::string TM = "┬";
static const std::string TR = "┐";
static const std::string ML = "├";
static const std::string MM = "┼";
static const std::string MR = "┤";
static const std::string BL = "└";
static const std::string BM = "┴";
static const std::string BR = "┘";

// ANSI colors (zero-width in terminal; disabled if --no-color)
static inline const char* ansi_reset()  { return "\033[0m";  }
static inline const char* ansi_red()    { return "\033[31m"; }
static inline const char* ansi_yellow() { return "\033[33m"; }
static inline const char* ansi_green()  { return "\033[32m"; }
static inline const char* ansi_gray()   { return "\033[90m"; }

static const char* ansi_for_verdict(const std::string& v) {
  if (v == "HIGH")   return ansi_red();
  if (v == "MEDIUM") return ansi_yellow();
  if (v == "LOW")    return ansi_green();
  if (v == "NONE")   return ansi_gray();
  return ansi_reset();
}

static std::string colorize(bool enable, const std::string& s, const char* code) {
  if (!enable) return s;
  return std::string(code) + s + ansi_reset();
}


namespace core {

// ---------- small utils ----------
static std::string human_size(std::size_t b){
  const char* u[]={"B","KB","MB","GB"};
  double v=b; int i=0; while(v>=1024 && i<3){ v/=1024; ++i; }
  std::string fmtstr = i ? "{:.1f} {}" : "{} {}";
  return fmt::format(fmt::runtime(fmtstr), (i ? v : static_cast<double>(b)), u[i]);
}
static std::string base_name(const std::string& path){
  auto pos = path.find_last_of("/\\");
  return (pos == std::string::npos) ? path : path.substr(pos+1);
}
static std::string join_csv(const std::vector<std::string>& v){
  if (v.empty()) return "-";
  std::string s; for (size_t i=0;i<v.size();++i){ if(i) s += ", "; s += v[i]; }
  return s;
}
static std::string repeat_str(const std::string& s, int n){
  if (n <= 0) return "";
  std::string out; out.reserve(s.size() * n);
  for (int i=0;i<n;++i) out += s;
  return out;
}

static std::string fit_left(const std::string& s, int width){
  if (width <= 0) return "";
  if ((int)s.size() <= width) return s + std::string(width - (int)s.size(), ' ');
  if (width <= 3) return s.substr(0, width);
  return s.substr(0, width - 3) + "...";
}

static std::string fit_right(const std::string& s, int width){
  if (width <= 0) return "";
  if ((int)s.size() <= width) return std::string(width - (int)s.size(), ' ') + s;
  if (width <= 3) return s.substr((int)s.size() - width);
  return "..." + s.substr((int)s.size() - (width - 3));
}


static int term_columns() {
  // Try ioctl (only if stdout is a TTY), else $COLUMNS, else 100.
#ifdef __unix__
  if (isatty(STDOUT_FILENO)) {
    struct winsize w{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 20) return w.ws_col;
  }
#endif
  if (const char* c = std::getenv("COLUMNS")) {
    int v = std::atoi(c);
    if (v > 20) return v;
  }
  return 100;
}

// ---------- risk aggregation ----------
struct RiskAgg { std::string verdict; std::vector<std::string> tags; };
static RiskAgg aggregate(const InspectResult& r){
  bool any_high=false, any_med=false, any_low=false;
  std::unordered_set<std::string> tags;
  for (auto& f : r.fields) {
    if (f.risk=="HIGH") any_high=true;
    else if (f.risk=="MEDIUM") any_med=true;
    else if (f.risk=="LOW") any_low=true;

    const auto& c = f.canonical;
    if (c.rfind("EXIF.GPS",0)==0) tags.insert("GPS");
    else if (c=="EXIF.Model" || c=="EXIF.Make") tags.insert("Device");
    else if (c=="XMP.CreatorTool") tags.insert("Software");
    else if (c=="PDF.Author") tags.insert("Author");
    else if (c=="PDF.Creator" || c=="PDF.Producer") tags.insert("Producer");
    else if (c=="PDF.CreationDate" || c=="PDF.ModDate") tags.insert("Timestamps");
    else if (c=="ID3.TPE1") tags.insert("Artist");
    else if (c=="ID3.TDRC") tags.insert("Year");
    else if (c=="ZIP.Comment") tags.insert("Comment");
  }
  RiskAgg ra;
  if (any_high) ra.verdict="HIGH";
  else if (any_med) ra.verdict="MEDIUM";
  else if (any_low) ra.verdict="LOW";
  else ra.verdict="NONE";
  ra.tags.assign(tags.begin(), tags.end());
  std::sort(ra.tags.begin(), ra.tags.end());
  return ra;
}
static std::string ftype(FileType t){
  switch(t){
    case FileType::Image: return "image";
    case FileType::PDF:   return "pdf";
    case FileType::Audio: return "audio";
    case FileType::ZIP:   return "zip";
    default:              return "unknown";
  }
}

// ---------- dynamic column layout ----------
struct Row {
  std::string file, type, size, risk, verd;
};
struct ColSpec {
  int natural=0; // longest content length
  int width=0;   // final width after shrinking/expanding
  int minw=0;    // minimum allowed width
  int maxw=1000; // cap to avoid silly widths
  bool right=false; // right-align (size column)
};

static void print_border(const ColSpec& A,const ColSpec& B,const ColSpec& C,const ColSpec& D,const ColSpec& E,
                         const std::string& left, const std::string& mid, const std::string& right) {
  std::string line;
  line.reserve(200);
  line += left;
  line += repeat_str(H, A.width+2); line += mid;
  line += repeat_str(H, B.width+2); line += mid;
  line += repeat_str(H, C.width+2); line += mid;
  line += repeat_str(H, D.width+2); line += mid;
  line += repeat_str(H, E.width+2); line += right;
  line += "\n";
  fmt::print("{}", line);
}

void print_inspection_batch(const std::vector<InspectResult>& results, int verbose, bool enable_color) {
  // Build rows from data
  std::vector<Row> rows; rows.reserve(results.size());
  for (auto& r : results) {
    auto ra = aggregate(r);
    rows.push_back({
      base_name(r.file),
      ftype(r.type),
      human_size(r.meta_bytes),
      join_csv(ra.tags),
      ra.verdict
    });
  }

  // Natural widths from header + content
  ColSpec A,B,C,D,E; // File, Type, Size, Risk, Verdict
  auto upd = [](int& cur, const std::string& s){ cur = std::max(cur, (int)s.size()); };
  upd(A.natural, "File");
  upd(B.natural, "Type");
  upd(C.natural, "Meta size");
  upd(D.natural, "Risk");
  upd(E.natural, "Verdict");
  for (auto& r : rows) {
    upd(A.natural, r.file);
    upd(B.natural, r.type);
    upd(C.natural, r.size);
    upd(D.natural, r.risk);
    upd(E.natural, r.verd);
  }

  // Set min/max and alignment
  A.minw = 8;   A.maxw = 64;
  B.minw = 4;   B.maxw = 12;
  C.minw = 6;   C.maxw = 12; C.right = true; // right-align size
  D.minw = 6;   D.maxw = 48;
  E.minw = 4;   E.maxw = 10;

  A.width = std::clamp(A.natural, A.minw, A.maxw);
  B.width = std::clamp(B.natural, B.minw, B.maxw);
  C.width = std::clamp(C.natural, C.minw, C.maxw);
  D.width = std::clamp(D.natural, D.minw, D.maxw);
  E.width = std::clamp(E.natural, E.minw, E.maxw);

  // Compute total width with borders & padding
  auto total_width = [&](const ColSpec& a,const ColSpec& b,const ColSpec& c,const ColSpec& d,const ColSpec& e){
    return 6 /*seps*/ + (a.width+2) + (b.width+2) + (c.width+2) + (d.width+2) + (e.width+2);
  };

  int tw = term_columns();
  int cur = total_width(A,B,C,D,E);

  // If too wide, shrink columns in priority: Risk → File → Size → Type → Verdict
  auto shrink = [&](ColSpec& col, int need){
    int can = col.width - col.minw;
    int take = std::min(can, need);
    col.width -= take;
    return take;
  };
  if (cur > tw) {
    int need = cur - tw;
    need -= shrink(D, need); if (need<=0) goto sized;
    need -= shrink(A, need); if (need<=0) goto sized;
    need -= shrink(C, need); if (need<=0) goto sized;
    need -= shrink(B, need); if (need<=0) goto sized;
    need -= shrink(E, need); if (need<=0) goto sized;
  }
sized:

  // Draw table
  fmt::print("▶ Inspecting: {} file{}\n", results.size(), results.size()==1?"":"s");

  print_border(A,B,C,D,E, TL, TM, TR);
  fmt::print("│ {} │ {} │ {} │ {} │ {} │\n",
    fit_left("File",    A.width),
    fit_left("Type",    B.width),
    fit_left("Meta size", C.width),
    fit_left("Risk",    D.width),
    fit_left("Verdict", E.width)
  );
  print_border(A,B,C,D,E, ML, MM, MR);

  for (auto& r : rows) {
    auto file_cell = fit_left(r.file,  A.width);
    auto type_cell = fit_left(r.type,  B.width);
    auto size_cell = (C.right ? fit_right(r.size, C.width) : fit_left(r.size, C.width));
    auto risk_cell = fit_left(r.risk,  D.width);
    auto verd_cell = fit_left(r.verd,  E.width);
    verd_cell = colorize(enable_color, verd_cell, ansi_for_verdict(r.verd));

    fmt::print("│ {} │ {} │ {} │ {} │ {} │\n",
      file_cell, type_cell, size_cell, risk_cell, verd_cell
    );
  }
  print_border(A,B,C,D,E, BL, BM, BR);
  fmt::print("\n");

  // Verbose per-file details
  if (verbose > 0) {
    for (size_t i=0;i<results.size();++i) {
      const auto& r = results[i];
      fmt::print("{}\n", r.file);
      for (auto& f : r.fields) {
        fmt::print("  • {} = {} ({}) [{}]\n", f.canonical, f.value, f.risk, f.block);
      }
      auto ra = aggregate(r);
      if (r.type==FileType::Image && (ra.verdict=="HIGH" || ra.verdict=="MEDIUM"))
        fmt::print("Suggestion: `metasweep strip {} --safe`\n\n", r.file);
      else if (r.type==FileType::PDF && !r.fields.empty())
        fmt::print("Suggestion: `metasweep strip {}`\n\n", r.file);
      else
        fmt::print("\n");
    }
  }
}


void print_plan(const InspectResult& r, const Policy& p) {
  fmt::print("Plan for {} (policy: {}):\n", r.file, p.name);
  for (auto& f : r.fields) {
    bool keep = policy_keep(p, f.canonical);
    fmt::print("  {}  {}\n", keep? "KEEP":"DROP", f.canonical);
  }
}

void print_summary(const InspectResult& before, const InspectResult& after, const std::string& out_path) {
  fmt::print("Stripped {} → {}\n", before.file, out_path);
  fmt::print("  before fields: {} | after fields: {}\n", before.fields.size(), after.fields.size());
}

void print_risks(const InspectResult& r, int /*verbose*/) {
  auto ra = aggregate(r);
  std::string tags = join_csv(ra.tags);
  auto vcol = colorize(true, ra.verdict, ansi_for_verdict(ra.verdict));
  fmt::print("Risks for {}: {} [{}]\n", r.file, vcol, tags);
}

static std::string json_escape(const std::string& s){
  std::string o; o.reserve(s.size()+8);
  for(char c: s){
    switch(c){
      case '\\': o += "\\\\"; break;
      case '\"': o += "\\\""; break;
      case '\n': o += "\\n"; break;
      case '\r': o += "\\r"; break;
      case '\t': o += "\\t"; break;
      default: o += (unsigned char)c < 0x20 ? '?' : c; break;
    }
  }
  return o;
}

void write_json_report(const std::vector<InspectResult>& results, const std::string& path){
  std::ofstream f(path, std::ios::binary|std::ios::trunc);
  f << "{\n  \"files\": [\n";
  for (size_t i=0;i<results.size();++i){
    const auto& r = results[i];
    f << "    {\n";
    f << "      \"file\": \"" << json_escape(r.file) << "\",\n";
    f << "      \"type\": \"" << (r.type==FileType::Image?"image":r.type==FileType::PDF?"pdf":r.type==FileType::Audio?"audio":r.type==FileType::ZIP?"zip":"unknown") << "\",\n";
    f << "      \"detected\": [";
    for (size_t j=0;j<r.detected_blocks.size();++j) {
      if (j) f << ", ";
      f << "\"" << json_escape(r.detected_blocks[j]) << "\"";
    }
    f << "],\n";
    f << "      \"meta_bytes\": " << r.meta_bytes << ",\n";
    f << "      \"fields\": [\n";
    for (size_t k=0;k<r.fields.size();++k) {
      const auto& fld = r.fields[k];
      f << "        {\"name\":\"" << json_escape(fld.canonical) << "\","
        << "\"value\":\"" << json_escape(fld.value) << "\","
        << "\"risk\":\"" << json_escape(fld.risk) << "\","
        << "\"block\":\"" << json_escape(fld.block) << "\","
        << "\"bytes\":" << fld.bytes << "}";
      if (k+1<r.fields.size()) f << ",";
      f << "\n";
    }
    f << "      ]\n";
    f << "    }";
    if (i+1<results.size()) f << ",";
    f << "\n";
  }
  f << "  ]\n}\n";
}


// simple placeholders
std::string to_json(const InspectResult&) { return "{}"; }
std::string to_html(const InspectResult&) { return "<!-- TODO -->"; }

} // namespace core
