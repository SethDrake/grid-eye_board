#pragma once
#ifndef __MLX90640_H
#define __MLX90640_H

#define MLX90640_ADDR 0x33 << 1
#define MLX90640_DEVICEID1 0x2407
#define MLX90640_DEVICEID2 0x2408
#define MLX90640_DEVICEID3 0x2409

#define SCALEALPHA 0.000001

typedef enum mlx90640_mode {
  MLX90640_INTERLEAVED, ///< Read data from camera by interleaved lines
  MLX90640_CHESS,       ///< Read data from camera in alternating pixels
} mlx90640_mode_t;

/** Internal ADC resolution for pixel calculation */
typedef enum mlx90640_resolution {
  MLX90640_ADC_16BIT,
  MLX90640_ADC_17BIT,
  MLX90640_ADC_18BIT,
  MLX90640_ADC_19BIT,
} mlx90640_resolution_t;

/** How many PAGES we will read per second (2 pages per frame) */
typedef enum mlx90640_refreshrate {
  MLX90640_0_5_HZ,
  MLX90640_1_HZ,
  MLX90640_2_HZ,
  MLX90640_4_HZ,
  MLX90640_8_HZ,
  MLX90640_16_HZ,
  MLX90640_32_HZ,
  MLX90640_64_HZ,
} mlx90640_refreshrate_t;

typedef struct {
  int16_t kVdd;
  int16_t vdd25;
  float KvPTAT;
  float KtPTAT;
  uint16_t vPTAT25;
  float alphaPTAT;
  int16_t gainEE;
  float tgc;
  float cpKv;
  float cpKta;
  uint8_t resolutionEE;
  uint8_t calibrationModeEE;
  float KsTa;
  float ksTo[5];
  int16_t ct[5];
  uint16_t alpha[768];
  uint8_t alphaScale;
  int16_t offset[768];
  int8_t kta[768];
  uint8_t ktaScale;
  int8_t kv[768];
  uint8_t kvScale;
  float cpAlpha[2];
  int16_t cpOffset[2];
  float ilChessC[3];
  uint16_t brokenPixels[20];
  uint16_t outlierPixels[20];
} paramsMLX90640_t;

#define OPENAIR_TA_SHIFT 8 ///< Default 8 degree offset from ambient air

#endif /* __MLX90640_H */