MSC     ?= /opt/MSC60A
OWC     ?= /opt/WATCOM

DOSBOX  ?= $(shell command -v dosbox         2>/dev/null || \
                   command -v dosbox-x       2>/dev/null || \
                   command -v dosbox-staging 2>/dev/null)

ifeq ($(strip $(DOSBOX)),)
$(error No DOSBox found. Install dosbox, dosbox-x or dosbox-staging, or set DOSBOX=/path/to/dosbox)
endif

OUT    = SLED.EXE

dosbox = $(DOSBOX) \
	-c "config -set cpu cycles=$(1)" \
	-c "mount q $(CURDIR)" \
	-c "q:" \
	-exit

.PHONY: build_msc build_owc tests bench run clean dosway

build_msc:
	$(call dosbox,50000) \
		-c "mount c $(MSC)" \
		-c "set PATH=C:\BIN;C:\BINB" \
		-c "set INCLUDE=C:\INCLUDE" \
		-c "set LIB=C:\LIB" \
		-c "BUILD MSC"
	cat BUILD.LOG

build_owc:
	$(call dosbox,50000) \
		-c "mount c $(OWC)" \
		-c "set WATCOM=C:" \
		-c "set PATH=C:\BINW" \
		-c "BUILD OWC"
	cat BUILD.LOG

tests:
	$(call dosbox,25000) \
		-c "BUILD TESTS"
	cat TESTS.LOG

bench:
	$(call dosbox,25000) \
		-c "BUILD BENCH"
	cat BENCH.LOG

demos:
	$(call dosbox,25000) \
		-c "DEMO"

run:
	$(call dosbox,25000) \
		-c "$(OUT)"

dosway:
	$(call dosbox,50000) \
		-c "CD dosway" \
		-c "DOSWAY"

clean:
	rm -f $(OUT) *.OBJ *.LOG
