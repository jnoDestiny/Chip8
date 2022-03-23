#include <fstream>
#include <chrono>
#include <random>
#include <cstdint>
#include <cstring>
#include "Chip8.hpp"

const unsigned int START_ADDRESS = 0x200;
const unsigned int FONTSET_SIZE = 80;
const unsigned int FONTSET_START_ADDRESS = 0x50;


uint8_t fontset[FONTSET_SIZE] =
{
	0xF0, 0x90, 0x90, 0x90, 0xF0,   // 0
	0x20, 0x60, 0x20, 0x20, 0x70,   // 1
	0xF0, 0x10, 0xF0, 0x80, 0xF0,   // 2
	0xF0, 0x10, 0xF0, 0x10, 0xF0,   // 3
	0x90, 0x90, 0xF0, 0x10, 0x10,   // 4
	0xF0, 0x80, 0xF0, 0x10, 0xF0,   // 5
	0xF0, 0x80, 0xF0, 0x90, 0xF0,   // 6
	0xF0, 0x10, 0x20, 0x40, 0x40,   // 7
	0xF0, 0x90, 0xF0, 0x90, 0xF0,   // 8
	0xF0, 0x90, 0xF0, 0x10, 0xF0,   // 9
	0xF0, 0x90, 0xF0, 0x90, 0x90,   // A
	0xE0, 0x90, 0xE0, 0x90, 0xE0,   // B
	0xF0, 0x80, 0x80, 0x80, 0xF0,   // C
	0xE0, 0x90, 0x90, 0x90, 0xE0,   // D
	0xF0, 0x80, 0xF0, 0x80, 0xF0,   // E
	0xF0, 0x80, 0xF0, 0x80, 0x80    // F
};


// Load a ROM file containing instructions into memory
void Chip8::LoadROM(char const* filename)
{
    // Open the file as a stream of binary and seek to its end
    std::ifstream file(filename, std::ios::binary | std::ios::ate);

    if (file.is_open())
    {
        // Get the size of the file and allocate a buffer of length size
        std::streampos size = file.tellg();
        char* buffer = new char[size];

        // Seek to the beginning of the file and fill the buffer with its contents
        file.seekg(0, std::ios::beg);
        file.read(buffer, size);
        file.close();

        // Load the ROM contents into the Chip8's memory, starting at address 0x200
        for (long i = 0; i < size; i++)
        {
            memory[START_ADDRESS + i] = buffer[i];
        }

        // Free the buffer
        delete[] buffer;
    }
}


/*
 * The Chip8 constructor.
 */
Chip8::Chip8()
        : randGen(std::chrono::system_clock::now().time_since_epoch().count())
{
    /************************************/
    /********** INITIALIZATION **********/
    /************************************/

    // Initialize Program counter
    counter = START_ADDRESS;

    // Load fonts into memory
    for (unsigned int i = 0; i < FONTSET_SIZE; ++i)
    {
        memory[FONTSET_START_ADDRESS + i] = fontset[i];
    }

    // Initialize RNG
    randByte = std::uniform_int_distribution<uint8_t>(0, 255U);

    // Set up function pointer table
	table[0x0] = &Chip8::Table0;
	table[0x1] = &Chip8::OP_1nnn;
	table[0x2] = &Chip8::OP_2nnn;
	table[0x3] = &Chip8::OP_3xkk;
	table[0x4] = &Chip8::OP_4xkk;
	table[0x5] = &Chip8::OP_5xy0;
	table[0x6] = &Chip8::OP_6xkk;
	table[0x7] = &Chip8::OP_7xkk;
	table[0x8] = &Chip8::Table8;
	table[0x9] = &Chip8::OP_9xy0;
	table[0xA] = &Chip8::OP_Annn;
	table[0xB] = &Chip8::OP_Bnnn;
	table[0xC] = &Chip8::OP_Cxkk;
	table[0xD] = &Chip8::OP_Dxyn;
	table[0xE] = &Chip8::TableE;
	table[0xF] = &Chip8::TableF;

	table0[0x0] = &Chip8::OP_00E0;
	table0[0xE] = &Chip8::OP_00EE;

	table8[0x0] = &Chip8::OP_8xy0;
	table8[0x1] = &Chip8::OP_8xy1;
	table8[0x2] = &Chip8::OP_8xy2;
	table8[0x3] = &Chip8::OP_8xy3;
	table8[0x4] = &Chip8::OP_8xy4;
	table8[0x5] = &Chip8::OP_8xy5;
	table8[0x6] = &Chip8::OP_8xy6;
	table8[0x7] = &Chip8::OP_8xy7;
	table8[0xE] = &Chip8::OP_8xyE;

	tableE[0x1] = &Chip8::OP_ExA1;
	tableE[0xE] = &Chip8::OP_Ex9E;

	tableF[0x07] = &Chip8::OP_Fx07;
	tableF[0x0A] = &Chip8::OP_Fx0A;
	tableF[0x15] = &Chip8::OP_Fx15;
	tableF[0x18] = &Chip8::OP_Fx18;
	tableF[0x1E] = &Chip8::OP_Fx1E;
	tableF[0x29] = &Chip8::OP_Fx29;
	tableF[0x33] = &Chip8::OP_Fx33;
	tableF[0x55] = &Chip8::OP_Fx55;
	tableF[0x65] = &Chip8::OP_Fx65;
}


