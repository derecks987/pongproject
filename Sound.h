#ifndef __Sound_h_
#define __Sound_h_

#include <stdio.h>
#include <stdlib.h>

#include "SDL.h"
#include "SDL_audio.h"
#include "SDL_mixer.h"
enum SongType
{
    SONG_MENU = 0,
    SONG_LEVEL1,
	SONG_LEVEL2,
	SONG_LEVEL3,
	SONG_WIN,
	SONG_LOSE,
    SONG_AI1,
    SONG_AI2,
    SONG_AI3,
    SONG_BOSS,
    SONG_MULTI,
    empty
};

void play_Music(SongType song);
void init_Audio();
void play_Sound(char *file);
void musicDone();


#endif // #ifndef __Physics_h_
