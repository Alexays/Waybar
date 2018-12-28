.PHONY: build run clean default

default: run

build:
	meson build
	ninja -C build

run: build
	./build/waybar

clean:
	rm -rf build