void Chip8::table0()
{
    (this->*(table0[opcode & 0x000Fu]))();
}


void Chip8::table8()
{
    (this->*(table8[opcode & 0x000Fu]))();
}


void Chip8::tableE()
{
    (this->*(tableE[opcode & 0x000Fu]))();
}


void Chip8::tableF()
{
    (this->*(tableF[opcode & 0x00FFu]))();
}


/* The cycle function of the emulated CPU,
 * that deals with fetching, decoding and executing opcodes.
 */
void Chip8::Cycle()
{
    // Fetch
    // We get the first digit of the opcode with a bitmask,
    // shift it over so that it becomes a single digit from 0 to F,
    // and use that as an index into the funciton pointer array.
    opcode = (memory[counter] << 8u) | memory[counter + 1];

    // Increment the program counter before execution
    counter += 2;

    // Decode and execute
    (this->*(table[(opcode & 0xF000u) >> 12u]))();

    // Decrement the delay timer if it's been set
    if (delayTimer > 0)
    {
        --delayTimer;
    }

    // Decrement the sound timer if ti's been set
    if (soundTimer > 0)
    {
        --soundTimer;
    }
}


/************************************/
/****** INSTRUCTIONS / OPCODES ******/
/************************************/


/* NOP
 *
 */
void Chip8::OP_NULL()
{}


/* 00E0: CLS
 * WHAT: Clears the display
 * HOW: Setting the entirety of the display buffer to zeroes
 */
 void Chip8::OP_00E0()
 {
    memset(display, 0, sizeof(display));
 }


 /* 00EE: RET
  * WHAT: Return from a subroutine
  * HOW: Decrement the stack pointer and copy the current value
  * on the stack to the program counter
  */
void Chip8::OP_00EE()
{
    --sp;
    counter = stack[sp];
}


/* 1nnn: JMP addr
 * WHAT: Jump to address nnn
 * HOW: Setting the program counter to nnn
 */
void Chip8::OP_1nnn()
{
    uint16_t address = opcode & 0x0FFFu;
    counter = address;
}


/* 2nnn: CALL addr
 * WHAT: Call subroutine at nnn
 * HOW: Push PC on the stack, and change PC's value to nnn
 */
 void Chip8::OP_2nnn()
 {
    uint16_t address = opcode & 0x0FFFu;

    stack[sp] = counter;
    ++sp;
    counter = address;
 }


/* 3xkk: SE Vx, byte
 * WHAT: Skip the next instruction if register Vx == byte
 * HOW: Using a bitmask to isolate Vx and byte, increment PC by 2
 */
 void Chip8::OP_3xkk()
 {
    uint8_t Vx = (opcode & 0x0F00u) >> 8u;
    uint8_t byte = opcode & 0x00FFu;

    if (registers[Vx] == byte)
    {
        counter += 2;
    }
 }


 /* 4xkk: SNE Vx, byte
  * WHAT: Skip next instruction if register Vx != byte
  * HOW: Using a bitmask to isolate Vx and byte, incrementing PC by 2
  */
void Chip8::OP_4xkk()
{
    uint8_t Vx = (opcode & 0x0F00u) >> 8u;
    uint8_t byte = opcode & 0x00FFu;

    if (registers[Vx] != byte)
    {
        counter += 2;
    }
}


/* 5xy0: SE Vx, Vy
  * WHAT: Skip next instruction if register Vx == register Vy
  * HOW: Using a bitmask to isolate Vx and Vy, incrementing PC by 2
  */
