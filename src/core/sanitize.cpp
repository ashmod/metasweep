#include "sanitize.hpp"
#ifdef HAVE_EXIV2
#include "../backends/image_exiv2.hpp"
#endif
#include "../backends/pdf_info.hpp"

namespace core {

InspectResult inspect(const Detected& d) {
#ifdef HAVE_EXIV2
  if (backends::image_can_handle(d)) return backends::image_inspect(d);
#endif
  if (backends::pdf_can_handle(d)) return backends::pdf_inspect(d);

  InspectResult ir; ir.file = d.path; ir.type = d.type;
  if (d.type == FileType::Audio) ir.fields.push_back({"ID3", "not yet implemented", "LOW", "INFO", 0});
  if (d.type == FileType::ZIP)   ir.fields.push_back({"ZIP", "not yet implemented", "LOW", "INFO", 0});
  return ir;
}

InspectResult strip_to(const std::string& in_path,
                       const std::string& out_path,
                       const Policy& policy) {
  Detected d = detect_file(in_path);
#ifdef HAVE_EXIV2
  if (backends::image_can_handle(d)) return backends::image_strip_to(in_path, out_path, policy);
#endif
  if (backends::pdf_can_handle(d))   return backends::pdf_strip_to(in_path, out_path, policy);

  (void)policy;
  Detected just{in_path, d.type, {}};
  return inspect(just);
}

} // namespace core
