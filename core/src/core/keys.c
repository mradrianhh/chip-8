#include <assert.h>

#include "core/keys.h"

static uint16_t KeyBitMap[16] = {
    CH8_IO_KEY0_BIT,
    CH8_IO_KEY1_BIT,
    CH8_IO_KEY2_BIT,
    CH8_IO_KEY3_BIT,
    CH8_IO_KEY4_BIT,
    CH8_IO_KEY5_BIT,
    CH8_IO_KEY6_BIT,
    CH8_IO_KEY7_BIT,
    CH8_IO_KEY8_BIT,
    CH8_IO_KEY9_BIT,
    CH8_IO_KEYA_BIT,
    CH8_IO_KEYB_BIT,
    CH8_IO_KEYC_BIT,
    CH8_IO_KEYD_BIT,
    CH8_IO_KEYE_BIT,
    CH8_IO_KEYF_BIT,
};

uint16_t MapKeyBit(uint8_t key)
{
    assert(key < 16);
    return KeyBitMap[key];
}

uint8_t MapBitKey(uint16_t bit)
{
    for(uint8_t i = 0; i < 16; i++)
    {
        if(KeyBitMap[i] == bit)
        {
            return i;
        }
    }

    // Invalid return. No match.
    return 0xFF;
}
