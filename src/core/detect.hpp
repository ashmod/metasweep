#pragma once
#include <string>
#include <vector>
#include <cstddef>

namespace core {

enum class FileType { Unknown, Image, PDF, Audio, ZIP };

struct Block { std::string name; std::size_t size=0; };

struct Detected {
  std::string path;
  FileType type = FileType::Unknown;
  std::vector<Block> blocks; // filled by backends during inspect
};

Detected detect_file(const std::string& path);

struct Field {
  std::string canonical; // EXIF.GPSLatitude
  std::string value;     // "37.4219"
  std::string risk;      // HIGH|MEDIUM|LOW|SAFE
  std::string block;     // EXIF / XMP / IPTC
  std::size_t bytes=0;
};

struct InspectResult {
  std::string file;
  FileType type{FileType::Unknown};
  std::vector<std::string> detected_blocks; // ["EXIF","XMP"]
  std::vector<std::string> risk_tags;       // ["gps","device_model"]
  std::vector<Field> fields;
  std::size_t meta_bytes=0;
};

// High-level API
InspectResult inspect(const Detected& d);

}
