#include "init.h"
#include "mpak.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <cstdio>
#include <cstdlib>

MPAK_FILE pak_file;

#ifndef MPK_DIR
#define MPK_DIR "res/"
#endif
#ifndef OVERRIDE_DIR
#define OVERRIDE_DIR "null"
#endif

void init_sdl_and_gl() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_AUDIO) < 0) {
        fprintf(stderr, "Unable to init SDL: %s\n", SDL_GetError());
        exit(-1);
    }
    atexit(SDL_Quit);

    SDL_ShowCursor(SDL_DISABLE);

    // Open the pak file, with globally define OVERRIDE_DIR being the override directory
    pak_file.init();
    if (!pak_file.open_mpk(MPAK_READ, MPK_DIR "tomatoes.mpk", OVERRIDE_DIR)) {
        fprintf(stderr, "Unable to open 'tomatoes.mpk'.\nThe file either doesn't exist or is corrupted.");
        exit(-1);
    }
}