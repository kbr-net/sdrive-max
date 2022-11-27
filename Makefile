DIRS = atmega328* sdrive-ctrl

all:	sboot.h
	for dir in $(DIRS); \
		do $(MAKE) -C $$dir || exit 1; \
	done

sboot.h:	sboot.xa
	xa -o boot_xex_loader $<
	xxd -i boot_xex_loader > $@
	sed -i 's/char/char EEMEM/' $@

clean:
	for dir in $(DIRS); \
		do $(MAKE) -C $$dir clean; \
	done
	rm boot_xex_loader sboot.h eeprom_writer.h
