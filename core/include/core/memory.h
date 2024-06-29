#ifndef CORE_MEMORY_H
#define CORE_MEMORY_H

#define READ_8BIT(mem_ptr, idx) \
    ((idx) += 1, \
     (uint8_t)(((mem_ptr)[(idx) - 1])))

#define READ_16BIT(mem_ptr, idx) \
    ((idx) += 2, \
     (uint16_t)(((mem_ptr)[(idx) - 2] << 8) | (mem_ptr)[(idx) - 1]))

#endif
