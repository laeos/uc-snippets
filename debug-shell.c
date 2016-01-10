#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "debug.h"

#define MAX_ARGC        10
#define BUFFER_LENGTH	32

/* you can undefine this to ignore history */
#define HISTORY_LENGTH	10

/* how deep the table stack goes */
#define TABLE_STACK_LENGTH 4

enum key_code {
#define DEFKEY(k, c)	k = c,
# include "keycode-list.h"
#undef DEFKEY
};

enum decode_state {
    ST_IDLE,
    ST_ESCAPE,
    ST_BRACKET,
    ST_BRACKET_EXTENDED,
    ST_OH
};

struct history_entry {
    char buf[BUFFER_LENGTH + 1];
};

struct edit_state {
    char buf[BUFFER_LENGTH + 1];
    char cpl[BUFFER_LENGTH + 1];
    int len;
    int pos;
    const char *prompt;
    const struct rline_command *table_stack[TABLE_STACK_LENGTH];
    uint8_t table_index;
#if HISTORY_LENGTH
    struct history_entry history[HISTORY_LENGTH];
    int history_current;
    int history_pos;
#endif
};

/* global edit state */
static struct edit_state edit_state;

/* forward decl. */
static void update_line(struct edit_state *s);
static void help_cmd(int argc, char *argv[]);

static enum key_code decode_key(char ch)
{
    static enum decode_state decode_state;
    static char decode_store;

    switch (decode_state) {
	case ST_IDLE:
	    if (ch == ESC) {
		decode_state = ST_ESCAPE;
		return NO_KEY;
	    } else
		return (enum key_code)ch;

	case ST_ESCAPE:
	    if (ch == '[')
		decode_state = ST_BRACKET;
	    else if (ch == 'O')
		decode_state = ST_OH;
	    else {
		dmsg("[%d] unk esc 0x%x\n", __LINE__, ch);
		decode_state = ST_IDLE;
	    }
	    return NO_KEY;

	case ST_BRACKET:
	    decode_state = ST_IDLE;
	    if ((ch >= '0') && (ch <= '9')) {
		decode_state = ST_BRACKET_EXTENDED;
		decode_store = ch;
		return NO_KEY;
	    } else 
		switch (ch) {
		    case 'A': return KEY_UP;
		    case 'B': return KEY_DOWN;
		    case 'C': return KEY_RIGHT;
		    case 'D': return KEY_LEFT;
		    case 'H': return KEY_HOME;
		    case 'F': return KEY_END;
		    default:
			      dmsg("[%d] unk esc 0x%x\n", __LINE__, ch);
			      return NO_KEY;
		}

	case ST_BRACKET_EXTENDED:
	    decode_state = ST_IDLE;
	    if ((ch == '~') && (decode_store == '3'))
		return KEY_DELETE;
	    dmsg("[%d] unk esc 0x%x 0x%x\n", __LINE__, decode_store, ch);
	    return NO_KEY;

	case ST_OH:
	    decode_state = ST_IDLE;
	    switch (ch) {
		case 'H': return KEY_HOME;
		case 'F': return KEY_END;
		default:
			  dmsg("[%d] unk esc 0x%x\n", __LINE__, ch);
			  return NO_KEY;
	    }
    }
    decode_state = ST_IDLE;
    dmsg("broken decoder state\n");
    return NO_KEY;
}

#if 0
static const char *key_to_string(enum key_code code)
{
    switch (code) {
#define DEFKEY(k, c)	case k : return # k ;
# include "keycode-list.h"
#undef DEFKEY
    }
    return NULL;
}
#endif

static bool is_prompt(const struct rline_command *t)
{
    return ((t->cmd == NULL) && t->help);
}

static const char *get_prompt(const struct rline_command *t)
{
    if (is_prompt(t))
	return t->help;
    if (t == debug_commands)
	return "";
    dmsg("table missing prompt\n");
    return "";
}

static const struct rline_command *get_table(void)
{
    if (edit_state.table_index == 0)
	return debug_commands;
    return edit_state.table_stack[edit_state.table_index - 1];
}

static void pop_tables(uint8_t count)
{
    if (count >= edit_state.table_index)
	edit_state.table_index = 0;
    else
	edit_state.table_index -= count;

    edit_state.prompt = get_prompt(get_table());
}

static void push_table(const struct rline_command *t)
{
    if (edit_state.table_index > TABLE_STACK_LENGTH) {
	dmsg("table stack overflow\n");
	return;
    }
    edit_state.table_stack[edit_state.table_index++] = t;
    edit_state.prompt = get_prompt(t);
}

