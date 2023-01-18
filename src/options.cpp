#include "options.h"

#include <ini.h>
#include <type_traits>

#include "audio.h"
#include "debugger.h"
#include "overlay/overlay.h"
#include "symbols.h"
#include "version.h"

options               Options;
const options         Default_options;
std::filesystem::path Options_base_path;
std::filesystem::path Options_prefs_path;
std::filesystem::path Options_ini_path;

mINI::INIStructure Cmdline_ini;
mINI::INIStructure Inifile_ini;

std::vector<uint32_t> Break_options;
std::vector<std::pair<std::string, symbol_bank_type>> Sym_options;

static const char *token_or_empty(const std::string &str, const char *token)
{
	static char *str_buffer = nullptr;

	if (str_buffer != nullptr) {
		delete[] str_buffer;
	}

	str_buffer = new char[str.size() + 1];
	str.copy(str_buffer, str.size());
	str_buffer[str.size()] = '\0';

	char *result = strtok(str_buffer, token);
	return result != nullptr ? result : "";
}

static const char *token_or_empty(char *str, const char *token)
{
	char *result = strtok(str, token);
	return result != nullptr ? result : "";
}

static void usage()
{
	printf("%s %s (%s)\n", VER_TITLE, VER_NUM, VER_NAME);
	printf("Copyright (c) 2019-2022 Michael Steil,\n");
	printf("              2020 Frank van den Hoen,\n");
	printf("              2021-2022 Stephen Horn, et al.\n");
	printf("All rights reserved. License: 2-clause BSD\n\n");

	printf("Usage: box16 [option] ...\n\n");

	printf("-abufs <number of audio buffers>\n");
	printf("\tIs provided for backward-compatibility with x16emu toolchains,\n");
	printf("\tbut is non-functional in Box16.\n ");

	printf("-bas <app.txt>\n");
	printf("\tInject a BASIC program in ASCII encoding through the\n");
	printf("\tkeyboard.\n");

	printf("-debug <address>\n");
	printf("\tSet a breakpoint in the debugger\n");

	printf("-dump {C|R|B|V}...\n");
	printf("\tConfigure system dump: (C)PU, (R)AM, (B)anked-RAM, (V)RAM\n");
	printf("\tMultiple characters are possible, e.g. -dump CV ; Default: RB\n");

	printf("-echo [{iso|raw}]\n");
	printf("\tPrint all KERNAL output to the host's stdout.\n");
	printf("\tBy default, everything but printable ASCII characters get\n");
	printf("\tescaped. \"iso\" will escape everything but non-printable\n");
	printf("\tISO-8859-15 characters and convert the output to UTF-8.\n");
	printf("\t\"raw\" will not do any substitutions.\n");
	printf("\tWith the BASIC statement \"LIST\", this can be used\n");
	printf("\tto detokenize a BASIC program.\n");

	printf("-hypercall_path <path>\n");
	printf("\tSet the base path for hypercalls (effectively, the current working directory when no SD card is attached).\n");

	printf("-geos\n");
	printf("\tLaunch GEOS at startup.\n");

	printf("-gif <file.gif>[,wait]\n");
	printf("\tRecord a gif for the video output.\n");
	printf("\tUse ,wait to start paused.\n");

	printf("-help\n");
	printf("\tPrint this message and exit.\n");

	printf("-ignore_ini\n");
	printf("\tDo not attempt to apply Box16 options from any ini file.\n");

	printf("-ini <inifile.ini>\n");
	printf("\tUse this ini file for emulator settings and options.\n");
	printf("\tIf -ignore_ini is also specified, this will set the location of the ini file, but not actually load settings from it.\n");
	printf("\tIf -save_ini is also specified, the emulator settings for this run will be saved to this ini file.\n");

	printf("-keymap <keymap>\n");
	printf("\tEnable a specific keyboard layout decode table.\n");

#if defined(TRACE)
	printf("-log {K|S|V|Cl|Cb|Ca|Co|Mw|Mr}...\n");
	printf("\tEnable logging of (K)eyboard, (S)peed, (V)ideo, (C)pu, (M)emory.\n");
	printf("\tMultiple characters are possible, e.g. -log KS\n");
	printf("\tCpu activity logging works with zones:\n");
	printf("\t\t- Cl = Cpu activity logging in low ram,     from $0000 to $07FF.\n");
	printf("\t\t- Cm = Cpu activity logging in main ram,    from $0800 to $9FFF.\n");
	printf("\t\t- Ca = Cpu activity logging in banked ram,  from $A000 to $BFFF.\n");
	printf("\t\t- Co = Cpu activity logging in banked rom,  from $C000 to $FFFF.\n");
	printf("\tMemory activity logging works in two modes:\n");
	printf("\t\t- Mr = Memory read activity logging.\n");
	printf("\t\t- Mw = Memory write activity logging.\n");

#else
	printf("-log {K|S|V}...\n");
	printf("\tEnable logging of (K)eyboard, (S)peed, (V)ideo, (C)pu.\n");
	printf("\tMultiple characters are possible, e.g. -log KS\n");
#endif

	printf("-nobinds\n");
	printf("\tDisable most emulator keyboard shortcuts.\n");

	printf("-noemucmdkeys\n");
	printf("\tAlias for -nobinds.\n");

	printf("-nohostieee\n");
	printf("\tDisable IEEE-488 hypercalls. These are normally enabled unless an SD card is attached or -serial is specified.\n");

	printf("-nohypercalls\n");
	printf("\tDisable all hypercalls in Box16.\n");

	printf("-nopanels\n");
	printf("\tDo not automatically re-open any panels from the previous session.\n");

	printf("-nosound\n");
	printf("\tDisables audio. Incompatible with -sound.\n");

	printf("-nvram <nvram.bin>\n");
	printf("\tSpecify NVRAM image. By default, the machine starts with\n");
	printf("\tempty NVRAM and does not save it to disk.\n");

	printf("-prg <app.prg>[,<load_addr>]\n");
	printf("\tLoad application from the local disk into RAM\n");
	printf("\t(.PRG file with 2 byte start address header)\n");
	printf("\tThe override load address is hex without a prefix.\n");

	printf("-quality {nearest|linear|best}\n");
	printf("\tScaling algorithm quality\n");

	printf("-ram <ramsize>\n");
	printf("\tSpecify banked RAM size in KB (8, 16, 32, ..., 2048).\n");
	printf("\tThe default is 512.\n");

	printf("-rom <rom.bin>\n");
	printf("\tOverride KERNAL/BASIC/* ROM file.\n");

	printf("-romcart [bank] <cart.bin>\n");
	printf("\tLoad a cartridge into ROM space starting at the bank specified in decimal, otherwise default to bank 32.\n");

	printf("-rtc\n");
	printf("\tSet the real-time-clock to the current system time and date.\n");

	printf("-randram\n");
	printf("\tRandomize the byte contents of memory on first boot.\n");

	printf("-run\n");
	printf("\tStart the -prg/-bas program using RUN or SYS, depending\n");
	printf("\ton the load address.\n");

	printf("-save_ini\n");
	printf("\tSave current emulator settings to ini file. This includes the other command-line options specified with this run.\n");
	printf("\tIf -ini has not been specified, this uses the default ini location under %%APPDATA%%\\Box16\\Box16 or ~/.local/Box16.\n");

	printf("-scale {1|2|3|4}\n");
	printf("\tScale output to an integer multiple of 640x480\n");

	printf("-sdcard <sdcard.img>\n");
	printf("\tSpecify SD card image (partition map + FAT32)\n");

	printf("-serial\n");
	printf("\tEnable the serial bus (experimental)\n");

	printf("-sound <output device>\n");
	printf("\tSet the output device used for audio emulation. Incompatible with -nosound.\n");

	printf("-stds\n");
	printf("\tLoad standard (ROM) symbol files\n");

	printf("-sym <filename>\n");
	printf("\tLoad a VICE label file. Note that not all VICE debug commands are available.\n");
	printf("\tSupported commands are:\n");
	printf("\t\tadd_label <address> <label>\n");
	printf("\t\tal <address> <label>\n");
	printf("\t\t\tMap a given address to a label.\n");
	printf("\t\tbreak <address>\n");
	printf("\t\t\tSet a breakpoint at the specified address.\n");

	printf("-test {0, 1, 2, 3}\n");
	printf("\tImmediately invoke the TEST command with the provided test number.\n");

	printf("-verbose\n");
	printf("\tPrint additional debug output from the emulator.\n");

	printf("-version\n");
	printf("\tPrint additional version information the emulator and ROM.\n");

	printf("-vsync {none|get|wait}\n");
	printf("\tUse specified vsync rendering strategy to avoid visual tearing.\n");
	printf("\tUse 'none' if the content area remains white after start.\n");

	printf("-warp {factor}\n");
	printf("\tEnable warp mode, run emulator as fast as possible.\n");
	printf("\tIf specified, the warp factor [1...16] determines how frequently to skip video rendering.\n");
	printf("\tThis can significantly boost the emulated speed, at the cost of not seeing video.\n");

	printf("-wav <file.wav>[{,wait|,auto}]\n");
	printf("\tRecord a wav for the audio output.\n");
	printf("\tUse ,wait to start paused.\n");
	printf("\tUse ,auto to start paused, but begin recording once a non-zero audio signal is detected.\n");

	printf("-widescreen\n");
    printf("\tDisplay the emulated X16 in a 16:9 aspect ratio instead of 4:3.\n");

	printf("-wuninit\n");
	printf("\tPrint a warning whenever uninitialized RAM is accessed.\n");

	printf("-ymirq\n");
	printf("\tEnable the YM2151's IRQ generation.\n");

	printf("-ymstrict\n");
	printf("\tEnable strict enforcement of YM behaviors.\n");
	printf("\n");

	printf("\nThe following options are deprecated and will be ignored:\n\n");

	printf("-create_patch <target.bin>\n");
	//printf("\tCreate a patch from the current ROM image (specified by -rom) to this target ROM image.\n");

	printf("-ignore_patch\n");
	//printf("\tWhen loading ROM data, ignore the current patch file.\n");

	printf("-joy1\n");
	//printf("\tEnable binding a gamepad to SNES controller port 1\n");

	printf("-joy2\n");
	//printf("\tEnable binding a gamepad to SNES controller port 2\n");

	printf("-joy3\n");
	//printf("\tEnable binding a gamepad to SNES controller port 3\n");

	printf("-joy4\n");
	// printf("\tEnable binding a gamepad to SNES controller port 4\n");

	printf("-nopatch\n");
	//printf("\tThis is an alias for -ignore_patch.\n");

	printf("-patch <patch.bpf>\n");
	//printf("\tApply the following patch file to rom.\n");
	//printf("\tIf -create_patch has also been specified, this patch file is overwritten with the newly created patch data.\n");
	//printf("\tIf -ignore_patch has also specified, this patch file will not be applied during system boot.\n");

	exit(1);
}

