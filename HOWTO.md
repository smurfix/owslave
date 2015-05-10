# HOWTO create a new MoaT 1wire device

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

