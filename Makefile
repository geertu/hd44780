
CC =		gcc
CFLAGS =	-Wall -Wstrict-prototypes -Werror
OFLAGS =	-O3 -fomit-frame-pointer
KFLAGS =	-DMODULE -D__KERNEL__ -I$(KERNEL_INC)
LFLAGS =
KERNEL_INC =	/home/geert/linux/linuxppc_2_4/include

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
