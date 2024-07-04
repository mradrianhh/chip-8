#include "maths/maths.h"

uint32_t ClampUINT_32(uint32_t num, uint32_t min, uint32_t max)
{
    const double temp = num < min ? min : num;
    return temp > max ? max : temp;
}
