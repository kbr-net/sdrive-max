CC = gcc
CFLAGS = -Wall
OBJ = atxinfo.o atx.o
TARGET = atxinfo

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm $(OBJ) $(TARGET)
