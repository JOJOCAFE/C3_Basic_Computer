#include "basic.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#define BASIC_INITIAL_CAPACITY 16

typedef struct {
    char name;
    int value;
    bool used;
} basic_var_t;

typedef struct {
    int return_pc;
    bool active;
} gosub_frame_t;

typedef struct {
    char var;
    int limit;
    int step;
    int line_index;
    bool active;
} for_frame_t;

static basic_var_t g_vars[26];

static void write_str(const basic_io_t *io, const char *text)
{
    if (io && io->write) {
        io->write(io->ctx, text);
    }
}

static void write_ln(const basic_io_t *io, const char *text)
{
    write_str(io, text);
    write_str(io, "\r\n");
}

static const char *skip_ws(const char *s)
{
    while (s && *s && isspace((unsigned char)*s)) {
        s++;
    }
    return s;
}

static int find_var(char name)
{
    if (name >= 'A' && name <= 'Z') {
        return name - 'A';
    }
    if (name >= 'a' && name <= 'z') {
        return toupper((unsigned char)name) - 'A';
    }
    return -1;
}

static int get_var(char name)
{
    int idx = find_var(name);
    if (idx < 0) {
        return 0;
    }
    return g_vars[idx].value;
}

static void set_var(char name, int value)
{
    int idx = find_var(name);
    if (idx < 0) {
        return;
    }
    g_vars[idx].name = (char)('A' + idx);
    g_vars[idx].value = value;
    g_vars[idx].used = true;
}

static void reset_vars(void)
{
    memset(g_vars, 0, sizeof(g_vars));
}

static int parse_expr(const char **s, const basic_io_t *io, bool *ok);

static bool parse_identifier(const char **s, char *name, size_t name_len)
{
    size_t idx = 0;
    const char *p = *s;
    while (isalpha((unsigned char)*p)) {
        if (idx + 1 < name_len) {
            name[idx++] = (char)toupper((unsigned char)*p);
        }
        p++;
    }
    if (idx == 0) {
        return false;
    }
    name[idx] = '\0';
    *s = p;
    return true;
}

static int parse_factor(const char **s, const basic_io_t *io, bool *ok)
{
    (void)io;
    *s = skip_ws(*s);
    if (**s == '+') {
        (*s)++;
        return parse_factor(s, io, ok);
    }
    if (**s == '-') {
        (*s)++;
        return -parse_factor(s, io, ok);
    }
    if (**s == '(') {
        (*s)++;
        int value = parse_expr(s, io, ok);
        *s = skip_ws(*s);
        if (**s != ')') {
            *ok = false;
            return 0;
        }
        (*s)++;
        return value;
    }

    if (isalpha((unsigned char)**s)) {
        char ident[16];
        if (!parse_identifier(s, ident, sizeof(ident))) {
            *ok = false;
            return 0;
        }

        *s = skip_ws(*s);
        if (**s == '(') {
            (*s)++;
            bool fn_ok = true;
            int a = 0;
            if (strcmp(ident, "ABS") == 0) {
                a = parse_expr(s, io, &fn_ok);
                *s = skip_ws(*s);
                if (**s != ')') fn_ok = false;
                if (fn_ok) (*s)++;
                return fn_ok ? abs(a) : (*ok = false, 0);
            }
            if (strcmp(ident, "INT") == 0) {
                a = parse_expr(s, io, &fn_ok);
                *s = skip_ws(*s);
                if (**s != ')') fn_ok = false;
                if (fn_ok) (*s)++;
                return fn_ok ? a : (*ok = false, 0);
            }
            if (strcmp(ident, "RND") == 0) {
                if (**s != ')') {
                    a = parse_expr(s, io, &fn_ok);
                    *s = skip_ws(*s);
                    if (**s != ')') fn_ok = false;
                }
                if (fn_ok) (*s)++;
                if (!fn_ok) {
                    *ok = false;
                    return 0;
                }
                if (a <= 0) {
                    return rand() & 0x7fff;
                }
                return rand() % a;
            }

            *ok = false;
            return 0;
        }

        if (strlen(ident) == 1) {
            return get_var(ident[0]);
        }
        *ok = false;
        return 0;
    }

    char *end = NULL;
    long value = strtol(*s, &end, 10);
    if (end == *s) {
        *ok = false;
        return 0;
    }
    *s = end;
    return (int)value;
}

