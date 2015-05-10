# MoaT

or "Master of all Things" is a project to control various interesting
little things using 1wire, via a basic common interface.

## Goals

1wire is a nice protocol for wire-based home automation. It's reasonably
fast, stock hardware like basic I/O or temperature sensors are dirt cheap,
and you can attach tiny AVR controllers (think Arduino) without any
additional hardware.

It also has drawbacks. There's only one master, so you need to poll.
The available I/O solutions are limited. Most slaves can't be parameterized
in any meaningful way, can't work autonomously if the bus is wedged, and
don't use CONDITIONAL SEARCH (required for any non-trivial network).

There are efforts to overcome this problem, most notably the BAE0910/0911
which is a generic proprammable slave. I happen to not like that approach,
as OWFS then exports a whole lot of attributes, most of which are
irrelevant.

MoaT slaves do things differently. A common configuration file lists the
features for each device you program. The parameters are added to the flash
code (or, optionally, to the device's EEPROM) and can be interrogated via
1wire. OWFS then exports exactly those featurs which your device actually
uses.

## License

This is 1wire slave code for AVR microcontrollers.
Copyright (C) 2010-2015 Matthias Urlichs <matthias@urlichs.de>.


    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

## Implementation

The core code implements the basic 1wire discovery methods, including
single-device mode and conditional discovery.
Basic Code for e.g. the DS2423 fits in 2k on an ATtiny.
Barely, but it fits. ;-)

Overdrive speed is not implemented, and probably never will be,
because the timing constraints are too tight.

You can enable a debug pin which is a great help if you have timing
problems; just add a 2-channel oscilloscope.

The 64-bit ID is optionally read from EEPROM. A tool to generate the
8-bit CRC is included.

### History

Originally, this project consisted of an effort to convert convoluted code
from the net, history unknown but apparently licensed GPL2, to something
equally convoluted but more generic. And large. The work then languished
for a couple of years.

At the beginning of 2015, Matthias Urlichs used code from Tobias Mueller
<mail@tobynet.de> to shrink down and rewrite the whole thing.

The Makefile was trimmed, build and device configuration was moved to a
separate config file, and the project goal became a whole lot more
ambitious.

### 1wire slaves

It appears that Dallas Semiconductor doesn't like people who
implement 1wire slaves in software.

On the other hand, they do discontinue ICs like the DS2423 counter
for which no known substitute exists.

Therefore, code to emulate specific 1wire slave ICs will only be added to
this project's repository if the ICs are no longer available or "not
recommended for new design".

Of course, code that does things which doesn't have a silicon equivalent is
always welcome.

### OWFS

A nice and shiny 1wire client does not help if there's no server.
Therefore, if you add your own code, please also submit
appropriate changes to the owfs project so that other people 
can actually talk to your stuff.

The OWFS code for MoaT devices is located at
git@github.com:M-o-a-T/owfs.git .

NB: OWFS supports conditional search even if the "alarm" directory is
missing. You can still access it.

## HOWTO

See HOWTO.md

## TODO

### bugs

* make sure that an idle 1wire never results in a hung device

### optimizations

* interrupt-based port monitoring
* hardware-based PWM

### implementations

* secondary 1wire bus
* store config in EEPROM
* over-the-wire config changes
* over-the-wire firmware update
* /alarm handling
* some (esp. 1wire) statistics
* ADC
* PID
* SMOKE
* I2C interface
* THERMO via I2C
* HYGRO via I2C
* RF12 instead of 1wire

