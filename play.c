
/*
 *  Copyright 2000-2001 by Geert Uytterhoeven <geert@linux-m68k.org>
 *
 *  This programs is subject to the terms and conditions of the GNU General
 *  Public License
 */

#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/io.h>
#include <sys/times.h>
#include <time.h>
#include <unistd.h>

typedef unsigned char u8;

#include "hd44780.h"
#include "parlcd.h"


static const char *ProgramName = NULL;
static int Verbose = 0;
static int Dump = 0;

static long clk_tck;


    /*
     *  Function Prototypes
     */

static void Die(const char *fmt, ...)
    __attribute__ ((noreturn))
    __attribute__ ((format (printf, 1, 2)));
static void Usage(void)
    __attribute__ ((noreturn));
int main(int argc, char *argv[]);


/* ------------------------------------------------------------------------- */

    /*
     *  ANSI Escape Sequences
     */

#define BLACK		"\e[30m"
#define RED		"\e[31m"
#define GREEN		"\e[32m"
#define YELLOW		"\e[33m"
#define BLUE		"\e[34m"
#define PURPLE		"\e[35m"
#define CYAN		"\e[36m"
#define WHITE		"\e[37m"

#define NORMAL		"\e[0m"
#define BOLD		"\e[1m"
#define UNDERLINE	"\e[4m"
#define REVERSE		"\e[7m"


/* ------------------------------------------------------------------------- */

    /*
     *  I/O Port Access
     */

#ifdef __powerpc__
unsigned long isa_io_base;
static int io_fd = -1;

#define REAL_ISA_IO_BASE	0xf8000000	/* for CHRP LongTrail */
#define REAL_ISA_IO_SIZE	0x01000000

static void enable_isa_io(void)
{
    if ((io_fd = open("/dev/mem", O_RDWR)) == -1) {
	perror("open /dev/mem");
	exit(1);
    }
    isa_io_base = (unsigned long)mmap(0, REAL_ISA_IO_SIZE,
				      PROT_READ | PROT_WRITE, MAP_SHARED,
				      io_fd, REAL_ISA_IO_BASE);
    if (isa_io_base == (unsigned long)-1)
	Die("mmap 0x%08x: %s", REAL_ISA_IO_BASE, strerror(errno));
}

static void disable_isa_io(void)
{
    if (isa_io_base != (unsigned long)-1) {
	munmap((caddr_t)isa_io_base, REAL_ISA_IO_SIZE);
	isa_io_base = (unsigned long)-1;
    }
    if (io_fd != -1) {
	close(io_fd);
	io_fd = -1;
    }
}

#else /* !__powerpc__ */

static void enable_isa_io(void)
{
    /* Get access to all of I/O space. */
    if (iopl(3) < 0)
	Die("This program must be run as root.\n");
}

#define disable_isa_io()	do { } while (0)

#endif /* !__powerpc__ */


/* ------------------------------------------------------------------------- */


    /*
     *  Print an Error Message and Exit
     */

static void Die(const char *fmt, ...)
    __attribute__ ((format (printf, 1, 2)));

static void Die(const char *fmt, ...)
{
    va_list ap;

    fflush(stdout);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    exit(1);
}


    /*
     *  Print the Usage Template and Exit
     */

static void Usage(void)
{
    Die("Usage: %s [options] [file ...]\n\n"
	"Valid options are:\n"
	"    --help               Display this usage information\n"
	"    -d, --dump           Dump stdin to the LCD\n"
	"    -v, --verbose        Enable verbose mode\n"
	"\n",
	ProgramName);
}


/* ------------------------------------------------------------------------- */


    /*
     *  Command Processing
     */

#define MAX_CMD_LINE	256

static char CommandLine[MAX_CMD_LINE];

static int Stop = 0;


struct Command {
    const char *name;
    void (*func)(int argc, const char **);
};

#define arraysize(x)	(sizeof(x)/sizeof(*(x)))


