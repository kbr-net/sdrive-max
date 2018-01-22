all: sdrive sdriven sdrivenh

sdrive:
	xa -o $@.com sdrive.xa -DTYPE=1 -DHELP=0

sdriven:
	xa -o $@.com sdrive.xa -DTYPE=2 -DHELP=0

sdrivenh:
	xa -o $@.com sdrive.xa -DTYPE=2 -DHELP=1

clean:
	rm *.com
