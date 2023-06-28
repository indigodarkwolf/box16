// Commander X16 Emulator
// Copyright (c) 2022 Michael Steil
// All rights reserved. License: 2-clause BSD

// Commodore Bus emulation
// * L2: TALK/LISTEN layer: https://www.pagetable.com/?p=1031
// * L3: Commodore DOS: https://www.pagetable.com/?p=1038
// This is used from
// * serial.c: L1: Serial Bus emulation (low level)
// * main.c: IEEE KERNAL call hooks (high level)

#include "ieee.h"
#include "files.h"
#include "loadsave.h"
#include "memory.h"
#include "options.h"
#include <SDL.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <ctime>
#include <unistd.h>

#define UNIT_NO 8

//static constexpr bool log_ieee = true;
static constexpr bool log_ieee = false;

char error[80];
int  error_len = 0;
int  error_pos = 0;
char cmd[80];
int  cmdlen      = 0;
int  namelen     = 0;
int  channel     = 0;
bool listening   = false;
bool talking     = false;
bool opening     = false;
bool overwrite   = false;
bool path_exists = false;

std::filesystem::path hostfscwd = "";

uint8_t                             dirlist[65536];
int                                 dirlist_len        = 0;
int                                 dirlist_pos        = 0;
bool                                dirlist_cwd        = false; // whether we're doing a cwd dirlist or a normal one
bool                                dirlist_eof        = true;
bool                                dirlist_timestmaps = false;
std::filesystem::directory_iterator dirlist_dirp;
std::filesystem::directory_iterator dirlist_dirp_end;
char                                dirlist_wildcard[256];
char                                dirlist_type_filter;

uint16_t cbdos_flags = 0;

const char *blocks_free = "BLOCKS FREE.";

struct channel_t {
	char     name[80];
	bool     read;
	bool     write;
	x16file *f;
};

channel_t channels[16];

// Prototypes for some of the static functions

static void clear_error();
static void set_error(int e, int t, int s);
static void cchdir(char *dir);
static int  cgetcwd(char *buf, size_t len);
static void cseek(int channel, uint32_t pos);
static void cmkdir(char *dir);
static void crmdir(char *dir);
static void cunlink(char *f);
static void crename(char *f);

// Functions

static void set_kernal_cbdos_flags(uint8_t flags)
{
	if (cbdos_flags) {
		write6502(cbdos_flags, flags);
	}
}

static uint8_t get_kernal_cbdos_flags()
{
	if (cbdos_flags) {
		return read6502(cbdos_flags);
	} else {
		return 0;
	}
}

// Puts the emulated cwd in buf, up to the maximum length specified by len
// Turn null termination into a space
// This is for displaying in the directory header
static int cgetcwd(char *buf, size_t len)
{
	strncpy(buf, hostfscwd.generic_string().c_str(), len);
	buf[len - 1] = '\0';

	len = strlen(buf);

	// Turn backslashes into slashes
	for (size_t o = 0; o < len; o++) {
		if (buf[o] == 0) {
			buf[o] = ' ';
		}
		if (buf[o] == '\\') {
			buf[o] = '/';
		}
	}

	return 0;
}

static char *parse_dos_filename(const char *name)
{
	// in case the name starts with something with special meaning,
	// such as @0:
	char *newname = static_cast<char *>(malloc(strlen(name) + 1));
	int   i, j;

	newname[strlen(name)] = 0;

	overwrite = false;

	// [[@][<media 0-9>][</relative_path/> | <//absolute_path/>]:]<file_path>[*]
	// Examples of valid dos filenames
	//   ":FILE.PRG"  (same as "FILE.PRG")
	//   "@:FILE.PRG"  (same as "FILE.PRG" but overwrite okay)
	//   "@0:FILE.PRG"  (same as above)
	//   "//DIR/:FILE.PRG"  (same as "/DIR/FILE.PRG")
	//   "/DIR/:FILE.PRG"  (same as "./DIR/FILE.PRG")
	//   "FILE*" (matches the first file in the directory which starts with "FILE")

	// This routine only parses the bits before the ':'
	// and normalizes directory parts by attaching them to the name part

	// resolve_path() is responsible for resolving absolute and relative
	// paths, and for processing the wildcard option

	const char *name_ptr = strchr(name, ':');
	if (name_ptr != nullptr) {
		name_ptr++;
		i = 0;
		j = 0;

		// @ is overwrite flag
		if (name[i] == '@') {
			overwrite = true;
			i++;
		}

		// Medium, we don't care what this number is
		while (name[i] >= '0' && name[i] <= '9')
			i++;

		// Directory name
		if (name[i] == '/') {
			i++;
			for (; name + i + 1 < name_ptr; i++, j++) {
				newname[j] = name[i];
			}

			// Directory portion must end with /
			if (newname[j - 1] != '/') {
				free(newname);
				return NULL;
			}
		}

		strcpy(newname + j, name_ptr);

	} else {
		strcpy(newname, name);
	}

	return newname;
}

