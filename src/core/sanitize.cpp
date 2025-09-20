#include "sanitize.hpp"
#ifdef HAVE_EXIV2
#include "../backends/image_exiv2.hpp"
#endif
#include "../backends/pdf_info.hpp"
#ifdef HAVE_TAGLIB
#include "../backends/audio_taglib.hpp"
#endif
#include "../backends/zip_minizip.hpp"

namespace core {

InspectResult inspect(const Detected& d) {
#ifdef HAVE_EXIV2
  if (backends::image_can_handle(d)) return backends::image_inspect(d);
#endif
  if (backends::pdf_can_handle(d))   return backends::pdf_inspect(d);
#ifdef HAVE_TAGLIB
  if (backends::audio_can_handle(d)) return backends::audio_inspect(d);
#endif
  if (backends::zip_can_handle(d))   return backends::zip_inspect(d);

  InspectResult ir; ir.file=d.path; ir.type=d.type; return ir;
}

InspectResult strip_to(const std::string& in_path,
                       const std::string& out_path,
                       const Policy& policy) {
  Detected d = detect_file(in_path);
#ifdef HAVE_EXIV2
  if (backends::image_can_handle(d)) return backends::image_strip_to(in_path, out_path, policy);
#endif
  if (backends::pdf_can_handle(d))   return backends::pdf_strip_to(in_path, out_path, policy);
#ifdef HAVE_TAGLIB
  if (backends::audio_can_handle(d)) return backends::audio_strip_to(in_path, out_path, policy);
#endif
  if (backends::zip_can_handle(d))   return backends::zip_strip_to(in_path, out_path, policy);

  (void)policy;
  Detected just{in_path, d.type, {}}; return inspect(just);
}

}
