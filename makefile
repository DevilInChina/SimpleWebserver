com=gcc
std=-std=gnu99
cc=${com} ${std}
compile:
	${cc} webserver.c thpool.c jwHash.c list.c shift.c heap.c filesys.c -DHASHTHREADED -o webserver -lpthread -lrt -fopenmp -pthread
debug:
	${cc} webserver.c thpool.c jwHash.c list.c shift.c heap.c filesys.c -g -DHASHTHREADED -o webserver -lpthread -lrt -fopenmp -pthread
