#include <array>
#include <cstring>
#include <SDL3/SDL.h>
#include <SDL3/SDL_keycode.h>
#include <fstream>
#include <iostream>

static uint8_t font_set[]{
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

struct Context {
    std::array<uint8_t, 16> V{};
    uint16_t I{};
    uint16_t PC{0x200};
    uint8_t SP{};
    uint8_t DT{};
    uint8_t ST{};
    uint16_t stack[16]{};
};

class CHIP8 {
private:
    uint8_t memory[4096]{};
    bool display[64*32]{};
    bool keys[16]{};
    Context ctx{};

public:
    CHIP8() {
        std::copy(std::begin(font_set), std::end(font_set), memory+0x50);
    }

    void step() {
        const uint16_t opcode = (memory[ctx.PC] << 8) | memory[ctx.PC + 1];
        ctx.PC += 2;

        switch (opcode & 0xF000) {
            case 0x0000: {
                switch (opcode & 0x00FF) {
                    case 0x00E0:
                        clearDisplay();
                        break;
                }
                break;
            }
            case 0x1000: {
                jump(opcode & 0x0FFF);
                break;
            }
            case 0x6000: {
                const size_t reg = (opcode & 0x0F00) >> 8;
                const uint8_t kk = opcode & 0x00FF;
                loadVx(reg, kk);
                break;
            }
            case 0x7000: {
                const size_t reg = (opcode & 0x0F00) >> 8;
                const uint8_t kk = opcode & 0x00FF;
                addVx(reg, kk);
                break;
            }
            case 0xA000: {
                loadI(opcode & 0x0FFF);
                break;
            }
        }
    }

private:
    void clearDisplay() {
        std::fill(std::begin(display), std::end(display), false);
    }

    void jump(const uint16_t address) {
        ctx.PC = address;
    }

    void loadVx(const size_t reg, const uint8_t kk) {
        ctx.V[reg] = kk;
    }

    void addVx(const size_t reg, const uint8_t kk) {
        ctx.V[reg] += kk;
    }

    void loadI(const uint16_t address) {
        ctx.I = address;
    }
};

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: chipate <rom>\n";
        return 1;
    }

    CHIP8 chip8{};

    std::ifstream rom{argv[1], std::ios::binary};
    if (!rom) {
        std::cerr << "Failed to open ROM: " << argv[1] << "\n";
        return 1;
    }
    rom.read(reinterpret_cast<char*>(chip8.memory + 0x200), 4096-0x200);

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("chipate", 1024, 512, 0);
    if (!window) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << "\n";
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer) {
        std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << "\n";
        SDL_Quit();
        return 1;
    }

    bool running = true;
    SDL_Event event;
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) running = false;
            if (event.type == SDL_EVENT_KEY_DOWN &&
                event.key.key == SDLK_ESCAPE) running = false;
        }
        SDL_Delay(16);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}