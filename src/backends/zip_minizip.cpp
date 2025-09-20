#include "zip_minizip.hpp"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>

#include "../core/detect.hpp"
#include "../core/sanitize.hpp"
#include "../core/policy.hpp"

namespace fs = std::filesystem;

namespace {

// ---- ZIP constants (little-endian signatures)
static constexpr uint32_t SIG_EOCD  = 0x06054b50; // End of central dir
static constexpr uint32_t SIG_CEN   = 0x02014b50; // Central directory file header

// helper: read whole file
static bool read_file(const std::string& path, std::vector<unsigned char>& buf) {
  std::ifstream f(path, std::ios::binary);
  if (!f) return false;
  f.seekg(0, std::ios::end);
  std::streamoff len = f.tellg();
  if (len <= 0) { buf.clear(); return true; }
  buf.resize(static_cast<size_t>(len));
  f.seekg(0, std::ios::beg);
  f.read(reinterpret_cast<char*>(buf.data()), len);
  return f.good();
}

static inline uint16_t u16(const unsigned char* p) {
  return uint16_t(p[0]) | (uint16_t(p[1])<<8);
}
static inline uint32_t u32(const unsigned char* p) {
  return uint32_t(p[0]) | (uint32_t(p[1])<<8) | (uint32_t(p[2])<<16) | (uint32_t(p[3])<<24);
}

// find EOCD by scanning backwards up to 66KB (spec says comment<=65535)
struct EOCD {
  size_t  off = std::string::npos; // offset of EOCD
  uint16_t disk=0, cd_disk=0, disk_entries=0, total_entries=0;
  uint32_t cd_size=0, cd_offset=0, comment_len=0;
  bool ok=false;
};
static EOCD find_eocd(const std::vector<unsigned char>& b) {
  EOCD e{};
  size_t max_back = std::min<size_t>(b.size(), 0x10000 + 22); // EOCD min size is 22
  for (size_t i = 0; i < max_back; ++i) {
    size_t p = b.size() - 22 - i;
    if (p+22 > b.size()) continue;
    if (u32(&b[p]) == SIG_EOCD) {
      e.off = p;
      e.disk         = u16(&b[p+4]);
      e.cd_disk      = u16(&b[p+6]);
      e.disk_entries = u16(&b[p+8]);
      e.total_entries= u16(&b[p+10]);
      e.cd_size      = u32(&b[p+12]);
      e.cd_offset    = u32(&b[p+16]);
      e.comment_len  = u16(&b[p+20]);
      e.ok = true;
      return e;
    }
  }
  return e;
}

struct ZipAgg {
  uint32_t archive_comment=0;
  uint64_t sum_extra=0;
  uint64_t files_with_extra=0;
  uint64_t sum_file_comments=0;
  uint64_t files_with_comment=0;
};

// Walk central directory to count per-file extras/comments
static void scan_central_dir(const std::vector<unsigned char>& b, const EOCD& e, ZipAgg& z) {
  if (!e.ok) return;
  size_t p = e.cd_offset;
  size_t end = e.cd_offset + e.cd_size;
  if (end > b.size()) end = b.size();
  for (uint16_t i=0; i<e.total_entries && p+46 <= end; ++i) {
    if (u32(&b[p]) != SIG_CEN) break;
    // central dir fields we need
    uint16_t fname_len = u16(&b[p+28]);
    uint16_t extra_len = u16(&b[p+30]);
    uint16_t cmnt_len  = u16(&b[p+32]);

    if (extra_len) { z.sum_extra += extra_len; z.files_with_extra++; }
    if (cmnt_len)  { z.sum_file_comments += cmnt_len; z.files_with_comment++; }

    size_t advance = 46u + fname_len + extra_len + cmnt_len;
    if (p + advance > b.size()) break;
    p += advance;
  }
}

// strip: clear archive comment by patching EOCD and truncating file
static bool clear_archive_comment(const std::string& in, const std::string& out) {
  std::vector<unsigned char> b;
  if (!read_file(in, b)) return false;
  auto e = find_eocd(b);
  if (!e.ok) {
    // just copy if no EOCD
    std::ifstream src(in, std::ios::binary);
    std::ofstream dst(out, std::ios::binary|std::ios::trunc);
    dst << src.rdbuf();
    return true;
  }

  // write out copy first
  {
    std::ofstream dst(out, std::ios::binary|std::ios::trunc);
    dst.write(reinterpret_cast<const char*>(b.data()), b.size());
    if (!dst) return false;
  }

  if (e.comment_len == 0) return true; // nothing to do

  // patch comment length to 0 and truncate bytes after EOCD header
  try {
    // set 2 bytes at EOCD+20 to 0
    {
      std::fstream f(out, std::ios::in|std::ios::out|std::ios::binary);
      if (!f) return false;
      f.seekp(static_cast<std::streamoff>(e.off + 20), std::ios::beg);
      unsigned char zero[2]{0,0};
      f.write(reinterpret_cast<const char*>(zero), 2);
      f.flush();
    }
    // truncate file to EOCD start + 22 (minimum EOCD size)
    fs::resize_file(out, static_cast<uintmax_t>(e.off + 22));
    return true;
  } catch (...) {
    return false;
  }
}

} // anon

namespace backends {

bool zip_can_handle(const core::Detected& d) {
  return d.type == core::FileType::ZIP;
}

core::InspectResult zip_inspect(const core::Detected& d) {
  core::InspectResult ir; ir.file = d.path; ir.type = core::FileType::ZIP;

  std::vector<unsigned char> b;
  if (!read_file(d.path, b)) return ir;

  auto e = find_eocd(b);
  ZipAgg z{};
  if (e.ok) {
    z.archive_comment = e.comment_len;
    scan_central_dir(b, e, z);

    if (z.archive_comment > 0) {
      ir.fields.push_back({"ZIP.Comment", "<archive comment>", "LOW", "ZIP", (size_t)z.archive_comment});
      ir.meta_bytes += z.archive_comment;
    }
    if (z.files_with_extra > 0) {
      ir.fields.push_back({"ZIP.ExtraFields", std::to_string(z.files_with_extra) + " files", "LOW", "ZIP", (size_t)z.sum_extra});
      ir.meta_bytes += (size_t)z.sum_extra;
    }
    if (z.files_with_comment > 0) {
      ir.fields.push_back({"ZIP.FileComments", std::to_string(z.files_with_comment) + " files", "LOW", "ZIP", (size_t)z.sum_file_comments});
      ir.meta_bytes += (size_t)z.sum_file_comments;
    }
  }
  if (z.archive_comment==0 && z.files_with_extra==0 && z.files_with_comment==0) {
    // no metadata-y bits detected; still a valid zip
  }
  ir.detected_blocks.push_back("central-directory");
  return ir;
}

core::InspectResult zip_strip_to(const std::string& in_path,
                                 const std::string& out_path,
                                 const core::Policy& /*policy*/) {
  // MVP: clear only the archive comment (safe, lossless)
  // If that fails, fallback to a plain copy.
  if (!clear_archive_comment(in_path, out_path)) {
    std::ifstream src(in_path, std::ios::binary);
    std::ofstream dst(out_path, std::ios::binary|std::ios::trunc);
    dst << src.rdbuf();
  }
  core::Detected d2{out_path, core::FileType::ZIP, {}};
  return zip_inspect(d2);
}

} // namespace backends