static int parse_term(const char **s, const basic_io_t *io, bool *ok)
{
    int value = parse_factor(s, io, ok);
    while (*ok) {
        *s = skip_ws(*s);
        char op = **s;
        if (op != '*' && op != '/' && op != '%') {
            break;
        }
        (*s)++;
        int rhs = parse_factor(s, io, ok);
        if (!*ok) {
            return 0;
        }
        if (op == '*') {
            value *= rhs;
        } else if (op == '/') {
            value = rhs == 0 ? 0 : value / rhs;
        } else {
            value = rhs == 0 ? 0 : value % rhs;
        }
    }
    return value;
}

static int parse_expr(const char **s, const basic_io_t *io, bool *ok)
{
    int value = parse_term(s, io, ok);
    while (*ok) {
        *s = skip_ws(*s);
        char op = **s;
        if (op != '+' && op != '-') {
            break;
        }
        (*s)++;
        int rhs = parse_term(s, io, ok);
        if (!*ok) {
            return 0;
        }
        if (op == '+') {
            value += rhs;
        } else {
            value -= rhs;
        }
    }
    return value;
}

static bool parse_int(const char **s, int *value)
{
    char *end = NULL;
    long parsed = strtol(*s, &end, 10);
    if (end == *s) {
        return false;
    }
    *value = (int)parsed;
    *s = end;
    return true;
}

static int find_line(const basic_program_t *program, int number)
{
    for (size_t i = 0; i < program->count; ++i) {
        if (program->lines[i].number == number) {
            return (int)i;
        }
    }
    return -1;
}

void basic_init(basic_program_t *program)
{
    if (!program) {
        return;
    }
    memset(program, 0, sizeof(*program));
    reset_vars();
    srand((unsigned int)time(NULL));
}

void basic_clear(basic_program_t *program)
{
    if (!program) {
        return;
    }
    for (size_t i = 0; i < program->count; ++i) {
        free(program->lines[i].text);
    }
    free(program->lines);
    memset(program, 0, sizeof(*program));
}

static char *basic_strdup(const char *text)
{
    size_t len = strlen(text);
    char *copy = malloc(len + 1);
    if (!copy) {
        return NULL;
    }
    memcpy(copy, text, len + 1);
    return copy;
}

static bool basic_reserve_lines(basic_program_t *program, size_t needed)
{
    if (needed <= program->capacity) {
        return true;
    }

    size_t next = program->capacity ? program->capacity : BASIC_INITIAL_CAPACITY;
    while (next < needed) {
        if (next > (SIZE_MAX / 2)) {
            return false;
        }
        next *= 2;
    }

    basic_line_t *lines = realloc(program->lines, next * sizeof(*lines));
    if (!lines) {
        return false;
    }
    program->lines = lines;
    program->capacity = next;
    return true;
}

