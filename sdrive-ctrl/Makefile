all: sdrive.com sdriven.com sdrivenh.com

sdrive.com: sdrive.xa version.xa
	xa -o $@ sdrive.xa -DTYPE=1 -DHELP=0

sdriven.com: sdrive.xa version.xa
	xa -o $@ sdrive.xa -DTYPE=2 -DHELP=0

sdrivenh.com: sdrive.xa manual.xa version.xa
	xa -o $@ sdrive.xa -DTYPE=2 -DHELP=1

helptest:
	xa -o $@.com $@.xa

clean:
	rm *.com
