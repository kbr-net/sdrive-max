DIRS = atmega328* sdrive-ctrl

all:	sboot.h highspeed.h
	for dir in $(DIRS); \
		do $(MAKE) -C $$dir || exit 1; \
	done

sboot.h:	sboot.xa
	xa -o boot_xex_loader $<
	xxd -i boot_xex_loader > $@
	sed -i 's/char/char EEMEM/' $@

highspeed.h:	highspeed.xa
	xa -o highspeed $<
	xxd -i highspeed > $@
	sed -i 's/char/char EEMEM/' $@

clean:
	for dir in $(DIRS); \
		do $(MAKE) -C $$dir clean; \
	done
	rm boot_xex_loader sboot.h eeprom_writer.h highspeed.h highspeed
