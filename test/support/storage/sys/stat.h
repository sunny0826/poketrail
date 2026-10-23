#pragma once
#include <cstddef>
struct stat { int st_mode; long st_size; };
#define S_ISDIR(m) ((m) == 1)
#define S_ISREG(m) ((m) == 2)
int stat(const char*, struct stat*);