static std::filesystem::path wildcard_match(const std::filesystem::path &origin, const std::string &pattern)
{
	for (auto const &dp : std::filesystem::directory_iterator{ origin }) {
		auto dpname = dp.path().filename().generic_string();

		bool matched = [&]() {
			// in a wildcard match that starts at first position, leading dot filenames are not considered
			if (pattern[0] == '?' || pattern[0] == '*') {
				if(dpname[0] == '.') {
					return false;
				}
			} else if (pattern[0] != dpname[0]) {
				return false;
			}

			for (size_t i = 1, j = 1; i < pattern.length() && j < dpname.length(); ++i) {
				switch (pattern[i]) {
					case '?':
						++j;
						break;
					case '*':
						++i;
						if (i >= pattern.length()) {
							return true;
						}
						while (pattern[i] != dpname[j] && j < dpname.length()) {
							++j;
						}
						if (j >= dpname.length()) {
							return false;
						}
						break;
					default:
						if (pattern[i] != dpname[j]) {
							return false;
						}
						++j;
						break;
				}
			}
			return true;
		}();

		if (matched) {
			return dp.path();
		}
	}
	return "";
}

static std::filesystem::path resolve_path(const std::string &name, bool must_exist)
{
	clear_error();

	const bool is_absolute = (name[0] == '/' || name[0] == '\\');
	const bool is_wildcard = (name.find_first_of('?') != std::string::npos || name.find_first_of('*') != std::string::npos);

	const auto &old_path      = is_absolute ? Options.fsroot_path : hostfscwd;
	const auto  relative_name = is_absolute ? name.substr(1) : name;

	const auto resolved_path = is_wildcard ? wildcard_match(old_path, relative_name) : old_path / relative_name;
	if (resolved_path.empty()) {
		set_error(0x62, 0, 0);
		return "";
	}

	const auto resolved_absolute_path = std::filesystem::absolute(resolved_path);
	if (must_exist && !std::filesystem::exists(resolved_absolute_path)) {
		set_error(0x62, 0, 0);
		return "";
	}

	if (!resolved_absolute_path.generic_string().starts_with(Options.fsroot_path.generic_string())) {
		set_error(0x62, 0, 0);
		return "";
	}

	return resolved_absolute_path;
}

static int create_directory_listing(uint8_t *data, char *dirstring)
{
	uint8_t *data_start = data;

	dirlist_eof = true;
	dirlist_cwd = false;
	size_t i       = 1;
	size_t j;

	dirlist_timestmaps  = false;
	dirlist_type_filter = 0;
	dirlist_wildcard[0] = 0;

	// Here's where we parse out directory listing options
	// Such as "$=T:MATCH*=P"

	// position 0 is assumed to be $
	// so i starts at 1
	while (i < strlen(dirstring)) {
		if (dirstring[i] == '=') {
			i++;
			if (dirstring[i] == 'T') {
				dirlist_timestmaps = true;
			}
		} else if (dirstring[i] == ':') {
			i++;
			j = 0;
			while (i < strlen(dirstring)) {
				if (dirstring[i] == '=' || dirstring[i] == 0) {
					dirlist_wildcard[j] = 0;

					if (dirstring[++i] == 'D') {
						dirlist_type_filter = 'D';
					} else if (dirstring[i] == 'P') {
						dirlist_type_filter = 'P';
					}

					break;
				} else {
					dirlist_wildcard[j++] = dirstring[i++];
				}
			}
		}
		i++;
	}

	// load address
	*data++ = 1;
	*data++ = 8;
	// link
	*data++ = 1;
	*data++ = 1;
	// line number
	*data++ = 0;
	*data++ = 0;
	*data++ = 0x12; // REVERSE ON
	*data++ = '"';
	for (int i = 0; i < 16; i++) {
		*data++ = ' ';
	}
	if (cgetcwd((char *)data - 16, 16)) {
		return false;
	}
	*data++ = '"';
	*data++ = ' ';
	*data++ = 'H';
	*data++ = 'O';
	*data++ = 'S';
	*data++ = 'T';
	*data++ = ' ';
	*data++ = 0;

	if (!std::filesystem::is_directory(hostfscwd)) {
		return 0;
	}

	dirlist_dirp     = std::filesystem::directory_iterator{ hostfscwd };
	dirlist_dirp_end = std::filesystem::end(dirlist_dirp);
	dirlist_eof      = false;
	return static_cast<int>(data - data_start);
}