// This must match the KERNAL's set!
static constexpr const char *keymaps[] = {
	"en-us",
	"en-us-int",
	"en-gb",
	"sv",
	"de",
	"da",
	"it",
	"pl",
	"nb",
	"hu",
	"es",
	"fi",
	"pt-br",
	"cz",
	"jp",
	"fr",
	"de-ch",
	"en-us-dvo",
	"et",
	"fr-be",
	"fr-ca",
	"is",
	"pt",
	"hr",
	"sk",
	"sl",
	"lv",
	"lt",
};

static constexpr const char *keymaps_strict[] = {
	"abc/x16",
	"en-us/int",
	"en-gb",
	"sv-se",
	"de-de",
	"da-dk",
	"it-it",
	"pl-pl",
	"nb-no",
	"hu-hu",
	"es-es",
	"fi-fi",
	"pt-br",
	"cs-cz",
	"ja-jp",
	"fr-fr",
	"de-ch",
	"en-us/dvo",
	"et-ee",
	"fr-be",
	"en-ca",
	"is-is",
	"pt-pt",
	"hr-hr",
	"sk-sk",
	"sl-si",
	"lv-lv",
	"lt-lt"
};

void usage_ram()
{
	printf("The following ram are supported:\n");
	for (int cmp = 8; cmp <= 2048; cmp *= 2) {
		printf("\t%d\n", cmp);
	}
	exit(1);
}

void usage_keymap()
{
	printf("The following keymaps are supported:\n");
	for (size_t i = 0; i < sizeof(keymaps) / sizeof(*keymaps); i++) {
		printf("\t%s\n", keymaps_strict[i]);
	}
	printf("\nAlternatively, the following labels may also be used:\n");
	for (size_t i = 0; i < sizeof(keymaps) / sizeof(*keymaps); i++) {
		printf("\t%s\n", keymaps[i]);
	}
	exit(1);
}

