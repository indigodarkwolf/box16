#pragma once

#include "zlib.h"

bool    file_is_compressed_type(char const *path);
size_t  gzsize(gzFile f);
size_t  gzwrite8(gzFile f, uint8_t val);
uint8_t gzread8(gzFile f);