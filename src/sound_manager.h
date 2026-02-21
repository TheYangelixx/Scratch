#ifndef SOUND_MANAGER_H
#define SOUND_MANAGER_H

#include <SDL2/SDL_mixer.h>
#include <string>
#include <vector>

/* ساختار صدا */
struct Sound {
    std::string name;
    std::string filepath;
    Mix_Chunk* chunk;
    int channel;
    bool loaded;
};

/* مدیریت صدا */
struct SoundManager {
    std::vector<Sound> sounds;
    bool initialized;
    int master_volume;
    bool muted;
    int next_channel;
};

/* توابع */
void sound_manager_init(SoundManager& sm);
void sound_manager_shutdown(SoundManager& sm);

int  sound_load(SoundManager& sm, const std::string& name, const std::string& filepath);
void sound_unload(SoundManager& sm, const std::string& name);
void sound_unload_all(SoundManager& sm);

void sound_play(SoundManager& sm, const std::string& name);
void sound_play_and_wait(SoundManager& sm, const std::string& name);
bool sound_is_playing(const SoundManager& sm, const std::string& name);
void sound_stop(SoundManager& sm, const std::string& name);
void sound_stop_all(SoundManager& sm);

void sound_set_volume(SoundManager& sm, int vol);
int  sound_get_volume(const SoundManager& sm);
void sound_mute(SoundManager& sm);
void sound_unmute(SoundManager& sm);
void sound_toggle_mute(SoundManager& sm);

Sound* sound_find(SoundManager& sm, const std::string& name);
const Sound* sound_find_const(const SoundManager& sm, const std::string& name);

#endif /* SOUND_MANAGER_H */
