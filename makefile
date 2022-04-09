CC=clang++

build:util/spirv_embeder.out util/ninja_gen.out
	./util/ninja_gen.out 
	ninja

clean:
	ninja -t clean
	rm -f build.ninja 


util/ninja_gen.out:util/ninja_gen.cpp
	$(CC) -lfmt -std=c++20 -O2 -s $< -o $@

util/spirv_embeder.out:util/spirv_embeder.cpp
	$(CC) -std=c++20 $< -lfmt -O2 -s -o $@ 

.PHONY:build clean 