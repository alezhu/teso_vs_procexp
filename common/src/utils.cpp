
#include "../include/utils.h"

#include <codecvt>
#include <locale>

std::wstring a2w(std::string_view s) {
    std::wstring wideString = std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(s.data());
    return wideString;
}
