all: sdrive.com sdriven.com sdrivenh.com boot.bin disk

sdrive.com: sdrive.xa version.xa
	xa -o $@ sdrive.xa -DTYPE=1 -DHELP=0

sdriven.com: sdrive.xa version.xa
	xa -o $@ sdrive.xa -DTYPE=2 -DHELP=0

sdrivenh.com: sdrive.xa manual.xa version.xa
	xa -o $@ sdrive.xa -DTYPE=2 -DHELP=1

boot.bin: boot.xa
	xa -o $@ boot.xa

helptest:
	xa -o $@.com $@.xa

unix2atr:

disk: sdrive.atr
sdrive.atr: sdrive.com sdriven.com sdrivenh.com boot.bin unix2atr
	[ -d disk ] || mkdir disk
	cp *.com addon/* disk/
	./unix2atr -u 720 $@ disk/
	dd if=boot.bin of=$@ conv=notrunc bs=1 seek=16

clean:
	rm -f *.com sdrive.atr boot.bin unix2atr
	rm -rf disk