bool basic_store_line(basic_program_t *program, const char *line)
{
    if (!program || !line) {
        return false;
    }

    const char *p = skip_ws(line);
    int number = 0;
    if (!parse_int(&p, &number)) {
        return false;
    }

    if (*p != '\0' && !isspace((unsigned char)*p)) {
        return false;
    }

    p = skip_ws(p);
    if (*p == '\0') {
        for (size_t i = 0; i < program->count; ++i) {
            if (program->lines[i].number == number) {
                free(program->lines[i].text);
                for (size_t j = i; j + 1 < program->count; ++j) {
                    program->lines[j] = program->lines[j + 1];
                }
                program->count--;
                return true;
            }
        }
        return true;
    }

    char *stored_text = basic_strdup(p);
    if (!stored_text) {
        return false;
    }

    size_t idx = 0;
    while (idx < program->count && program->lines[idx].number < number) {
        idx++;
    }

    if (idx < program->count && program->lines[idx].number == number) {
        free(program->lines[idx].text);
        program->lines[idx].text = stored_text;
        return true;
    }

    if (!basic_reserve_lines(program, program->count + 1)) {
        free(stored_text);
        return false;
    }

    for (size_t j = program->count; j > idx; --j) {
        program->lines[j] = program->lines[j - 1];
    }

    program->lines[idx].number = number;
    program->lines[idx].text = stored_text;
    program->count++;
    return true;
}

void basic_list(const basic_program_t *program, basic_write_fn write, void *ctx)
{
    if (!program || !write) {
        return;
    }

    char line[160];
    for (size_t i = 0; i < program->count; ++i) {
        snprintf(line, sizeof(line), "%d ", program->lines[i].number);
        write(ctx, line);
        write(ctx, program->lines[i].text ? program->lines[i].text : "");
        write(ctx, "\r\n");
    }
}

esp_err_t basic_load_buffer(const char *text, size_t len, basic_program_t *program)
{
    if (!text || !program) {
        return ESP_ERR_INVALID_ARG;
    }

    basic_clear(program);

    size_t offset = 0;
    while (offset < len) {
        size_t start = offset;
        while (offset < len && text[offset] != '\r' && text[offset] != '\n') {
            offset++;
        }

        size_t line_len = offset - start;
        char *line = malloc(line_len + 1);
        if (!line) {
            basic_clear(program);
            return ESP_ERR_NO_MEM;
        }
        memcpy(line, text + start, line_len);
        line[line_len] = '\0';

        if (*skip_ws(line) != '\0' && !basic_store_line(program, line)) {
            free(line);
            basic_clear(program);
            return ESP_FAIL;
        }
        free(line);

        if (offset < len && text[offset] == '\r') {
            offset++;
        }
        if (offset < len && text[offset] == '\n') {
            offset++;
        }
    }

    return ESP_OK;
}

esp_err_t basic_load_file(const char *path, basic_program_t *program)
{
    if (!path || !program) {
        return ESP_ERR_INVALID_ARG;
    }

    FILE *f = fopen(path, "r");
    if (!f) {
        return ESP_FAIL;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return ESP_FAIL;
    }
    long size = ftell(f);
    if (size < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return ESP_FAIL;
    }

    char *text = malloc((size_t)size + 1);
    if (!text) {
        fclose(f);
        return ESP_ERR_NO_MEM;
    }
    size_t read_len = fread(text, 1, (size_t)size, f);
    if (read_len != (size_t)size && ferror(f)) {
        free(text);
        fclose(f);
        return ESP_FAIL;
    }
    text[read_len] = '\0';

    fclose(f);
    esp_err_t err = basic_load_buffer(text, read_len, program);
    free(text);
    return err;
}

esp_err_t basic_save_file(const char *path, const basic_program_t *program)
{
    if (!path || !program) {
        return ESP_ERR_INVALID_ARG;
    }

    FILE *f = fopen(path, "w");
    if (!f) {
        return ESP_FAIL;
    }

    for (size_t i = 0; i < program->count; ++i) {
        fprintf(f, "%d %s\n", program->lines[i].number, program->lines[i].text);
    }

    fclose(f);
    return ESP_OK;
}

static bool parse_keyword(const char **s, const char *keyword)
{
    size_t len = strlen(keyword);
    if (strncasecmp(*s, keyword, len) == 0) {
        const char next = (*s)[len];
        if (next == '\0' || isspace((unsigned char)next) || next == '(' || next == '=' ) {
            *s += len;
            return true;
        }
    }
    return false;
}

