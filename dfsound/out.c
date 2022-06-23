#include <stdio.h>
#include <stdlib.h>
#include "out.h"

#define HAVE_SDL
#define MAX_OUT_DRIVERS 2

static struct out_driver out_drivers[MAX_OUT_DRIVERS];
struct out_driver *out_current;
static int driver_count;

#define REGISTER_DRIVER(d) { \
	extern void out_register_##d(struct out_driver *drv); \
	out_register_##d(&out_drivers[driver_count++]); \
}

void SetupSound(void)
{
	int i;

	if (driver_count == 0) {
#ifdef HAVE_SDL
		REGISTER_DRIVER(sdl);
#endif
#ifdef HAVE_CUBE
		REGISTER_DRIVER(cube);
#endif
	}

	for (i = 0; i < driver_count; i++)
		if (out_drivers[i].init() == 0)
			break;

	if (i < 0 || i >= driver_count) {
		//printf("the impossible happened\n");
		abort();
	}

	out_current = &out_drivers[i];
	//printf("selected sound output driver: %s\n", out_current->name);
}

