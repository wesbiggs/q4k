/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <q4k/probe.h>

void probe_stage(unsigned char stage)
{
    PROBE[2] = stage;
    PROBE[0] = PROBE_MAGIC_LO;
    PROBE[1] = PROBE_MAGIC_HI;
}
