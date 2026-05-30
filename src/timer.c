/*
 * timer.c — multimedia timer wrapper, portable stub.
 *
 * The original used Win32 winmm: timeGetDevCaps + timeBeginPeriod +
 * timeSetEvent for periodic callbacks. In the SDL build we let SDL drive
 * frame pacing via VSYNC, so this file degrades to a placeholder.
 *
 * The full Win32 implementation is reproduced verbatim in the report's
 * §3.4 in case anyone wants to restore it.
 */
#include "wacki.h"
#include <string.h>

int InitializeMmTimer(void *self_) { (void)self_; return 1; }

int ArmPeriodicCallback(void *self_, uint32_t period_ms,
                        uint32_t flags, void (*fn)(void))
{
    (void)self_; (void)period_ms; (void)flags; (void)fn;
    return 0;
}
