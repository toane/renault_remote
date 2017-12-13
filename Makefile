default: main_atmega.hex

main_atmega.o: main_atmega.c
	avr-gcc -g -Os -mmcu=atmega88 -c -std=gnu99 main_atmega.c 

main_atmega.elf: main_atmega.o
	avr-gcc -g -mmcu=atmega88 -o main_atmega.elf main_atmega.o
	
main_atmega.hex: main_atmega.elf
	avr-objcopy -j .text -j .data -O ihex main_atmega.elf main_atmega.hex
	
flash: main_atmega.hex
	avrdude -pm88 -cusbasp -Uflash:w:main_atmega.hex:a
	
clean:
	rm -f main_atmega.elf main_atmega.hex main_atmega.o
	rm -f main_attiny.elf main_attiny.hex main_attiny.o
	
