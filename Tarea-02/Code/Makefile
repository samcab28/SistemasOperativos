CC = clang
CFLAGS = -Wall -Wextra -pthread

OBJ = main.o product.o station.o scheduler.o metrics.o

all: simulador

simulador: $(OBJ)
	$(CC) $(CFLAGS) -o simulador $(OBJ)

clean:
	rm -f *.o simulador
