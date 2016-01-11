#pragma once

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>

#define ARRAY_SIZE(x)   (sizeof(x) / sizeof(x[0]))

#ifdef __cplusplus
extern "C" {
#endif

void dmsg(const char *format, ...);
void dhexdump(size_t saddr, const void *vbuf, size_t count);

/* debug-shell.c */
#define END_COMMAND_LIST 	{ 0, 0, 0, 0 }
#define DEFPROMPT(prompt)	{ .help = prompt }
#define DEFCMD(name, fn, helpstr) { .cmd  = name, .func = fn, .help = helpstr }

struct rline_command {
    const char *cmd;
    void (*func)(int argc, char *argv[]);
    const char *help;
    const struct rline_command *children;
};

extern const struct rline_command debug_commands[];

void debug_init(void);
void process_debug_char(char ch);

/* debug-uart.c */
void debug_setup(void);
void debug_poll(void);
void debug_write(const char *str, size_t len);

/* debug-usbh.c */
void dev_cmd(int argc, char *argv[]);
void cfg_cmd(int argc, char *argv[]);
void enum_cmd(int argc, char *argv[]);
void class_cmd(int argc, char *argv[]);

#ifdef __cplusplus
}
#endif
