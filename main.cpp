/* REminiscence - Flashback interpreter
 * Copyright (C) 2005 Gregory Montoir
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "game.h"
#include "resource.h"
#include "systemstub.h"

static const char *USAGE = 
	"REminiscence - Flashback Interpreter\n"
	"Usage: rs [OPTIONS]...\n"
	"  --datapath=PATH   Path to where the game is installed (default '.')\n"
	"  --version=VER     Version of the game to load : fr, us (default)\n";

static const struct {
	const char *name;
	Version ver;
} VERSIONS[] = {
	{ "fr",  VER_FR },
	{ "us",  VER_US }
};

static bool parseOption(const char *arg, const char *longCmd, const char **opt) {
	bool ret = false;
	if (arg[0] == '-' && arg[1] == '-') {
		if (strncmp(arg + 2, longCmd, strlen(longCmd)) == 0) {
			*opt = arg + 2 + strlen(longCmd);
			ret = true;
		}
	}
	return ret;
}

#undef main
int main(int argc, char *argv[]) {
	const char *dataPath = ".";
	const char *version = 0;
	Version ver = VER_US;
	for (int i = 1; i < argc; ++i) {
		bool opt = false;
		if (strlen(argv[i]) >= 2) {
			opt |= parseOption(argv[i], "datapath=", &dataPath);
			opt |= parseOption(argv[i], "version=", &version);
		}
		if (!opt) {
			printf(USAGE);
			return 0;
		}
	}
	if (version) {
		for (unsigned int j = 0; j < ARRAYSIZE(VERSIONS); ++j) {
			if (strcmp(version, VERSIONS[j].name) == 0) {
				ver = VERSIONS[j].ver;
				break;
			}
		}
	}
	g_debugMask = 0; // DBG_CUT | DBG_VIDEO | DBG_RES | DBG_MENU | DBG_PGE | DBG_GAME | DBG_UNPACK | DBG_COL;
	SystemStub *stub = SystemStub_SDL_create();
	Game *g = new Game(stub, dataPath, ver);
	g->run();
	delete g;
	delete stub;
	return 0;
}
