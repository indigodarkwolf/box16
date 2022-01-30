#include "rom_patch.h"

#include <algorithm>
#include <queue>
#include <stack>
#include <unordered_map>

#include "SDL.h"
#include "glue.h"

#define ROM_PATCH_FILE_VERSION (1)

uint64_t fnv_hash(const void *const data, const size_t len)
{
	constexpr uint64_t fnv_offset_basis = 0xcbf29ce484222325ULL;
	constexpr uint64_t fnv_prime        = 0x00000100000001b3ULL;

	uint64_t hash = fnv_offset_basis;

	const uint8_t *const start = reinterpret_cast<const uint8_t *>(data);
	const uint8_t *const end   = start + len;
	for (const uint8_t *b = start; b != end; ++b) {
		hash ^= *b;
		hash *= fnv_prime;
	}

	return hash;
}

template <typename T, size_t ARRAY_SIZE>
uint64_t fnv_hash(const T (&data)[ARRAY_SIZE])
{
	return fnv_hash(data, sizeof(T) * ARRAY_SIZE);
}

// The Boost IO method, apparently.
template <class T>
inline void hash_combine(std::size_t &seed, const T &v)
{
	std::hash<T> hasher;
	seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

struct diff {
	uint32_t location;
	uint8_t  length;
};

template <>
class std::less<diff>
{
public:
	bool operator()(const diff &a, const diff &b)
	{
		if (a.length < b.length) {
			return true;
		} else if (a.length > b.length) {
			return false;
		}

		return a.location < b.location;
	}
};

char const     rom_patch_file_signature_str[] = "Box16 ROM patch file";
const uint64_t rom_patch_file_signature       = fnv_hash(rom_patch_file_signature_str);

// #define VERBOSE_OUTPUT 1

#if defined(VERBOSE_OUTPUT)
#	define VERBOSE_PRINTF(...) printf(__VA_ARGS__)
#else
#	define VERBOSE_PRINTF(...)
#endif

int rom_patch_create(const uint8_t (&rom0)[ROM_SIZE], const uint8_t (&rom1)[ROM_SIZE], SDL_RWops *patch_file)
{
	printf("rom_patch_create\n");

	if (patch_file == nullptr) {
		return ROM_PATCH_CREATE_ERROR_COULD_NOT_OPEN_PATCH_OUT;
	}

	// Build patch

	const uint64_t r0_hash = fnv_hash(rom0);
	const uint64_t r1_hash = fnv_hash(rom1);

	if (r0_hash == r1_hash) {
		return ROM_PATCH_CREATE_ERROR_HASH_MATCH;
	}

	printf("Calculating patch from hash %016" SDL_PRIX64 " to hash %016" SDL_PRIX64 "\n", r0_hash, r1_hash);

	std::priority_queue<diff> diff_set[256];
	diff                      current_diff{ 0, 0 };
	bool                      diffing = false;
	for (uint32_t i = 0; i < ROM_SIZE; ++i) {
		if (rom0[i] != rom1[i]) {
			if (diffing) {
				if (current_diff.length == 0xff) {
					diff_set[current_diff.length].push(current_diff);
					current_diff.location = i;
					current_diff.length   = 1;
				} else {
					++current_diff.length;
				}
			} else {
				current_diff.location = i;
				current_diff.length   = 1;
				diffing               = true;
			}
		} else if (diffing) {
			diff_set[current_diff.length].push(current_diff);
			diffing = false;
		}
	}

	printf("Verifying diffs\n");
	{
		uint8_t vf_data[ROM_SIZE];
		memcpy(vf_data, rom0, ROM_SIZE);

		std::priority_queue<diff> vf_diffs[256];
		for (int i = 0; i < 256; ++i) {
			vf_diffs[i] = diff_set[i];
		}

		for (int i = 255; i >= 0; --i) {
			while (!vf_diffs[i].empty()) {
				diff d = vf_diffs[i].top();
				memcpy(vf_data + d.location, rom1 + d.location, d.length);
				vf_diffs[i].pop();
			}
		}

		bool passing = true;
		for (uint32_t i = 0; i < ROM_SIZE; ++i) {
			if (vf_data[i] != rom1[i]) {
				if (passing) {
					printf("Verification failed. Incorrect bytes:\n");
					passing = false;
				}
				printf("\t$%06X: $%02X instead of $%02X\n", i, (uint32_t)vf_data[i], (uint32_t)rom1[i]);
			}
		}

		if (passing) {
			printf("Verified!\n");
		} else {
			return ROM_PATCH_CREATE_ERROR_INTERNAL_FAILURE;
		}
	}

	printf("Writing patch\n");

	SDL_RWwrite(patch_file, &rom_patch_file_signature, sizeof(rom_patch_file_signature), 1);
	VERBOSE_PRINTF("%016" SDL_PRIX64 "\n", rom_patch_file_signature);

	uint8_t version = ROM_PATCH_FILE_VERSION;
	SDL_RWwrite(patch_file, &version, sizeof(version), 1);
	VERBOSE_PRINTF("%02X\n", (int)version);

	SDL_RWwrite(patch_file, &r0_hash, sizeof(r0_hash), 1);
	VERBOSE_PRINTF("%016" SDL_PRIX64 "\n", r0_hash);

	SDL_RWwrite(patch_file, &r1_hash, sizeof(r1_hash), 1);
	VERBOSE_PRINTF("%016" SDL_PRIX64 "\n", r1_hash);

	for (int i = 255; i >= 0; --i) {
		while (diff_set[i].size() > 0) {
			uint8_t length = (uint8_t)i;
			uint8_t count  = (uint8_t)(std::min(diff_set[i].size(), (size_t)(255)));
			SDL_RWwrite(patch_file, &length, 1, 1);
			SDL_RWwrite(patch_file, &count, 1, 1);
			while (!diff_set[i].empty() && count > 0) {
				diff d = diff_set[i].top();
				diff_set[i].pop();
				SDL_RWwrite(patch_file, &d.location, 3, 1);
				SDL_RWwrite(patch_file, rom1 + d.location, d.length, 1);
				--count;
			}
		}
	}
	printf("%d bytes written\n", (uint32_t)SDL_RWsize(patch_file));

	return ROM_PATCH_CREATE_OK;
}

int rom_patch_load(SDL_RWops *patch_file, uint8_t (&rom)[ROM_SIZE])
{
	const uint64_t rom_hash = fnv_hash(rom);

	uint64_t signature = 0;
	uint8_t  version   = 0;
	uint64_t r0_hash   = 0;
	uint64_t r1_hash   = 0;

	if (SDL_RWread(patch_file, &signature, sizeof(signature), 1) < 1) {
		printf("Patch failed: Invalid patch file\n");
		return ROM_PATCH_LOAD_INVALID_PATCH_FILE;
	}
	VERBOSE_PRINTF("%016" SDL_PRIX64 "\n", signature);

	if (SDL_RWread(patch_file, &version, sizeof(version), 1) < 1) {
		printf("Patch failed: Invalid patch file\n");
		return ROM_PATCH_LOAD_INVALID_PATCH_FILE;
	}
	VERBOSE_PRINTF("%02X\n", (int)version);
	
	if (SDL_RWread(patch_file, &r0_hash, sizeof(r0_hash), 1) < 1) {
		printf("Patch failed: Invalid patch file\n");
		return ROM_PATCH_LOAD_INVALID_PATCH_FILE;
	}
	VERBOSE_PRINTF("%016" SDL_PRIX64 "\n", r0_hash);
	
	if (SDL_RWread(patch_file, &r1_hash, sizeof(r1_hash), 1) < 1) {
		printf("Patch failed: Invalid patch file\n");
		return ROM_PATCH_LOAD_INVALID_PATCH_FILE;
	}
	VERBOSE_PRINTF("%016" SDL_PRIX64 "\n", r1_hash);
	
	if (signature != rom_patch_file_signature) {
		printf("Patch failed: Invalid patch file\n");
		return ROM_PATCH_LOAD_INVALID_PATCH_FILE;
	}

	if (version != ROM_PATCH_FILE_VERSION) {
		printf("Patch failed: Patch file version mismatch\n");
		return ROM_PATCH_LOAD_VERSION_MISMATCH;
	}

	printf("Patch from %" SDL_PRIX64 " to %" SDL_PRIX64 "\n", r0_hash, r1_hash);
	if (rom_hash != r0_hash) {
		printf("Patch failed: Incorrect ROM (%" SDL_PRIX64 " != %" SDL_PRIX64 "\n", rom_hash, r0_hash);
		return ROM_PATCH_LOAD_INCORRECT_ROM_TO_PATCH;
	}

	Sint64 fsize = SDL_RWsize(patch_file);
	while (SDL_RWtell(patch_file) < fsize) {
		uint8_t length = 0;
		if (SDL_RWread(patch_file, &length, 1, 1) < 1) {
			return ROM_PATCH_LOAD_INVALID_PATCH_FILE;
		}

		uint8_t count = 0;
		if (SDL_RWread(patch_file, &count, 1, 1) < 1) {
			return ROM_PATCH_LOAD_INVALID_PATCH_FILE;
		}

		for (uint8_t c = 0; c < count; ++c) {
			uint32_t location = 0;
			if (SDL_RWread(patch_file, &location, 3, 1) < 1) {
				return ROM_PATCH_LOAD_INVALID_PATCH_FILE;
			}

			if (SDL_RWread(patch_file, rom + location, length, 1) < 1) {
				return ROM_PATCH_LOAD_INVALID_PATCH_FILE;
			}
		}
	}

	const uint64_t result_hash = fnv_hash(rom);
	if (result_hash != r1_hash) {
		printf("Patch failed: Hash mismatch after application (%" SDL_PRIX64 " != %" SDL_PRIX64 ")\n", result_hash, r1_hash);
		return ROM_PATCH_LOAD_PATCH_FAILED;
	}

	return ROM_PATCH_CREATE_OK;
}