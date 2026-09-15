#ifndef MPU_7
#define MPU_7
#include <stdint.h>
typedef struct MPU {
  uint16_t ready_region;
  uint8_t current_region;
} mpu_t;
#endif