static void parse_cmdline(mINI::INIMap<std::string> &ini, int argc, char **argv)
{
	argc--;
	argv++;

	while (argc > 0) {
		if (argv[0][0] != '-') {
			usage();
		}
		char *v = argv[0] + 1;
		if (!strcmp(argv[0], "-abufs")) {
			argc--;
			argv++;
			if (!argc || argv[0][0] == '-') {
				usage();
			}

			ini["abufs"] = argv[0];
			argc--;
			argv++;

		} else if (!strcmp(argv[0], "-bas")) {
			argc--;
			argv++;
			if (!argc || argv[0][0] == '-') {
				usage();
			}

			ini["bas"] = argv[0];
			argc--;
			argv++;

		} else if (!strcmp(argv[0], "-create_patch")) {
			argc--;
			argv++;
			if (!argc || argv[0][0] == '-') {
				usage();
			}

			// Deprecated and ignored
			// ini["create_patch"] = "true";
			// ini["patch_target"] = argv[0];
			argc--;
			argv++;

		} else if (!strcmp(argv[0], "-debug")) {
			argc--;
			argv++;
			if (!argc || argv[0][0] == '-') {
				usage();
			}

			// Add a breakpoint
			uint32_t bp = strtol(argv[0], NULL, 16);
			Break_options.push_back(bp);

			argc--;
			argv++;

		} else if (!strcmp(argv[0], "-dump")) {
			argc--;
			argv++;
			if (!argc || argv[0][0] == '-') {
				usage();
			}

			ini["dump"] = argv[0];
			argc--;
			argv++;

		} else if (!strcmp(argv[0], "-echo")) {
			argc--;
			argv++;
			if (argc && argv[0][0] != '-') {

				ini["echo"] = argv[0];
				argc--;
				argv++;
			} else {
				ini["echo"] = "cooked";
			}

		} else if (!strcmp(argv[0], "-hypercall_path")) {
			argc--;
			argv++;
			if (!argc || argv[0][0] == '-') {
				usage();
			}

			ini["hypercall_path"] = argv[0];
			argc--;
			argv++;

		} else if (!strcmp(argv[0], "-geos")) {
			argc--;
			argv++;
			ini["geos"] = "true";

		} else if (!strcmp(argv[0], "-gif")) {
			argc--;
			argv++;
			if (!argc || argv[0][0] == '-') {
				usage();
			}

			ini["gif"] = argv[0];
			argv++;
			argc--;

		} else if (!strcmp(argv[0], "-help")) {
			argc--;
			argv++;

			usage();

		} else if (!strcmp(argv[0], "-ignore_ini")) {
			argc--;
			argv++;
			ini["ignore_ini"] = "true";

		} else if (!strcmp(argv[0], "-ignore_patch")) {
			argc--;
			argv++;
			// Deprecated and ignored
			// ini["ignore_patch"] = "true";

		} else if (!strcmp(argv[0], "-ini")) {
			argc--;
			argv++;

			if (!argc || argv[0][0] == '-') {
				usage();
			}

			ini["ini"] = argv[0];
			argv++;
			argc--;

		} else if (!strcmp(argv[0], "-keymap")) {
			argc--;
			argv++;
			if (!argc || argv[0][0] == '-') {
				usage_keymap();
			}

			ini["keymap"] = argv[0];
			argc--;
			argv++;

		} else if (!strcmp(argv[0], "-log")) {
			argc--;
			argv++;
			if (!argc || argv[0][0] == '-') {
				usage();
			}

			ini["log"] = argv[0];
			argc--;
			argv++;

		} else if (!strcmp(argv[0], "-nobinds")) {
			argc--;
			argv++;
			ini["nobinds"] = "true";

		} else if (!strcmp(argv[0], "-noemucmdkeys")) {
			argc--;
			argv++;
			ini["nobinds"] = "true";

		} else if (!strcmp(argv[0], "-nohostieee")) {
			argc--;
			argv++;
			ini["nohostieee"] = "true";

		} else if (!strcmp(argv[0], "-nohypercalls")) {
			argc--;
			argv++;
			ini["nohypercalls"] = "true";

		} else if (!strcmp(argv[0], "-nopanels")) {
			argc--;
			argv++;
			ini["nopanels"] = "true";

		} else if (!strcmp(argv[0], "-nopatch")) {
			argc--;
			argv++;
			// Deprecated and ignored
			// ini["ignore_patch"] = "true";

		} else if (!strcmp(argv[0], "-nosound")) {
			argc--;
			argv++;
			ini["nosound"] = "true";

		} else if (!strcmp(argv[0], "-nvram")) {
			argc--;
			argv++;
			if (!argc || argv[0][0] == '-') {
				usage();
			}

			ini["nvram"] = argv[0];
			argc--;
			argv++;

		} else if (!strcmp(argv[0], "-patch")) {
			argc--;
			argv++;
			if (!argc || argv[0][0] == '-') {
				usage();
			}

			// Deprecated and ignored
			// ini["patch"] = argv[0];
			argc--;
			argv++;

		} else if (!strcmp(argv[0], "-prg")) {
			argc--;
			argv++;
			if (!argc || argv[0][0] == '-') {
				usage();
			}

			ini["prg"] = argv[0];
			argc--;
			argv++;

		} else if (!strcmp(argv[0], "-quality")) {
			argc--;
			argv++;
			if (!argc || argv[0][0] == '-') {
				usage();
			}

			ini["quality"] = argv[0];
			argc--;
			argv++;

		} else if (!strcmp(argv[0], "-ram")) {
			argc--;
			argv++;
			if (!argc || argv[0][0] == '-') {
				usage_ram();
			}

			ini["ram"] = argv[0];
			argc--;
			argv++;

		} else if (!strcmp(argv[0], "-randram")) {
			argc--;
			argv++;
			ini["randram"] = "true";

		} else if (!strcmp(argv[0], "-rom")) {
			argc--;
			argv++;
			if (!argc || argv[0][0] == '-') {
				usage();
			}

			ini["rom"] = argv[0];
			argc--;
			argv++;

		} else if (!strcmp(argv[0], "-romcart")) {
			argc--;
			argv++;
			if (!argc || argv[0][0] == '-') {
				usage();
			}

			int bank;
			// If first argument after flag is a number, use that as the bank (multiple of 32) to load to
			if (sscanf(argv[0], "%d", &bank) == 1) {
				argc--;
				argv++;
				if (!argc || argv[0][0] == '-') {
					usage();	
				}
			} else {
				bank = 32;				
			}

			if (bank == 32) {
				ini["cart32"] = argv[0];
			} else if (bank == 64) {
				ini["cart64"] = argv[0];
			} else if (bank == 96) {
				ini["cart96"] = argv[0];
			} else if (bank == 128) {
				ini["cart128"] = argv[0];
			} else if (bank == 160) {
				ini["cart160"] = argv[0];
			} else if (bank == 192) {
				ini["cart192"] = argv[0];
			} else if (bank == 224) {
				ini["cart224"] = argv[0];
			} else {
				printf("bank must be a positive multiple of 32 between 32 and 224!\n");
				exit(1);
			}
			argc--;
			argv++;

		} else if (!strcmp(argv[0], " - rtc ")) {
			argc--;
			argv++;
			ini["rtc"] = "true";

		} else if (!strcmp(argv[0], "-run")) {
			argc--;
			argv++;
			ini["run"] = "true";

		} else if (!strcmp(argv[0], "-save_ini")) {
			argc--;
			argv++;
			ini["save_ini"] = "true";

		} else if (!strcmp(argv[0], "-scale")) {
			argc--;
			argv++;
			if (!argc || argv[0][0] == '-') {
				usage();
			}

			ini["scale"] = argv[0];
			argc--;
			argv++;

		} else if (!strcmp(argv[0], "-sdcard")) {
			argc--;
			argv++;
			if (!argc || argv[0][0] == '-') {
				usage();
			}

			ini["sdcard"] = argv[0];
			argc--;
			argv++;

		} else if (!strcmp(argv[0], "-serial")) {
			argc--;
			argv++;
			ini["serial"] = true;

		} else if (!strcmp(argv[0], "-sound")) {
			argc--;
			argv++;

			ini["sound"] = argv[0];
			argc--;
			argv++;

		} else if (!strcmp(argv[0], "-stds")) {
			argc--;
			argv++;
			if (!argc || argv[0][0] != '-') {
				usage();
			}

			ini["stds"] = "true";

		} else if (!strcmp(argv[0], "-sym")) {
			argc--;
			argv++;
			if (!argc || argv[0][0] == '-') {
				usage();
			}

			// Add a symbols file
			Sym_options.push_back({ argv[0], 0 });

			argc--;
			argv++;

		} else if (!strcmp(argv[0], "-test")) {
			argc--;
			argv++;
			if (!argc || argv[0][0] == '-') {
				usage();
			}

			ini["test"] = argv[0];
			argc--;
			argv++;

		} else if (!strcmp(argv[0], "-verbose")) {
			argc--;
			argv++;
			ini["verbose"] = "true";

		} else if (!strcmp(argv[0], "-version")) {
			argc--;
			argv++;
			printf("%s %s\n", VER_NUM, VER_NAME);
			exit(0);

		} else if (!strcmp(argv[0], "-vsync")) {
			argc--;
			argv++;
			if (!argc || argv[0][0] == '-') {
				usage();
			}

			ini["vsync"] = argv[0];
			argc--;
			argv++;

		} else if (!strcmp(argv[0], "-warp")) {
			argc--;
			argv++;

			if (argc && isdigit(argv[0][0])) {
				ini["warp"] = argv[0];
				argc--;
				argv++;
			} else {
				ini["warp"] = "true";			
			}
		} else if (!strcmp(argv[0], "-wav")) {
			argc--;
			argv++;
			if (!argc || argv[0][0] == '-') {
				usage();
			}

			ini["wav"] = argv[0];
			argv++;
			argc--;

		} else if (!strcmp(argv[0], "-widescreen")) {
			argc--;
			argv++;
			ini["widescreen"] = "true";

		} else if (!strcmp(argv[0], "-wuninit")) {
			argc--;
			argv++;
			ini["wuninit"] = "true";
	
		} else if (!strcmp(argv[0], "-ymirq")) {
			argc--;
			argv++;
			ini["ymirq"] = "true";

		} else if (!strcmp(argv[0], "-ymstrict")) {
			argc--;
			argv++;
			ini["ymstrict"] = "true";

		} else {
			usage();
		}
	}
}

