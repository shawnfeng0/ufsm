#pragma once

#include <cstring>
#include <iostream>

// Precompiler define to get only filename;
#if !defined(__FILENAME__)
#define __FILENAME__                                       \
  (strrchr(__FILE__, '/')    ? strrchr(__FILE__, '/') + 1  \
   : strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 \
                             : __FILE__)
#endif

#define LOG std::cout << "(" << __FILENAME__ << ":" << __LINE__ << ") "

#define MARK_FUNCTION LOG << __FUNCTION__ << std::endl

#define MARK_CLASS(TYPE)    \
 public:                    \
  TYPE() { MARK_FUNCTION; } \
  ~TYPE() { MARK_FUNCTION; }