static int eval_condition(const char **s, const basic_io_t *io, bool *ok)
{
    int left = parse_expr(s, io, ok);
    if (!*ok) {
        return 0;
    }

    *s = skip_ws(*s);
    const char *op = *s;
    int result = 0;
    if (strncmp(op, "<=", 2) == 0 || strncmp(op, ">=", 2) == 0 || strncmp(op, "<>", 2) == 0) {
        *s += 2;
    } else if (*op == '<' || *op == '>' || *op == '=') {
        (*s)++;
    } else {
        *ok = false;
        return 0;
    }

    int right = parse_expr(s, io, ok);
    if (!*ok) {
        return 0;
    }

    if (strncmp(op, "=", 1) == 0) {
        result = (left == right);
    } else if (strncmp(op, "<>", 2) == 0) {
        result = (left != right);
    } else if (strncmp(op, "<=", 2) == 0) {
        result = (left <= right);
    } else if (strncmp(op, ">=", 2) == 0) {
        result = (left >= right);
    } else if (*op == '<') {
        result = (left < right);
    } else if (*op == '>') {
        result = (left > right);
    }

    return result;
}

static bool read_user_line(const basic_io_t *io, char *buf, size_t len)
{
    if (!io || !io->read_line) {
        return false;
    }
    return io->read_line(io->ctx, buf, len) > 0;
}

static void print_prompt_var(const basic_io_t *io, const char *name)
{
    char msg[32];
    snprintf(msg, sizeof(msg), "? %s: ", name);
    write_str(io, msg);
}

static bool parse_quoted_string(const char **s, char *out, size_t out_len)
{
    if (!s || !*s || !out || out_len == 0) {
        return false;
    }

    const char *cursor = skip_ws(*s);
    if (*cursor != '"') {
        return false;
    }
    cursor++;

    size_t idx = 0;
    while (*cursor && *cursor != '"') {
        if (idx + 1 >= out_len) {
            return false;
        }
        out[idx++] = *cursor++;
    }

    if (*cursor != '"') {
        return false;
    }
    cursor++;
    cursor = skip_ws(cursor);
    if (*cursor != '\0') {
        return false;
    }

    out[idx] = '\0';
    *s = cursor;
    return true;
}

static esp_err_t execute_service_statement(const basic_io_t *io, const char *cursor, bool hardware)
{
    if (!io || !io->service_exec) {
        write_ln(io, hardware ? "HARDWARE service unavailable" : "SHELL service unavailable");
        return ESP_FAIL;
    }

    char command[160];
    if (!parse_quoted_string(&cursor, command, sizeof(command))) {
        return ESP_FAIL;
    }

    return io->service_exec(io->ctx, command, hardware);
}

