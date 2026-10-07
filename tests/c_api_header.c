#include "foliage/c_api.h"
/* This translation unit verifies that consumers need no C++ headers. */
uint32_t foliage_c_header_check(void) { return fu_api_version(); }
