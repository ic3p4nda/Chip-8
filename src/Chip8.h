#pragma once
#include <array>
#include <string>

struct Quirks
{
    bool vfReset = true;
    bool memoryIncrement = true;
    bool displayWait = true;
    bool shiftUsesVY = true;
    bool jumpUsesVX = false;
};

class Chip8
{
public:
    Quirks quirks;
    
    static constexpr int WIDTH = 64;
    static constexpr int HEIGHT = 32;

    Chip8();
    bool loadROM(const std::string& path);
    
    
    void cycle();
    void tickTimers();

    bool soundActive() const { return soundTimer > 0; }
    
    std::array<uint8_t, WIDTH * HEIGHT> display{};  // 0 or 1 per pixel
    std::array<uint8_t, 16> keys{};
    bool drawFlag = false;

private:
    std::array<uint8_t, 4096> memory{};
    std::array<uint8_t, 16> V{};
    std::array<uint16_t, 16> stack{};
    uint16_t I = 0, pc = 0x200;
    uint8_t sp = 0, delayTimer = 0, soundTimer = 0;
    
    bool vblank = false;
    bool waitingForRelease = false;
    uint8_t waitKey = 0;
};
