#pragma once

#include "zlib.h"

bool    file_is_compressed_type(char const *path);
size_t  gzsize(gzFile f);
size_t  gzwrite8(gzFile f, uint8_t val);
uint8_t gzread8(gzFile f);

struct x16file;

#define XSEEK_SET 0
#define XSEEK_END 1
#define XSEEK_CUR 2

bool        file_is_compressed_type(char const *path);
const char *file_find_extension(const char *path, const char *mark);

void files_shutdown();

x16file *x16open(const char *path, const char *attribs);
void     x16close(x16file *f);

size_t x16size(x16file *f);
int    x16seek(x16file *f, size_t pos, int origin);
size_t x16tell(x16file *f);

int     x16write8(x16file *f, uint8_t val);
uint8_t x16read8(x16file *f);

size_t x16write(x16file *f, const void *data, size_t data_size, size_t data_count);
size_t x16read(x16file *f, void *data, size_t data_size, size_t data_count);