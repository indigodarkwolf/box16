#include "files.h"

#include <SDL.h>
#include <inttypes.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // Added to resolve Microsoft c++ warnings around POSIX and other depreciated errors.
#include <zlib.h>

#include "options.h"
#include "zlib.h"

bool files_find(std::filesystem::path &real_path, const std::filesystem::path &search_path)
{
	options_log_verbose("Finding file: %s\n", search_path.generic_string().c_str());

	// 1. Local CWD or absolute path
	real_path = search_path;
	if (std::filesystem::exists(real_path)) {
		options_log_verbose("Found file: %s (%s)\n", real_path.generic_string().c_str(), std::filesystem::absolute(real_path).generic_string().c_str());
		return true;
	}

	if (!search_path.is_absolute()) {
		// 2. Relative to the location of box16.exe
		real_path = options_get_base_path() / search_path;
		if (std::filesystem::exists(real_path)) {
			options_log_verbose("Found file: %s (%s)\n", real_path.generic_string().c_str(), std::filesystem::absolute(real_path).generic_string().c_str());
			return true;
		}

		// 3. Relative to the prefs directory
		real_path = options_get_prefs_path() / search_path;
		if (std::filesystem::exists(real_path)) {
			options_log_verbose("Found file: %s (%s)\n", real_path.generic_string().c_str(), std::filesystem::absolute(real_path).generic_string().c_str());
			return true;
		}
	}

	printf("Could not find %s in the following locations:\n", search_path.generic_string().c_str());
	printf("\t%s\n", search_path.generic_string().c_str());
	if (!search_path.is_absolute()) {
		printf("\t%s\n", (options_get_base_path() / search_path).generic_string().c_str());
		printf("\t%s\n", (options_get_prefs_path() / search_path).generic_string().c_str());
	}
	return false;
}

std::tuple<void *, size_t> files_load(const std::filesystem::path &path)
{
	std::filesystem::path real_path;
	if (!files_find(real_path, path)) {
		return std::make_tuple(nullptr, 0);
	}

	const auto [ops_data, ops_size] = [&]() -> std::tuple<void *, size_t> {
		SDL_RWops *ops = SDL_RWFromFile(real_path.generic_string().c_str(), "r+b");
		if (ops == nullptr) {
			options_log_verbose("Could not open file for read: %s", real_path.generic_string().c_str());
			return std::make_tuple(nullptr, 0);
		}

		const size_t ops_size = static_cast<size_t>(SDL_RWsize(ops));
		void        *ops_data = malloc(ops_size);
		const auto   ops_read = SDL_RWread(ops, ops_data, ops_size, 1);
		SDL_RWclose(ops);

		if (ops_read == 0) {
			free(ops_data);
			return std::make_tuple(nullptr, 0);
		}

		return std::make_tuple(ops_data, ops_size);
	}();

	return std::make_tuple(ops_data, ops_size);
}

struct x16file {
	char path[PATH_MAX];

	SDL_RWops *file;
	size_t     size;
	size_t     pos;
	bool       modified;

	x16file *next;
};

x16file *open_files = NULL;

static bool get_tmp_name(char *path_buffer, const char *original_path, char const *extension)
{
	if (strlen(original_path) > PATH_MAX - strlen(extension)) {
		printf("Path too long, cannot create temp file: %s\n", original_path);
		return false;
	}

	strcpy(path_buffer, original_path);
	strcat(path_buffer, extension);

	return true;
}

bool file_is_compressed_type(char const *path)
{
	int len = (int)strlen(path);

	if (strcmp(path + len - 3, ".gz") == 0 || strcmp(path + len - 3, "-gz") == 0) {
		return true;
	} else if (strcmp(path + len - 2, ".z") == 0 || strcmp(path + len - 2, "-z") == 0 || strcmp(path + len - 2, "_z") == 0 || strcmp(path + len - 2, ".Z") == 0) {
		return true;
	}

	return false;
}

const char *file_find_extension(const char *path, const char *mark)
{
	if (path == NULL) {
		return NULL;
	}

	if (mark == NULL) {
		mark = path + strlen(path);
		if (file_is_compressed_type(path)) {
			mark -= 3;
		}
	}

	while (mark > path) {
		if (*mark == '.') {
			return mark;
		}
		--mark;
	}

	return NULL;
}

void files_shutdown()
{
	x16file *f      = open_files;
	x16file *next_f = NULL;
	for (; f != NULL; f = next_f) {
		next_f = f->next;
		x16close(f);
	}
}