static int continue_directory_listing(uint8_t *data)
{
	uint8_t *data_start = data;

	while (dirlist_dirp != dirlist_dirp_end) {
		auto const &dp = *dirlist_dirp;

		const auto  &filename = dp.path().filename().generic_string();
		const size_t namlen   = filename.length();
		auto         st       = std::filesystem::status(dp.path());

		// Type match
		if (dirlist_type_filter) {
			switch (dirlist_type_filter) {
				case 'D':
					if (!std::filesystem::is_directory(st)) {
						++dirlist_dirp;
						continue;
					}
					break;
				case 'P':
					if (!std::filesystem::is_regular_file(st)) {
						++dirlist_dirp;
						continue;
					}
					break;
			}
		}

		// don't show the . or .. in the root directory
		// this behaves like SD card/FAT32
		if (!strcmp("..", filename.c_str()) || !strcmp(".", filename.c_str())) {
			if (hostfscwd == Options.fsroot_path) {
				++dirlist_dirp;
				continue;
			}
		}

		if (dirlist_wildcard[0]) { // wildcard match selected
			// in a wildcard match that starts at first position, leading dot filenames are not considered
			if ((dirlist_wildcard[0] == '*' || dirlist_wildcard[0] == '?') && filename[0] == '.') {
				++dirlist_dirp;
				continue;
			}

			bool found = false;
			size_t i = 0;
			for (; i < strlen(dirlist_wildcard) && i < filename.length(); i++) {
				if (dirlist_wildcard[i] == '*') {
					found = true;
					break;
				} else if (dirlist_wildcard[i] == '?') {
					++dirlist_dirp;
					continue;
				} else if (dirlist_wildcard[i] != filename[i]) {
					break;
				}
			}

			// If we reach the end of both strings, it's a match
			if (i == filename.length() && i == strlen(dirlist_wildcard)) {
				found = true;
			}

			if (!found) {
				++dirlist_dirp;
				continue;
			}
		}

		int file_size = std::filesystem::is_directory(st) ? 0 : static_cast<int>((std::filesystem::file_size(dp.path()) + 255) / 256);
		if (file_size > 0xFFFF) {
			file_size = 0xFFFF;
		}

		// link
		*data++ = 1;
		*data++ = 1;

		*data++ = file_size & 0xFF;
		*data++ = file_size >> 8;
		if (file_size < 1000) {
			*data++ = ' ';
			if (file_size < 100) {
				*data++ = ' ';
				if (file_size < 10) {
					*data++ = ' ';
				}
			}
		}
		*data++ = '"';

		memcpy(data, filename.c_str(), namlen);
		data += namlen;
		*data++ = '"';
		for (size_t i = namlen; i < 16; i++) {
			*data++ = ' ';
		}
		*data++ = ' ';
		if (std::filesystem::is_directory(st)) {
			*data++ = 'D';
			*data++ = 'I';
			*data++ = 'R';
		} else {
			*data++ = 'P';
			*data++ = 'R';
			*data++ = 'G';
		}
		// This would be a '<' if file were protected, but it's a space instead
		*data++ = ' ';

		if (dirlist_timestmaps) {
			auto   fwtime = std::filesystem::last_write_time(dp.path());
			time_t fttime = fwtime.time_since_epoch().count();

			// ISO-8601 date+time
			const tm *mtime = std::localtime(&fttime);
			if (mtime != nullptr) {
				*data++ = ' '; // space before the date
				data += strftime((char *)data, 20, "%Y-%m-%d %H:%M:%S", mtime);
			}
		}

		*data++ = 0;
		++dirlist_dirp;
		return static_cast<int>(data - data_start);
	}

	// link
	*data++ = 1;
	*data++ = 1;

	*data++ = 255; // "65535"
	*data++ = 255;

	memcpy(data, blocks_free, strlen(blocks_free));
	data += strlen(blocks_free);
	*data++ = 0;

	// link
	*data++     = 0;
	*data++     = 0;
	dirlist_eof = true;
	return static_cast<int>(data - data_start);
}