static char const *set_options(options &opts, mINI::INIMap<std::string> &ini)
{
	if (ini.has("rom")) {
		opts.rom_path = ini["rom"];
	}

	if (ini.has("cart32")) {
		opts.cart32_path = ini["cart32"];
	}
	if (ini.has("cart64")) {
		opts.cart64_path = ini["cart64"];
	}
	if (ini.has("cart96")) {
		opts.cart96_path = ini["cart96"];
	}
	if (ini.has("cart128")) {
		opts.cart128_path = ini["cart128"];
	}
	if (ini.has("cart160")) {
		opts.cart160_path = ini["cart160"];
	}
	if (ini.has("cart192")) {
		opts.cart192_path = ini["cart192"];
	}
	if (ini.has("cart224")) {
		opts.cart224_path = ini["cart224"];
	}

	// Deprecated and ignored
	//if (ini.has("patch") && ini["patch"] != "") {
	//	opts.patch_path = ini["patch"];
	//	if (!ini.has("ignore_patch") || ini["ignore_patch"] != "true") {
	//		opts.apply_patch = true;
	//	}
	//}

	// Deprecated and ignored
	//if (ini.has("ignore_patch") && ini["ignore_patch"] == "true") {
	//	opts.apply_patch = false;
	//}

	if (ini.has("ram")) {
		int  kb    = atoi(ini["ram"].c_str());
		if (kb & 0x7) {
			return "ram";
		}
		opts.num_ram_banks = kb / 8;
	}

	if (ini.has("hypercall_path")) {
		opts.hyper_path = ini["hypercall_path"];
	}

	if (ini.has("keymap")) {
		bool found = false;
		for (uint8_t i = 0; i < sizeof(keymaps) / sizeof(*keymaps); i++) {
			if (!strcmp(ini["keymap"].c_str(), keymaps[i])) {
				found       = true;
				opts.keymap = i;
				break;
			}
		}
		if (!found) {
			for (uint8_t i = 0; i < sizeof(keymaps_strict) / sizeof(*keymaps_strict); i++) {
				if (!strcmp(ini["keymap"].c_str(), keymaps_strict[i])) {
					found       = true;
					opts.keymap = i;
					break;
				}
			}
		}
		if (!found) {
			return "keymap";
		}
	}

	if (ini.has("prg")) {
		opts.prg_path                    = token_or_empty(ini["prg"], ",");
		const char *prg_override_address = token_or_empty(nullptr, ",");
		opts.prg_override_start          = (uint16_t)strtol(prg_override_address, nullptr, 16);
	}

	if (ini.has("run") && ini["run"] == "true") {
		opts.run_after_load = true;
	}

	if (ini.has("bas")) {
		opts.bas_path = ini["bas"];
	}

	if (ini.has("geos") && ini["geos"] == "true") {
		opts.run_geos = true;
	}

	if (ini.has("test")) {
		opts.test_number = atoi(ini["test"].c_str());
		opts.run_test    = opts.test_number >= 0;
	}

	if (ini.has("nvram")) {
		opts.nvram_path = ini["nvram"];
	}

	if (ini.has("sdcard")) {
		opts.sdcard_path = ini["sdcard"];
	}

	if (ini.has("warp")) {
		if (ini["warp"] == "true") {
			opts.warp_factor = 9;
		} else {
			opts.warp_factor = atoi(ini["warp"].c_str());
		}
	}

	if (ini.has("echo")) {
		char const *echo_mode = ini["echo"].c_str();
		if (!strcmp(echo_mode, "raw")) {
			opts.echo_mode = echo_mode_t::ECHO_MODE_RAW;
		} else if (!strcmp(echo_mode, "iso")) {
			opts.echo_mode = echo_mode_t::ECHO_MODE_ISO;
		} else if (!strcmp(echo_mode, "cooked")) {
			opts.echo_mode = echo_mode_t::ECHO_MODE_COOKED;
		} else if (!strcmp(echo_mode, "none")) {
			opts.echo_mode = echo_mode_t::ECHO_MODE_NONE;
		} else {
			return "echo";
		}
	}

	if (ini.has("log")) {
		for (const char *p = ini["log"].c_str(); *p; p++) {
			switch (tolower(*p)) {
				case 'k':
					opts.log_keyboard = true;
					break;
				case 's':
					opts.log_speed = true;
					break;
				case 'v':
					opts.log_video = true;
					break;
				case 'c':
					p++;
					switch (tolower(*p)) {
						case 'l':
							opts.log_cpu_low = true;
							break;
						case 'm':
							opts.log_cpu_main = true;
							break;
						case 'a':
							opts.log_cpu_bram = true;
							break;
						case 'o':
							opts.log_cpu_brom = true;
							break;
						default:
							return "log";
					}
					break;
				case 'm':
					p++;
					switch (tolower(*p)) {
						case 'r':
							opts.log_mem_read = true;
							break;
						case 'w':
							opts.log_mem_write = true;
							break;
						default:
							return "log";
					}
					break;
				default:
					return "log";
			}
		}
	}

	if (ini.has("dump")) {
		opts.dump_cpu = false;
		opts.dump_ram = false;
		opts.dump_bank = false;
		opts.dump_vram = false;

		for (const char *p = ini["dump"].c_str(); *p; p++) {
			switch (tolower(*p)) {
				case 'c':
					opts.dump_cpu = true;
					break;
				case 'r':
					opts.dump_ram = true;
					break;
				case 'b':
					opts.dump_bank = true;
					break;
				case 'v':
					opts.dump_vram = true;
					break;
				default:
					return "dump";
			}
		}
	}

	if (ini.has("gif")) {
		opts.gif_path     = token_or_empty(ini["gif"], ",");
		char const *start = token_or_empty(nullptr, ",");
		if (start[0] == '\0') {
			opts.gif_start = gif_recorder_start_t::GIF_RECORDER_START_NOW;
		} else if (strcmp(start, "wait") == 0) {
			opts.gif_start = gif_recorder_start_t::GIF_RECORDER_START_WAIT;
		} else {
			return "gif";
		}
	}

	if (ini.has("wav")) {
		opts.wav_path     = token_or_empty(ini["wav"], ",");
		char const *start = token_or_empty(nullptr, ",");
		if (start[0] == '\0') {
			opts.wav_start = wav_recorder_start_t::WAV_RECORDER_START_NOW;
		} else if (strcmp(start, "wait") == 0) {
			opts.wav_start = wav_recorder_start_t::WAV_RECORDER_START_WAIT;
		} else if (strcmp(start, "auto") == 0) {
			opts.wav_start = wav_recorder_start_t::WAV_RECORDER_START_AUTO;
		} else {
			return "wav";
		}
	}

	if (ini.has("stds")) {
		Sym_options.push_back({ "kernal.sym", 0 });
		Sym_options.push_back({ "keymap.sym", 1 });
		Sym_options.push_back({ "dos.sym", 2 });
		Sym_options.push_back({ "geos.sym", 3 });
		Sym_options.push_back({ "basic.sym", 4 });
		Sym_options.push_back({ "monitor.sym", 5 });
		Sym_options.push_back({ "charset.sym", 0 });
	}

	if (ini.has("scale")) {
		for (const char *p = ini["scale"].c_str(); *p; p++) {
			switch (tolower(*p)) {
				case '1':
				case '2':
				case '3':
				case '4':
					opts.window_scale = *p - '0';
					break;
				default:
					return "scale";
			}
		}
	}

	if (ini.has("quality")) {
		char const *q = ini["quality"].c_str();
		if (!strcmp(q, "nearest")) {
			opts.scale_quality = scale_quality_t::NEAREST;
		} else if (!strcmp(q, "linear")) {
			opts.scale_quality = scale_quality_t::LINEAR;
		} else if (!strcmp(q, "best")) {
			opts.scale_quality = scale_quality_t::BEST;
		} else {
			return "quality";
		}
	}

	if (ini.has("vsync")) {
		char const *q = ini["vsync"].c_str();
		if (!strcmp(q, "none")) {
			opts.vsync_mode = vsync_mode_t::VSYNC_MODE_NONE;
		} else if (!strcmp(q, "get")) {
			opts.vsync_mode = vsync_mode_t::VSYNC_MODE_GET_SYNC;
		} else if (!strcmp(q, "wait")) {
			opts.vsync_mode = vsync_mode_t::VSYNC_MODE_WAIT_SYNC;
		} else if (!strcmp(q, "debug")) {
			opts.vsync_mode = vsync_mode_t::VSYNC_MODE_DEBUG;
		} else {
			return "vsync";
		}
	}

	if (ini.has("serial") && ini["serial"] == "true") {
		opts.enable_serial = true;
	}

	if (ini.has("nosound") && ini["nosound"] == "true") {
		if (ini.has("sound")) {
			return "nosound";
		}
		opts.no_sound = true;
	}

	if (ini.has("sound")) {
		if (ini.has("nosound") && ini["nosound"] == "true") {
			return "sound";
		}
		opts.no_sound       = false;
		opts.audio_dev_name = ini["sound"];
	}

	if (ini.has("abufs")) {
		opts.audio_buffers = (int)strtol(ini["abufs"].c_str(), NULL, 10);
	}

	if (ini.has("rtc") && ini["rtc"] == "true") {
		opts.set_system_time = true;
	}

	if (ini.has("nobinds") && ini["nobinds"] == "true") {
		opts.no_keybinds = true;
	}

	if (ini.has("nohostieee") && ini["nohostieee"] == "true") {
		opts.no_ieee_hypercalls = true;
	}

	if (ini.has("nohypercalls") && ini["nohypercalls"] == "true") {
		opts.no_hypercalls = true;
	}

	if (ini.has("ymirq") && ini["ymirq"] == "true") {
		opts.ym_irq = true;
	}

	if (ini.has("ymstrict") && ini["ymstrict"] == "true") {
		opts.ym_strict = true;
	}

	if (ini.has("widescreen") && ini["widescreen"] == "true") {
		opts.widescreen = true;
	}

	if (ini.has("randram") && ini["randram"] == "true") {
		opts.memory_randomize = true;
	}

	if (ini.has("wuninit") && ini["wuninit"] == "true") {
		opts.memory_uninit_warn = true;
	}

	return nullptr;
}

