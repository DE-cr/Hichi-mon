/*
 * Hichi power monitor (https://github.com/DE-cr/Hichi-mon)
 * using esp32 with (optional) ssd1306 display
 * (file bmp_header.h)
 */

const unsigned char bmp_header[] = {
  0x42,0x4D,0x82,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x82,0x00,0x00,0x00,0x6C,0x00,
  0x00,0x00,0x80,0x00,0x00,0x00,0x40,0x00,0x00,0x00,0x01,0x00,0x01,0x00,0x00,0x00,
  0x00,0x00,0x00,0x04,0x00,0x00,0x23,0x2E,0x00,0x00,0x23,0x2E,0x00,0x00,0x02,0x00,
  0x00,0x00,0x02,0x00,0x00,0x00,0x42,0x47,0x52,0x73,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0xFF,
  0xFF,0x00
};

#define BMP_HEADER_SIZE sizeof( bmp_header )
#define BMP_IMAGE_SIZE ( 128 * 64 / 8 )
#define BMP_SIZE ( BMP_HEADER_SIZE + BMP_IMAGE_SIZE )