#if HISTORY_LENGTH
static int __cycle_history(int pos, int dir)
{
    int n = pos + dir;

    if (n < 0)
	n = HISTORY_LENGTH - 1;
    if (n >= HISTORY_LENGTH)
	n = 0;

    return n;
}

static void set_line(struct edit_state *s, char *str)
{
    s->len = s->pos = strlen(str);
    memcpy(s->buf, str, s->len);
    update_line(s);
}
#endif

static void cycle_history(struct edit_state *s, int dir)
{
#if HISTORY_LENGTH
    int n;

    if ((dir > 0) && (s->history_pos == s->history_current)) {
	s->history_pos = -1;
	s->len = s->pos = 0;
	update_line(s);
	return;
    }

    if ((dir < 0) && (s->history_pos == -1)) {
	n = s->history_current;
    } else {
	n = __cycle_history(s->history_pos, dir);
    }

    if (s->history[n].buf[0] == '\0')
	return;

    s->history_pos = n;
    set_line(s, s->history[n].buf);
#endif
}

static void history_cmd(int argc, char *argv[])
{
#if HISTORY_LENGTH
    int c, i, n =  __cycle_history(edit_state.history_pos, 1);

    for (c = 0, i = 0; i < HISTORY_LENGTH; i++) {
	char *b = edit_state.history[n].buf;
	if (b[0])
	    dmsg("%d  %s\r\n", c++, b);
	n =  __cycle_history(n, 1);
    }
#endif
}

static void up_level_cmd(int argc, char *argv[])
{
    pop_tables(1);
}

static const struct rline_command common_commands[] = {
    { "help",    &help_cmd, ": help..." },
    { "?",       &help_cmd, ": help..." },
    { "history", &history_cmd, ": history..." },
    { "exit",    &up_level_cmd, ": up" },
    END_COMMAND_LIST
};

static void dump_help_for(const struct rline_command *t)
{
    if (is_prompt(t))
	++t;
    while (t->cmd) {
	dmsg("%s\t%s\n", t->cmd, t->help ? t->help : "");
	++t;
    }
}

static void help_cmd(int argc, char *argv[])
{
    dump_help_for(get_table());
    dump_help_for(common_commands);
}

static const struct rline_command *next_command(const struct rline_command *t, const struct rline_command **append)
{
    t += 1;

    if (t->cmd == NULL) {
	if (*append) {
	    t = *append;
	    *append = NULL;
	}
    }
    return t;
}

static uint32_t find_command(const struct rline_command *t, const char *txt, const struct rline_command **r)
{
    const struct rline_command *common = common_commands;
    uint32_t count = 0;
    bool printed = false;
    int txtlen = strlen(txt);

    if (is_prompt(t))
	++t;

    *r = NULL;
    while (t->cmd) {
	int cmdlen = strlen(t->cmd);

	if (txtlen <= cmdlen) {
	    if (strncasecmp(t->cmd, txt, txtlen) == 0) {
		++count;
		if (*r == NULL) {
		    *r = t;
		} else {
		    if (!printed) {
			dmsg("  %s\n", (*r)->cmd);
			printed = true;
		    }
		    dmsg("  %s\n", t->cmd);
		}
	    }
	}
	t = next_command(t, &common);
    }
    return count;
}

static int split(char *cmd, char *argv[])
{
    char *ptr;
    int i = 0;

    while ((ptr = strtok(cmd, " ")) &&(i < MAX_ARGC)) {
	cmd = NULL;
	argv[i++] = ptr;
    }
    return i;
}

/* find last index of word, starting from this position */
static int find_word_end(int from)
{
    while (edit_state.buf[from] && !isspace(edit_state.buf[from]))
	++from;
    return from - 1;
}

/* return position of the first character of word (0 == first word) */
static int find_word_start(int word)
{
    int pos = 0;

    while (word && edit_state.buf[pos]) {
	pos = find_word_end(pos) + 1;
	while (edit_state.buf[pos] && isspace(edit_state.buf[pos]))
	    ++pos;
	--word;
    }
    return pos;
}

/* replace word in command, as a result of tab expansion */
static void replace_word(int word, const char *replacement)
{
    int need = strlen(replacement);
    int start = find_word_start(word);
    int end = find_word_end(start);
    int len = end - start + 1;
    int delta = need - len;

    if (delta) {
	if ((edit_state.len - len + need) >= BUFFER_LENGTH)
	    return; /* argh */
	memmove(edit_state.buf + start + need,  edit_state.buf + end + 1, edit_state.len - end);
	edit_state.len += delta;
    }

    if (edit_state.pos <= end)
	edit_state.pos = start + need;
    else
	edit_state.pos += delta;

    memcpy(edit_state.buf + start, replacement, need);
}

