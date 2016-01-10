
#include <stdio.h>
#include <signal.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/termios.h>

#include "debug.h"

/* Original and Our terminal settings */
static struct termios oldb, newb;

volatile bool ctrl_c_hit;

static void sigint(int unused)
{
    ctrl_c_hit = 1;
}


static void setup_raw_input(void)
{
    tcgetattr(STDIN_FILENO, &oldb);

    newb = oldb;

    cfmakeraw(&newb);

    tcsetattr(STDIN_FILENO, TCSADRAIN, &newb);
}

static void restore_terminal(void)
{
    tcsetattr(STDIN_FILENO, TCSADRAIN, &oldb);
    printf("\n");

}

/* hooks for debug.c / debug-shell.c */
void debug_write(const char *str, size_t len)
{
    write(STDOUT_FILENO, str, len);
}

static void quit_cmd(int argc, char *argv[])
{
    ctrl_c_hit = 1;
}

static void hello_cmd(int argc, char *argv[])
{
    dmsg("margorp\n");
}

static const struct rline_command linux_commands[] = {
    DEFPROMPT("linux"),
    { "hegorp",    &hello_cmd },
    END_COMMAND_LIST
};


/* table of commands for debug-shell.c (needs c linkage) */
const struct rline_command debug_commands[] = {
    { "linux",   NULL,         ": linux...", .children = linux_commands },
    { "quit",   &quit_cmd,     ": exit!" },
    END_COMMAND_LIST
};

int main(int argc, char *argv[])
{
    char ch;

    signal(SIGINT, sigint);
    ctrl_c_hit = 0;

    setup_raw_input();
    debug_init();

    while (!ctrl_c_hit) {
	if (read(STDIN_FILENO, &ch, 1) == 1)
	    process_debug_char(ch);
    }
    restore_terminal();

    return 0;

}
