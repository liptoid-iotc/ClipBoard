CC = gcc
CFLAGS = -Wall -Wextra -O2 $(shell pkg-config --cflags gtk+-3.0 x11)
LDFLAGS = $(shell pkg-config --libs gtk+-3.0 x11) -lpthread

SRC = src/main.c src/ui.c src/storage.c src/hotkey.c
OBJ = $(SRC:.c=.o)
BIN = clipboard-manager

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(OBJ) -o $(BIN) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(BIN)

install: $(BIN)
	install -Dm755 $(BIN) $(DESTDIR)/usr/local/bin/$(BIN)
	install -Dm644 clipboard-manager.desktop $(DESTDIR)/etc/xdg/autostart/clipboard-manager.desktop

.PHONY: all clean install