static int create_cwd_listing(uint8_t *data)
{
	const auto &hostfscwd_str = hostfscwd.generic_string();
	const auto  hostfscwd_len = hostfscwd_str.length();

	const auto &fsroot_path_str = Options.fsroot_path.generic_string();
	const auto  fsroot_path_len = fsroot_path_str.length();

	uint8_t *data_start = data;
	int      file_size;

	// load address
	*data++ = 1;
	*data++ = 8;
	// link
	*data++ = 1;
	*data++ = 1;
	// line number
	*data++ = 0;
	*data++ = 0;
	*data++ = 0x12; // REVERSE ON
	*data++ = '"';
	for (int i = 0; i < 16; i++) {
		*data++ = ' ';
	}
	if (cgetcwd((char *)data - 16, 16)) {
		dirlist_eof = true;
		return 0;
	}
	*data++ = '"';
	*data++ = ' ';
	*data++ = 'H';
	*data++ = 'O';
	*data++ = 'S';
	*data++ = 'T';
	*data++ = ' ';
	*data++ = 0;

	char *tmp = static_cast<char *>(malloc(hostfscwd_len + 1));
	if (tmp == NULL) {
		set_error(0x70, 0, 0);
		return 0;
	}
	int i = static_cast<int>(hostfscwd_len);
	int j = static_cast<int>(fsroot_path_len);
	strcpy(tmp, hostfscwd_str.c_str());

	for (; i >= j - 1; --i) {
		// find the beginning of a path element
		if (i >= j && tmp[i - 1] != '/' && tmp[i - 1] != '\\')
			continue;

		tmp[i - 1] = 0;

		if (i < j) {
			strcpy(tmp + i, "/");
		}

		file_size     = 0;
		size_t namlen = strlen(tmp + i);

		if (!namlen)
			continue; // there was a doubled path separator

		// link
		*data++ = 1;
		*data++ = 1;

		*data++ = file_size & 0xFF;
		*data++ = file_size >> 8;
		if (file_size < 1000) {
			*data++ = ' ';
			if (file_size < 100) {
				*data++ = ' ';
				if (file_size < 10) {
					*data++ = ' ';
				}
			}
		}
		*data++ = '"';

		memcpy(data, tmp + i, namlen);
		data += namlen;
		*data++ = '"';
		for (size_t i = namlen; i < 16; i++) {
			*data++ = ' ';
		}
		*data++ = ' ';
		*data++ = 'D';
		*data++ = 'I';
		*data++ = 'R';
		*data++ = 0;
	}

	free(tmp);

	// link
	*data++ = 1;
	*data++ = 1;

	*data++ = 255; // "65535"
	*data++ = 255;

	memcpy(data, blocks_free, strlen(blocks_free));
	data += strlen(blocks_free);
	*data++ = 0;

	// link
	*data++ = 0;
	*data++ = 0;

	dirlist_eof = true;
	dirlist_cwd = true;

	return static_cast<int>(data - data_start);
}

static const char *error_string(int e)
{
	switch (e) {
		case 0x00:
			return " OK";
		case 0x01:
			return " FILES SCRATCHED";
		case 0x02:
			return "PARTITION SELECTED";
		// 0x2x: Physical disk error
		case 0x20:
			return "READ ERROR"; // generic read error
		case 0x25:
			return "WRITE ERROR"; // generic write error
		case 0x26:
			return "WRITE PROTECT ON";
		// 0x3x: Error parsing the command
		case 0x30: // generic
		case 0x31: // invalid command
		case 0x32: // command buffer overflow
			return "SYNTAX ERROR";
		case 0x33: // illegal filename
			return "ILLEGAL FILENAME";
		case 0x34: // empty file name
			return "EMPTY FILENAME";
		case 0x39: // subdirectory not found
			return "SUBDIRECTORY NOT FOUND";
		// 0x4x: Controller error (CMD addition)
		case 0x49:
			return "INVALID FORMAT"; // partition present, but not FAT32
		// 0x5x: Relative file related error
		// unsupported
		// 0x6x: File error
		case 0x62:
			return " FILE NOT FOUND";
		case 0x63:
			return "FILE EXISTS";
		// 0x7x: Generic disk or device error
		case 0x70:
			return "NO CHANNEL"; // error allocating context
		case 0x71:
			return "DIRECTORY ERROR"; // FAT error
		case 0x72:
			return "PARTITION FULL"; // filesystem full
		case 0x73:
			return "HOST FS V1.0 X16";
		case 0x74:
			return "DRIVE NOT READY"; // illegal partition for any command but "CP"
		case 0x75:
			return "FORMAT ERROR";
		case 0x77:
			return "SELECTED PARTITION ILLEGAL";
		default:
			return "";
	}
}

static void set_activity(bool active)
{
	uint8_t cbdos_flags = get_kernal_cbdos_flags();
	if (active) {
		cbdos_flags |= 0x10; // set activity flag
	} else {
		cbdos_flags &= ~0x10; // clear activity flag
	}
	set_kernal_cbdos_flags(cbdos_flags);
}