static void set_panels(mINI::INIMap<std::string> &ini)
{
	auto get_option = [&](const char *name, auto &option_value) {
		if (ini.has(name) && ini[name] == "true") {
			option_value = true;
		}
	};

	get_option("memory_dump_1", Show_memory_dump_1);
	get_option("memory_dump_2", Show_memory_dump_2);
	get_option("cpu_monitor", Show_cpu_monitor);
	get_option("disassembler", Show_disassembler);
	get_option("breakpoints", Show_breakpoints);
	get_option("watch_list", Show_watch_list);
	get_option("symbols_list", Show_symbols_list);
	get_option("symbols_files", Show_symbols_files);
	get_option("cpu_visualizer", Show_cpu_visualizer);
	get_option("vram_visualizer", Show_VRAM_visualizer);
	get_option("vera_monitor", Show_VERA_monitor);
	get_option("vera_palette", Show_VERA_palette);
	get_option("vera_layers", Show_VERA_layers);
	get_option("vera_sprites", Show_VERA_sprites);
	get_option("vera_psg_monitor", Show_VERA_PSG_monitor);
	get_option("ym2151_monitor", Show_YM2151_monitor);
	get_option("midi_overlay", Show_midi_overlay);
}

static void set_ini_main(mINI::INIMap<std::string> &ini_main, bool all)
{
	std::stringstream value;

	auto set_option = [&](const char *name, auto option_value, auto default_value) {
		if constexpr (std::is_same<decltype(option_value), char *>::value) {
			if (all || strcmp(option_value, default_value)) {
				ini_main[name] = option_value;
			}
		} else if constexpr (std::is_same<decltype(option_value), const char *>::value) {
			if (all || strcmp(option_value, default_value)) {
				ini_main[name] = option_value;
			}
		} else if constexpr (std::is_same<decltype(option_value), bool>::value) {
			if (all || option_value != default_value) {
				ini_main[name] = option_value ? "true" : "false";
			}
		} else if constexpr (std::is_same<decltype(option_value), std::filesystem::path>::value) {
			if (all || option_value != default_value) {
				ini_main[name] = option_value.generic_string();
			}
		} else {
			if (all || option_value != default_value) {
				value << option_value;
				ini_main[name] = value.str();
				value.str(std::string());
			}
		}
	};

	auto set_comma_option = [&](const char *name, auto option_value1, auto default_value1, auto option_value2, auto default_value2) {
		if constexpr (std::is_same<decltype(option_value1), char *>::value) {
			if (all || strcmp(option_value1, default_value1)) {
				ini_main[name] = option_value1;
			}
		} else if constexpr (std::is_same<decltype(option_value1), const char *>::value) {
			if (all || strcmp(option_value1, default_value1)) {
				ini_main[name] = option_value1;
			}
		} else if constexpr (std::is_same<decltype(option_value1), bool>::value) {
			if (all || option_value1 != default_value1) {
				ini_main[name] = option_value1 ? "true" : "false";
			}
		} else if constexpr (std::is_same<decltype(option_value1), std::filesystem::path>::value) {
			if (all || option_value1 != default_value1) {
				ini_main[name] = option_value1.generic_string();
			}
		} else {
			if (all || option_value1 != default_value1) {
				value << option_value1;
				ini_main[name] = value.str();
				value.str(std::string());
			}
		}

		if constexpr (std::is_same<decltype(option_value2), char *>::value) {
			if (all || strcmp(option_value2, default_value2)) {
				ini_main[name].append(",");
				ini_main[name].append(option_value2);
			}
		} else if constexpr (std::is_same<decltype(option_value2), const char *>::value) {
			if (all || strcmp(option_value2, default_value2)) {
				ini_main[name].append(",");
				ini_main[name].append(option_value2);
			}
		} else if constexpr (std::is_same<decltype(option_value2), bool>::value) {
			if (all || option_value2 != default_value2) {
				ini_main[name].append(",");
				ini_main[name].append(option_value2 ? "true" : "false");
			}
		} else if constexpr (std::is_same<decltype(option_value2), std::filesystem::path>::value) {
			if (all || option_value2 != default_value2) {
				ini_main[name].append(",");
				ini_main[name].append(option_value2.generic_string());
			}
		} else {
			if (all || option_value2 != default_value2) {
				ini_main[name].append(",");
				value << option_value2;
				ini_main[name].append(value.str());
				value.str(std::string());
			}
		}
	};

	auto echo_mode_str = [](echo_mode_t mode) -> const char * {
		switch (mode) {
			case echo_mode_t::ECHO_MODE_NONE: return "none";
			case echo_mode_t::ECHO_MODE_RAW: return "raw";
			case echo_mode_t::ECHO_MODE_ISO: return "iso";
			case echo_mode_t::ECHO_MODE_COOKED: return "cooked";
		}
		return "none";
	};

	auto quality_str = [](scale_quality_t q) -> const char * {
		switch (q) {
			case scale_quality_t::NEAREST: return "nearest";
			case scale_quality_t::LINEAR: return "linear";
			case scale_quality_t::BEST: return "best";
		}
		return "nearest";
	};

	auto vsync_mode_str = [](vsync_mode_t mode) -> const char * {
		switch (mode) {
			case vsync_mode_t::VSYNC_MODE_NONE: return "none";
			case vsync_mode_t::VSYNC_MODE_GET_SYNC: return "get";
			case vsync_mode_t::VSYNC_MODE_WAIT_SYNC: return "wait";
			case vsync_mode_t::VSYNC_MODE_DEBUG: return "debug";
		}
		return "none";
	};

	auto gif_recorder_start_str = [](gif_recorder_start_t mode) -> const char * {
		switch (mode) {
			case gif_recorder_start_t::GIF_RECORDER_START_NOW: return "now";
			case gif_recorder_start_t::GIF_RECORDER_START_WAIT: return "wait";
		}
		return "now";
	};

	auto wav_recorder_start_str = [](wav_recorder_start_t mode) -> const char * {
		switch (mode) {
			case wav_recorder_start_t::WAV_RECORDER_START_NOW: return "now";
			case wav_recorder_start_t::WAV_RECORDER_START_WAIT: return "wait";
			case wav_recorder_start_t::WAV_RECORDER_START_AUTO: return "auto";
		}
		return "now";
	};

	set_option("rom", Options.rom_path, Default_options.rom_path);
	set_option("cart32", Options.cart32_path, Default_options.cart32_path);
	set_option("cart64", Options.cart64_path, Default_options.cart64_path);
	set_option("cart96", Options.cart96_path, Default_options.cart96_path);
	set_option("cart128", Options.cart128_path, Default_options.cart128_path);
	set_option("cart160", Options.cart160_path, Default_options.cart160_path);
	set_option("cart192", Options.cart192_path, Default_options.cart192_path);
	set_option("cart224", Options.cart224_path, Default_options.cart224_path);
	// Deprecated and ignored
	//set_option("patch", Options.patch_path, Default_options.patch_path);
	//set_option("ignore_patch", !Options.apply_patch, Options.patch_path.empty());
	set_option("ram", Options.num_ram_banks * 8, Default_options.num_ram_banks * 8);
	set_option("keymap", keymaps_strict[Options.keymap], keymaps_strict[Default_options.keymap]);
	set_option("hypercall_path", Options.hyper_path, Default_options.hyper_path);
	set_comma_option("prg", Options.prg_path, Default_options.prg_path, Options.prg_override_start, Default_options.prg_override_start);
	set_option("run", Options.run_after_load, Default_options.run_after_load);
	set_option("bas", Options.bas_path, Default_options.bas_path);
	set_option("geos", Options.run_geos, Default_options.run_geos);
	set_option("test", Options.test_number, Default_options.test_number);
	set_option("nvram", Options.nvram_path, Default_options.nvram_path);
	set_option("sdcard", Options.sdcard_path, Default_options.sdcard_path);
	set_option("warp", Options.warp_factor > 0, Default_options.warp_factor > 0);
	set_option("echo", echo_mode_str(Options.echo_mode), echo_mode_str(Default_options.echo_mode));

	if (all || Options.log_keyboard != Default_options.log_keyboard || Options.log_speed != Default_options.log_speed || Options.log_video != Default_options.log_video) {
		if (Options.log_keyboard) {
			value << "k";
		}
		if (Options.log_speed) {
			value << "s";
		}
		if (Options.log_video) {
			value << "v";
		}
		ini_main["log"] = value.str();
		value.str(std::string());
	}

	if (all || Options.dump_cpu != Default_options.dump_cpu || Options.dump_ram != Default_options.dump_ram || Options.dump_bank != Default_options.dump_bank || Options.dump_vram != Default_options.dump_vram) {
		if (Options.dump_cpu) {
			value << "c";
		}
		if (Options.dump_ram) {
			value << "r";
		}
		if (Options.dump_bank) {
			value << "b";
		}
		if (Options.dump_vram) {
			value << "v";
		}
		ini_main["dump"] = value.str();
		value.str(std::string());
	}

	set_comma_option("gif", Options.gif_path, Default_options.gif_path, gif_recorder_start_str(Options.gif_start), gif_recorder_start_str(Default_options.gif_start));
	set_comma_option("wav", Options.wav_path, Default_options.wav_path, wav_recorder_start_str(Options.wav_start), wav_recorder_start_str(Default_options.wav_start));
	set_option("stds", Options.load_standard_symbols, Default_options.load_standard_symbols);
	set_option("scale", Options.window_scale, Default_options.window_scale);
	set_option("quality", quality_str(Options.scale_quality), quality_str(Default_options.scale_quality));
	set_option("vsync", vsync_mode_str(Options.vsync_mode), vsync_mode_str(Default_options.vsync_mode));
	set_option("nosound", Options.no_sound, Default_options.no_sound);
	set_option("sound", Options.audio_dev_name, Default_options.audio_dev_name);
	set_option("abufs", Options.audio_buffers, Default_options.audio_buffers);
	set_option("rtc", Options.set_system_time, Default_options.set_system_time);
	set_option("nobinds", Options.no_keybinds, Default_options.no_keybinds);
	set_option("nohostieee", Options.no_ieee_hypercalls, Default_options.no_ieee_hypercalls);
	set_option("nohypercalls", Options.no_hypercalls, Default_options.no_hypercalls);
	set_option("serial", Options.enable_serial, Default_options.enable_serial);
	set_option("ymirq", Options.ym_irq, Default_options.ym_irq);
	set_option("ymstrict", Options.ym_strict, Default_options.ym_strict);
	set_option("widescreen", Options.widescreen, Default_options.widescreen);
	set_option("randram", Options.memory_randomize, Default_options.memory_randomize);
	set_option("wuninit", Options.memory_uninit_warn, Default_options.memory_uninit_warn);
}

