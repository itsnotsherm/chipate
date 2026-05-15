#include <array>
#include <cstring>
#include <SDL3/SDL.h>
#include <SDL3/SDL_keycode.h>
#include <fstream>
#include <iostream>
#include <random>
#include <unordered_map>

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

static std::unordered_map<SDL_Scancode, uint8_t> scancode_map {
    {SDL_SCANCODE_1, 0x1},
    {SDL_SCANCODE_2, 0x2},
    {SDL_SCANCODE_3, 0x3},
    {SDL_SCANCODE_4, 0xC},
    {SDL_SCANCODE_Q, 0x4},
    {SDL_SCANCODE_W, 0x5},
    {SDL_SCANCODE_E, 0x6},
    {SDL_SCANCODE_R, 0xD},
    {SDL_SCANCODE_A, 0x7},
    {SDL_SCANCODE_S, 0x8},
    {SDL_SCANCODE_D, 0x9},
    {SDL_SCANCODE_F, 0xE},
    {SDL_SCANCODE_Z, 0xA},
    {SDL_SCANCODE_X, 0x0},
    {SDL_SCANCODE_C, 0xB},
    {SDL_SCANCODE_V, 0xF}
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

    // Additional state
    bool waiting_for_key = false;
    uint8_t wait_key_reg = 0x10;

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

    void notifyKeyDown(const SDL_Scancode code) {
        auto it = scancode_map.find(code);
        if (it == scancode_map.end()) return;

        uint8_t key = it->second;
        wait_key_reg = key;
        keys[key] = true;
    }

    void notifyKeyUp(const SDL_Scancode code) {
        auto it = scancode_map.find(code);
        if (it == scancode_map.end()) return;

        uint8_t key = it->second;
        if (wait_key_reg == key) wait_key_reg = 0x10;
        keys[key] = false;
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
            case 0x4000: {
                skipNextNotEquals(x, kk);
                break;
            }
            case 0x5000: {
                skipNextRegEquals(x, y);
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
            case 0x8000: {
                switch (n) {
                    case 0x0:
                        loadRegVx(x, y);
                        break;
                    case 0x1:
                        regOR(x, y);
                        break;
                    case 0x2:
                        regAND(x, y);
                        break;
                    case 0x3:
                        regXOR(x, y);
                        break;
                    case 0x4:
                        addRegVx(x, y);
                        break;
                    case 0x5:
                        subRegVx(x, y);
                        break;
                    case 0x6:
                        shiftRightVx(x);
                        break;
                    case 0x7:
                        subNRegVx(x, y);
                        break;
                    case 0xE:
                        shiftLeftVx(x);
                        break;
                }
                break;
            }
            case 0x9000: {
                skipNextRegNotEquals(x, y);
                break;
            }
            case 0xA000: {
                loadI(addr);
                break;
            }
            case 0xB000: {
                jumpOffset(addr);
                break;
            }
            case 0xC000: {
                randVx(x, kk);
                break;
            }
            case 0xD000: {
                draw(x, y, n);
                break;
            }
            case 0xE000: {
                switch (opcode & 0x00FF) {
                    case 0x9E:
                        skipNextKeyPressed(x);
                        break;
                    case 0xA1:
                        skipNextKeyNotPressed(x);
                        break;
                }
                break;
            }
            case 0xF000: {
                switch (opcode & 0x00FF) {
                    case 0x07:
                        loadDelayTimerVx(x);
                        break;
                    case 0x0A:
                        loadKeyPressVx(x);
                        break;
                    case 0x15:
                        setDelayTimer(x);
                        break;
                    case 0x18:
                        setSoundTimer(x);
                        break;
                    case 0x1E:
                        addRegI(x);
                        break;
                    case 0x29:
                        setSpriteLocationI(x);
                        break;
                    case 0x33:
                        setBCDVx(x);
                        break;
                    case 0x55:
                        loadRegsToMemory(x);
                        break;
                    case 0x65:
                        readRegsFromMemory(x);
                        break;
                }
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

    void jump(const uint16_t addr) {
        PC = addr;
    }

    void call(const uint16_t addr) {
        stack[++SP] = PC;
        PC = addr;
    }

    void skipNextEquals(const uint8_t x, const uint8_t kk) {
        if (V[x] == kk) PC += 2;
    }

    void skipNextNotEquals(const uint8_t x, const uint8_t kk) {
        if (V[x] != kk) PC += 2;
    }

    void skipNextRegEquals(const uint8_t x, const uint8_t y) {
        if (V[x] == V[y]) PC += 2;
    }

    void loadVx(const uint8_t x, const uint8_t kk) {
        V[x] = kk;
    }

    void addVx(const uint8_t x, const uint8_t kk) {
        V[x] += kk;
    }

    void loadRegVx(const uint8_t x, const uint8_t y) {
        V[x] = V[y];
    }

    void regOR(const uint8_t x, const uint8_t y) {
        V[x] |= V[y];
    }

    void regAND(const uint8_t x, const uint8_t y) {
        V[x] &= V[y];
    }

    void regXOR(const uint8_t x, const uint8_t y) {
        V[x] ^= V[y];
    }

    void addRegVx(const uint8_t x, const uint8_t y) {
        const uint8_t carry = (V[x] > 0xFF - V[y]) ? 1 : 0;
        V[x] += V[y];
        V[0xF] = carry;
    }

    void subRegVx(const uint8_t x, const uint8_t y) {
        const uint8_t borrow = (V[x] >= V[y]) ? 0 : 1;
        V[x] -= V[y];
        V[0xF] = !borrow;
    }

    void shiftRightVx(const uint8_t x) {
        const uint8_t lsb = V[x] & 0x1;
        V[x] >>= 1;
        V[0xF] = lsb;
    }

    void subNRegVx(const uint8_t x, const uint8_t y) {
        const uint8_t borrow = (V[y] >= V[x]) ? 0 : 1;
        V[x] = V[y] - V[x];
        V[0xF] = !borrow;
    }

    void shiftLeftVx(const uint8_t x) {
        const uint8_t msb = (V[x] >> 7) & 0x1;
        V[x] <<= 1;
        V[0xF] = msb;
    }

    void skipNextRegNotEquals(const uint8_t x, const uint8_t y) {
        if (V[x] != V[y]) PC += 2;
    }

    void loadI(const uint16_t addr) {
        I = addr;
    }

    void jumpOffset(const uint16_t addr) {
        PC = addr + V[0];
    }

    void randVx(const uint8_t x, const uint8_t kk) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dist(0, 255);

        V[x] = dist(gen) & kk;
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

    void skipNextKeyPressed(const uint8_t x) {
        if (keys[V[x]]) PC += 2;
    }

    void skipNextKeyNotPressed(const uint8_t x) {
        if (!keys[V[x]]) PC += 2;
    }

    void loadDelayTimerVx(const uint8_t x) {
        V[x] = DT;
    }

    void loadKeyPressVx(const uint8_t x) {
        if (!waiting_for_key) {
            waiting_for_key = true;
            wait_key_reg = 0x10;
            PC -= 2;
        } else if (wait_key_reg == 0x10) {
            PC -= 2;
        } else {
            waiting_for_key = false;
            V[x] = wait_key_reg;
            wait_key_reg = 0x10;
        }
    }

    void setDelayTimer(const uint8_t x) {
        DT = V[x];
    }

    void setSoundTimer(const uint8_t x) {
        ST = V[x];
    }

    void addRegI(const uint8_t x) {
        I += V[x];
    }

    void setSpriteLocationI(const uint8_t x) {
        I = 0x50 + 5 * V[x];
    }

    void setBCDVx(const uint8_t x) {
        uint8_t num = V[x];

        for (int i = 2; i >= 0; i--) {
            const uint8_t digit = num % 10;
            memory[I + i] = digit;
            num /= 10;
        }
    }

    void loadRegsToMemory(const uint8_t x) {
        for (uint8_t i = 0; i <= x; i++) {
            memory[I + i] = V[i];
        }
    }

    void readRegsFromMemory(const uint8_t x) {
        for (uint8_t i = 0; i <= x; i++) {
            V[i] = memory[I + i];
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

            if (running && event.type == SDL_EVENT_KEY_DOWN) {
                chip8.notifyKeyDown(event.key.scancode);
            }
            if (running && event.type == SDL_EVENT_KEY_UP) {
                chip8.notifyKeyUp(event.key.scancode);
            }
        }
        SDL_Delay(16);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}