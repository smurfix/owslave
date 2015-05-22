# HOWTO create a new MoaT 1wire device

## Quick intro

First, edit `world.cfg`. This is a YAML-formatted file. Aou'll need to edit
the `env` section and tell it which programmer you use.

Then, add your slaves to the "devices" section.

Let's say you want to use three ports of an ATmega88: a PWM output on pin
B0, a counter input on B1, and an alarm input on B2. Let's call your slave
"try1". So you'd do this:

    devices:
      try1:
        _doc: my first test slave
        _ref: defaults.target.m88
        types:
          port: 3
          pwm: 1
          count: 1
        port:
          1: B0_
          2: B1~
          3: B2~*
        pwm:
          1: 1
        count:
          1: 2

Now `make try1`. Your slave will get a randomized 1wire ID assigned, which
will be added to `world.cfg`.

Next, use `make burn_try1` to flash your device.

## Definitions

The `MoaT` code is pretty modular because basic features should fit in 4kBytes
of Flash storage. Therefore you can turn various features on. Or not.

### `have_uart`

This adds basic code to use the serial port.

### `have_uart_irq`

Use this to have the UART be interrupt-based. If this is off, the code will
use polling.

### `have_uart_sync`

If this is on, the UART will use neither interrupts nor polling; instead
it'll simply wait until the ATmega's send buffer is empty.

Use this for hard-to-find crashes; beware that about any serial output will
disrupt real functions.

### `have_timer`

Set this if your code uses the timer interrupt, i.e. functions from `timer.h`.

### `have_watchdog`

Turns on the watchdog timer at the start of the program. Uses the
longest-possible timeout.

If you're using MoaT, this means that you have to poll the bus, using
CONDITIONAL SEARCH, every eight seconds; to be safe, you should poll at
least every five seconds). This should be sufficient to restart a polling
server.

### `debug_uart`

If you need to debug the low-level UART code itself, set this. Should not be necessary.

### `debug_onewire`

If you need to debug the low-level 1wire code itself, set this. Should not be necessary.

### `uart_debug`

The debugger macros from `debug.h` will send their data to the UART.
Only do this if you connect a serial terminal to your ATmega.
Do not do this if you debug the UART.

### `console_debug`

The debugger macros from `debug.h` will send their data to the 1wire
console buffer.
Careful when debugging 1wire itself.

### `console_write`

You can write to the console via 1wire. This is not yet useful, except for
echo testing.

### `console_ping`

Write a '!' every N tenths-of-a-second, as a keepalive test. Needs the timer.

### `have_dbg_port`

Use a port for diagnostic code output. Which port to use is defined in
`features.h`. The DBG(x) macro will then take the binary value of X and
display it on the port in question. Useful for low-level timing analysis
using a 'scope with digital inputs, or to discover where your slave code
crashes.

### `have_dbg_pin`

Use a pin for diagnostic signalling. Which pin to use is defined in 
`features.h`. The `DBG_ON()`/`DBG_OFF()` macros will turn this pin on or
off. Useful for low-level timing analysis using a 'scope with a trigger
input.

### `is_onewire`

The 1wire device type (moat, ds2408, ds2423). `moat` is tested, everything
else has not been, lately.

### `need_bits`

Include code required to send or receive single bits on 1wire.

### `use_eeprom`

Store configuration data in EEPROM. This may or may not make config data
actually writeable.

### `conditional_search`

Make your 1wire slave discoverable conditionally. You can configure which
conditions may cause an alarm.

### `single_device`

Add 1wire code for `SKIP_ROM` and `READ_ROM`. You probably do not need this.

## Features

The MoaT slave code can do a lot of things. You can use the device's `types`
section to control what gets included (and, for some features, how many).

### port

Your basic digital input/output pin. Your AVR device's data sheet names
them "B0" or "D7" or whatever. On the 1wire side, pins are numbered 1 to N.
Use the `port` section to tell MoaT which pin numbers correspond to which
hardware output.

Pins can have one of four states:

* One or High, i.e. tied to +5V (or whatever).
* Zero or Low, i.e. tied to ground.
* Pull-Up, i.e. tied to +5V (or whatever) via an internal resistor,
  so that an external input can still ground it ("wired-AND").
* High impedance, i.e. sensing whether the input is 1 or 0 passively.

Reading the pin will return whatever signal is physically present.
Writing to the pin may or may not change its state. Typically, 
an output will switch between 0 and 1. If you write to an input, you can
turn the pull-up resistor on and off.

This is not always what you want: if you have a wired-AND pin, you'd want
to switch between "pull-up is on" and "pull-down to zero". You can do that
by adding a `/` to the port description. Alternately, if you have an
external pull-up resistor, you can add `!`, which switches between "high
impedance" and "pull-down to zero", instead. These modifiers also apply to
the other two states, but that's usually not useful.

Please do not bother this author with Arduino-based port numbers.

Thus, if you want to control the output ports B0 and B1, both of which
shall be low initially:

    types:
      port: 2
    port:
      1: B0_
      2: B1_

### pwm

You can tell MoaT to switch a port on and off periodically. Let's say you
want to toggle B1 every second, you'd extend the above example thus:

    types:
      port: 2
      pwm: 1
    port:
      1: B0_
      2: B1_
    pwm:
      1: 2

and then "owwrite /F0.123456789ABC.DE/pwm.1 10,10"

This is (amost) exactly as accurate as the clock of your ATmega.

### count

If you're more interested in how often an input pin changes state than in
its actual state, you should use a counter.

Let's add the counted input pin D5 to our example:

    types:
      port: 2
      pwm: 1
      count: 1
    port:
      1: B0_
      2: B1_
      3: D5~
    count:
      1: 3

You can now `owread /F0.123456789ABC.DE/count.1`.

