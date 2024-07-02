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

.PHONY: clean
clean:
	rm -r ./build
