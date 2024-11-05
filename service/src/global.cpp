#include "global.h"

Logger* getLogger() {
    static Logger instance{L"Service"};
    return &instance;
};