static esp_err_t execute_line(const basic_program_t *program, int *pc, const basic_io_t *io,
                              gosub_frame_t *gosub_stack, int *gosub_sp, for_frame_t *for_stack, int *for_sp)
{
    const char *line = program->lines[*pc].text;
    line = skip_ws(line);
    if (*line == '\0') {
        return ESP_OK;
    }

    const char *cursor = line;
    if (parse_keyword(&cursor, "REM")) {
        return ESP_OK;
    }

    if (parse_keyword(&cursor, "PRINT")) {
        cursor = skip_ws(cursor);
        if (*cursor == '\0') {
            write_ln(io, "");
            return ESP_OK;
        }
        if (*cursor == '"') {
            cursor++;
            const char *end = strchr(cursor, '"');
            if (!end) {
                return ESP_FAIL;
            }
            char tmp[128];
            size_t n = (size_t)(end - cursor);
            if (n >= sizeof(tmp)) {
                n = sizeof(tmp) - 1;
            }
            memcpy(tmp, cursor, n);
            tmp[n] = '\0';
            write_ln(io, tmp);
            return ESP_OK;
        }

        bool ok = true;
        int value = parse_expr(&cursor, io, &ok);
        if (!ok) {
            return ESP_FAIL;
        }
        char tmp[32];
        snprintf(tmp, sizeof(tmp), "%d", value);
        write_ln(io, tmp);
        return ESP_OK;
    }

    if (parse_keyword(&cursor, "SHELL")) {
        return execute_service_statement(io, cursor, false);
    }

    if (parse_keyword(&cursor, "HARDWARE")) {
        return execute_service_statement(io, cursor, true);
    }

    if (parse_keyword(&cursor, "LET")) {
        cursor = skip_ws(cursor);
    }

    const char *assignment_cursor = cursor;
    if (isalpha((unsigned char)*cursor)) {
        char var = *cursor++;
        cursor = skip_ws(cursor);
        if (*cursor == '=') {
            cursor++;
            bool ok = true;
            int value = parse_expr(&cursor, io, &ok);
            if (!ok) {
                return ESP_FAIL;
            }
            set_var(var, value);
            return ESP_OK;
        }
        cursor = assignment_cursor;
    }

    if (parse_keyword(&cursor, "INPUT")) {
        cursor = skip_ws(cursor);
        if (!isalpha((unsigned char)*cursor)) {
            return ESP_FAIL;
        }
        char var = *cursor;
        char prompt[16];
        snprintf(prompt, sizeof(prompt), "%c", (char)toupper((unsigned char)var));
        print_prompt_var(io, prompt);

        char input[64];
        if (!read_user_line(io, input, sizeof(input))) {
            return ESP_FAIL;
        }
        int value = atoi(input);
        set_var(var, value);
        return ESP_OK;
    }

    if (parse_keyword(&cursor, "IF")) {
        bool ok = true;
        int cond = eval_condition(&cursor, io, &ok);
        if (!ok) {
            return ESP_FAIL;
        }
        cursor = skip_ws(cursor);
        if (!parse_keyword(&cursor, "THEN")) {
            return ESP_FAIL;
        }
        cursor = skip_ws(cursor);
        int then_target = 0;
        if (!parse_int(&cursor, &then_target)) {
            return ESP_FAIL;
        }

        int else_target = -1;
        const char *after_then = skip_ws(cursor);
        if (parse_keyword(&after_then, "ELSE")) {
            after_then = skip_ws(after_then);
            if (!parse_int(&after_then, &else_target)) {
                return ESP_FAIL;
            }
        }

        if (cond) {
            int next = find_line(program, then_target);
            if (next < 0) {
                return ESP_FAIL;
            }
            *pc = next - 1;
        } else if (else_target >= 0) {
            int next = find_line(program, else_target);
            if (next < 0) {
                return ESP_FAIL;
            }
            *pc = next - 1;
        }
        return ESP_OK;
    }

    if (parse_keyword(&cursor, "GOTO")) {
        cursor = skip_ws(cursor);
        int target = 0;
        if (!parse_int(&cursor, &target)) {
            return ESP_FAIL;
        }
        int next = find_line(program, target);
        if (next < 0) {
            return ESP_FAIL;
        }
        *pc = next - 1;
        return ESP_OK;
    }

    if (parse_keyword(&cursor, "GOSUB")) {
        cursor = skip_ws(cursor);
        int target = 0;
        if (!parse_int(&cursor, &target) || *gosub_sp >= BASIC_STACK_DEPTH) {
            return ESP_FAIL;
        }
        int next = find_line(program, target);
        if (next < 0) {
            return ESP_FAIL;
        }
        gosub_stack[*gosub_sp].return_pc = *pc;
        gosub_stack[*gosub_sp].active = true;
        (*gosub_sp)++;
        *pc = next - 1;
        return ESP_OK;
    }

    if (parse_keyword(&cursor, "RETURN")) {
        if (*gosub_sp <= 0) {
            return ESP_FAIL;
        }
        (*gosub_sp)--;
        *pc = gosub_stack[*gosub_sp].return_pc;
        return ESP_OK;
    }

    if (parse_keyword(&cursor, "FOR")) {
        cursor = skip_ws(cursor);
        if (!isalpha((unsigned char)*cursor)) {
            return ESP_FAIL;
        }
        char var = *cursor++;
        cursor = skip_ws(cursor);
        if (*cursor != '=') {
            return ESP_FAIL;
        }
        cursor++;

        bool ok = true;
        int start = parse_expr(&cursor, io, &ok);
        if (!ok) {
            return ESP_FAIL;
        }
        cursor = skip_ws(cursor);
        if (!parse_keyword(&cursor, "TO")) {
            return ESP_FAIL;
        }
        int limit = parse_expr(&cursor, io, &ok);
        if (!ok) {
            return ESP_FAIL;
        }
        int step = 1;
        cursor = skip_ws(cursor);
        if (parse_keyword(&cursor, "STEP")) {
            step = parse_expr(&cursor, io, &ok);
            if (!ok || step == 0) {
                return ESP_FAIL;
            }
        }
        if (*for_sp >= BASIC_FOR_DEPTH) {
            return ESP_FAIL;
        }
        set_var(var, start);
        for_stack[*for_sp].var = (char)toupper((unsigned char)var);
        for_stack[*for_sp].limit = limit;
        for_stack[*for_sp].step = step;
        for_stack[*for_sp].line_index = *pc;
        for_stack[*for_sp].active = true;
        (*for_sp)++;
        return ESP_OK;
    }

    if (parse_keyword(&cursor, "NEXT")) {
        cursor = skip_ws(cursor);
        char var = 0;
        if (isalpha((unsigned char)*cursor)) {
            var = (char)toupper((unsigned char)*cursor);
        }
        if (*for_sp <= 0) {
            return ESP_FAIL;
        }
        for (int i = *for_sp - 1; i >= 0; --i) {
            if (!for_stack[i].active) {
                continue;
            }
            if (var != 0 && for_stack[i].var != var) {
                continue;
            }
            int current = get_var(for_stack[i].var);
            current += for_stack[i].step;
            set_var(for_stack[i].var, current);
            if ((for_stack[i].step > 0 && current <= for_stack[i].limit) ||
                (for_stack[i].step < 0 && current >= for_stack[i].limit)) {
                *pc = for_stack[i].line_index;
            } else {
                for_stack[i].active = false;
                while (*for_sp > 0 && !for_stack[*for_sp - 1].active) {
                    (*for_sp)--;
                }
            }
            return ESP_OK;
        }
        return ESP_FAIL;
    }

    if (parse_keyword(&cursor, "END")) {
        return ESP_ERR_NOT_FOUND;
    }

    if (parse_keyword(&cursor, "STOP")) {
        return ESP_ERR_NOT_FOUND;
    }

    return ESP_FAIL;
}