void set_ini_panels(mINI::INIMap<std::string> &ini, bool all)
{
	std::stringstream value;

	auto set_option = [&](const char *name, auto option_value, auto default_value) {
		if constexpr (std::is_same<decltype(option_value), char *>::value) {
			if (all || strcmp(option_value, default_value)) {
				ini[name] = option_value;
			} else {
				ini.remove(name);
			}
		} else if constexpr (std::is_same<decltype(option_value), const char *>::value) {
			if (all || strcmp(option_value, default_value)) {
				ini[name] = option_value;
			} else {
				ini.remove(name);
			}
		} else if constexpr (std::is_same<decltype(option_value), bool>::value) {
			if (all || option_value != default_value) {
				ini[name] = option_value ? "true" : "false";
			} else {
				ini.remove(name);
			}
		} else if constexpr (std::is_same<decltype(option_value), std::filesystem::path>::value) {
			if (all || option_value != default_value) {
				ini[name] = option_value.generic_string();
			} else {
				ini.remove(name);
			}
		} else {
			if (all || option_value != default_value) {
				value << option_value;
				ini[name] = value.str();
				value.str(std::string());
			} else {
				ini.remove(name);
			}
		}
	};

	set_option("memory_dump_1", Show_memory_dump_1, false);
	set_option("memory_dump_2", Show_memory_dump_2, false);
	set_option("cpu_monitor", Show_cpu_monitor, false);
	set_option("disassembler", Show_disassembler, false);
	set_option("breakpoints", Show_breakpoints, false);
	set_option("watch_list", Show_watch_list, false);
	set_option("symbols_list", Show_symbols_list, false);
	set_option("symbols_files", Show_symbols_files, false);
	set_option("cpu_visualizer", Show_cpu_visualizer, false);
	set_option("vram_visualizer", Show_VRAM_visualizer, false);
	set_option("vera_monitor", Show_VERA_monitor, false);
	set_option("vera_palette", Show_VERA_palette, false);
	set_option("vera_layers", Show_VERA_layers, false);
	set_option("vera_sprites", Show_VERA_sprites, false);
	set_option("vera_psg_monitor", Show_VERA_PSG_monitor, false);
	set_option("ym2151_monitor", Show_YM2151_monitor, false);
	set_option("midi_overlay", Show_midi_overlay, false);
}

