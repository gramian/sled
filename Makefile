MSC     ?= /opt/MSC60A
OWC     ?= /opt/WATCOM

DOSBOX  ?= $(shell command -v dosbox         2>/dev/null || \
                   command -v dosbox-x       2>/dev/null || \
                   command -v dosbox-staging 2>/dev/null)

ifeq ($(strip $(DOSBOX)),)
$(error No DOSBox found. Install dosbox, dosbox-x or dosbox-staging, or set DOSBOX=/path/to/dosbox)
endif

SRC      = sled.c
OUT      = SLED.EXE

dosbox = $(DOSBOX) \
	-c "config -set cpu cycles=$(1)" \
	-c "mount q $(CURDIR)" \
	-c "q:"

.PHONY: build_msc build_owc tests bench run clean

build_msc:
	$(call dosbox,50000) \
		-c "mount c $(MSC)" \
		-c "set PATH=C:\BIN;C:\BINB" \
		-c "set INCLUDE=C:\INCLUDE" \
		-c "set LIB=C:\LIB" \
		-c "cl /AS /Fe$(OUT) /FPa /W2 /Ox /Zp2 /G2 $(SRC) /link /NOE /STACK:8192 > BUILD.LOG" \
		-c "exit"
	cat BUILD.LOG
	ls -lS $(OUT)

build_owc:
	$(call dosbox,50000) \
		-c "mount c $(OWC)" \
		-c "set WATCOM=C:\ " \
		-c "set PATH=C:\BINW" \
		-c "wcl /ms /fe=$(OUT) /l=dos /w2 /ox /zp=2 /2 $(SRC) /k8192 > BUILD.LOG" \
		-c "exit"
	cat BUILD.LOG
	ls -lS $(OUT)

tests:
	$(call dosbox,25000) \
		-c "$(OUT) /I tests.scm > TESTS.LOG" \
		-c "exit"
	cat TESTS.LOG

bench:
	$(call dosbox,25000) \
		-c "time > BENCH.LOG" \
		-c "$(OUT) /B bench.scm >> BENCH.LOG" \
		-c "time >> BENCH.LOG" \
		-c "exit"
	cat BENCH.LOG

run:
	$(call dosbox,25000) \
		-c "$(OUT)"

clean:
	rm -f $(OUT) *.obj *.map