static void set_error(int e, int t, int s)
{
	snprintf(error, sizeof(error), "%02x,%s,%02d,%02d\r", e, error_string(e), t, s);
	error_len           = static_cast<int>(strlen(error));
	error_pos           = 0;
	uint8_t cbdos_flags = get_kernal_cbdos_flags();
	if (e < 0x10 || e == 0x73) {
		cbdos_flags &= ~0x20; // clear error
	} else {
		cbdos_flags |= 0x20; // set error flag
	}
	set_kernal_cbdos_flags(cbdos_flags);
}

static void clear_error()
{
	set_error(0, 0, 0);
}

static void command(char *cmd)
{
	if (!cmd[0]) {
		return;
	}
	printf("  COMMAND \"%s\"\n", cmd);
	switch (cmd[0]) {
		case 'C': // C (copy), CD (change directory), CP (change partition)
			switch (cmd[1]) {
				case 'D': // Change directory
					if (cmd[2] == ':') {
						cchdir(cmd + 3);
						return;
					}
				case 'P': // Change partition
					set_error(0x02, 0, 0);
					return;
				default: // Copy
					// NYI
					set_error(0x30, 0, 0);
					return;
			}
		case 'I': // Initialize
			clear_error();
			return;
		case 'M': // MD
			switch (cmd[1]) {
				case 'D': // Make directory
					if (cmd[2] == ':') {
						cmkdir(cmd + 3);
						return;
					}
				default: // Memory (not implemented)
					set_error(0x31, 0, 0);
					return;
			}
		case 'P': // Seek
			cseek(cmd[1], ((uint8_t)cmd[2]) | ((uint8_t)cmd[3] << 8) | ((uint8_t)cmd[4] << 16) | ((uint8_t)cmd[5] << 24));
			return;
		case 'R': // RD
			switch (cmd[1]) {
				case 'D': // Remove directory
					if (cmd[2] == ':') {
						crmdir(cmd + 3);
						return;
					}
				default:          // Rename
					crename(cmd); // Need to parse out the arg in this function
					return;
			}
		case 'S':
			switch (cmd[1]) {
				case '-': // Swap
					set_error(0x31, 0, 0);
					return;
				default:          // Scratch
					cunlink(cmd); // Need to parse out the arg in this function
					return;
			}
		case 'U':
			switch (cmd[1]) {
				case 'I': // UI: Reset
					set_error(0x73, 0, 0);
					return;
			}
		default:
			if (log_ieee) {
				printf("    (unsupported command ignored)\n");
			}
	}
	set_error(0x30, 0, 0);
}

static void cchdir(char *dir)
{
	// The directory name is in dir, coming from the command channel
	// with the CD: portion stripped off
	std::filesystem::path resolved = resolve_path(dir, true);

	if (resolved.empty()) {
		// error already set
		return;
	}

	auto st = std::filesystem::status(resolved);

	// Is it a directory?
	if (!std::filesystem::exists(st)) {
		// FNF
		set_error(0x62, 0, 0);
	} else if (!std::filesystem::is_directory(st)) {
		// Not a directory
		set_error(0x39, 0, 0);
	} else {
		// cwd has been changed
		hostfscwd = resolved;
	}

	return;
}

static void cmkdir(char *dir)
{
	// The directory name is in dir, coming from the command channel
	// with the MD: portion stripped off
	std::filesystem::path resolved = resolve_path(dir, false);

	clear_error();
	if (resolved.empty()) {
		// error already set
		return;
	}

	if (!std::filesystem::create_directory(resolved)) {
		if (std::filesystem::exists(resolved)) {
			set_error(0x63, 0, 0);
		} else {
			set_error(0x62, 0, 0);
		}
	}
}

static void crename(char *f)
{
	// This function receives the whole R command, which could be
	// "R:NEW=OLD" or "RENAME:NEW=OLD" or anything in between
	// let's simply find the first colon and chop it there
	char *tmp = static_cast<char *>(malloc(strlen(f) + 1));
	if (tmp == NULL) {
		set_error(0x70, 0, 0);
		return;
	}
	strcpy(tmp, f);
	char *d = strchr(tmp, ':');

	if (d == NULL) {
		// No colon, not a valid rename command
		free(tmp);
		set_error(0x34, 0, 0);
		return;
	}

	d++; // one character after the colon

	// Now split on the = sign to find
	char *s = strchr(d, '=');

	if (s == NULL) {
		// No equals sign, not a valid rename command
		free(tmp);
		set_error(0x34, 0, 0);
		return;
	}

	*(s++) = 0; // null-terminate d and advance s

	std::filesystem::path src = resolve_path(s, true);
	clear_error();
	if (src.empty()) {
		// source not found
		free(tmp);
		set_error(0x62, 0, 0);
		return;
	}

	std::filesystem::path dst = resolve_path(d, false);
	if (dst.empty()) {
		// dest not found
		free(tmp);
		set_error(0x39, 0, 0);
		return;
	}

	free(tmp); // we're now done with d and s (part of tmp)

	std::error_code ec;
	std::filesystem::rename(src, dst, ec);
	if (ec.value() != 0) {
		if (ec.value() == EACCES) {
			set_error(0x63, 0, 0);
		} else if (ec.value() == EINVAL) {
			set_error(0x33, 0, 0);
		} else {
			set_error(0x62, 0, 0);
		}
	}
}

