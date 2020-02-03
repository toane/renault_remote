Circuit and code to convert button presses on a generic wired Renault remote to commands for an aftermarket JVC car stereo, using an ATtiny85 or an ATmega88.

ATmega version supports:
VOLUME_UP
VOLUME_DOWN
MUTE
NEXT
PREV
EQUALIZER
SOURCE
![Circuit schematic for ATmega88](circuit_schematic_atmega.png)

ATtiny version supports:
VOLUME_UP
VOLUME_DOWN
MUTE
NEXT
PREV
![Circuit schematic for ATtiny85](circuit_schematic_attiny.png)

## Prerequisites

```sh
apt-get install avr-libc gcc-avr avrdude
```

### ATmega

```sh
# compile for ATmega
make
# flash to ATmega
make flash
```

### ATtiny
```sh
# compile for ATtiny
make tiny
# flash to ATtiny
make flashtiny
```

### clean workspace
```sh
$ make clean
```
