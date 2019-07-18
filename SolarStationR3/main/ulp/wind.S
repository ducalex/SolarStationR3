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
	.global io_number
io_number:
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
	ld r2, r3, 0
	sub r2, r2, 1
	st r2, r3, 0
	jump timer_reset, eq
	jump timer_ok

timer_reset:
	move r3, loops_in_period
	ld r2, r3, 0
	move r3, loops_before_reset
	st r2, r3, 0

	/* Reset edge counter */
	move r3, edge_count
	move r2, 0
	st r2, r3, 0

timer_ok:
	/* Load io_number */
	move r3, io_number
	ld r3, r3, 0
	move r0, r3
	jumpr read_io_high, 16, ge

	/* Read the value of lower 16 RTC IOs into R0 */
read_io_low:
	READ_RTC_REG(RTC_GPIO_IN_REG, RTC_GPIO_IN_NEXT_S, 16)
	rsh r0, r0, r3
	jump read_done

	/* Read the value of RTC IOs 16-17, into R0 */
read_io_high:
	READ_RTC_REG(RTC_GPIO_IN_REG, RTC_GPIO_IN_NEXT_S + 16, 2)
	sub r3, r3, 16
	rsh r0, r0, r3

read_done:
	and r0, r0, 1
	/* State of input changed? */
	move r3, next_edge
	ld r3, r3, 0
	add r3, r0, r3
	and r3, r3, 1
	jump edge_detected, eq
	/* Not changed */
	/* End program */
	halt

edge_detected:
	/* Flip next_edge */
	move r3, next_edge
	ld r2, r3, 0
	add r2, r2, 1
	and r2, r2, 1
	st r2, r3, 0
	/* Increment edge_count */
	move r3, edge_count
	ld r2, r3, 0
	add r2, r2, 1
	st r2, r3, 0
	/* Check if it's a new maximum */
	move r3, edge_count_max
	ld r1, r3, 0
	move r3, edge_count
	ld r0, r3, 0
	sub r2, r1, r0
	jump new_edge_max, ov
	/* No new max */
	/* End program */
	halt

new_edge_max:
	move r3, edge_count
	ld r0, r3, 0
	move r3, edge_count_max
	st r0, r3, 0
	/* End program */
	halt