static char *ReadCommandLine(void)
{
    u_int last;

    fgets(CommandLine, MAX_CMD_LINE, stdin);
    last = strlen(CommandLine)-1;
    if (CommandLine[last] == '\n')
	CommandLine[last] = '\0';
    return CommandLine;
}


    /*
     *  Split a Line into Arguments
     */

static void CreateArgs(char *cmdline, int *argcp, const char **argvp[])
{
    int argc = 0, i;
    char *p, *q;
    const char **argv = NULL;

    *argcp = 0;
    p = cmdline;
    for (argc = 0;; argc++) {
	while (*p == ' ' || *p == '\t')
	    p++;
	if (!*p)
	    break;
	if (*p == '"') {
	    for (p++; *p && *p != '"'; p++)
		if (*p == '\\') {
		    p++;
		    if (!*p)
			break;
		}
	    if (*p)
		p++;
	} else
	    while (*p && *p != ' ' && *p != '\t')
		p++;
    }
    if (argc) {
	if (!(argv = malloc((argc+1)*sizeof(const char*))))
	    return;
	p = cmdline;
	for (i = 0; i < argc; i++) {
	    while (*p == ' ' || *p == '\t')
		p++;
	    if (!*p)
		break;
	    if (*p == '"') {
		q = p++;
		argv[i] = q;
		while (*p && *p != '"') {
		    if (*p == '\\') {
			p++;
			if (!*p)
			    break;
		    }
		    *q++ = *p++;
		}
	    } else {
		q = p;
		argv[i] = q;
		while (*p && *p != ' ' && *p != '\t') {
		    if (*p == '\\') {
			p++;
			if (!*p)
			    break;
		    }
		    *q++ = *p++;
		}
	    }
	    *q = '\0';
	    p++;
	}
    }
    *argcp = argc;
    *argvp = argv;
}


static void DeleteArgs(int argc, const char *argv[])
{
    if (argc)
	free(argv);
}


    /*
     *  Partial strcasecmp() (s1 may be an abbreviation for s2)
     */

static int PartStrCaseCmp(const char *s1, const char *s2)
{
    int n1 = strlen(s1);
    int n2 = strlen(s2);

    return n1 <= n2 ? strncasecmp(s1, s2, n1) : 1;
}


static int ExecCommand(const struct Command commands[], u_int numcommands,
		       int argc, const char *argv[])
{
    u_int i;

    if (argc)
	for (i = 0; i < numcommands; i++)
	    if (!PartStrCaseCmp(argv[0], commands[i].name)) {
		commands[i].func(argc-1, argv+1);
		return 1;
	    }
    return 0;
}


static void Do_Help(int argc, const char **argv)
{
    fputs("\nDemo\n\n"
	 "\n  General commands\n"
	 "    Help, ?                Display this help\n"
	 "    Quit, eXit             Terminate program\n"
	 "\n  LCD commands\n"
	 "    Init [8|4]             Initialize for 20x4 LCD, 8 or 4 bit bus\n"
	 "    HELLo                  Show the welcome message\n"
	 "    Raw <string>           Print a raw string to the LCD\n"
	 "    Print <string>         Print a string to the LCD\n"
	 "    Clear, Clr             Clear display\n"
	 "    HOme                   Return home\n"
	 "    Goto <val>             Move cursor to address <val>\n"
	 "    Font                   Show all characters of the font\n"
	 "    Move Left|Right [cnt]  Move the cursor left or right\n"
	 "    Shift Left|Right [cnt] Shift the display left or right\n"
	 "    CMd <val>              Special LCD command <val>\n"
	 "    Backlight [on|off]     Control backlight\n"
	 "\n  Parallel port commands\n"
	 "    Data                   Dump the data register\n"
	 "    Data <val>             Write <val> to the data register\n"
	 "    STatus                 Dump the status register\n"
	 "    STatus <val>           Write <val> to the status register\n"
	 "    COntrol                Dump the control register\n"
	 "    COntrol, CTrl <val>    Write <val> to the control register\n"
	 "\n", stderr);
}

static void Do_Quit(int argc, const char **argv)
{
    Stop = 1;
}

