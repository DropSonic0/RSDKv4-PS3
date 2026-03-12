#ifndef UPNP_H
#define UPNP_H

#include <stdint.h>

#if RETRO_PLATFORM == RETRO_PS3

void UPnP_AddPortMapping(uint16_t port);
void UPnP_DeletePortMapping(uint16_t port);

bool UPnP_IsMapped();
uint16_t UPnP_GetMappedPort();

#endif

#endif