void Chip8::OP_5xy0()
{
    uint8_t Vx = (opcode & 0x0F00u) >> 8u;
    uint8_t Vy = (opcode & 0x00F0u) >> 4u;

    if (registers[Vx] == registers[Vy])
    {
        counter += 2;
    }
}


/* 6xkk: LD Vx, byte
  * WHAT: Load kk (the byte) into Vx (register #x out of 16)
  */
void Chip8::OP_6xkk()
{
    uint8_t Vx = (opcode & 0x0F00u) >> 8u;
    uint8_t byte = opcode & 0x00FFu;

    registers[Vx] = byte;
}


/* 7xkk: ADD Vx, byte
  * WHAT: Add kk (the byte) to Vx (register #x out of 16)
  */
void Chip8::7xkk()
{
    uint8_t Vx = (opcode & 0x0F00u) >> 8u;
    uint8_t byte = opcode & 0x00FFu;

    registers[Vx] += byte;
}


/* 8xy0: LD Vx, Vy
  * WHAT: Load registers[Vy]'s value into registers[Vx]
  */
void Chip8::OP_8xy0()
{
    uint8_t Vx = (opcode & 0x0F00u) >> 8u;
    uint8_t Vy = (opcode & 0x00F0u) >> 4u;

    registers[Vx] = registers[Vy];
}


/* 8xy1: OR Vx, Vy
  * WHAT: Bitwise OR registers[Vx] and registers[Vy],
  * and store the result in registers[Vx]
  */
void Chip8::OP_8xy1()
{
    uint8_t Vx = (opcode & 0x0F00u) >> 8u;
    uint8_t Vy = (opcode & 0x00F0u) >> 4u;

    registers[Vx] |= registers[Vy];
}


/* 8xy2: AND Vx, Vy
  * WHAT: Bitwise AND registers[Vx] and registers[Vy],
  * and store the result in registers[Vx]
  */
void Chip8::OP_8xy2()
{
    uint8_t Vx = (opcode & 0x0F00u) >> 8u;
    uint8_t Vy = (opcode & 0x00F0u) >> 4u;

    registers[Vx] &= registers[Vy];
}


/* 8xy3: XOR Vx, Vy
  * WHAT: Bitwise XOR registers[Vx] and registers[Vy],
  * and store the result in registers[Vx]
  */
void Chip8::OP_8xy3()
{
    uint8_t Vx = (opcode & 0x0F00u) >> 8u;
    uint8_t Vy = (opcode & 0x00F0u) >> 4u;

    registers[Vx] ^= registers[Vy];
}


/* 8xy4: ADD Vx, Vy
  * WHAT: Set Vx = Vx + Vy, set VF = overflow.
  * VF is registers[0xF], which functions as a flag register.
  */
void Chip8::OP_8xy4()
{
    uint8_t Vx = (opcode & 0x0F00u) >> 8u;
    uint8_t Vy = (opcode & 0x00F0u) >> 4u;
    uint16_t sum = registers[Vx] + registers[Vy];

    if (sum > 255U)  // If an overflow is detected, set flag register to 1.
    {
        registers[0xF] = 1;
    }
    else    // Otherwise, set it to 0.
    {
        registers[0xF] = 0;
    }

    registers[Vx] = sum & 0xFFu;
}


/* 8xy5: SUB Vx, Vy
  * WHAT: Set Vx = Vx - Vy, set VF = NOT carry.
  * VF is registers[0xF], which functions as a flag register.
  */
void Chip8::OP_8xy5()
{
    uint8_t Vx = (opcode & 0x0F00u) >> 8u;
    uint8_t Vy = (opcode & 0x00F0u) >> 4u;
    uint8_t difference = registers[Vx] - registers[Vy];

    if (difference < registers[Vx])  // If no carry is detected, set flag register to 1.
    {
        registers[0xF] = 1;
    }
    else    // Otherwise, set it to 0.
    {
        registers[0xF] = 0;
    }

    registers[Vx] = difference;
}


/* 8xy6: SHR Vx
 * WHAT: Right-shift registers[Vx] by 1.
 * If the least significant bit of Vx is 1,
 * then VF is set to 1. Otherwise, it is set to 0.
 */
void Chip8::OP_8xy6()
{
    uint8_t Vx = (opcode & 0x0F00u) >> 8u;
    
    // if (registers[Vx] & 0x0001 == 0x0001)
    // {
    //     registers[0xF] = 1;
    // }
    // else{
    //     registers[0xF] = 0;
    // }
    // THIS IS BETTER! Fucking facepalm
    registers[0xF] = registers[Vx] & 0x1u;

    registers[Vx] >>= 1;
}


