all:	mtdaemon

mtdaemon:	mtdaemon.o
	gcc -o mtdaemon mtdaemon.o

mtdaemon.o:	mtdaemon.c
		gcc -c mtdaemon.c

clean:
	rm -f *.o mtdaemon
