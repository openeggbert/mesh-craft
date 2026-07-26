// SYS-W11-04 — corpus-driven libFuzzer harness over Mc3JsonParser.
//
// XML and JSON are independent MC3 parsing surfaces. Keep this tiny wrapper
// separate from the XML target so coverage and minimized crashes retain their
// format-specific corpus and reproducer.

#include "MeshCraft/Mc3/Mc3Document.hpp"
#include "MeshCraft/Mc3/Mc3LoadPolicy.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    std::string json(reinterpret_cast<const char*>(data), size);
    try {
        (void)MeshCraft::Mc3::Mc3Document::loadFromJsonString(
            json, {}, MeshCraft::Mc3::Mc3LoadPolicy::untrusted());
    } catch (const std::exception&) {
        // Rejected hostile input is normal; sanitizer findings are not.
    }
    return 0;
}
