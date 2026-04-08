DIRS = atmega328* sdrive-ctrl

all:	sboot.h highspeed.h
	for dir in $(DIRS); \
		do $(MAKE) -C $$dir || exit 1; \
	done

sboot.h:	sboot.xa
	xa -o boot_xex_loader $<
	xxd -i boot_xex_loader > $@
	sed -i 's/char/char EEMEM/' $@
	sed -i 's/unsigned int.*//' $@

highspeed.h:	highspeed.xa
	xa -o highspeed $<
	xxd -i highspeed > $@
	sed -i 's/char/char EEMEM/' $@
	sed -i 's/unsigned int/const unsigned int/' $@

clean:
	for dir in $(DIRS); \
		do $(MAKE) -C $$dir clean; \
	done
	rm boot_xex_loader sboot.h eeprom_writer.h highspeed.h highspeed
