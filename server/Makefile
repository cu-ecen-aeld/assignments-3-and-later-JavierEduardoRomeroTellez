CC ?= $(CROSS_COMPILE)gcc

RM := rm -f

BIN := aesdsocket

INCLUDES ?= -I./inc
CFLAGS ?= -g -Wall -Werror

LDFLAGS ?= -lpthread -lrt

SRC := $(wildcard ./src/*.c)
OBJ := $(patsubst %.c, %.o, $(SRC))

ALL: $(BIN)

$(BIN): $(OBJ)
	@$(CC) -o $(BIN) $(OBJ) $(CFLAGS) $(LDFLAGS)

clean:
	@$(RM) $(OBJ) $(BIN)

%o: %c
	$(CC) -c $< -o $@ $(CFLAGS)
