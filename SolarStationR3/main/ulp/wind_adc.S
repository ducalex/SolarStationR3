#include "soc/rtc_cntl_reg.h"
#include "soc/rtc_io_reg.h"
#include "soc/soc_ulp.h"

	.bss

	.global rtc_io
rtc_io:
	.long 0

	.global adc_reading_max
adc_reading_max:
	.long 0

	.text
	.global entry
entry:
	move r0, 0
	/* Take four measurements */
	adc r1, 0, 6
	add r0, r0, r1
	adc r1, 0, 6
	add r0, r0, r1
	adc r1, 0, 6
	add r0, r0, r1
	adc r1, 0, 6
	add r0, r0, r1

    /* Average our results (divide by 4) */
	rsh r0, r0, 2
	move r3, last_reading
	st r0, r3, 0

    /* Check if we have a new maximum */
	move r3, adc_reading_max
	ld r3, r3, 0
	sub r1, r3, r0
	jump new_max, ov
    /* No new max, we stop here */
    halt

new_max:
	st r1, r3, 0
	halt
