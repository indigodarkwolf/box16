#include <SDL.h>

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

size_t gzsize(gzFile f)
{
	auto oldseek = gztell(f);
	gzseek(f, 0, SEEK_SET);

	uint8_t read_buffer[64 * 1024];
	size_t  total_bytes = 0;
	size_t  bytes_read  = 0;

	do {
		bytes_read = gzread(f, read_buffer, 64 * 1024);
		total_bytes += bytes_read;
	} while (bytes_read == 64 * 1024);

	gzseek(f, oldseek, SEEK_SET);
	return total_bytes;
}

size_t gzwrite8(gzFile f, uint8_t val)
{
	return gzwrite(f, &val, 1);
}

uint8_t gzread8(gzFile f)
{
	uint8_t value;
	gzread(f, &value, 1);
	return value;
}