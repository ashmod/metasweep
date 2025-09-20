#include "image_exiv2.hpp"
#include <exiv2/exiv2.hpp>
#include <filesystem>
#include <cstdio>
#ifndef _WIN32
#include <unistd.h>
#endif

using namespace core;

namespace {
std::string canon_from_exif(const std::string& key) {
  if (key.rfind("Exif.GPSInfo.GPSLatitude",0)==0)  return "EXIF.GPSLatitude";
  if (key.rfind("Exif.GPSInfo.GPSLongitude",0)==0) return "EXIF.GPSLongitude";
  if (key=="Exif.Image.Orientation")               return "EXIF.Orientation";
  if (key=="Exif.Image.Make")                      return "EXIF.Make";
  if (key=="Exif.Image.Model")                     return "EXIF.Model";
  if (key=="Exif.Photo.BodySerialNumber" || key=="Exif.Image.BodySerialNumber") return "EXIF.SerialNumber";
  return "EXIF." + key;
}
std::string canon_from_xmp(const std::string& key) {
  if (key=="Xmp.xmp.CreatorTool") return "XMP.CreatorTool";
  if (key.rfind("Xmp.xmpMM.History",0)==0) return "XMP.History";
  return "XMP." + key;
}
std::string canon_from_iptc(const std::string& key) {
  return "IPTC." + key;
}
} // anon

namespace backends {

bool image_can_handle(const Detected& d) {
  return d.type == FileType::Image;
}

core::InspectResult image_inspect(const Detected& d) {
  core::InspectResult ir; ir.file = d.path; ir.type = FileType::Image;
  try {
    auto image = Exiv2::ImageFactory::open(d.path);
    image->readMetadata();

    const auto& exif = image->exifData();
    const auto& xmp  = image->xmpData();
    const auto& iptc = image->iptcData();

    size_t meta_bytes = 0;
    if (!exif.empty()) ir.detected_blocks.push_back("EXIF");
    if (!xmp.empty())  ir.detected_blocks.push_back("XMP");
    if (!iptc.empty()) ir.detected_blocks.push_back("IPTC");

    for (const auto& md : exif) {
      std::string c = canon_from_exif(md.key());
      std::string v = md.toString();
      size_t sz = v.size() + md.key().size();
      meta_bytes += sz;
      ir.fields.push_back({c, v, risk_for(c), "EXIF", sz});
    }
    for (const auto& md : xmp) {
      std::string c = canon_from_xmp(md.key());
      std::string v = md.toString();
      size_t sz = v.size() + md.key().size();
      meta_bytes += sz;
      ir.fields.push_back({c, v, risk_for(c), "XMP", sz});
    }
    for (const auto& md : iptc) {
      std::string c = canon_from_iptc(md.key());
      std::string v = md.toString();
      size_t sz = v.size() + md.key().size();
      meta_bytes += sz;
      ir.fields.push_back({c, v, risk_for(c), "IPTC", sz});
    }
    ir.meta_bytes = meta_bytes;

    for (auto& f : ir.fields) {
      if (f.canonical.rfind("EXIF.GPS",0)==0) ir.risk_tags.push_back("gps");
      else if (f.canonical=="EXIF.SerialNumber") ir.risk_tags.push_back("device_serial");
      else if (f.canonical=="XMP.CreatorTool") ir.risk_tags.push_back("software");
      else if (f.canonical=="EXIF.Model") ir.risk_tags.push_back("device_model");
    }
  } catch (...) {
    // leave empty if read fails
  }
  return ir;
}

core::InspectResult image_strip_to(const std::string& in_path,
                                   const std::string& out_path,
                                   const core::Policy& p) {
  namespace fs = std::filesystem;
  fs::path tmp = fs::path(out_path).parent_path() / (fs::path(out_path).filename().string()+".tmp");

  std::error_code ec;
  fs::create_directories(tmp.parent_path(), ec);
  fs::copy_file(in_path, tmp, fs::copy_options::overwrite_existing, ec);

  try {
    auto image = Exiv2::ImageFactory::open(tmp.string());
    image->readMetadata();

    auto& exif = image->exifData();
    auto& xmp  = image->xmpData();
    auto& iptc = image->iptcData();

    for (auto it = exif.begin(); it != exif.end(); ) {
      std::string c = canon_from_exif(it->key());
      bool keep = policy_keep(p, c);
      if (!keep) it = exif.erase(it); else ++it;
    }
    for (auto it = xmp.begin(); it != xmp.end(); ) {
      std::string c = canon_from_xmp(it->key());
      bool keep = policy_keep(p, c);
      if (!keep) it = xmp.erase(it); else ++it;
    }
    for (auto it = iptc.begin(); it != iptc.end(); ) {
      std::string c = canon_from_iptc(it->key());
      bool keep = policy_keep(p, c);
      if (!keep) it = iptc.erase(it); else ++it;
    }

    image->setExifData(exif);
    image->setXmpData(xmp);
    image->setIptcData(iptc);
    image->writeMetadata();

#ifndef _WIN32
    { FILE* f = std::fopen(tmp.string().c_str(), "rb");
      if (f) { int fd = fileno(f); if (fd!=-1) ::fsync(fd); std::fclose(f); } }
#endif
    fs::rename(tmp, out_path, ec);
  } catch (...) {
    std::error_code del_ec;
    std::filesystem::remove(tmp, del_ec);
    core::Detected d{in_path, core::FileType::Image, {}};
    return image_inspect(d);
  }

  core::Detected d2{out_path, core::FileType::Image, {}};
  return image_inspect(d2);
}

} // namespace backends
