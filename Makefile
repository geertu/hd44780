
CC =		gcc
CFLAGS =	-Wall -Wstrict-prototypes
OFLAGS =	-O3 -fomit-frame-pointer
KFLAGS =	-DMODULE -D__KERNEL__ -I$(USR_SRC_LINUX)/include
LFLAGS =
USR_SRC_LINUX =	/home/geert/linux/cassiopeia-2.2.17

OBJS =		play.o hd44780.o parlcd.o
KOBJS =		hd44780.ko parlcd.ko lcdcon.ko

TARGETS =	play $(KOBJS)

all:		$(TARGETS)


play:		$(OBJS)
		$(CC) $(LFLAGS) -o play $(OBJS)

lcdcon:		$(KOBJS)

clean:
		$(RM) play $(OBJS) $(KOBJS)

%.o:		%.c
		$(CC) $(CFLAGS) $(OFLAGS) -c $< -o $@

%.ko:		%.c
		$(CC) $(CFLAGS) $(OFLAGS) $(KFLAGS) -c $< -o $@