static void crmdir(char *dir)
{
	// The directory name is in dir, coming from the command channel
	// with the RD: portion stripped off
	std::filesystem::path resolved = resolve_path(dir, true);

	clear_error();
	if (resolved.empty()) {
		set_error(0x39, 0, 0);
		return;
	}

	if (std::filesystem::is_directory(resolved)) {
		if (std::filesystem::is_empty(resolved)) {
			std::filesystem::remove(resolved);
		} else {
			set_error(0x63, 0, 0);
		}
	} else {
		set_error(0x62, 0, 0);
	}
}

static void cunlink(char *f)
{
	// This function receives the whole S command, which could be
	// "S:FILENAME" or "SCRATCH:FILENAME" or anything in between
	// let's simply find the first colon and chop it there
	// TODO path syntax and multiple files
	char *tmp = static_cast<char *>(malloc(strlen(f) + 1));
	if (tmp == NULL) {
		set_error(0x70, 0, 0);
		return;
	}
	strcpy(tmp, f);
	char *fn = strchr(tmp, ':');

	if (fn == NULL) {
		// No colon, not a valid scratch command
		free(tmp);
		set_error(0x34, 0, 0);
		return;
	}

	fn++; // one character after the colon
	std::filesystem::path resolved = resolve_path(fn, true);

	clear_error();
	if (resolved.empty()) {
		free(tmp);
		set_error(0x62, 0, 0);
		return;
	}

	free(tmp); // we're now done with fn (part of tmp)

	std::error_code ec;
	if (std::filesystem::remove(resolved, ec)) {
		switch (ec.value()) {
			case 0:
				set_error(0x01, 0, 0); // 1 file scratched
				break;
			case EACCES:
				set_error(0x63, 0, 0);
				break;
			default:
				set_error(0x62, 0, 0);
				break;
		}
	}
}

static int copen(int channel)
{
	if (channel == 15) {
		command(channels[channel].name);
		return -1;
	}

	int ret = -1;

	// decode ",P,W"-like suffix to know whether we're writing
	bool append             = false;
	channels[channel].write = false;
	char *first             = strchr(channels[channel].name, ',');
	if (first) {
		*first       = 0; // truncate name here
		char *second = strchr(first + 1, ',');
		if (second) {
			switch (second[1]) {
				case 'A':
					append = true;
					// fallthrough
				case 'W':
					channels[channel].read  = false;
					channels[channel].write = true;
					break;
				case 'M':
					channels[channel].read  = true;
					channels[channel].write = true;
					break;
			}
		}
	} else {
		channels[channel].read = true;
	}

	if (channel <= 1) {
		// channels 0 and 1 are magic
		channels[channel].write = channel;
		channels[channel].read  = !channel;
	}
	if (log_ieee) {
		printf("  OPEN \"%s\",%d (%s%s)\n", channels[channel].name, channel, channels[channel].read ? "R" : "", channels[channel].write ? "W" : "");
	}

	if (!channels[channel].write && channels[channel].name[0] == '$') {
		dirlist_pos = 0;
		if (!strncmp(channels[channel].name, "$=C", 3)) {
			// This emulates the behavior in the ROM code in
			// https://github.com/X16Community/x16-rom/pull/5
			dirlist_len = create_cwd_listing(dirlist);
		} else {
			dirlist_len = create_directory_listing(dirlist, channels[channel].name);
		}
	} else {
		if (!strcmp(channels[channel].name, ":*") && !Options.prg_path.empty()) {
			channels[channel].f = x16open(Options.prg_path.generic_string().c_str(), "rb"); // special case
		} else {
			char *parsed_filename = parse_dos_filename(channels[channel].name);
			if (parsed_filename == NULL) {
				set_error(0x32, 0, 0); // Didn't parse out properly
				return -2;
			}

			std::filesystem::path resolved_filename = resolve_path(parsed_filename, false);
			free(parsed_filename);

			if (resolved_filename.empty()) {
				// Resolve the path, if we get a null ptr back, error is already set.
				return -2;
			}

			if (path_exists && !overwrite && !append && !channels[channel].read) {
				set_error(0x63, 0, 0); // forbid overwrite unless requested
				return -1;
			}

			if (append) {
				channels[channel].f = x16open(resolved_filename.generic_string().c_str(), "ab+");
			} else if (channels[channel].read && channels[channel].write) {
				channels[channel].f = x16open(resolved_filename.generic_string().c_str(), "rb+");
			} else {
				channels[channel].f = x16open(resolved_filename.generic_string().c_str(), channels[channel].write ? "wb6" : "rb");
			}
		}

		if (channels[channel].f == nullptr) {
			if (log_ieee) {
				printf("  FILE NOT FOUND\n");
			}
			set_error(0x62, 0, 0);
			ret = 2; // FNF
		} else {
			if (!channels[channel].write) {
				x16seek(channels[channel].f, 0, XSEEK_SET);
			} else if (append) {
				x16seek(channels[channel].f, 0, XSEEK_END);
			}
			clear_error();
		}
	}
	return ret;
}

