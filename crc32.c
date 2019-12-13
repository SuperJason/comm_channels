#include <stdint.h> /* uint32_t */

#define CRC32_POLYNOMIAL_REV 0xedb88320l
uint32_t crc32(uint8_t *buf, uint32_t len)
{
    uint32_t crc = 0xffffffff;
    uint32_t crc_temp;
    int j;

    while (len > 0) {
        crc_temp = (crc ^ *buf) & 0xff;
        for (j = 8; j > 0; j--) {
            if (crc_temp & 1)
                crc_temp = (crc_temp >> 1)^CRC32_POLYNOMIAL_REV;
            else
                crc_temp >>= 1;
        }
        crc = ((crc >> 8) & 0x00FFFFFFL)^crc_temp;
        len--;
        buf++;
    }

    return(crc);
}