static const struct rline_command *table_expand(const struct rline_command *t, int word, const char *txt)
{
    const struct rline_command *c;
    uint32_t count;

    count = find_command(t, txt, &c);
    if (count == 1) {
	replace_word(word, c->cmd);
	return c;
    }
    return NULL;
}

/* should it handle tab completion for subarguments? */
static void complete(struct edit_state *s)
{
    const struct rline_command *table = get_table();
    const struct rline_command *c;
    char *arg[MAX_ARGC];
    int count, word = 0;

    memcpy(s->cpl, s->buf, s->pos);
    s->cpl[s->pos] = '\0';
    count = split(s->cpl, arg);

    while (table && (word < count) && (c = table_expand(table, word, arg[word]))) {
	table = c->children;
	++word;
    }
}

static void exec_cmd(struct edit_state *s, int argc, char *argv[])
{
    const struct rline_command *table = get_table();
    int pushed = 0;

    do {
	const struct rline_command *c;
	uint32_t count = find_command(table, argv[0], &c);

	if (count != 1) {
	    const char *err = count ? "ambiguous" : "not found";
	    dmsg("command '%s': %s\r\n", argv[0], err);
	    pop_tables(pushed);
	    return;
	}

	--argc, ++argv;
	table = c->children;
	if (table) {
	    ++pushed;
	    push_table(table);
	}
	if (c->func) {
	    pop_tables(pushed);
	    (*c->func)(argc, argv);
	    return;
	}
    } while (argc);
}

static void clear_line(void)
{
    dmsg("\r\x1b[0K");
}

static void update_line(struct edit_state *s)
{
    s->buf[s->len] = '\0';
    dmsg("\r%s> %s\x1b[0K\r\x1b[%dC", 
	    s->prompt,
	    s->buf,
	    s->pos + strlen(s->prompt) + 2);
}

static void push_history(struct edit_state *s)
{
#if HISTORY_LENGTH
    s->history_pos = -1;
    if (strcmp(s->buf, s->history[s->history_current].buf) == 0)
	return;
    if (++s->history_current >= HISTORY_LENGTH) 
	s->history_current = 0;
    strcpy(s->history[s->history_current].buf, s->buf);
#endif
}

static void exec_line(struct edit_state *s)
{
    char *arg[MAX_ARGC];
    int count;

    dmsg("\r\n");
    s->buf[s->len] = '\0';
    push_history(s);
    count = split(s->buf, arg);
    exec_cmd(s, count, arg);
}

#if HISTORY_LENGTH
static void exec_bang_index(struct edit_state *s, int idx)
{
    int c, i, n =  __cycle_history(edit_state.history_pos, 1);

    for (c = 0, i = 0; i < HISTORY_LENGTH; i++) {
	char *b = edit_state.history[n].buf;
	if (b[0])
	    if (c++ == idx) {
		set_line(s, b);
		exec_line(s);
		return;
	    }
	n =  __cycle_history(n, 1);
    }
    dmsg("\nnot found\n");
}

static void exec_bang_prefix(struct edit_state *s, const char *prefix)
{
    int i, n =  __cycle_history(edit_state.history_pos, -1);
    int prefix_len = strlen(prefix);

    for (i = 0; i < HISTORY_LENGTH; i++) {
	char *b = edit_state.history[n].buf;
	int bl = strlen(b);

	if ((bl > prefix_len) && (memcmp(b, prefix, prefix_len) == 0)) {
	    set_line(s, b);
	    exec_line(s);
	    return;
	}
	n =  __cycle_history(n, -1);
    }
    dmsg("\nnot found\n");
}
#endif

static bool maybe_exec_bang(struct edit_state *s)
{
    if (s->buf[0] == '!') {
#if HISTORY_LENGTH
	char *end = NULL;
	if (isdigit(s->buf[1])) {
	    int digit = strtoul(s->buf + 1, &end, 0);
	    if (*end)  {
		dmsg("malformed\n");
	    } else {
		exec_bang_index(s, digit);
	    }
	} else {
	    exec_bang_prefix(s, s->buf + 1);
	}
#else
	dmsg("\nno history support\n");
#endif
	return true;
    }

    return false;
}

