#ifndef CORE_KEYS_H
#define CORE_KEYS_H

#include <stdint.h>

#define CH8_IO_KEY0_BIT  (0x1 << 0)
#define CH8_IO_KEY1_BIT  (0x1 << 1)
#define CH8_IO_KEY2_BIT  (0x1 << 2)
#define CH8_IO_KEY3_BIT  (0x1 << 3)
#define CH8_IO_KEY4_BIT  (0x1 << 4)
#define CH8_IO_KEY5_BIT  (0x1 << 5)
#define CH8_IO_KEY6_BIT  (0x1 << 6)
#define CH8_IO_KEY7_BIT  (0x1 << 7)
#define CH8_IO_KEY8_BIT  (0x1 << 8)
#define CH8_IO_KEY9_BIT  (0x1 << 9)
#define CH8_IO_KEYA_BIT  (0x1 << 10)
#define CH8_IO_KEYB_BIT  (0x1 << 11)
#define CH8_IO_KEYC_BIT  (0x1 << 12)
#define CH8_IO_KEYD_BIT  (0x1 << 13)
#define CH8_IO_KEYE_BIT  (0x1 << 14)
#define CH8_IO_KEYF_BIT  (0x1 << 15)

uint16_t MapKeyBit(uint8_t key);
uint8_t MapBitKey(uint16_t bit);

#endif