void apply_ini(mINI::INIStructure &dst, const mINI::INIStructure &src)
{
	for (const auto &i : src) {
		dst.set(i.first, i.second);
	}
}

void options_init(const char *base_path, const char *prefs_path, int argc, char **argv)
{
	if (base_path != nullptr) {
		Options_base_path = base_path;
	} else {
		Options_base_path = ".";
	}

	if (prefs_path != nullptr) {
		Options_prefs_path = prefs_path;
	} else {
		Options_prefs_path = ".";
	}

	auto &cmdline_main = Cmdline_ini["main"];
	parse_cmdline(cmdline_main, argc, argv);

	if (cmdline_main.has("verbose")) {
		Options.log_verbose = true;
	}

	// Deprecated and ignored
	//if (cmdline_main.has("create_patch")) {
	//	if (cmdline_main["create_patch"] == "true") {
	//		Options.create_patch = true;
	//		Options.patch_target = cmdline_main["patch_target"];
	//	}
	//}

	bool have_ini = false;
	if (cmdline_main.has("ini")) {
		have_ini = options_find_file(Options_ini_path, std::filesystem::absolute(cmdline_main["ini"]));
	} else {
		have_ini = options_find_file(Options_ini_path, "box16.ini");
	}

	if (!cmdline_main.has("ignore_ini") || cmdline_main["ignore_ini"] != "true") {
		if (have_ini) {
			mINI::INIFile file(Options_ini_path.generic_string());
			file.read(Inifile_ini);
		} else {
			options_get_prefs_path(Options_ini_path, "box16.ini");
		}
	}

	auto &inifile_main = Inifile_ini["main"];

	auto apply_ini = [&](mINI::INIMap<std::string> &ini) {
		const char *fail = set_options(Options, ini);
		if (fail != nullptr) {
			printf("Error applying ini file option \"%s\"\n", fail);

			if (!strcmp(fail, "ram")) {
				usage_ram();
			} else if (!strcmp(fail, "keymap")) {
				usage_keymap();
			} else {
				usage();
			}
			exit(1);
		}
	};

	apply_ini(inifile_main);
	apply_ini(cmdline_main);

	if (!cmdline_main.has("nopanels") || cmdline_main["nopanels"] != "true") {
		set_panels(Inifile_ini["panels"]);
	}

	if (cmdline_main.has("save_ini")) {
		save_options(false);
	}
}

