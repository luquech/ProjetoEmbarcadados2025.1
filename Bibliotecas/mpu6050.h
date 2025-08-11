#ifndef MPU6050_H
#define MPU6050_H

#include <driver/i2c.h>
#include <stdint.h>

// MPU-6050
#define MPU6050_ADDR 0x68
#define MPU6050_ACCEL_XOUT_H 0x3B
#define MPU6050_PWR_MGMT_1 0x6B

void mpu6050_init();
void mpu6050_read_accel(int16_t *ax, int16_t *ay, int16_t *az);
float low_pass_filter(float new_value, float old_value, float alpha);

#endif // MPU6050_H
