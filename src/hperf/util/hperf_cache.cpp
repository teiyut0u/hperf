// #include "hperf/util/hperf_cache.hpp"
//
// #include <filesystem>
//
// bool hperf::HperfCache::clean(const std::string& target) {
//   try {
//     if (target == "all") {
//       fs::remove_all(CACHE_DIR());
//     } else {
//       fs::remove(CACHE_DIR() / target);
//     }
//     return true;
//   } catch (const fs::filesystem_error& e) {
//     return false;
//   }
// }
