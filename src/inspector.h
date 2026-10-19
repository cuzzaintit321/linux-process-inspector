#ifndef INSPECTOR_H
#define INSPECTOR_H
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

typedef struct {
    unsigned long pid;
    unsigned long uid;
    unsigned long long rss_kib;
    bool has_rss;
    char name[256];
    char state[80];
} Process;

bool parse_number(const char *text, unsigned long long *number);
bool read_status(FILE *stream, unsigned long pid, Process *result);
int compare_rss(const void *a, const void *b);
int compare_pid(const void *a, const void *b);
int compare_name(const void *a, const void *b);
void json_string(FILE *stream, const char *text);
#endif
