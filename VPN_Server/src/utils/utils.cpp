#include "utils.hpp"

#include <chrono>

namespace {
    /**
     * @brief currentTime
     * @return string with time in format "<WWW MMM DD hh:mm:ss yyyy>"
     */
    std::string currentTime() {
        auto currTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::string time = std::string() +  "<" + ctime(&currTime);
        time = time.substr(0, time.length() - 1) + ">";
        return time;
    }
}

void utils::Logger::log(const std::__cxx11::string &msg, std::ostream &s) {
    // Check if GCC ver. bigger or equals 5.0.0
    // (because there is no std::put_time on older versions):
    #if GCC_VERSION >= 50000
        auto timeNow = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    timeNow.time_since_epoch()
                    ) % 1000;

        std::time_t currTime
                = std::chrono::system_clock::to_time_t(timeNow);

        s << std::put_time(std::localtime(&currTime), "<%d.%m.%y %H:%M:%S.")
          << ms.count() << '>'
          << " <THREAD ID: " << std::this_thread::get_id()  << '>'
          << ' '
          << msg << std::endl;
    #else
        s << currentTime() << ' '
          << msg << std::endl;
    #endif
}
