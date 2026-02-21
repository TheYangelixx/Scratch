#include "sound_manager.h"
#include <iostream>
#include <SDL2/SDL.h>

void sound_manager_init(SoundManager& sm) {
    sm.sounds.clear();
    sm.initialized = false;
    sm.master_volume = 100;
    sm.muted = false;
    sm.next_channel = 0;

    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        std::cerr << "[SoundManager] SDL_mixer init failed: " << Mix_GetError() << std::endl;
        return;
    }
    Mix_AllocateChannels(32);
    sm.initialized = true;
}

void sound_manager_shutdown(SoundManager& sm) {
    sound_unload_all(sm);
    if (sm.initialized) {
        Mix_CloseAudio();
        sm.initialized = false;
    }
}

int sound_load(SoundManager& sm, const std::string& name, const std::string& filepath) {
    if (!sm.initialized) return -1;

    for (int i = 0; i < (int)sm.sounds.size(); i++) {
        if (sm.sounds[i].name == name && sm.sounds[i].loaded) {
            return i;
        }
    }

    Mix_Chunk* chunk = Mix_LoadWAV(filepath.c_str());
    if (!chunk) {
        std::cerr << "[SoundManager] Failed to load: " << filepath
                  << " - " << Mix_GetError() << std::endl;
        return -1;
    }

    Sound s;
    s.name = name;
    s.filepath = filepath;
    s.chunk = chunk;
    s.channel = -1;
    s.loaded = true;
    sm.sounds.push_back(s);
    return (int)sm.sounds.size() - 1;
}

void sound_unload(SoundManager& sm, const std::string& name) {
    for (int i = 0; i < (int)sm.sounds.size(); i++) {
        if (sm.sounds[i].name == name && sm.sounds[i].loaded) {
            if (sm.sounds[i].chunk) {
                Mix_FreeChunk(sm.sounds[i].chunk);
                sm.sounds[i].chunk = nullptr;
            }
            sm.sounds[i].loaded = false;
            return;
        }
    }
}

void sound_unload_all(SoundManager& sm) {
    for (int i = 0; i < (int)sm.sounds.size(); i++) {
        if (sm.sounds[i].chunk) {
            Mix_FreeChunk(sm.sounds[i].chunk);
            sm.sounds[i].chunk = nullptr;
        }
        sm.sounds[i].loaded = false;
    }
    sm.sounds.clear();
}

Sound* sound_find(SoundManager& sm, const std::string& name) {
    for (int i = 0; i < (int)sm.sounds.size(); i++) {
        if (sm.sounds[i].name == name) return &sm.sounds[i];
    }
    return nullptr;
}

const Sound* sound_find_const(const SoundManager& sm, const std::string& name) {
    for (int i = 0; i < (int)sm.sounds.size(); i++) {
        if (sm.sounds[i].name == name) return &sm.sounds[i];
    }
    return nullptr;
}

void sound_play(SoundManager& sm, const std::string& name) {
    if (!sm.initialized || sm.muted) return;
    Sound* s = sound_find(sm, name);
    if (!s || !s->loaded || !s->chunk) return;

    int ch = Mix_PlayChannel(-1, s->chunk, 0);
    s->channel = ch;
    if (ch >= 0) {
        int vol = (int)(sm.master_volume * 128.0f / 100.0f);
        Mix_Volume(ch, vol);
    }
}

void sound_play_and_wait(SoundManager& sm, const std::string& name) {
    if (!sm.initialized || sm.muted) return;
    Sound* s = sound_find(sm, name);
    if (!s || !s->loaded || !s->chunk) return;

    int ch = Mix_PlayChannel(-1, s->chunk, 0);
    s->channel = ch;
    if (ch >= 0) {
        int vol = (int)(sm.master_volume * 128.0f / 100.0f);
        Mix_Volume(ch, vol);
        while (Mix_Playing(ch)) {
            SDL_Delay(10);
        }
    }
}

bool sound_is_playing(const SoundManager& sm, const std::string& name) {
    const Sound* s = sound_find_const(sm, name);
    if (!s || s->channel < 0) return false;
    return Mix_Playing(s->channel) != 0;
}

void sound_stop(SoundManager& sm, const std::string& name) {
    Sound* s = sound_find(sm, name);
    if (!s || s->channel < 0) return;
    Mix_HaltChannel(s->channel);
    s->channel = -1;
}

void sound_stop_all(SoundManager& sm) {
    Mix_HaltChannel(-1);
    for (int i = 0; i < (int)sm.sounds.size(); i++) {
        sm.sounds[i].channel = -1;
    }
}

void sound_set_volume(SoundManager& sm, int vol) {
    if (vol < 0) vol = 0;
    if (vol > 100) vol = 100;
    sm.master_volume = vol;
    int mix_vol = (int)(vol * 128.0f / 100.0f);
    Mix_Volume(-1, mix_vol);
}

int sound_get_volume(const SoundManager& sm) {
    return sm.master_volume;
}

void sound_mute(SoundManager& sm) {
    sm.muted = true;
    Mix_Volume(-1, 0);
}

void sound_unmute(SoundManager& sm) {
    sm.muted = false;
    int mix_vol = (int)(sm.master_volume * 128.0f / 100.0f);
    Mix_Volume(-1, mix_vol);
}

void sound_toggle_mute(SoundManager& sm) {
    if (sm.muted) sound_unmute(sm);
    else sound_mute(sm);
}
