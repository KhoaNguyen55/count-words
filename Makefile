multithread:
	mkdir -p ./build
	gcc main.c -o ./build/multithread ./thirdparty/hashmap.c/hashmap.c -lpthread

singlethread:
	mkdir -p ./build
	gcc main.c -o ./build/singlethread ./thirdparty/hashmap.c/hashmap.c -lpthread -DSINGLETHREAD

dev:
	mkdir -p ./build
	gcc main.c -o ./build/main ./thirdparty/hashmap.c/hashmap.c -lpthread -g -O0
