all: server-side client-side
	# Successfully built!

server-side: server-side.c bank-function-lib.o
	gcc -Wall -g -o server-side server-side.c bank-function-lib.o -pthread

client-side: client-side.c
	gcc -Wall -g -o client-side client-side.c -pthread

bank-function-lib.o: bank-function-lib.c bank-function-lib.h
	gcc -Wall -g -c bank-function-lib.c -pthread

clean:
	rm *.o 