static void Do_Init(int argc, const char *argv[])
{
    int width = 8;

    if (argc == 1) {
	width = strtoul(argv[0], NULL, 0);
    }
    if (width != 4 && width != 8)
	return;
    parlcd_init(width);
}

static void Do_Hello(int argc, const char *argv[])
{
    struct tms tms;
    time_t t;

    t = times(&tms);
    lcd_puts("Welcome to your\n"
	     "Hitachi HD44780U\n"
	     "driving a 20x4 LCD!\n");
    t = times(&tms)-t;
    lcd_printf("[%f seconds]", (double)t/clk_tck);
}

static void Do_Print(int argc, const char *argv[])
{
    while (argc--) {
	lcd_puts(argv[0]);
	argv++;
	if (argc)
	    lcd_putc(' ');
    }
    lcd_putc('\n');
}

static void Do_Raw(int argc, const char *argv[])
{
    int i;

    while (argc--) {
	for (i = 0; argv[0][i]; i++)
	    lcd_write(argv[0][i]);
	argv++;
	if (argc)
	    lcd_write(' ');
    }
}

static void Do_Clear(int argc, const char *argv[])
{
    lcd_clr();
}

static void Do_Home(int argc, const char *argv[])
{
    lcd_home();
}

static void Do_Goto(int argc, const char *argv[])
{
    if (argc == 1)
	lcd_ddram(strtoul(argv[0], NULL, 0));
}

static void Do_Font(int argc, const char *argv[])
{
    int i, j, start = 0, end = 255;

    if (argc >= 1) {
	start = strtoul(argv[0], NULL, 0);
	if (argc >= 2)
	    end = strtoul(argv[1], NULL, 0);
    }
    j = 0;
    for (i = start; i <= end; i++, j++) {
	if (j == 20) {
	    sleep(1);
	    j = 0;
	}
	lcd_putc(i);
    }
}

static void Do_Move(int argc, const char *argv[])
{
    int dir = LCD_SHIFT_RIGHT, cnt = 1;

    if (argc >= 1) {
	if (!PartStrCaseCmp(argv[0], "left"))
	    dir = LCD_SHIFT_LEFT;
	else if (PartStrCaseCmp(argv[0], "right"))
	    return;
	if (argc >= 2)
	    cnt = strtoul(argv[1], NULL, 0);
    }
    while (cnt--)
	lcd_shift(LCD_MOVE_CURSOR, dir);
}

static void Do_Shift(int argc, const char *argv[])
{
    int dir = LCD_SHIFT_RIGHT, cnt = 1;

    if (argc >= 1) {
	if (!PartStrCaseCmp(argv[0], "left"))
	    dir = LCD_SHIFT_LEFT;
	else if (PartStrCaseCmp(argv[0], "right"))
	    return;
	if (argc >= 2)
	    cnt = strtoul(argv[1], NULL, 0);
    }
    while (cnt--)
	lcd_shift(LCD_SHIFT_DISP, dir);
}

static void Do_Cmd(int argc, const char *argv[])
{
    if (argc == 1)
	lcd_write_cmd(strtoul(argv[0], NULL, 0));
}

static void Do_Backlight(int argc, const char *argv[])
{
    int light = 1;

    if (argc == 1) {
	if (!strcmp(argv[0], "on"))
	    light = 1;
	else if (!strcmp(argv[0], "off"))
	    light = 0;
	else
	    light = strtoul(argv[0], NULL, 0);
    }
    lcd_backlight(light);
}

static const char *Binary8(u8 val)
{
    static char binary[9];
    int i;

    for (i = 0; i < 8; i++, val >>= 1)
	binary[7-i] = (val & 1) ? '1' : '0';
    binary[8] = '\0';
    return binary;
}

static void PrintLogicColor(int val, const char *name)
{
    printf("%s%s" BLACK, val ? RED : GREEN, name);
}

static void Do_Data(int argc, const char *argv[])
{
    u8 val;

    if (argc == 0) {
	val = parport_read_data();
	printf("Data = 0x%02x = %sb\n", val, Binary8(val));
    } else {
	val = strtoul(argv[0], NULL, 0);
	parport_write_data(val);
    }
}