x16file *x16open(const char *path, const char *attribs)
{
	x16file *f = (x16file *)malloc(sizeof(x16file));
	strcpy(f->path, path);

	if (file_is_compressed_type(path)) {
		char tmp_path[PATH_MAX];
		if (!get_tmp_name(tmp_path, path, ".tmp")) {
			printf("Path too long, cannot create temp file: %s\n", path);
			goto error;
		}

		gzFile zfile = gzopen(path, "rb");
		if (zfile == Z_NULL) {
			printf("Could not open file for decompression: %s\n", path);
			goto error;
		}

		SDL_RWops *tfile = SDL_RWFromFile(tmp_path, "wb");
		if (tfile == NULL) {
			gzclose(zfile);
			printf("Could not open file for write: %s\n", tmp_path);
			goto error;
		}

		printf("Decompressing %s\n", path);

		const int buffer_size = 16 * 1024 * 1024;
		char     *buffer      = (char *)malloc(buffer_size);

		int          read               = gzread(zfile, buffer, buffer_size);
		size_t       total_read         = read;
		const size_t progress_increment = 128 * 1024 * 1024;
		size_t       progress_threshold = progress_increment;
		while (read > 0) {
			if (total_read > progress_threshold) {
				printf("%zd MB\n", total_read / (1024 * 1024));
				progress_threshold += progress_increment;
			}
			SDL_RWwrite(tfile, buffer, read, 1);
			read = gzread(zfile, buffer, buffer_size);
			total_read += read;
		}
		printf("%zd MB\n", total_read / (1024 * 1024));

		SDL_RWclose(tfile);
		gzclose(zfile);
		free(buffer);

		f->file = SDL_RWFromFile(tmp_path, attribs);
		if (f->file == NULL) {
			unlink(tmp_path);
			goto error;
		}
		f->size = total_read;
	} else {
		f->file = SDL_RWFromFile(path, attribs);
		if (f->file == NULL) {
			goto error;
		}
		f->size = (size_t)SDL_RWsize(f->file);
	}
	f->pos      = 0;
	f->modified = false;
	f->next     = open_files ? open_files : NULL;
	open_files  = f;

	return f;

error:
	free(f);
	return NULL;
}

void x16close(x16file *f)
{
	if (f == NULL) {
		return;
	}

	SDL_RWclose(f->file);

	if (file_is_compressed_type(f->path)) {
		char tmp_path[PATH_MAX];
		if (!get_tmp_name(tmp_path, f->path, ".tmp")) {
			printf("Path too long, cannot create temp file: %s\n", f->path);

			if (f == open_files) {
				open_files = f->next;
			} else {
				for (x16file *fi = open_files; fi != NULL; fi = fi->next) {
					if (fi->next == f) {
						fi->next = f->next;
						break;
					}
				}
			}
			free(f);
			return;
		}

		if (f->modified == false) {
			unlink(tmp_path);
			if (f == open_files) {
				open_files = f->next;
			} else {
				for (x16file *fi = open_files; fi != NULL; fi = fi->next) {
					if (fi->next == f) {
						fi->next = f->next;
						break;
					}
				}
			}
			free(f);
			return;
		}

		gzFile zfile = gzopen(f->path, "wb6");
		if (zfile == Z_NULL) {
			printf("Could not open file for compression: %s\n", f->path);
			unlink(tmp_path);
			if (f == open_files) {
				open_files = f->next;
			} else {
				for (x16file *fi = open_files; fi != NULL; fi = fi->next) {
					if (fi->next == f) {
						fi->next = f->next;
						break;
					}
				}
			}
			free(f);
			return;
		}

		SDL_RWops *tfile = SDL_RWFromFile(tmp_path, "rb");
		if (tfile == NULL) {
			gzclose(zfile);
			printf("Could not open file for read: %s\n", tmp_path);

			if (zfile != Z_NULL) {
				gzclose(zfile);
			}
			unlink(tmp_path);
			if (f == open_files) {
				open_files = f->next;
			} else {
				for (x16file *fi = open_files; fi != NULL; fi = fi->next) {
					if (fi->next == f) {
						fi->next = f->next;
						break;
					}
				}
			}
			free(f);
			return;
		}

		printf("Recompressing %s\n", f->path);

		const size_t buffer_size = 16 * 1024 * 1024;
		char        *buffer      = (char *)malloc(buffer_size);

		const size_t progress_increment = 128 * 1024 * 1024;
		size_t       progress_threshold = progress_increment;
		size_t       read               = SDL_RWread(tfile, buffer, 1, buffer_size);
		size_t       total_read         = read;
		while (read > 0) {
			if (total_read > progress_threshold) {
				printf("%d%%\n", (int)(total_read * 100 / f->size));
				progress_threshold += progress_increment;
			}
			gzwrite(zfile, buffer, (unsigned int)read);
			read = SDL_RWread(tfile, buffer, 1, buffer_size);
			total_read += read;
		}

		free(buffer);

		if (tfile != NULL) {
			SDL_RWclose(tfile);
		}

		if (zfile != Z_NULL) {
			gzclose(zfile);
		}

		unlink(tmp_path);
	}

	if (f == open_files) {
		open_files = f->next;
	} else {
		for (x16file *fi = open_files; fi != NULL; fi = fi->next) {
			if (fi->next == f) {
				fi->next = f->next;
				break;
			}
		}
	}
	free(f);
}