static void cclose(int channel)
{
	if (log_ieee) {
		printf("  CLOSE %d\n", channel);
	}
	channels[channel].name[0] = 0;
	if (channels[channel].f) {
		x16close(channels[channel].f);
		channels[channel].f = nullptr;
	}
}

static void cseek(int channel, uint32_t pos)
{
	if (channel == 15) {
		set_error(0x30, 0, 0);
		return;
	}

	if (channels[channel].f) {
		x16seek(channels[channel].f, pos, XSEEK_SET);
	}
}

void ieee_init()
{
	int ch;

	static bool initd = false;
	if (!initd) {
		// Init the hostfs "jail" and cwd
		if (Options.fsroot_path.empty()) { // if null, default to cwd
			// We hold this for the lifetime of the program, and we don't have
			// any sort of destructor, so we rely on the OS teardown to free() it.
			Options.fsroot_path = getcwd(NULL, 0);
		} else {
			// Normalize it
			Options.fsroot_path = std::filesystem::absolute(Options.fsroot_path);
		}

		if (Options.startin_path.empty()) {
			// same as above
			Options.startin_path = getcwd(NULL, 0);
		} else {
			// Normalize it
			Options.startin_path = std::filesystem::absolute(Options.startin_path);
		}

		// Quick error checks
		if (Options.fsroot_path.empty()) {
			fprintf(stderr, "Failed to resolve argument to -fsroot\n");
			exit(1);
		}

		if (Options.startin_path.empty()) {
			fprintf(stderr, "Failed to resolve argument to -startin\n");
			exit(1);
		}

		// Now we verify that startin_path is within fsroot_path
		// In other words, if fsroot_path is a left-justified substring of startin_path

		// If startin_path is not reachable, we instead default to setting it
		// back to fsroot_path
		if (Options.startin_path.generic_string().substr(0, Options.fsroot_path.generic_string().length()) != Options.fsroot_path.generic_string()) {
			Options.startin_path = Options.fsroot_path;
		}

		for (ch = 0; ch < 16; ch++) {
			channels[ch].f       = NULL;
			channels[ch].name[0] = 0;
		}

		initd = true;
	} else {
		for (ch = 0; ch < 16; ch++) {
			cclose(ch);
		}

		listening = false;
		talking   = false;
		opening   = false;
	}

	// Now initialize our emulated cwd.
	hostfscwd = Options.startin_path;

	// Locate and remember cbdos_flags variable address in KERNAL vars
	const bool located_cbdos_flags = []() -> bool {
		// check JMP instruction at ACPTR API
		if (debug_read6502(0xffa5, 0) != 0x4c) {
			return false;
		}

		// get address of ACPTR routine
		uint16_t kacptr = debug_read6502(0xffa6, 0) | debug_read6502(0xffa7, 0) << 8;
		if (kacptr < 0xc000) {
			return false;
		}

		// first instruction is BIT cbdos_flags
		if (debug_read6502(kacptr, 0) != 0x2c) {
			return false;
		}

		// get the address of cbdos_flags
		cbdos_flags = debug_read6502(kacptr + 1, 0) | debug_read6502(kacptr + 2, 0) << 8;

		if (cbdos_flags < 0x0200 || cbdos_flags >= 0x0400) {
			return false;
		}

		return true;
	}();
	if (!located_cbdos_flags) {
		printf("Unable to find KERNAL cbdos_flags\n");
		cbdos_flags = 0;
	}

	set_error(0x73, 0, 0);
}

int SECOND(uint8_t a)
{
	int ret = -1;
	if (log_ieee) {
		printf("%s $%02x\n", __func__, a);
	}
	if (listening) {
		channel = a & 0xf;
		opening = false;
		if (channel == 15) {
			ret = 0;
		}
		switch (a & 0xf0) {
			case 0x60:
				if (log_ieee) {
					printf("  WRITE %d...\n", channel);
				}
				break;
			case 0xe0:
				cclose(channel);
				break;
			case 0xf0:
				if (log_ieee) {
					printf("  OPEN %d...\n", channel);
				}
				opening = true;
				namelen = 0;
				break;
		}
	}
	return ret;
}

