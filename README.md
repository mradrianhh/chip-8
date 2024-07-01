# chip-8
chip-8 emulator written in C.

## Project structure

### Core library

Contains the logic of the device and it's components.

### UI library

Contains the UI logic. The internal graphic logic of the device is contained in the core library.

### Graphics library

Contains a shared API for displaying graphics. Both the UI and Core library uses this to handle graphics.

### App

Contains the executable.

## Specifications

- 64 width x 32 height pixel monochrome display.
- 4K 8-bit RAM.
- 16 8-bit variable registers. V0-VF.
- 1 16-bit address register. I.
- Stack of 16-bit addresses.
- 16-bit PC.
- 8-bit delay timer. Decrements at 60hz until it reaches 0.
- 8-bit sound timer. Decrements at 60hz until it reaches 0 and beeps.
- 16-key keypad with scan codes 0x1-0xF.

## ISA.

The ISA consists of 35 opcodes which are all two bytes long and big endian.

The CPU has 16 8-bit variable registers ranging from V0-VF, 1 16-bit index register, I,
and 1 16-bit PC.

Additionally, to support subroutine calls, the CPU depends on a stack of 16-bit addresses.

### Opcode table.

- NNN: Address.
- NN:  8-bit constant.
- N:   4-bit constant.
- X/Y: 4-bit register identifier.
- PC:  Program counter.
- I:   12-bit register for memory address.
- VN:  One of the variable registers. N = 0-F.

| Opcode | Type     | C psuedo          | Explanation |
|--------|----------|-------------------|-------------|
| 0NNN   | Call     |                   | Calls machine code routine at address NNN. |
| 00E0   | Display  | disp_clear()      | Clears the screen. |
| 00EE   | Flow     | return            | Returns from a subroutine. |
| 1NNN   | Flow     | goto NNN          | Jumps to address NNN. |
| 2NNN   | Flow     | *(0xNNN)()        | Calls subroutine at address NNN. |
| 3XNN   | Cond     | if (Vx == NN)     | Skips next instruction if Vx == NN. |
| 4XNN   | Cond     | if (Vx != NN)     | Skips the next instruction if Vx != NN. |
| 5XY0   | Cond     | if (Vx == Vy)     | Skips the next instruction if Vx == Vy. |
| 6XNN   | Const    | Vx = NN           | Sets Vx to NN. |
| 7XNN   | Const    | Vx += NN          | Adds NN to Vx. Carry flag not changed. |
| 8XY0   | Assig    | Vx = Vy           | Sets Vx to Vy. |
| 8XY1   | BitOp    | Vx = Vx | Vy      | Sets Vx to Vx | Vy. |
| 8XY2   | BitOp    | Vx = Vx & Vy      | Sets Vx to Vx & Vy. |
| 8XY3   | BitOp    | Vx = Vx ^ Vy      | Sets Vx to Vx ^ Vy. |
| 8XY4   | Math     | Vx = Vx + Vy      | Adds Vy to Vx. VF set if overflow. |
| 8XY5   | Math     | Vx = Vx - Vy      | Subtracts Vy from Vx. VF set if not underflow. |
| 8XY6   | BitOp    | Vx = Vx >> 1      | Right-shifts Vx by 1 and stores least-significant bit prior to shift in VF. |
| 8XY7   | Math     | Vx = Vy - Vx      | Sets Vx to Vy - Vx. VF set if not underflow. |
| 8XYE   | BitOp    | Vx = Vx << 1      | Left-shifts Vx by 1 and stores most-significant bit prior to shift in VF. |
| 9XY0   | Cond     | if (Vx != Vy)     | Skips the next instruction if Vx != Vy. |
| ANNN   | MEM      | I = NNN           | Sets register I to NNN. |
| BNNN   | Flow     | PC = V0 + NNN     | Jumps to address V0 + NNN. |
| CXNN   | Rand     | Vx = rand() & NN  | Sets Vx to bitwise-and between random number and NN. |
| DXYN   | Display  | draw(Vx, Vy, N)   | Draws a sprite at (Vx, Vy) with width of 8 pixels and      height of N pixels. VF is set if any pixels are flipped from set to unset when sprite is drawn. |
| EX9E   | KeyOp    | if (key() == Vx ) | Skips the next instruction if the key stored in Vx is pressed. |
| EXA1   | KeyOp    | if (key() != Vx)  | Skips the next instruction if the key stored in Vx is not pressed. |
| FX07   | Timer    | Vx = delay_timer() | Sets Vx to the value of the delay timer. |
| FX0A   | KeyOp    | Vx = get_key()    | Waits for keypress and assigns it to Vx. |
| FX15   | Timer    | delay_timer(Vx)   | Sets delay timer to Vx. |
| FX18   | Timer    | sound_timer(Vx)   | Sets sound timer to Vx. |
| FX1E   | MEM      | I = I + Vx        | Adds Vx to I. VF not affected. |
| FX29   | MEM      | I = sprite_addr[Vx] | Sets I to the location of the sprite indexed by Vx. |
| FX33   | BCD      |                   | Stores the BCD of VX with 100th degit at I. 10th at I+1, 1th at I+2. |
| FX55   | MEM      | reg_dump(Vx, &I)  | Stores V0-VX in memory starting from I.
| FX65   | MEM      | reg_load(Vx, &I)  | Loads V0-VX from memory starting from I.
