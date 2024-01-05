#include <stdio.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

#include "game.h"

#ifdef __EMSCRIPTEN__
int main(void) {
	init_cfg_t cfg = {0};
	if (init(cfg)) {
		fputs("[MAIN] init() failed\n", stderr);
		return 1;
	}
	fputs("[MAIN] init done\n", stdout);
	fputs("[MAIN] emscripten\n", stdout);
	emscripten_set_main_loop(loop, 0, 0);
	return 0;
}
#else
#include <string.h>
int main(const int argc, const char** argv) {
	init_cfg_t cfg = {0};

	/* Parse CLI Args */
	for (int i=1; i<argc; i++) {
		if (strcmp(argv[i], "-nopl")==0) {
			cfg.no_pointlights = 1;
		} else if (strcmp(argv[i], "-nosc")==0) {
			cfg.no_shadowcasters = 1;
		}
	}

	if (init(cfg)) {
		fputs("[MAIN] init() failed\n", stderr);
		return 1;
	}
	fputs("[MAIN] init done\n", stdout);
#ifdef __EMSCRIPTEN__
	fputs("[MAIN] emscripten\n", stdout);
	emscripten_set_main_loop(loop, 0, 0);
#else
	while (!check_if_quitting()) {
		loop();
	}
	quit();
#endif
	return 0;
}
#endif
