#include "Sound.h"

Mix_Chunk *phaser = NULL;
Mix_Music *music = NULL;

Mix_Chunk *test = NULL;

SongType currentSong = empty;

int phaserChannel = -1;
int testChannel = -1;

void init_Audio(){
	 /* Same setup as before */
  int audio_rate = 22050;
  Uint16 audio_format = AUDIO_S16; 
  int audio_channels = 2;
  int audio_buffers = 4096;

  SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);

  if(Mix_OpenAudio(audio_rate, audio_format, audio_channels, audio_buffers)) {
    printf("Unable to open audio!\n");
    exit(1);
  }
}
void play_Music(SongType song){
	//Mix_HaltMusic();
	//Mix_FreeMusic(music);
    //music = NULL;
	if(currentSong == song)
        return;
    else
        currentSong = song;

	switch(song){
	case SONG_MENU:
		music = Mix_LoadMUS("menu.mid");
		break;
	case SONG_LEVEL1:
		music = Mix_LoadMUS("level1.mid");
		break;
	case SONG_LEVEL2:
		music = Mix_LoadMUS("level2.mid");
		break;
	case SONG_LEVEL3:
		music = Mix_LoadMUS("level3.mid");
		break;
	case SONG_WIN:
		music = Mix_LoadMUS("win.mid");
		break;
	case SONG_LOSE:
		music = Mix_LoadMUS("lose.mid");
		break;
    case SONG_AI1:
		music = Mix_LoadMUS("ai1.mid");
		break;
    case SONG_AI2:
		music = Mix_LoadMUS("ai2.mid");
		break;
    case SONG_AI3:
		music = Mix_LoadMUS("ai3.mid");
		break;
    case SONG_BOSS:
		music = Mix_LoadMUS("boss.mid");
		break;
    case SONG_MULTI:
		music = Mix_LoadMUS("multi.mid");
		break;
	default:
		break;
	}
	Mix_PlayMusic(music, 3);
	Mix_HookMusicFinished(musicDone);
}
void play_Sound(char *sound) {

  /* Pre-load sound effects */

  test = Mix_LoadWAV(sound);

  testChannel = Mix_PlayChannel(-1, test, 0);
  SDL_Delay(50);


 Mix_FreeChunk(phaser);
}
void musicDone() {
  Mix_HaltMusic();
  Mix_FreeMusic(music);
  music = NULL;
}
