# Makefile para AlencarSoftTVBOX-Clang
# Suporta compilacao nativa para testes e cross-compilacao para CSKY e MIPS.

TARGET ?= native
APP_NAME = alencarsoft_tvbox

CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -Iinclude -D_GNU_SOURCE
LDFLAGS ?= 

SRCS = src/main.c \
       src/framebuffer.c \
       src/font.c \
       src/platform.c \
       src/display_vfd.c \
       src/input.c \
       src/iptv_catalog.c \
       src/player.c

OBJS = $(SRCS:.c=.o)

ifeq ($(TARGET), csky)
    CC = csky-linux-gcc
    CFLAGS += -mcpu=ck610 -static
endif

ifeq ($(TARGET), mips)
    CC = mips-linux-gnu-gcc
    CFLAGS += -mips32r2 -static
endif

all: $(APP_NAME)

$(APP_NAME): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)
	@echo "=========================================="
	@echo " Sucesso! Binario gerado: $(APP_NAME)"
	@echo "=========================================="

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(APP_NAME)

.PHONY: all clean