/* 8xy7: SUBN Vx, Vy
  * WHAT: Set Vx = Vy - Vx, set VF = carry.
  * Produces the negative (if interpreted as signed) of 8xy5.
  */
void Chip8::OP_8xy7()
{
    uint8_t Vx = (opcode & 0x0F00u) >> 8u;
    uint8_t Vy = (opcode & 0x00F0u) >> 4u;

    if (registers[Vy] > registers[Vx])  // No carry is detected
    {
        registers[0xF] = 1;
    }
    else    // Carry is detected
    {
        registers[0xF] = 0;
    }

    registers[Vx] = registers[Vy] - registers[Vx];
}


/* 8xyE: SHL Vx {, Vy}
 * WHAT: Left-shift VX by 1
 * If the most significant bit of Vx is 1,
 * then VF is set to 1. Otherwise, it is set to 0.
 */
void Chip8::OP_8xyE()
{
   uint8_t Vx = (opcode & 0x0F00u) >> 8u;
   registers[0xF] = (registers[Vx] & 0x80u) >> 7u;
   registers[Vx] <<= 1;
}


/* 9xy0: SNE Vx, Vy
 * WHAT: Skip next instruction if Vx != Vy
 */
void Chip8::OP_9xy0()
{
    uint8_t Vx = (opcode & 0x0F00u) >> 8u;
    uint8_t Vy = (opcode & 0x00F0u) >> 4u;

    if (registers[Vx] != registers[Vy])
    {
        counter += 2;
    }
}


/* Annn: LD I, addr
 * WHAT: Set I (uint16_t index{}; in the Chip8 class) to addr
 * WHY: To perform operations on memory :)
 * (index is similar to si/di in x86 ASM)
 */
void Chip8::OP_Annn()
{
    uint16_t addr = opcode & 0x0FFFu;

    Chip8::index = addr;
}


/* Bnnn: JMP V0, addr
 * WHAT: Jump to location nnn + V0 (registers[0])
 */
void Chip8::OP_Bnnn()
{
    uint16_t addr = opcode & 0x0FFFu;

    counter = registers[0x0] + addr;
}


/* Cxkk: RND Vx, byte
 * WHAT: Set Vx = random byte AND kk
 */
void Chip8::OP_Cxkk()
{
    uint8_t Vx = (opcode & 0x0F00u) >> 8u;
    uint8_t byte = opcode & 0x00FFu;

    registers[Vx] = randByte(randGen) & byte;
}


/* Dxyn: DRW Vx, Vy, nibble
 * WHAT: Display n-byte sprite starting at memory location I (index)
 * at (Vx, Vy), and set VF = collision.
 * (every byte is a row of the sprite)
 */
void Chip8::OP_Dxyn()
{
    uint8_t Vx = (opcode & 0x0F00u) >> 8u;
    uint8_t Vy = (opcode & 0x00F0u) >> 4u;
    uint8_t height = opcode & 0x000Fu;

    // Wrap to the other side of the screen if going beyond boundaries
    uint8_t xPos = registers[Vx] % DISPLAY_WIDTH;
    uint8_t yPos = regsiters[Vy] % DISPLAY_HEIGHT;

    registers[0xF] = 0;

    for (unsigned int row = 0; row < height; ++row)
    {
        uint8_t spriteByte = memory[index + row];

        for (unsigned int col = 0; col < width; ++col)
        {
            uint8_t spritePixel = spriteByte & (0x80u >> col);  // 0x80u = 10000000b
            uint32_t* screenPixel = &display[(yPos + row) * DISPLAY_WIDTH + (xPos + col)];

            // If sprite pixel is on
            if (spritePixel)
            {
                // If the screen pixel is also on, there is a collision
                if (*screenPixel == 0xFFFFFFFF)
                {
                    registers[0xF] = 1;
                }

                // XORing the sprite pixel
                *screenPixel ^= 0xFFFFFFFF;
            }
        }
    }
}


/* Ex9E: SKP Vx
 * WHAT: Skip next instruction if key with the value of Vx is pressed
 */
void Chip8::OP_Ex9E()
{
    uint8_t Vx = (opcode & 0x0F00u) >> 8u; 

    uint8_t key = registers[Vx];

    if (keypad[key])
    {
        counter += 2;
    }
}


/* ExA1: SKNP Vx
 * WHAT: Skip next instruction if key with the value of Vx is not pressed
 */
