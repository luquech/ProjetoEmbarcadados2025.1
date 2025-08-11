#ifndef SDCARD_H
#define SDCARD_H

#include <stdbool.h>
#include "driver/sdmmc_types.h"

// SD Card
#define PIN_NUM_MISO GPIO_NUM_19
#define PIN_NUM_MOSI GPIO_NUM_23
#define PIN_NUM_CLK  GPIO_NUM_18
#define PIN_NUM_CS   GPIO_NUM_5
#define MOUNT_POINT "/sdcard"

extern sdmmc_card_t *card;
extern bool sd_card_initialized;

bool init_sd_card();
int read_high_score(const char *game_name);
void write_high_score(const char *game_name, int score);

#endif // SDCARD_H
