.PHONY: build build-debug run clean default install

default: run

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

clean:
	rm -rf build