static void Do_Status(int argc, const char *argv[])
{
    u8 val;

    if (argc == 0) {
	val = parport_read_status();
	printf("Status = 0x%02x = %sb", val, Binary8(val));
	PrintLogicColor(val & PARPORT_STATUS_BUSY, " *BUSY");
	PrintLogicColor(val & PARPORT_STATUS_ACK, " *ACK");
	PrintLogicColor(val & PARPORT_STATUS_PAPEROUT, " PAPEROUT");
	PrintLogicColor(val & PARPORT_STATUS_SELECT, " *SELECT");
	PrintLogicColor(val & PARPORT_STATUS_ERROR, " *ERROR");
	putchar('\n');
    } else {
	val = strtoul(argv[0], NULL, 0);
	parport_write_status(val);
    }
}

static void Do_Control(int argc, const char *argv[])
{
    u8 val;

    if (argc == 0) {
	val = parport_read_control();
	printf("Control = 0x%02x = %sb", val, Binary8(val));
	PrintLogicColor(!(val & PARPORT_CONTROL_SELECT), " *SELECT");
	PrintLogicColor(val & PARPORT_CONTROL_INIT, " *INIT");
	PrintLogicColor(!(val & PARPORT_CONTROL_AUTOFD), " *AUTOFEED");
	PrintLogicColor(!(val & PARPORT_CONTROL_STROBE), " *STROBE");
	putchar('\n');
    } else {
	val = strtoul(argv[0], NULL, 0);
	parport_write_control(val);
    }
}

static const struct Command Commands[] = {
    /* General Commands */
    { "help", Do_Help },
    { "?", Do_Help },
    { "quit", Do_Quit },
    { "exit", Do_Quit },
    { "x", Do_Quit },
    /* LCD Commands */
    { "init", Do_Init },
    { "hello", Do_Hello },
    { "raw", Do_Raw },
    { "print", Do_Print },
    { "clear", Do_Clear },
    { "clr", Do_Clear },
    { "home", Do_Home },
    { "goto", Do_Goto },
    { "font", Do_Font },
    { "move", Do_Move },
    { "shift", Do_Shift },
    { "cmd", Do_Cmd },
    { "backlight", Do_Backlight },
    /* Parallel Port Commands */
    { "data", Do_Data },
    { "status", Do_Status },
    { "control", Do_Control },
    { "ctrl", Do_Control },
};


    /*
     *  Parse the Command Line
     */

static void ParseCommandLine(char *line)
{
    int argc;
    const char **argv;

    CreateArgs(line, &argc, &argv);
    if (argc && !ExecCommand(Commands, arraysize(Commands), argc, argv))
	fputs("Unknown command\n", stderr);
    DeleteArgs(argc, argv);
}


static void Interpreter(void)
{
    char *line;

    fputs("Please enter your commands (`help' for help)\n\n", stderr);
    do {
	fprintf(stderr, "play> ");
	line = ReadCommandLine();
	ParseCommandLine(line);
    } while (!Stop);
}


    /*
     *  Dump Stdin to the LCD
     */

static void Do_Dump(void)
{
    int c;

    while ((c = getchar()) != EOF)
	lcd_putc(c);
}


/* ------------------------------------------------------------------------- */


    /*
     *  Main Routine
     */

int main(int argc, char **argv)
{
    ProgramName = argv[0];

    while (--argc > 0) {
	argv++;
	if (!strcmp(argv[0], "--help"))
	    Usage();
	else if (!strcmp(argv[0], "-v") || !strcmp(argv[0], "--verbose"))
	    Verbose = 1;
	else if (!strcmp(argv[0], "-d") || !strcmp(argv[0], "--dump"))
	    Dump = 1;
	else
	    Usage();
    }

    clk_tck = sysconf(_SC_CLK_TCK);

    enable_isa_io();

    parlcd_init(8);
    if (Dump)
	Do_Dump();
    else
	Interpreter();
    parlcd_cleanup();

    disable_isa_io();

    return 0;
}

