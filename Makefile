# Makefile — file compression, encryption, and management tool
# ---------------------------------------------------
# Targets:
#   make          — build normally
#   make debug    — build with DEBUG preprocessing enabled
#   make clean    — remove compiled output

CC      = gcc
CFLAGS  = -Wall -Wextra -pedantic -std=c90
TARGET  = encryption_test_prog
SRCS    = main.c encryption.c compress.c helpers.c
OBJS    = $(SRCS:.c=.o)

# ---- Default target (normal build) ----------------------------------------
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# ---- Debug build — defines DEBUG so debug_print_* calls are compiled in ----
debug: CFLAGS += -DDEBUG -g
debug: $(TARGET)

# ---- Dependency hints (keep rebuilds correct) ------------------------------
main.o:    main.c    encryption.h compress.h helpers.h
encryption.o:    encryption.c    encryption.h
compress.o:		compress.c		compress.h
helpers.o: helpers.c helpers.h encryption.h

# ---- Clean -----------------------------------------------------------------
clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all debug clean
