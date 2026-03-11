#ifndef UPNP_H
#define UPNP_H

#include <stdint.h>

#if RETRO_PLATFORM == RETRO_PS3

void UPnP_AddPortMapping(uint16_t port);
void UPnP_DeletePortMapping(uint16_t port);

#endif

#endif