static void basic_write_vars(const basic_io_t *io)
{
    bool any = false;
    for (size_t i = 0; i < sizeof(g_vars) / sizeof(g_vars[0]); ++i) {
        if (!g_vars[i].used) {
            continue;
        }
        char msg[32];
        snprintf(msg, sizeof(msg), "%c=%d ", g_vars[i].name, g_vars[i].value);
        write_str(io, msg);
        any = true;
    }
    write_ln(io, any ? "" : "No variables");
}

static void basic_write_debug_line(const basic_program_t *program, int pc, const basic_io_t *io)
{
    if (!program || pc < 0 || pc >= (int)program->count) {
        return;
    }
    char msg[48];
    snprintf(msg, sizeof(msg), "DBG %d ", program->lines[pc].number);
    write_str(io, msg);
    write_ln(io, program->lines[pc].text ? program->lines[pc].text : "");
}

esp_err_t basic_run(const basic_program_t *program, const basic_io_t *io)
{
    if (!program || !io) {
        return ESP_ERR_INVALID_ARG;
    }

    if (program->count == 0) {
        write_ln(io, "No BASIC program loaded.");
        return ESP_OK;
    }

    gosub_frame_t gosub_stack[BASIC_STACK_DEPTH] = {0};
    for_frame_t for_stack[BASIC_FOR_DEPTH] = {0};
    int gosub_sp = 0;
    int for_sp = 0;

    for (int pc = 0; pc < (int)program->count; ++pc) {
        esp_err_t err = execute_line(program, &pc, io, gosub_stack, &gosub_sp, for_stack, &for_sp);
        if (err == ESP_ERR_NOT_FOUND) {
            return ESP_OK;
        }
        if (err != ESP_OK) {
            write_ln(io, "BASIC ERROR");
            return err;
        }
    }

    return ESP_OK;
}