void TKSA(uint8_t a)
{
	if (log_ieee) {
		printf("%s $%02x\n", __func__, a);
	}
	if (talking) {
		channel = a & 0xf;
	}
}

int ACPTR(uint8_t *a)
{
	int ret = 0;
	if (channel == 15) {
		*a = error[error_pos++];
		if (error_pos >= error_len) {
			clear_error();
			ret = 0x40; // EOI
		}
	} else if (channels[channel].read) {
		if (channels[channel].name[0] == '$') {
			if (dirlist_pos < dirlist_len) {
				*a = dirlist[dirlist_pos++];
			} else {
				*a = 0;
			}
			if (dirlist_pos == dirlist_len) {
				if (dirlist_eof) {
					ret = 0x40;
				} else {
					dirlist_pos = 0;
					dirlist_len = continue_directory_listing(dirlist);
				}
			}
		} else if (channels[channel].f) {
			if (x16read(channels[channel].f, a, 1, 1) != 1) {
				ret = 0x42;
				*a  = 0;
			} else {
				// We need to send EOI on the last byte of the file.
				// We have to check every time since CMDR-DOS
				// supports random access R/W mode

				size_t curpos = x16tell(channels[channel].f);
				if (curpos == x16size(channels[channel].f)) {
					ret                    = 0x40;
					channels[channel].read = false;
					cclose(channel);
				}
			}
		} else {
			ret = 0x42;
		}
	} else {
		ret = 0x42; // FNF
	}
	if (log_ieee) {
		printf("%s-> $%02x\n", __func__, *a);
	}
	return ret;
}

int CIOUT(uint8_t a)
{
	int ret = -1;
	if (log_ieee) {
		printf("%s $%02x\n", __func__, a);
	}
	if (listening) {
		if (opening) {
			if (namelen < sizeof(channels[channel].name) - 1) {
				channels[channel].name[namelen++] = a;
			}
		} else {
			if (channel == 15) {
				// P command takes binary parameters, so we can't terminate
				// the command on CR.
				if ((a == 13) && (cmd[0] != 'P')) {
					cmd[cmdlen] = 0;
					command(cmd);
					cmdlen = 0;
				} else {
					if (cmdlen < sizeof(cmd) - 1) {
						cmd[cmdlen++] = a;
					}
				}
			} else if (channels[channel].write && channels[channel].f) {
				if (!x16write8(channels[channel].f, a)) {
					ret = 0x40;
				}
			} else {
				ret = 2; // FNF
			}
		}
	}
	return ret;
}

void UNTLK()
{
	if (log_ieee) {
		printf("%s\n", __func__);
	}
	talking = false;
	set_activity(false);
}

int UNLSN()
{
	int ret = -1;
	if (log_ieee) {
		printf("%s\n", __func__);
	}
	listening = false;
	set_activity(false);
	if (opening) {
		channels[channel].name[namelen] = 0; // term
		opening                         = false;
		ret                             = copen(channel);
	} else if (channel == 15) {
		cmd[cmdlen] = 0;
		command(cmd);
		cmdlen = 0;
	}
	return ret;
}

void LISTEN(uint8_t a)
{
	if (log_ieee) {
		printf("%s $%02x\n", __func__, a);
	}
	if ((a & 0x1f) == UNIT_NO) {
		listening = true;
		set_activity(true);
	}
}

void TALK(uint8_t a)
{
	if (log_ieee) {
		printf("%s $%02x\n", __func__, a);
	}
	if ((a & 0x1f) == UNIT_NO) {
		talking = true;
		set_activity(true);
	}
}

int MACPTR(uint16_t addr, uint16_t *c, uint8_t stream_mode)
{
	if (log_ieee) {
		printf("%s $%04x $%04x $%02x\n", __func__, addr, *c, stream_mode);
	}

	int     ret      = -1;
	int     count    = (*c != 0) ? (*c) : 256;
	uint8_t ram_bank = read6502(0);
	int     i        = 0;
	if (channels[channel].f) {
		do {
			uint8_t byte = 0;
			ret          = ACPTR(&byte);
			write6502(addr, byte);
			i++;
			if (!stream_mode) {
				addr++;
				if (addr == 0xc000) {
					addr = 0xa000;
					ram_bank++;
					write6502(0, ram_bank);
				}
			}
			if (ret >= 0) {
				break;
			}
		} while (i < count);
	} else {
		ret = 0x42;
	}
	*c = i;
	return ret;
}
