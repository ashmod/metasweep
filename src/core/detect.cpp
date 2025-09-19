#include "detect.hpp"
#include <fstream>
#include <array>
#include <filesystem>

namespace core {
namespace {

bool starts_with(const std::vector<unsigned char>& b, const std::string& s) {
  if (b.size() < s.size()) return false;
  for (size_t i=0;i<s.size();++i) if (b[i] != static_cast<unsigned char>(s[i])) return false;
  return true;
}

} // anon

Detected detect_file(const std::string& path) {
  Detected d; d.path = path;
  std::ifstream f(path, std::ios::binary);
  if (!f) return d;

  std::vector<unsigned char> head(16);
  f.read(reinterpret_cast<char*>(head.data()), static_cast<std::streamsize>(head.size()));
  const auto got = static_cast<size_t>(f.gcount());
  head.resize(got);

  // JPEG
  if (got >= 3 && head[0]==0xFF && head[1]==0xD8 && head[2]==0xFF) { d.type = FileType::Image; return d; }
  // PNG
  if (got >= 8 && head[0]==0x89 && head[1]==0x50 && head[2]==0x4E && head[3]==0x47 &&
      head[4]==0x0D && head[5]==0x0A && head[6]==0x1A && head[7]==0x0A) { d.type = FileType::Image; return d; }
  // WEBP (RIFF .... WEBP)
  if (got >= 12 && head[0]=='R'&&head[1]=='I'&&head[2]=='F'&&head[3]=='F' &&
      head[8]=='W'&&head[9]=='E'&&head[10]=='B'&&head[11]=='P') { d.type = FileType::Image; return d; }
  // PDF
  if (starts_with(head, "%PDF-")) { d.type = FileType::PDF; return d; }
  // MP3 (ID3) or FLAC
  if (starts_with(head, "ID3") || starts_with(head, "fLaC")) { d.type = FileType::Audio; return d; }
  // ZIP
  if (got >= 4 && head[0]=='P' && head[1]=='K' && (head[2]==3||head[2]==5||head[2]==7) && (head[3]==4||head[3]==6||head[3]==8)) {
    d.type = FileType::ZIP; return d;
  }
  d.type = FileType::Unknown;
  return d;
}

} // namespace core
