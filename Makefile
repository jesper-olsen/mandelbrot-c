CC = gcc
CFLAGS = -Wall -O3
LDFLAGS = -lm

TARGET = mandelbrot

all: $(TARGET)

$(TARGET): $(TARGET).c
	$(CC) $(CFLAGS) -o $(TARGET) $(TARGET).c $(LDFLAGS)

clean:
	rm -f $(TARGET)
