#include "lexical_cast.h"

namespace acid::detail {
    const char* s_true = "true";
    const char* s_false = "false";

    bool check_bool(const char* from, const size_t len, const char* s) {
        for (size_t i = 0; i < len; ++i) {
            if (from[i] != s[i]) {
                return false;
            }
        }
        return true;
    }

    bool convert(const char* from) {
        const unsigned int len = strlen(from);
        if (len != 4 && len != 5) {
            throw std::invalid_argument("argument is invalid");
        }
        bool result = true;
        if (len == 4) {
            result = check_bool(from, len, s_true);

            if (result) return true;
        }
        else {
            result = check_bool(from, len, s_false);

            if (result) return false;
        }

        throw std::invalid_argument("argument is invalid");
    }
}  // namespace acid