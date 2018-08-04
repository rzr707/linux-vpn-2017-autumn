#ifndef UTILS_HPP
#define UTILS_HPP

#include <iostream>

namespace utils {

    class Logger {
    public:
        static void log(const std::string& msg,
                        std::ostream& s = std::cout);
    };

}

#endif // !UTILS_HPP
