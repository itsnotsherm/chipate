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

class CHIP8 {
private:
    // Context (Registers + Stack)
    std::array<uint8_t, 16> V{};
    uint16_t I{};
    uint16_t PC{0x200};
    uint8_t SP{};
    uint8_t DT{};
    uint8_t ST{};
    uint16_t stack[16]{};

    // Machine
    uint8_t memory[4096]{};
    bool display[64*32]{};
    bool keys[16]{};

public:
    CHIP8() {
        std::copy(std::begin(font_set), std::end(font_set), memory+0x50);
    }

    bool loadROM(const char* path) {
        std::ifstream rom{path, std::ios::binary};
        if (!rom) return false;
        rom.read(reinterpret_cast<char*>(memory + 0x200), 4096 - 0x200);
        return true;
    }

    [[nodiscard]] const bool* getDisplay() const {
        return display;
    }

    void step() {
        const uint16_t opcode = (memory[PC] << 8) | memory[PC + 1];
        PC += 2;

        const uint16_t addr = opcode & 0x0FFF;
        const uint8_t n = opcode & 0x000F;
        const uint8_t x = (opcode & 0x0F00) >> 8;
        const uint8_t y = (opcode & 0x00F0) >> 4;
        const uint8_t kk = opcode & 0x00FF;
        switch (opcode & 0xF000) {
            case 0x0000: {
                switch (opcode & 0x00FF) {
                    case 0x00E0:
                        clearDisplay();
                        break;
                    case 0x00EE:
                        subReturn();
                        break;
                }
                break;
            }
            case 0x1000: {
                jump(addr);
                break;
            }
            case 0x2000: {
                call(addr);
                break;
            }
            case 0x3000: {
                skipNextEquals(x, kk);
                break;
            }
            case 0x6000: {
                loadVx(x, kk);
                break;
            }
            case 0x7000: {
                addVx(x, kk);
                break;
            }
            case 0xA000: {
                loadI(addr);
                break;
            }
            case 0xD000: {
                draw(x, y, n);
                break;
            }
        }
    }

private:
    void clearDisplay() {
        std::fill(std::begin(display), std::end(display), false);
    }

    void subReturn() {
        PC = stack[SP--];
    }

    void jump(const uint16_t address) {
        PC = address;
    }

    void call(const uint16_t address) {
        stack[++SP] = PC;
        PC = address;
    }

    void skipNextEquals(const uint8_t x, const uint16_t kk) {
        if (V[x] == kk) PC += 2;
    }

    void loadVx(const uint8_t x, const uint8_t kk) {
        V[x] = kk;
    }

    void addVx(const uint8_t x, const uint8_t kk) {
        V[x] += kk;
    }

    void loadI(const uint16_t address) {
        I = address;
    }

    void draw(const uint8_t x, const uint8_t y, const uint8_t n) {
        V[0xF] = 0;
        for (size_t i = 0; i < n; i++) {
            const uint8_t byte = memory[I + i];
            const auto py = V[y];
            for (size_t j = 0; j < 8; j++) {
                if (byte & (0x80 >> j)) {
                    const auto px = (V[x] + j) % 64;
                    const auto pixel = ((py + i) % 32) * 64 + px;
                    if (display[pixel]) {
                        V[0xF] = 1;
                    }
                    display[pixel] ^= 1;
                }
            }
        }
    }
};

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: chipate <rom>\n";
        return 1;
    }

    CHIP8 chip8{};

    if (!chip8.loadROM(argv[1])) {
        std::cerr << "Failed to open ROM: " << argv[1] << "\n";
        return 1;
    }

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
        chip8.step();
        auto display = chip8.getDisplay();

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

        for (size_t i = 0; i < 64*32; ++i) {
            if (display[i]) {
                const auto x = static_cast<float>(i % 64 * 16);
                const auto y = static_cast<float>(i / 64 * 16);

                SDL_FRect rect{x, y, 16.0f, 16.0f};
                SDL_RenderFillRect(renderer, &rect);
            }
        }
        SDL_RenderPresent(renderer);
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