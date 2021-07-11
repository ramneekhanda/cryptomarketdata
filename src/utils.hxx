#ifndef UTILS_H_
#define UTILS_H_

#include <chrono>
#include <sstream>

#include <date/tz.h>

namespace Utils {
    uint64_t microsFromDate(const std::string& s) {
      using namespace std::chrono;
      using sys_micros = time_point<system_clock, microseconds>;
      sys_micros pt;
      std::istringstream is(s);
      date::from_stream(is, "%FT%TZ", pt);
      return pt.time_since_epoch().count();
    }
}

#endif // UTILS_H_
