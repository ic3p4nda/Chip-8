#include "Chip8.h"
#include <algorithm>
#include <fstream>
#include <iostream>

static constexpr uint16_t FONT_START = 0x050;
static constexpr uint16_t ROM_START = 0x200;

static const uint8_t FONTSET[80] = {
    0xF0,0x90,0x90,0x90,0xF0, // 0
    0x20,0x60,0x20,0x20,0x70, // 1
    0xF0,0x10,0xF0,0x80,0xF0, // 2
    0xF0,0x10,0xF0,0x10,0xF0, // 3
    0x90,0x90,0xF0,0x10,0x10, // 4
    0xF0,0x80,0xF0,0x10,0xF0, // 5
    0xF0,0x80,0xF0,0x90,0xF0, // 6
    0xF0,0x10,0x20,0x40,0x40, // 7
    0xF0,0x90,0xF0,0x90,0xF0, // 8
    0xF0,0x90,0xF0,0x10,0xF0, // 9
    0xF0,0x90,0xF0,0x90,0x90, // A
    0xE0,0x90,0xE0,0x90,0xE0, // B
    0xF0,0x80,0x80,0x80,0xF0, // C
    0xE0,0x90,0x90,0x90,0xE0, // D
    0xF0,0x80,0xF0,0x80,0xF0, // E
    0xF0,0x80,0xF0,0x80,0x80  // F
};

Chip8::Chip8()
{
    std::copy(std::begin(FONTSET), std::end(FONTSET), memory.begin() + FONT_START);
}

bool Chip8::loadROM(const std::string& path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        std::cerr << "Could not open ROM: " << path << "\n";
        return false;
    }

    std::streamsize size = file.tellg();
    if (size <= 0 || size > static_cast<std::streamsize>(memory.size() - ROM_START))
    {
        std::cerr << "ROM size invalid or too big: " << size << " bytes\n";
        return false;
    }

    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(memory.data() + ROM_START), size);
    return true;
}

