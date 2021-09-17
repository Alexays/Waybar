.PHONY: build build-debug run clean default install

default: build

build:
	meson build
	ninja -C build

build-debug:
	meson build --buildtype=debug
	ninja -C build

install: build
	ninja -C build install

run: build
	./build/waybar

debug-run: build-debug
	./build/waybar --log-level debug

clean:
	rm -rf build
