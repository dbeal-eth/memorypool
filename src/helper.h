#include <sstream>

namespace helper {
    inline std::string uintToString(unsigned int in) {
        std::ostringstream converter;
        converter << in;
        return converter.str();
    }

    inline std::string doubleToString(double in) {
        std::ostringstream converter;
        converter << in;
        return converter.str();
    }

    inline unsigned int stringToUInt(std::string in) {
        std::istringstream convert(in);
        unsigned int result;
        return (!(convert >> result)) ? 0 : result;
    }

    inline double stringToDouble(std::string in) {
        std::istringstream convert(in);
        double result;
        return (!(convert >> result)) ? 0 : result;
    }

    inline char* stringToCstring(std::string in) {
        char* out = new char[in.size()];
        std::strcpy(out, in.c_str());
        return out;
    }
}
