Using a reed switch and assuming 0.30m circumference of the anemometer:
RPM per km/h: 55
RPM for 100km/h: 5500
RPM for 10km/h: 550

So we need a sampling frequency of at least 11khz to measure up to 100km/h
but more realistically 20khz should be the minimum for accuracy.
One sample takes about 50-100 ULP cycles, and the ULP runs at 8mhz so a 25%
duty cycle would give plenty resolution and allow for lower power.

We should manage sub-1mA power with that method

I suggest (pseudo-C):

    // Set ULP to wake up every 50us or something
    static {
        int reset_timer = 0;
        int top_rotations = 0;
        int rotations = 0;
        int last_state = 0;
    }

    if (reset_timer < now) {
        if (rotations > top_rotations) {
            top_rotations = rotations;
        }
        reset_timer = now + 10; // reset every 10 seconds
        rotations = 0;
    }

    if (last_state != GPIO) { // Transition
        rotations++;
    }

    last_state = GPIO;

When the ESP32 wakes up it can simply read top_rotations to get the top speed while it was sleeping

This seems to be doing exactly that: https://github.com/espressif/esp-idf/tree/master/examples/system/ulp
Very nice!








Using a motor and reading an ADC value assuming 0.30m circumference of the anemometer:
Assuming we can find a way to calculate RPM from voltage (or burden current), we could
sample the ADC value every 1ms.
That's assuming we can enable/disable the ADC on demand from the ULP, because it uses 1-2mA.

I suggest (pseudo-C):

    // Set ULP to wake up every 1ms or something
    static {
        int top_value = 0;
    }

    int adcval = ADC;

    if (adcval > top_value) {
        top_value = adcval;
    }


When the ESP32 wakes up it can simply read top_value to get the top speed while it was sleeping.
