// Expected: 42

#include "test20_header.h"

#ifdef LIMIT
  #define HAS_LIMIT 1
#else
  #define HAS_LIMIT 0
#endif

#ifndef NOT_DEFINED
  #define HAS_NOT_DEFINED 1
#else
  #define HAS_NOT_DEFINED 0
#endif

int main() {
    if (HAS_LIMIT == 1) {
        if (HAS_NOT_DEFINED == 1) {
            printf(MESSAGE);
            return LIMIT;
        }
    }
    return 0;
}
