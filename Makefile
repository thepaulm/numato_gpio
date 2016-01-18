CC=clang

gpio: gpio.o
	${CC} -o gpio gpio.o

gpio.o: gpio.c
	${CC} -c gpio.c

clean:
	rm -f *.o
	rm -f gpio
