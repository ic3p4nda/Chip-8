#include <SDL.h>
#include <iostream>
#include <vector>
#include "Chip8.h"

struct AudioState
{
    std::atomic<bool> beeping{false};
    double phase = 0.0;
};

void audioCallback(void* userdata, Uint8* stream, int len)
{
    auto* audio = static_cast<AudioState*>(userdata);
    auto* out = reinterpret_cast<Sint16*>(stream);
    int count = len / sizeof(Sint16);
    
    for (int i = 0; i < count; i++)
    {
        if (audio->beeping)
        {
            out[i] = (audio->phase < 0.5) ? 3000 : -3000;
            audio->phase += 440.0 / 44100.0; // 440hz
            if (audio->phase >= 1.0) audio->phase -= 1.0;
        }
        else
        {
            out[i] = 0;
        }
    }
}

int main(int argc, char* argv[])
{
    Chip8 Chip8;
    if (argc > 1 && !Chip8.loadROM(argv[1])) return 1;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0)
    {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
        return 1;
    }

    const int scale = 12;
    SDL_Window* window = SDL_CreateWindow("Chip-8",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        Chip8::WIDTH * scale, Chip8::HEIGHT * scale, SDL_WINDOW_SHOWN);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, 0);
    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING, Chip8::WIDTH, Chip8::HEIGHT);
    
    AudioState audio;
    SDL_AudioSpec want{}, have{};
    want.freq = 44100;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 512;
    want.callback = audioCallback;
    want.userdata = &audio;
    SDL_AudioDeviceID audioDev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    SDL_PauseAudioDevice(audioDev, 0);   // 0 = start playing

    std::vector<uint32_t> pixels(Chip8::WIDTH * Chip8::HEIGHT);
    
    // Chip-8 key index -> physical key
    // 1 2 3 C        1 2 3 4
    // 4 5 6 D   -->  Q W E R
    // 7 8 9 E        A S D F
    // A 0 B F        Z X C V
    const SDL_Scancode keymap[16] = {
        SDL_SCANCODE_X, SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_3,   // 0 1 2 3
        SDL_SCANCODE_Q, SDL_SCANCODE_W, SDL_SCANCODE_E, SDL_SCANCODE_A,   // 4 5 6 7
        SDL_SCANCODE_S, SDL_SCANCODE_D, SDL_SCANCODE_Z, SDL_SCANCODE_C,   // 8 9 A B
        SDL_SCANCODE_4, SDL_SCANCODE_R, SDL_SCANCODE_F, SDL_SCANCODE_V    // C D E F
    };
    
    const double frameTime = 1.0 / 60.0;
    double accumulator = 0.0;
    Uint64 last = SDL_GetPerformanceCounter();
    int cyclesPerFrame = 10;
    
    bool running = true;
    while (running)
    {
        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) running = false;
        }
        
        // input
        const Uint8* state = SDL_GetKeyboardState(nullptr);
        for (int i = 0; i < 16; i++) Chip8.keys[i] = state[keymap[i]];
        
        // timing: fixed 60hz steps
        Uint64 now = SDL_GetPerformanceCounter();
        accumulator += static_cast<double>(now - last) / static_cast<double>(SDL_GetPerformanceFrequency());
        last = now;
        if (accumulator >= 0.25) accumulator = 0.25;
        
        while (accumulator >= frameTime)
        {
            for (int i = 0; i < cyclesPerFrame; i++) Chip8.cycle();
            Chip8.tickTimers();
            accumulator -= frameTime;
        }
        
        audio.beeping = Chip8.soundActive();
        
        for (size_t i = 0; i < pixels.size(); i++)
            pixels[i] = Chip8.display[i] ? 0xFFFFFFFF : 0xFF000000;

        SDL_UpdateTexture(texture, nullptr, pixels.data(), Chip8::WIDTH * sizeof(uint32_t));
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);
        SDL_Delay(1);
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_CloseAudioDevice(audioDev);
    SDL_Quit();
    return 0;
}