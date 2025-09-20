#include "audio_taglib.hpp"
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/mpegfile.h>
#include <taglib/flacfile.h>
#include <taglib/vorbisfile.h>

#include <fstream>
#include <string>

using core::Detected;
using core::InspectResult;
using core::FileType;
using core::Policy;

namespace {

// Helper: convert TagLib::String -> UTF-8 std::string
static inline std::string s8(const TagLib::String& s) {
  return s.to8Bit(true);
}

// Add a canonical field to InspectResult and return counted bytes
static std::size_t add_field(InspectResult& ir,
                             const std::string& canon,
                             const std::string& value,
                             const std::string& block,
                             const std::string& risk = "LOW") {
  if (value.empty()) return 0;
  std::size_t bytes = value.size();
  ir.fields.push_back({canon, value, risk, block, bytes});
  return bytes;
}

// Read generic tag fields (covers ID3v1/ID3v2 basic, Vorbis comments, etc.)
static void read_basic(TagLib::Tag* t, const char* block, InspectResult& ir) {
  if (!t) return;
  std::size_t meta = 0;
  meta += add_field(ir, "ID3.TIT2", s8(t->title()),  block);   // Title
  meta += add_field(ir, "ID3.TPE1", s8(t->artist()), block);   // Artist
  meta += add_field(ir, "ID3.TALB", s8(t->album()),  block);   // Album
  if (t->year() > 0) meta += add_field(ir, "ID3.TDRC", std::to_string((int)t->year()), block); // Year

  ir.meta_bytes += meta;
}

} // namespace

namespace backends {

bool audio_can_handle(const Detected& d) {
  return d.type == FileType::Audio;
}

InspectResult audio_inspect(const Detected& d) {
  InspectResult ir; ir.file = d.path; ir.type = FileType::Audio;

  // Prefer specific containers first for nicer detected block labels
  if (TagLib::MPEG::File mp3(d.path.c_str()); mp3.isValid()) {
    ir.detected_blocks.push_back("ID3");
    read_basic(mp3.tag(), "ID3", ir);
    return ir;
  }
  if (TagLib::FLAC::File flac(d.path.c_str()); flac.isValid()) {
    ir.detected_blocks.push_back("Vorbis");
    read_basic(flac.tag(), "Vorbis", ir);
    return ir;
  }
  if (TagLib::Ogg::Vorbis::File ogg(d.path.c_str()); ogg.isValid()) {
    ir.detected_blocks.push_back("Vorbis");
    read_basic(ogg.tag(), "Vorbis", ir);
    return ir;
  }

  // Fallback: whatever TagLib can parse
  if (TagLib::FileRef f(d.path.c_str()); !f.isNull()) {
    ir.detected_blocks.push_back("Tag");
    read_basic(f.tag(), "Tag", ir);
  }
  return ir;
}

InspectResult audio_strip_to(const std::string& in_path,
                             const std::string& out_path,
                             const Policy& /*p*/) {
  // Copy input -> output (simple; you can swap to atomic temp+rename later)
  {
    std::ifstream src(in_path, std::ios::binary);
    std::ofstream dst(out_path, std::ios::binary | std::ios::trunc);
    dst << src.rdbuf();
  }

  // Strip tags on the OUTPUT file in-place
  if (TagLib::MPEG::File mp3(out_path.c_str()); mp3.isValid()) {
    // Remove ID3v1 & ID3v2; second arg 'true' updates file immediately
    mp3.strip(TagLib::MPEG::File::ID3v1 | TagLib::MPEG::File::ID3v2, true);
    mp3.save();
  } else if (TagLib::FLAC::File flac(out_path.c_str()); flac.isValid()) {
    if (auto* t = flac.tag()) {
      t->setTitle({}); t->setArtist({}); t->setAlbum({}); t->setComment({}); t->setYear(0); t->setTrack(0);
    }
    flac.save();
  } else if (TagLib::Ogg::Vorbis::File ogg(out_path.c_str()); ogg.isValid()) {
    if (auto* t = ogg.tag()) {
      t->setTitle({}); t->setArtist({}); t->setAlbum({}); t->setComment({}); t->setYear(0); t->setTrack(0);
    }
    ogg.save();
  } else if (TagLib::FileRef f(out_path.c_str()); !f.isNull() && f.tag()) {
    auto* t = f.tag();
    t->setTitle({}); t->setArtist({}); t->setAlbum({}); t->setComment({}); t->setYear(0); t->setTrack(0);
    f.file()->save();
  }

  Detected d2{out_path, FileType::Audio, {}};
  return audio_inspect(d2);
}

} // namespace backends
