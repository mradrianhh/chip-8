.PHONY: build-shaders
build-shaders:
	cd ./assets/shaders && \
	./compile.sh

.PHONY: build
build: build-shaders
	mkdir -p ./build && \
	cd ./build && \
	cmake .. && \
	make

.PHONY: run
run: build
	cd ./build/app && \
	./chip-8

.PHONY: test1 test2 test3 test4 test5 test6 test7 test8
test1: build
	cd ./build/app && \
	./chip-8 ../../assets/roms/test_suite/1-chip8-logo.ch8

test2: build
	cd ./build/app && \
	./chip-8 ../../assets/roms/test_suite/2-ibm-logo.ch8

test3: build
	cd ./build/app && \
	./chip-8 ../../assets/roms/test_suite/3-corax+.ch8

test4: build
	cd ./build/app && \
	./chip-8 ../../assets/roms/test_suite/4-flags.ch8

test5: build
	cd ./build/app && \
	./chip-8 ../../assets/roms/test_suite/5-quirks.ch8

test6: build
	cd ./build/app && \
	./chip-8 ../../assets/roms/test_suit/6-keypad.ch8

test7: build
	cd ./build/app && \
	./chip-8 ../../assets/roms/test_suite/7-beep.ch8

test8: build
	cd ./build/app && \
	./chip-8 ../../assets/roms/test_suite/8-scrolling.ch8

.PHONY: clean
clean:
	rm -r ./build