void load_options()
{
	mINI::INIFile      file(Options_ini_path.generic_string());
	mINI::INIStructure ini;

	bool force_write = !file.read(ini);

	if (!force_write) {
		set_options(Options, ini["main"]);
		set_panels(ini["panels"]);
	} else {
		save_options(true);
	}
}

void save_options(bool all)
{
	options_log_verbose("Saving ini to: %s\n", std::filesystem::absolute(Options_ini_path).generic_string().c_str());

	mINI::INIFile      file(Options_ini_path.generic_string());
	mINI::INIStructure ini;

	set_ini_main(ini["main"], all);
	set_ini_panels(ini["panels"], all);

	file.generate(ini);

	Inifile_ini = ini;
}

void save_options_on_close(bool all)
{
	options_log_verbose("Saving ini (on close) to: %s\n", std::filesystem::absolute(Options_ini_path).generic_string().c_str());

	mINI::INIFile file(Options_ini_path.generic_string());

	set_ini_panels(Inifile_ini["panels"], all);

	file.generate(Inifile_ini);
}

void options_apply_debugger_opts()
{
	for (uint32_t bp : Break_options) {
		debugger_add_breakpoint((uint16_t)(bp & 0xffff), (uint8_t)(bp >> 16));
	}

	for (auto &sym : Sym_options) {
		symbols_load_file(sym.first, sym.second);
	}
}

size_t options_get_base_path(std::filesystem::path &real_path, const std::filesystem::path &path)
{
	real_path = Options_base_path / path;
	return real_path.generic_string().length();
}

size_t options_get_prefs_path(std::filesystem::path &real_path, const std::filesystem::path &path)
{
	real_path = Options_prefs_path / path;
	return real_path.generic_string().length();
}

size_t options_get_hyper_path(std::filesystem::path &real_path, const std::filesystem::path &path)
{
	real_path = Options.hyper_path / path;
	return real_path.generic_string().length();
}

bool option_cmdline_option_was_set(char const *cmdline_option)
{
	return Cmdline_ini["main"].has(cmdline_option);
}

bool option_inifile_option_was_set(char const *cmdline_option)
{
	return Inifile_ini["main"].has(cmdline_option);
}

option_source option_get_source(char const *cmdline_option)
{
	if (Cmdline_ini["main"].has(cmdline_option)) {
		return option_source::CMDLINE;
	}
	if (Inifile_ini["main"].has(cmdline_option)) {
		return option_source::INIFILE;
	}
	return option_source::DEFAULT;
}

char const *option_get_source_name(option_source source)
{
	switch (source) {
		case option_source::DEFAULT: return "Default";
		case option_source::CMDLINE: return "Command-line parameter";
		case option_source::INIFILE: return "Ini file";
	}
	return "Unknown";
}

bool options_find_file(std::filesystem::path &real_path, const std::filesystem::path &search_path)
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
		real_path = Options_base_path / search_path;
		if (std::filesystem::exists(real_path)) {
			options_log_verbose("Found file: %s (%s)\n", real_path.generic_string().c_str(), std::filesystem::absolute(real_path).generic_string().c_str());
			return true;
		}

		// 3. Relative to the prefs directory
		real_path = Options_prefs_path / search_path;
		if (std::filesystem::exists(real_path)) {
			options_log_verbose("Found file: %s (%s)\n", real_path.generic_string().c_str(), std::filesystem::absolute(real_path).generic_string().c_str());
			return true;
		}
	}

	printf("Could not find %s in the following locations:\n", search_path.generic_string().c_str());
	printf("\t%s\n", search_path.generic_string().c_str());
	if (!search_path.is_absolute()) {
		printf("\t%s\n", (Options_base_path / search_path).generic_string().c_str());
		printf("\t%s\n", (Options_prefs_path / search_path).generic_string().c_str());
	}
	return false;
}

int options_log_verbose(const char *format, ...)
{
	if (Options.log_verbose) {
		va_list ap;
		va_start(ap, format);
		int result = vprintf(format, ap);
		va_end(ap);
		return result;
	}
	return 0;
}
