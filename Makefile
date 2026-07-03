.PHONY: build build-debug run clean default install

default: build

build:
	meson setup build
	ninja -C build

build-debug:
	meson setup build --buildtype=debug
	ninja -C build

install: build
	ninja -C build install

run: build
	./build/waybar

debug-run: build-debug
	./build/waybar --log-level debug

test:
	meson test -C build --verbose --suite waybar
.PHONY: test

test-detailed:
	meson test -C build --verbose --print-errorlogs --test-args='--reporter console -s'
.PHONY: test-detailed

clean:
	rm -rf build