size_t x16size(x16file *f)
{
	if (f == NULL) {
		return 0;
	}

	return f->size;
}

int x16seek(x16file *f, size_t pos, int origin)
{
	if (f == NULL) {
		return 0;
	}
	switch (origin) {
		case SEEK_SET:
			f->pos = (pos > f->size) ? f->size : pos;
			break;
		case SEEK_CUR:
			f->pos += pos;
			if (f->pos > f->size || f->pos < 0) {
				f->pos = f->size;
			}
			break;
		case SEEK_END:
			f->pos = f->size - pos;
			if (f->pos < 0) {
				f->pos = f->size;
			}
	}
	return (int)SDL_RWseek(f->file, f->pos, SEEK_SET);
}

size_t x16tell(x16file *f)
{
	if (f == NULL) {
		return 0;
	}
	return f->pos;
}

int x16write8(x16file *f, uint8_t val)
{
	if (f == NULL) {
		return 0;
	}
	int written = (int)SDL_RWwrite(f->file, &val, 1, 1);
	f->pos += written;
	return written;
}

uint8_t x16read8(x16file *f)
{
	if (f == NULL) {
		return 0;
	}
	uint8_t val;
	int     read = (int)SDL_RWread(f->file, &val, 1, 1);
	f->pos += read;
	return read;
}

size_t x16write(x16file *f, const void *data, size_t data_size, size_t data_count)
{
	if (f == NULL) {
		return 0;
	}
	size_t written = SDL_RWwrite(f->file, data, data_size, data_count);
	if (written) {
		f->modified = true;
	}
	f->pos += written * data_size;
	return written;
}

size_t x16write(x16file *f, const std::string &str)
{
	return x16write(f, str.c_str(), str.length(), 1);
}

size_t x16read(x16file *f, void *data, size_t data_size, size_t data_count)
{
	if (f == NULL) {
		return 0;
	}
	size_t read = SDL_RWread(f->file, data, data_size, data_count);
	f->pos += read * data_size;
	return read;
}

size_t x16write_memdump(x16file *f, const std::string &name, const void *src, const int start_addr, const int end_addr, const int addr_width, const int value_width)
{
	const uint8_t    *values = static_cast<const uint8_t *>(src);
	std::stringstream ss;
	ss << "[" << name << "]";
	ss << std::hex << std::uppercase;
	for (int i = start_addr; i < end_addr; ++i) {
		if ((i & 0xf) == 0) {
			ss << std::setw(0) << "\n";
			ss << std::setw(addr_width) << std::setfill('0') << i;
			ss << std::setw(0) << " ";
		} else if ((i & 0x7) == 0) {
			ss << std::setw(0) << "   ";
		} else {
			ss << std::setw(0) << " ";
		}
		ss << std::setw(value_width) << std::setfill('0') << static_cast<uint32_t>(values[i]);
	}
	ss << std::setw(0) << "\n\n";
	return x16write(f, ss.str());
}

size_t x16write_bankdump(x16file *f, const std::string &name, const void *src, const int start_addr, const int end_addr, const int num_banks, const int bank_offset, const int addr_width, const int value_width)
{
	const uint8_t *values    = static_cast<const uint8_t *>(src);
	const int   bank_size = end_addr - start_addr;

	std::stringstream ss;
	ss << "[" << name << "]";
	ss << std::hex << std::uppercase;

	for (int b = 0; b < num_banks; ++b) {
		for (int i = 0; i < bank_size; ++i) {
			if ((i & 0xf) == 0) {
				ss << std::setw(0) << "\n";
				ss << std::setw(2) << std::setfill('0') << (b + bank_offset);
				ss << std::setw(0) << ":";
				if (addr_width > 0) {
					ss << std::setw(addr_width) << std::setfill('0') << (start_addr + i);
				} else {
					ss << "--";
				}
				ss << std::setw(0) << " ";
			} else if ((i & 0x7) == 0) {
				ss << std::setw(0) << "   ";
			} else {
				ss << std::setw(0) << " ";
			}
			ss << std::setw(value_width) << std::setfill('0') << static_cast<uint32_t>(values[b * bank_size + i]);
		}
	}
	ss << std::setw(0) << "\n\n";
	return x16write(f, ss.str());
}