.PHONY: build run clean default

default: run

build:
	meson build
	ninja -C build

build-debug:
	meson build --buildtype=debug
	ninja -C build

run: build
	./build/waybar

clean:
	rm -rf build
