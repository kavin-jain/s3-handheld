// ESP-NOW mesh — router-free broadcast messaging. Framing in espnow_msg.h (tested).
#pragma once
#include <stdint.h>

bool        espnow_begin();
bool        espnow_active();
uint32_t    espnow_rx();
const char *espnow_last();
void        espnow_broadcast(const char *text);
