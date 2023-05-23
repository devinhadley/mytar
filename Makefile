CC = gcc
CFLAGS = -Wall -pedantic -ansi -Werror -g
TARGET = mytar
OBJS = mytar.o header.o writer.o reader.o

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

mytar.o: mytar.c
	$(CC) $(CFLAGS) -c -o $@ $<

header.o: header.c
	$(CC) $(CFLAGS) -c -o $@ $<

writer.o: writer.c
	$(CC) $(CFLAGS) -c -o $@ $<

reader.o: reader.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o $(TARGET)

format:
	find . -type f -iname '*.c' -o -iname '*.h' | xargs -I{} clang-format -i -style="{BasedOnStyle: LLVM, ColumnLimit: 80}" {}

