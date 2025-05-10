CC = gcc
CFLAGS = -Wall -Wextra -pthread
SRC = main.c lwrb.c
OUT = lora_sim

all: $(OUT)

$(OUT): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(OUT)

clean:
	rm -f $(OUT)
