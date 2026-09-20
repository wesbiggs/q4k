/*
 * q4k -- probe page: a debug mailbox the host test harness can read.
 *
 * $CF00-$CFFF is carved out of the C runtime's reach by cfg/c64-q4k.cfg, so
 * nothing the compiler generates will ever clobber it. The program writes
 * progress markers here; tools/screendump.py polls them over VICE's binary
 * monitor to know when the emulated machine has reached a testable state,
 * instead of guessing with sleeps.
 *
 *   $CF00  'Q'  magic
 *   $CF01  '4'  magic
 *   $CF02  stage   -- bumped by the program as it reaches known points
 *   $CF03  counter -- free-running; use for turn/frame counts
 *   $CF04+ scratch -- assertion values (player x/y, hp, whatever a test needs)
 */

#ifndef Q4K_PROBE_H
#define Q4K_PROBE_H

#define PROBE          ((unsigned char *)0xCF00)
#define PROBE_MAGIC_LO 0x51 /* 'Q' */
#define PROBE_MAGIC_HI 0x34 /* '4' */

#define PROBE_STAGE    (PROBE[2])
#define PROBE_COUNTER  (PROBE[3])
#define PROBE_SCRATCH  (PROBE + 4)

/* Publish a stage marker. Writing the magic last makes the whole record
   valid atomically from the host's point of view. */
void probe_stage(unsigned char stage);

#endif /* Q4K_PROBE_H */
