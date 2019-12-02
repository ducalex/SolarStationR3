#include "soc/rtc_cntl_reg.h"
#include "soc/rtc_io_reg.h"
#include "soc/soc_ulp.h"

	.bss

	/* Loops after which we reset edge_count. Set by main CPU. */
	.global loops_in_period
loops_in_period:
	.long 0

	/* Current progression in running cycle. */
loops_before_reset:
	.long 0

	/* RTC IO number used to sample the input signal. Set by main CPU. */
	.global rtc_io
rtc_io:
	.long 0

	/* Maximum number of signal edges acquired. Main CPU resets it when it wakes up. */
	.global edge_count_max
edge_count_max:
	.long 0

	/* Total number of signal edges acquired this cycle */
edge_count:
	.long 0

	/* Next input signal edge expected: 0 (negative) or 1 (positive) */
next_edge:
	.long 0

	.text
	.global entry
entry:
	/* Check if it is time to reset edge_count */
	move r3, loops_before_reset
	ld r0, r3, 0
	jumpr timer_ok, 1, ge

timer_reset:
	move r3, edge_count
	move r2, 0
	st r2, r3, 0
	move r3, loops_in_period
	ld r0, r3, 0
	move r3, loops_before_reset
	st r0, r3, 0

timer_ok: /* (r0: loops_before_reset value, r3: loops_before_reset label) */
	/* Update loops_before_reset */
	sub r0, r0, 1
	st r0, r3, 0
	/* Load rtc_io */
	move r3, rtc_io
	ld r3, r3, 0
	move r0, r3
	jumpr read_io_high, 16, ge

read_io_low:
	READ_RTC_REG(RTC_GPIO_IN_REG, RTC_GPIO_IN_NEXT_S, 16)
	jump read_done
read_io_high:
	READ_RTC_REG(RTC_GPIO_IN_REG, RTC_GPIO_IN_NEXT_S + 16, 2)
	sub r3, r3, 16

read_done: /* (r0: io reading, r3: bit num) */
	rsh r0, r0, r3
	and r0, r0, 1
	/* State of input changed? */
	move r3, next_edge
	ld r1, r3, 0
	add r2, r0, r1
	and r2, r2, 1
	jump edge_detected, eq
	/* Not changed, halt */
	halt

edge_detected: /* (r1: next_edge value, r3: next_edge label) */
	/* Flip next_edge */
	add r1, r1, 1
	and r1, r1, 1
	st r1, r3, 0
	/* Increment edge_count */
	move r3, edge_count
	ld r2, r3, 0
	add r2, r2, 1
	st r2, r3, 0
	/* Check if it's a new maximum */
	move r3, edge_count_max
	ld r1, r3, 0
	sub r0, r1, r2
	jump new_edge_max, ov
	/* No new max, halt */
	halt

new_edge_max: /* (r2: edge_count value, r3: edge_count_max label) */
	st r2, r3, 0
	halt