esp_err_t basic_debug(const basic_program_t *program, const basic_io_t *io)
{
    if (!program || !io) {
        return ESP_ERR_INVALID_ARG;
    }

    if (program->count == 0) {
        write_ln(io, "No BASIC program loaded.");
        return ESP_OK;
    }

    gosub_frame_t gosub_stack[BASIC_STACK_DEPTH] = {0};
    for_frame_t for_stack[BASIC_FOR_DEPTH] = {0};
    int gosub_sp = 0;
    int for_sp = 0;
    bool continuous = false;

    write_ln(io, "DEBUG");
    write_ln(io, "Enter/s step, c continue, p vars, l line, q quit");

    for (int pc = 0; pc < (int)program->count; ++pc) {
        if (!continuous) {
            while (true) {
                write_str(io, "debug> ");
                char command[16];
                if (!read_user_line(io, command, sizeof(command))) {
                    return ESP_FAIL;
                }
                const char *choice = skip_ws(command);
                if (*choice == '\0' || strcasecmp(choice, "s") == 0) {
                    break;
                }
                if (strcasecmp(choice, "c") == 0) {
                    continuous = true;
                    break;
                }
                if (strcasecmp(choice, "q") == 0) {
                    write_ln(io, "DEBUG QUIT");
                    return ESP_OK;
                }
                if (strcasecmp(choice, "p") == 0) {
                    basic_write_vars(io);
                    continue;
                }
                if (strcasecmp(choice, "l") == 0) {
                    basic_write_debug_line(program, pc, io);
                    continue;
                }
                write_ln(io, "Use Enter/s/c/p/l/q");
            }
        }

        basic_write_debug_line(program, pc, io);
        esp_err_t err = execute_line(program, &pc, io, gosub_stack, &gosub_sp, for_stack, &for_sp);
        if (err == ESP_ERR_NOT_FOUND) {
            write_ln(io, "DEBUG END");
            return ESP_OK;
        }
        if (err != ESP_OK) {
            write_ln(io, "BASIC ERROR");
            return err;
        }
    }

    write_ln(io, "DEBUG END");
    return ESP_OK;
}

esp_err_t basic_execute_immediate(basic_program_t *program, const char *line, const basic_io_t *io)
{
    if (!program || !line || !io) {
        return ESP_ERR_INVALID_ARG;
    }

    basic_program_t *scratch = calloc(1, sizeof(*scratch));
    if (!scratch) {
        return ESP_ERR_NO_MEM;
    }

    basic_init(scratch);
    if (!basic_reserve_lines(scratch, 1)) {
        free(scratch);
        return ESP_ERR_NO_MEM;
    }
    scratch->lines[0].number = 0;
    scratch->lines[0].text = basic_strdup(line);
    if (!scratch->lines[0].text) {
        free(scratch->lines);
        free(scratch);
        return ESP_ERR_NO_MEM;
    }
    scratch->count = 1;

    esp_err_t err = basic_run(scratch, io);
    basic_clear(scratch);
    free(scratch);
    return err;
}