// https://tobiasvl.github.io/blog/write-a-chip-8-emulator/
// Fetch/decode/execute loop
void Chip8::cycle()
{
    // Fetch the instruction from memory at the current PC (program counter)
    uint16_t opcode = (memory[pc] << 8) | memory[pc + 1];
    pc += 2;
    
    // Decode the instruction to find out what the emulator should do
    uint8_t x = (opcode & 0x0F00) >> 8;
    uint8_t y = (opcode & 0x00F0) >> 4;
    uint8_t n = opcode & 0x000F;
    uint8_t nn = opcode & 0x00FF;
    uint16_t nnn = opcode & 0x0FFF;
    
    // Execute the instruction and do what it tells you
    switch (opcode & 0xF000)
    {
        case 0x0000:
        if (opcode == 0x00E0) // clear screen
        {
            display.fill(0);
            drawFlag = true;
        }
        else if (opcode == 0x00EE) // return from subroutine
        {
            pc = stack[--sp];
        } else 
        {
            std::cerr << "Unknown opcode: " << std::hex << opcode << "\n";
        }
        break;
        
    case 0x1000: pc = nnn; break; // 1NNN: jump
    case 0x2000: 
        stack[sp++] = pc; 
        pc = nnn; 
        break;
        
    case 0x3000: if (V[x] == nn) pc += 2; break; // 3XNN: skip if VX == NN
    case 0x4000: if  (V[x] != nn) pc += 2; break; // 4XNN: skip if VX != NN
    case 0x5000: if (V[x] == V[y]) pc += 2; break; // 5NY0: skip if VX == VY
    case 0x6000: V[x] = nn; break; // 6XNN: VX = NN
    case 0x7000: V[x] += nn; break; // 7XNN: VX += NN (no carry flag)
        
    case 0x8000:                        // 8XYN: ALU
        {
            uint8_t flag = 0;
            switch (n)
            {
            case 0x0: V[x] = V[y];  break;
            case 0x1: V[x] |= V[y]; if (quirks.vfReset) V[0xF] = 0; break;
            case 0x2: V[x] &= V[y]; if (quirks.vfReset) V[0xF] = 0; break;
            case 0x3: V[x] ^= V[y]; if (quirks.vfReset) V[0xF] = 0; break;
            case 0x4:                       // VF = carry
                {
                    uint16_t sum = V[x] + V[y];
                    flag = sum > 0xFF;
                    V[x] = sum & 0xFF;
                    break;
                }
            case 0x5:                       // VX = VX - VY, VF = NOT borrow
                flag = V[x] >= V[y];
                V[x] = V[x] - V[y];
                break;
            case 0x7:                       // VX = VY - VX, VF = NOT borrow
                flag = V[y] >= V[x];
                V[x] = V[y] - V[x];
                break;
            case 0x6:                       // shift right, VF = bit shifted out
                if (quirks.shiftUsesVY) V[x] = V[y];
                flag = V[x] & 0x1;
                V[x] >>= 1;
                break;
            case 0xE:                       // shift left, VF = bit shifted out
                if (quirks.shiftUsesVY) V[x] = V[y];
                flag = (V[x] & 0x80) >> 7;
                V[x] <<= 1;
                break;
            default:
                std::cerr << "Unknown opcode: " << std::hex << opcode << "\n";
                return;
            }
            if (n >= 0x4) V[0xF] = flag;
            break;
        }
        
    case 0x9000: if (V[x] != V[y]) pc += 2; break; // 9XYO: skip if VX = VY
    
    case 0xA000: I = nnn; break; // ANNN: I = NNN        
    case 0xB000: pc = nnn + (quirks.jumpUsesVX ? V[x] : V[0]); break; // BNNN: jump + V0
    case 0xC000: V[x] = (std::rand() & 0xFF) & nn; break; // CXNN: random & NN
    
    case 0xD000: // DXYN: draw N-row sprite at (VX, VY)
        {
            if (quirks.displayWait)
            {
                if (!vblank) { pc -= 2; break; }   // not a new frame yet, retry this instruction
                vblank = false;
            }
            uint8_t xPos = V[x] % WIDTH; // starting coords
            uint8_t yPos = V[y] % HEIGHT;
            V[0xF] = 0;
            
            for (int row = 0; row < n; row++)
            {
                if (yPos + row >= HEIGHT) break; // Sprite clips the edge
                uint8_t spriteByte = memory[I + row];
                
                for (int col = 0; col < 8; col++)
                {
                    if (xPos + col >= WIDTH) break;
                    if (spriteByte & (0x80 >> col)) // is this sprite bit set?
                    {
                        uint8_t& pixel = display[(yPos + row) * WIDTH + (xPos + col)];
                        if (pixel) V[0xF] = 1; // pixel turned off = collision
                        pixel ^= 1; // XOR draw
                    }
                }
            }
            drawFlag = true;
            break;
        }
        
    case 0xE000:
        {
            if (nn == 0x9E) { if (keys[V[x]] & 0xF) pc += 2; } // EX9E: skip if key VX pressed
            else if (nn == 0xA1) { if (!keys[V[x]] & 0xF) pc += 2; }  // EXA1: skip if key VX NOT pressed
            else std::cerr << "Unknown opcode: " << std::hex << opcode << "\n";
            break;
        }
        
    case 0xF000:
        switch (nn)
        {
            case 0x1E: I += V[x]; break;                          // FX1E: I += VX
            case 0x07: V[x] = delayTimer; break;
            case 0x15: delayTimer = V[x]; break;
            case 0x18: soundTimer = V[x]; break;
            case 0x0A:
            {
                if (!waitingForRelease)
                {
                    for (int i = 0; i < 16; i++)
                        if (keys[i]) { waitKey = i; waitingForRelease = true; break; }
                }
                if (waitingForRelease && !keys[waitKey])
                {
                    V[x] = waitKey;
                    waitingForRelease = false;
                }
                else
                {
                    pc -= 2;
                }
                break;
            }
            case 0x29: I = FONT_START + (V[x] & 0xF) * 5; break;  // FX29: I = font char VX
            case 0x33:                                            // FX33: BCD
                memory[I]     = V[x] / 100;
                memory[I + 1] = (V[x] / 10) % 10;
                memory[I + 2] = V[x] % 10;
                break;
            
            case 0x55: for (int i = 0; i <= x; i++) memory[I + i] = V[i]; if (quirks.memoryIncrement) I += x+1; break;  // store V0..VX
            case 0x65: for (int i = 0; i <= x; i++) V[i] = memory[I + i]; if (quirks.memoryIncrement) I += x+1; break;  // load V0..VX
            
            default:
            std::cerr << "Unknown opcode: " << std::hex << opcode << "\n";
            break;
        }
        break;
    default:
        std::cerr << "Unknown opcode: " << std::hex << opcode << " at pc =" << (pc-2) << "\n";
        break;
    }
}

void Chip8::tickTimers()
{
    if (delayTimer > 0) delayTimer--;
    if (soundTimer > 0) soundTimer--;
    vblank = true;
}
