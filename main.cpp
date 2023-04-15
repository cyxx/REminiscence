
/*
 * REminiscence - Flashback interpreter
 * Copyright (C) 2005-2019 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include <SDL.h>
#include <ctype.h>
#include <getopt.h>
#include <sys/stat.h>
#include "file.h"
#include "fs.h"
#include "game.h"
#include "scaler.h"
#include "systemstub.h"
#include "util.h"

static const char *USAGE =
	"REminiscence - Flashback Interpreter\n"
	"Usage: %s [OPTIONS]...\n"
	"  --datapath=PATH   Path to data files (default 'DATA')\n"
	"  --savepath=PATH   Path to save files (default '.')\n"
	"  --levelnum=NUM    Start to level, bypass introduction\n"
	"  --fullscreen      Fullscreen display\n"
	"  --widescreen=MODE 16:9 display (adjacent,mirror,blur,none)\n"
	"  --scaler=NAME@X   Graphics scaler (default 'scale@3')\n"
	"  --language=LANG   Language (fr,en,de,sp,it,jp)\n"
	"  --autosave        Save game state automatically\n"
	"  --mididriver=MIDI Driver (adlib, mt32)\n"
;

static int detectVersion(FileSystem *fs) {
	static const struct {
		const char *filename;
		int type;
		const char *name;
	} table[] = {
		{ "DEMO_UK.ABA", kResourceTypeDOS, "DOS (Demo)" },
		{ "GLOB_FR.ABA", kResourceTypeDOS, "DOS" },
		{ "INTRO.SEQ", kResourceTypeDOS, "DOS CD" },
		{ "MENU1SSI.MAP", kResourceTypeDOS, "DOS SSI" },
		{ "LEVEL1.MAP", kResourceTypeDOS, "DOS" },
		{ "LEVEL1.BNQ", kResourceTypeDOS, "DOS (Demo)" },
		{ "LEVEL1.LEV", kResourceTypeAmiga, "Amiga" },
		{ "DEMO.LEV", kResourceTypeAmiga, "Amiga (Demo)" },
		{ "FLASHBACK.BIN", kResourceTypeMac, "Macintosh" },
		{ "FLASHBACK.RSRC", kResourceTypeMac, "Macintosh" },
		{ 0, -1, 0 }
	};
	for (int i = 0; table[i].filename; ++i) {
		File f;
		if (f.open(table[i].filename, "rb", fs)) {
			debug(DBG_INFO, "Detected %s version", table[i].name);
			return table[i].type;
		}
	}
	return -1;
}

static Language detectLanguage(FileSystem *fs) {
	static const struct {
		const char *filename;
		Language language;
	} table[] = {
		// DOS
		{ "GLOB_FR.ABA", LANG_FR },
		{ "ENGCINE.TXT", LANG_EN },
		{ "FR_CINE.TXT", LANG_FR },
		{ "GERCINE.TXT", LANG_DE },
		{ "SPACINE.TXT", LANG_SP },
		{ "ITACINE.TXT", LANG_IT },
		// Amiga
		{ "FRCINE.TXT", LANG_FR },
		{ 0, LANG_EN }
	};
	for (int i = 0; table[i].filename; ++i) {
		File f;
		if (f.open(table[i].filename, "rb", fs)) {
			return table[i].language;
		}
	}
	warning("Unable to detect language, defaults to English");
	return LANG_EN;
}

Options g_options;
const char *g_caption = "REminiscence";

static void initOptions() {
	// defaults
	g_options.bypass_protection = true;
	g_options.enable_password_menu = false;
	g_options.enable_language_selection = false;
	g_options.fade_out_palette = true;
	g_options.use_text_cutscenes = false;
	g_options.use_seq_cutscenes = true;
	g_options.use_words_protection = false;
	g_options.use_white_tshirt = false;
	g_options.use_prf_music = true;
	g_options.play_asc_cutscene = false;
	g_options.play_caillou_cutscene = false;
	g_options.play_metro_cutscene = false;
	g_options.play_serrure_cutscene = false;
	g_options.play_carte_cutscene = false;
	g_options.play_gamesaved_sound = false;
	g_options.restore_memo_cutscene = true;
	g_options.order_inventory_original = false;
	// read configuration file
	struct {
		const char *name;
		bool *value;
	} opts[] = {
		{ "bypass_protection", &g_options.bypass_protection },
		{ "enable_password_menu", &g_options.enable_password_menu },
		{ "enable_language_selection", &g_options.enable_language_selection },
		{ "fade_out_palette", &g_options.fade_out_palette },
		{ "use_tile_data", &g_options.use_tile_data },
		{ "use_text_cutscenes", &g_options.use_text_cutscenes },
		{ "use_seq_cutscenes", &g_options.use_seq_cutscenes },
		{ "use_words_protection", &g_options.use_words_protection },
		{ "use_white_tshirt", &g_options.use_white_tshirt },
		{ "use_prf_music", &g_options.use_prf_music },
		{ "play_asc_cutscene", &g_options.play_asc_cutscene },
		{ "play_caillou_cutscene", &g_options.play_caillou_cutscene },
		{ "play_metro_cutscene", &g_options.play_metro_cutscene },
		{ "play_serrure_cutscene", &g_options.play_serrure_cutscene },
		{ "play_carte_cutscene", &g_options.play_carte_cutscene },
		{ "play_gamesaved_sound", &g_options.play_gamesaved_sound },
		{ "restore_memo_cutscene", &g_options.restore_memo_cutscene },
		{ "order_inventory_original", &g_options.order_inventory_original },
		{ 0, 0 }
	};
	static const char *filename = "rs.cfg";
	FILE *fp = fopen(filename, "rb");
	if (fp) {
		char buf[256];
		while (fgets(buf, sizeof(buf), fp)) {
			if (buf[0] == '#') {
				continue;
			}
			const char *p = strchr(buf, '=');
			if (p) {
				++p;
				while (*p && isspace(*p)) {
					++p;
				}
				if (*p) {
					const bool value = (*p == 't' || *p == 'T' || *p == '1');
					bool foundOption = false;
					for (int i = 0; opts[i].name; ++i) {
						if (strncmp(buf, opts[i].name, strlen(opts[i].name)) == 0) {
							*opts[i].value = value;
							foundOption = true;
							break;
						}
					}
					if (!foundOption) {
						warning("Unhandled option '%s', ignoring", buf);
					}
				}
			}
		}
		fclose(fp);
	}
}

static void parseScaler(char *name, ScalerParameters *scalerParameters) {
	char *sep = strchr(name, '@');
	if (sep) {
		*sep = 0;
		scalerParameters->factor = atoi(sep + 1);
	}
	strncpy(scalerParameters->name, name, sizeof(scalerParameters->name) - 1);
	scalerParameters->name[sizeof(scalerParameters->name) - 1] = 0;
}

static WidescreenMode parseWidescreen(const char *mode) {
	static const struct {
		const char *name;
		WidescreenMode mode;
	} modes[] = {
		{ "adjacent", kWidescreenAdjacentRooms },
		{ "mirror", kWidescreenMirrorRoom },
		{ "blur", kWidescreenBlur },
		{ "cdi", kWidescreenCDi },
		{ 0, kWidescreenNone },
	};
	for (int i = 0; modes[i].name; ++i) {
		if (strcasecmp(modes[i].name, mode) == 0) {
			return modes[i].mode;
		}
	}
	warning("Unhandled widecreen mode '%s', defaults to 16:9 blur", mode);
	return kWidescreenBlur;
}

int main(int argc, char *argv[]) {
	const char *dataPath = "DATA";
	const char *savePath = ".";
	int levelNum = 0;
	bool fullscreen = false;
	bool autoSave = false;
	uint32_t cheats = 0;
	WidescreenMode widescreen = kWidescreenNone;
	ScalerParameters scalerParameters = ScalerParameters::defaults();
	int forcedLanguage = -1;
	int midiDriver = MODE_ADLIB;
	if (argc == 2) {
		// data path as the only command line argument
		struct stat st;
		if (stat(argv[1], &st) == 0 && S_ISDIR(st.st_mode)) {
			dataPath = strdup(argv[1]);
		}
	}
	while (1) {
		static struct option options[] = {
			{ "datapath",   required_argument, 0, 1 },
			{ "savepath",   required_argument, 0, 2 },
			{ "levelnum",   required_argument, 0, 3 },
			{ "fullscreen", no_argument,       0, 4 },
			{ "scaler",     required_argument, 0, 5 },
			{ "language",   required_argument, 0, 6 },
			{ "widescreen", required_argument, 0, 7 },
			{ "autosave",   no_argument,       0, 8 },
			{ "cheats",     required_argument, 0, 9 },
			{ "mididriver", required_argument, 0, 10 },
			{ 0, 0, 0, 0 }
		};
		int index;
		const int c = getopt_long(argc, argv, "", options, &index);
		if (c == -1) {
			break;
		}
		switch (c) {
		case 1:
			dataPath = strdup(optarg);
			break;
		case 2:
			savePath = strdup(optarg);
			break;
		case 3:
			levelNum = atoi(optarg);
			break;
		case 4:
			fullscreen = true;
			break;
		case 5:
			parseScaler(optarg, &scalerParameters);
			break;
		case 6: {
				static const struct {
					int lang;
					const char *str;
				} languages[] = {
					{ LANG_FR, "FR" },
					{ LANG_EN, "EN" },
					{ LANG_DE, "DE" },
					{ LANG_SP, "SP" },
					{ LANG_IT, "IT" },
					{ LANG_JP, "JP" },
					{ -1, 0 }
				};
				int i = 0;
				for (; languages[i].str; ++i) {
					if (strcasecmp(languages[i].str, optarg) == 0) {
						forcedLanguage = languages[i].lang;
						break;
					}
				}
				if (!languages[i].str) {
					warning("Invalid language '%s'", optarg);
				}
			}
			break;
		case 7:
			widescreen = parseWidescreen(optarg);
			break;
		case 8:
			autoSave = true;
			break;
		case 9:
			cheats = atoi(optarg);
			break;
		case 10: {
				static const struct {
					int mode;
					const char *str;
				} drivers[] = {
					{ MODE_ADLIB, "adlib" },
					{ MODE_MT32, "mt32" },
					{ -1, 0 }
				};
				int i = 0;
				for (; drivers[i].str; ++i) {
					if (strcasecmp(drivers[i].str, optarg) == 0) {
						midiDriver = drivers[i].mode;
						break;
					}
				}
				if (!drivers[i].str) {
					warning("Invalid MIDI driver '%s'", optarg);
				}
			}
			break;
		default:
			printf(USAGE, argv[0]);
			return 0;
		}
	}
	initOptions();
	g_debugMask = DBG_INFO; // DBG_CUT | DBG_VIDEO | DBG_RES | DBG_MENU | DBG_PGE | DBG_GAME | DBG_UNPACK | DBG_COL | DBG_MOD | DBG_SFX | DBG_FILE;
	FileSystem fs(dataPath);
	const int version = detectVersion(&fs);
	if (version == -1) {
		error("Unable to find data files, check that all required files are present");
		return -1;
	}
	const Language language = (forcedLanguage == -1) ? detectLanguage(&fs) : (Language)forcedLanguage;
	SystemStub *stub = SystemStub_SDL_create();
	Game *g = new Game(stub, &fs, savePath, levelNum, (ResourceType)version, language, widescreen, autoSave, midiDriver, cheats);
	stub->init(g_caption, g->_vid._w, g->_vid._h, fullscreen, widescreen, &scalerParameters);
	g->run();
	delete g;
	stub->destroy();
	delete stub;
	return 0;
}
