#include "inspector.h"
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

bool parse_number(const char *text, unsigned long long *number) {
    char *end;
    if (text == NULL || *text == '\0') return false;
    for (const unsigned char *p = (const unsigned char *)text; *p; p++) {
        if (!isdigit(*p)) return false;
    }
    errno = 0;
    unsigned long long value = strtoull(text, &end, 10);
    if (errno != 0 || *end != '\0') return false;
    *number = value;
    return true;
}

static void copy_text(char *dest, size_t size, const char *source) {
    while (*source == ' ' || *source == '\t') source++;
    size_t len = strcspn(source, "\r\n");
    if (len >= size) len = size - 1;
    memcpy(dest, source, len);
    dest[len] = '\0';
}

bool read_status(FILE *stream, unsigned long pid, Process *result) {
    char line[4096];
    bool name = false, state = false, uid = false;
    Process process = {.pid = pid};
    while (fgets(line, sizeof line, stream) != NULL) {
        if (strchr(line, '\n') == NULL && !feof(stream)) {
            int ch;
            while ((ch = fgetc(stream)) != '\n' && ch != EOF) {}
            return false; /* Refuse oversized fixture/input lines rather than truncating fields. */
        }
        if (strncmp(line, "Name:", 5) == 0) {
            copy_text(process.name, sizeof process.name, line + 5);
            name = process.name[0] != '\0';
        } else if (strncmp(line, "State:", 6) == 0) {
            copy_text(process.state, sizeof process.state, line + 6);
            state = process.state[0] != '\0';
        } else if (strncmp(line, "Uid:", 4) == 0) {
            char digits[32];
            unsigned long long value;
            if (sscanf(line + 4, "%31s", digits) != 1 || !parse_number(digits, &value) || value > ULONG_MAX) return false;
            process.uid = (unsigned long)value;
            uid = true;
        } else if (strncmp(line, "VmRSS:", 6) == 0) {
            char digits[32], unit[8], extra[8];
            unsigned long long value;
            if (sscanf(line + 6, "%31s %7s %7s", digits, unit, extra) != 2 || strcmp(unit, "kB") != 0 || !parse_number(digits, &value)) return false;
            process.rss_kib = value;
            process.has_rss = true;
        }
    }
    if (ferror(stream) || !name || !state || !uid) return false;
    *result = process;
    return true;
}

int compare_pid(const void *a, const void *b) {
    const Process *left = a, *right = b;
    return (left->pid > right->pid) - (left->pid < right->pid);
}

int compare_rss(const void *a, const void *b) {
    const Process *left = a, *right = b;
    if (left->has_rss != right->has_rss) return left->has_rss ? -1 : 1;
    if (left->rss_kib != right->rss_kib) return left->rss_kib > right->rss_kib ? -1 : 1;
    return compare_pid(a, b);
}

int compare_name(const void *a, const void *b) {
    const Process *left = a, *right = b;
    int result = strcmp(left->name, right->name);
    return result != 0 ? result : compare_pid(a, b);
}

void json_string(FILE *stream, const char *text) {
    fputc('"', stream);
    /* ASCII-only JSON: non-ASCII bytes are escaped individually for valid output. */
    for (const unsigned char *p = (const unsigned char *)text; *p; p++) {
        if (*p == '"' || *p == '\\') { fputc('\\', stream); fputc(*p, stream); }
        else if (*p < 0x20 || *p >= 0x7f) fprintf(stream, "\\u%04x", (unsigned int)*p);
        else fputc(*p, stream);
    }
    fputc('"', stream);
}
