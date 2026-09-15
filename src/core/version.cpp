#include "version.h"

#include <sstream>

namespace ra2yr {

std::string build_summary() {
    std::ostringstream os;
    os << "ra2yr " << kVersionString;
#if defined(NDEBUG)
    os << " (release)";
#else
    os << " (debug)";
#endif
    return os.str();
}

}  // namespace ra2yr