static void enter(struct edit_state *s)
{
    if (s->len) {
	s->buf[s->len] = '\0';
	if (!maybe_exec_bang(s))
	    exec_line(s);
    } else {
	dmsg("\r\n");
    }
    s->len = s->pos = 0;
    update_line(s);
}

static void insert(struct edit_state *s, char ch)
{
    if (s->len >= BUFFER_LENGTH)
	return;
    if (s->len != s->pos)
	memmove(s->buf+s->pos+1, s->buf+s->pos, s->len - s->pos);
    s->buf[s->pos] = ch;
    ++s->pos;
    ++s->len;
    update_line(s);
}

static void move_home(struct edit_state *s)
{
    s->pos = 0;
    update_line(s);
}

static void move_end(struct edit_state *s)
{
    s->pos = s->len;
    update_line(s);
}

static void move_left(struct edit_state *s)
{
    if (s->pos) {
	s->pos--;
	update_line(s);
    }
}

static void move_right(struct edit_state *s)
{
    if (s->pos < s->len) {
	s->pos++;
	update_line(s);
    }
}

static void move_backspace(struct edit_state *s)
{
    if (s->pos && s->len) {
	memmove(s->buf+s->pos-1, s->buf+s->pos, s->len - s->pos);
	s->pos--;
	s->len--;
	update_line(s);
    }
}

static void delete_char(struct edit_state *s)
{
    if (s->len && (s->pos < s->len)) {
	memmove(s->buf+s->pos, s->buf+s->pos+1, s->len - s->pos - 1);
	s->len--;
	update_line(s);
    }
}

static void delete_prev_word(struct edit_state *s)
{
    int old_pos = s->pos;

    while (s->pos && s->buf[s->pos - 1] == ' ')
	s->pos--;
    while (s->pos && s->buf[s->pos - 1] != ' ')
	s->pos--;

    memmove(s->buf+s->pos, s->buf+old_pos, s->len - old_pos + 1);
    s->len -= (old_pos - s->pos);
    update_line(s);
}

static void swap_chars(struct edit_state *s)
{
    if (s->pos && (s->pos < s->len)) {
	char t = s->buf[s->pos- 1];
	s->buf[s->pos- 1] = s->buf[s->pos];
	s->buf[s->pos] = t;
	s->pos++;
	update_line(s);
    }
}

void process_debug_char(char ch)
{
    enum key_code key = decode_key(ch);

    switch (key) {
	case ENTER:
	    enter(&edit_state);
	    break;

	case CTRL_B:
	case KEY_LEFT:
	    move_left(&edit_state);
	    break;

	case CTRL_F:
	case KEY_RIGHT:
	    move_right(&edit_state);
	    break;

	case KEY_HOME:
	case CTRL_A:
	    move_home(&edit_state);
	    break;

	case KEY_END:
	case CTRL_E:
	    move_end(&edit_state);
	    break;

	case KEY_DELETE:
	    delete_char(&edit_state);
	    break;

	case CTRL_D: /* delete or EOF */
	    if (edit_state.len)
		delete_char(&edit_state);
	    else  {
		pop_tables(1);
		update_line(&edit_state);
	    }
	    break;

	case TAB:
	    clear_line();
	    complete(&edit_state);
	    update_line(&edit_state);
	    break; 

	case CTRL_K:
	    edit_state.len = edit_state.pos;
	    update_line(&edit_state);
	    break;

	case CTRL_L: /* clear screen */
	    dmsg("\x1b[H\x1b[2J");
	    update_line(&edit_state);
	    break;

	case CTRL_T:
	    swap_chars(&edit_state);
	    break;

	case CTRL_C:
	    dmsg("\r\n");
	    /* FALLTHROUGH */
	case CTRL_U:
	    edit_state.len = edit_state.pos = 0;
	    update_line(&edit_state);
	    break;

	case CTRL_W: 
	    delete_prev_word(&edit_state);
	    break;

	case CTRL_H:
	case BACKSPACE:
	    move_backspace(&edit_state);
	    break;

	case CTRL_P: /* prev history */
	case KEY_UP:
	    cycle_history(&edit_state, -1);
	    break;

	case CTRL_N: /* next history */
	case KEY_DOWN:
	    cycle_history(&edit_state, 1);
	    break;

	case NO_KEY:
	    return;

	default:
	    insert(&edit_state, ch);
    }
}

void debug_init(void)
{
#if HISTORY_LENGTH
    edit_state.history_pos = -1;
#endif
    edit_state.prompt = get_prompt(debug_commands);
    update_line(&edit_state);
}
