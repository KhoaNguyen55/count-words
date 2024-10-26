release:
	mkdir ./build
	gcc main.c -o ./build/main ./thirdparty/hashmap.c/hashmap.c -lpthread

dev:
	mkdir ./build
	gcc main.c -o ./build/main ./thirdparty/hashmap.c/hashmap.c -lpthread -g -O0
