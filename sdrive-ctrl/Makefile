all: sdrive.com sdriven.com sdrivenh.com boot.bin

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

disk: sdrive-noboot.atr
sdrive-noboot.atr: sdrive.com
	[ -d disk ] || mkdir disk
	cp *.com disk/
	/media/soft/historic/atari-8bit/linux/unix2atr -u 720 sdrive-noboot.atr disk/

clean:
	rm -f *.com sdrive-noboot.atr boot.bin
	rm -rf disk
