#pragma once
#include <Arduino.h>
#include <FreeRTOS.h>
#include <freertos/semphr.h>

extern SemaphoreHandle_t spi_a_mutex;
extern SemaphoreHandle_t spi_b_mutex;

// Must be called early in setup()
void bus_locks_init();
