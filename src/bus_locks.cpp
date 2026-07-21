#include "bus_locks.h"

SemaphoreHandle_t spi_a_mutex = NULL;
SemaphoreHandle_t spi_b_mutex = NULL;

void bus_locks_init() {
  if (spi_a_mutex == NULL) spi_a_mutex = xSemaphoreCreateRecursiveMutex();
  if (spi_b_mutex == NULL) spi_b_mutex = xSemaphoreCreateRecursiveMutex();
}
