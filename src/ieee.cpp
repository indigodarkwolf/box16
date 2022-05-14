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
#include "loadsave.h"
#include "memory.h"
#include <SDL.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
extern SDL_RWops *prg_file;

#define UNIT_NO 8

// bool log_ieee = true;
bool log_ieee = false;

char error[80];
int  error_len = 0;
int  error_pos = 0;
char cmd[80];
int  cmdlen    = 0;
int  namelen   = 0;
int  channel   = 0;
bool listening = false;
bool talking   = false;
bool opening   = false;

uint8_t dirlist[65536];
int     dirlist_len = 0;
int     dirlist_pos = 0;

struct channel_t {
	char       name[80];
	bool       write;
	int        pos;
	int        size;
	SDL_RWops *f;
};

channel_t channels[16];

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
		case 0x33: // illegal filename
		case 0x34: // empty file name
		case 0x39: // subdirectory not found
			return "SYNTAX ERROR";
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

static void set_error(int e, int t, int s)
{
	snprintf(error, sizeof(error), "%02x,%s,%02d,%02d\r", e, error_string(e), t, s);
	error_len = (int)strlen(error);
	error_pos = 0;
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
		case 'U':
			switch (cmd[1]) {
				case 'I': // UI: Reset
					set_error(0x73, 0, 0);
					return;
			}
		case 'I': // Initialize
			clear_error();
			return;
	}
	set_error(0x30, 0, 0);
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
					channels[channel].write = true;
					break;
			}
		}
	}
	if (channel <= 1) {
		// channels 0 and 1 are magic
		channels[channel].write = channel;
	}
	if (log_ieee) {
		printf("  OPEN \"%s\",%d (%c)\n", channels[channel].name, channel, channels[channel].write ? 'W' : 'R');
	}

	if (!channels[channel].write && channels[channel].name[0] == '$') {
		dirlist_len = create_directory_listing(dirlist);
		dirlist_pos = 0;
	} else {
		if (!strcmp(channels[channel].name, ":*")) {
			channels[channel].f = prg_file;
		} else {
			channels[channel].f = SDL_RWFromFile(channels[channel].name, channels[channel].write ? "wb" : "rb");
		}
		if (!channels[channel].f) {
			if (log_ieee) {
				printf("  FILE NOT FOUND\n");
			}
			set_error(0x62, 0, 0);
			ret = 2; // FNF
		} else {
			if (!channels[channel].write) {
				SDL_RWseek(channels[channel].f, 0, RW_SEEK_END);
				channels[channel].size = (int)SDL_RWtell(channels[channel].f);
				SDL_RWseek(channels[channel].f, 0, RW_SEEK_SET);
				channels[channel].pos = 0;
			} else if (append) {
				SDL_RWseek(channels[channel].f, 0, RW_SEEK_END);
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
		SDL_RWclose(channels[channel].f);
		channels[channel].f = NULL;
	}
}

void ieee_init()
{
	set_error(0x73, 0, 0);
}

void SECOND(uint8_t a)
{
	if (log_ieee) {
		printf("%s $%02x\n", __func__, a);
	}
	if (listening) {
		channel = a & 0xf;
		opening = false;
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
	int ret = -1;
	if (channel == 15) {
		if (error_pos >= error_len) {
			clear_error();
		}
		*a = error[error_pos++];
	} else if (!channels[channel].write) {
		if (channels[channel].name[0] == '$') {
			if (dirlist_pos < dirlist_len) {
				*a = dirlist[dirlist_pos++];
			}
			if (dirlist_pos == dirlist_len) {
				ret = 0x40;
			}
		} else if (channels[channel].f) {
			*a = SDL_ReadU8(channels[channel].f);
			if (channels[channel].pos == channels[channel].size - 1) {
				ret = 0x40;
			} else {
				channels[channel].pos++;
			}
		}
	} else {
		ret = 2; // FNF
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
				if (a == 13) {
					cmd[cmdlen] = 0;
					command(cmd);
					cmdlen = 0;
				} else {
					if (cmdlen < sizeof(cmd) - 1) {
						cmd[cmdlen++] = a;
					}
				}
			} else if (channels[channel].write && channels[channel].f) {
				SDL_WriteU8(channels[channel].f, a);
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
}

int UNLSN()
{
	int ret = -1;
	if (log_ieee) {
		printf("%s\n", __func__);
	}
	listening = false;
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
	}
}

void TALK(uint8_t a)
{
	if (log_ieee) {
		printf("%s $%02x\n", __func__, a);
	}
	if ((a & 0x1f) == UNIT_NO) {
		talking = true;
	}
}

int MACPTR(uint16_t addr, uint16_t *c)
{
	int     ret      = -1;
	int     count    = (*c != 0) ? (*c) : 256;
	uint8_t ram_bank = read6502(0);
	int     i        = 0;
	do {
		uint8_t byte = 0;
		ret          = ACPTR(&byte);
		write6502(addr, byte);
		addr++;
		i++;
		if (addr == 0xc000) {
			addr = 0xa000;
			ram_bank++;
			write6502(0, ram_bank);
		}
		if (ret >= 0) {
			break;
		}
	} while (i < count);
	*c = i;
	return ret;
}
