# Core

## Graphics

The chip-8 has a 64x32 pixel monochrome display.
The pixels are accessed individually through a display buffer.
A pointer to this display buffer is provided to the graphics library so it's able to access the buffer
without passing it as an argument. This reduces overhead since the draw-command will be issued quite often.
When the chip-8 device issues a draw command, the graphics library accesses this buffer
to update a texture which is then rendered to the screen.