void Chip8::OP_ExA1()
{
    uint8_t Vx = (opcode & 0x0F00u) >> 8u;

    uint8_t key = registers[Vx];

    if (!keypad[key])
    {
        counter += 2;
    }
}


/* Fx07: LD Vx, DT
 * WHAT: Set Vx = delay timer value
 */
 void Chip8::OP_Fx07()
 {
    uint8_t Vx = (opcode & 0x0F00u) >> 8u;
    
    registers[Vx] = Chip8::delayTimer;
 }


 /* Fx0A: LD Vx, K
  * WHAT: Wait for a valid key press, store the value of the key in Vx
  */
void Chip8::OP_Fx0A()
{
    uint8_t Vx = (opcode & 0x0F00u) >> 8u;

    if (keypad[0])
    {
        registers[Vx] = 0;
    }
    else if (keypad[1])
    {
        registers[Vx] = 1;
    }
    else if (keypad[2])
    {
        registers[Vx] = 2;
    }
    else if (keypad[3])
    {
        registers[Vx] = 3;
    }
    else if (keypad[4])
    {
        registers[Vx] = 4;
    }
    else if (keypad[5])
    {
        registers[Vx] = 5;
    }
    else if (keypad[6])
    {
        registers[Vx] = 6;
    }
    else if (keypad[7])
    {
        registers[Vx] = 7;
    }
    else if (keypad[8])
    {
        registers[Vx] = 8;
    }
    else if (keypad[9])
    {
        registers[Vx] = 9;
    }
    else if (keypad[10])
    {
        registers[Vx] = 10;
    }
    else if (keypad[11])
    {
        registers[Vx] = 11;
    }
    else if (keypad[12])
    {
        registers[Vx] = 12;
    }
    else if (keypad[13])
    {
        registers[Vx] = 13;
    }
    else if (keypad[14])
    {
        registers[Vx] = 14;
    }
    else if (keypad[15])
    {
        registers[Vx] = 15;
    }
    else
    {
        counter -= 2;
    }
    
    //// Alternatively:
    //
    // for (uint8_t i = 0; i < 0x10u; ++i)
    // {
    //     if (keypad[i])
    //     {
    //         registers[Vx] = i;
    //         break;
    //     }
    //
    //     counter -= 2;
    // }
}


/* Fx15: LD DT, Vx
 * WHAT: Set delay timer to Vx
 */
void Chip8::OP_Fx15()
{
    uint8_t Vx = (opcode & 0x0F00u) >> 8u;

    Chip8::delayTimer = registers[Vx];
}


/* Fx18: LD ST, Vx
 * WHAT: Set sound timer to Vx
 */
void Chip8::OP_Fx18()
{
    uint8_t Vx = (opcode & 0x0F00u) >> 8u;

    Chip8::soundTimer = registers[Vx];
}


/* Fx1E: ADD I, Vx
 * WHAT: Set I = I + Vx
 */
void Chip8::OP_Fx1E()
{
    uint8_t Vx = (opcode & 0x0F00u) >> 8u;

    Chip8::index += registers[Vx];
}


/* Fx29: LD F, Vx
 * WHAT: Set I = location of sprite for font character registers[Vx]
 */
void Chip8::OP_Fx29()
{
    uint8_t Vx = (opcode & 0x0F00u) >> 8u;

    Chip8::index = 0x50 + 5 * registers[Vx];
}


/* Fx33: LD B, Vx
 * WHAT: Store BCD representation of Vx in memory locations I, I+1 and I+2
 */
void Chip8::OP_Fx33()
{
    uint8_t Vx = (opcode & 0x0F00u) >> 8u;
    uint8_t bcd = registers[Vx];

    memory[index + 2] = bcd % 10;
    bcd /= 10;

    memory[index + 1] = bcd % 10;
    bcd /= 10;

    memory[index] = bcd;
}


/* Fx55: LD [I], Vx
 * WHAT: Store registers V0 through Vx in memory starting at location I
 */
void Chip8::OP_Fx55()
{
    uint8_t Vx = (opcode & 0x0F00u) >> 8u;

    for (uint8_t i = 0; i <= Vx; ++i)
    {
        memory[index + i] = registers[i];
    }
}


/* Fx65: LD Vx, [I]
 * WHAT: Write the memory starting at location I
 * to registers V0 through Vx
 */
void Chip8::OP_Fx65()
{
    uint8_t Vx = (opcode & 0x0F00u) >> 8u;

    for (uint8_t i = 0; i <= Vx; ++i)
    {
        registers[i]; = memory[index + i];
    }
}