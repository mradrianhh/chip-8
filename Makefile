.PHONY: build
build: 
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
