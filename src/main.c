#define _POSIX_C_SOURCE 200809L
#include "inspector.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_PROCESSES 100000UL

static void usage(FILE *stream) {
    fputs("Linux Process Inspector\nUsage: process-inspector [--sort rss|pid|name] [--limit 25] [--name TEXT] [--uid UID] [--json] [--proc-root /proc]\nRead-only /proc snapshot; no command lines, signals, or environment variables.\n", stream);
}

static void terminal_text(const char *source, char *dest, size_t size) {
    size_t i;
    for (i = 0; i + 1 < size && source[i]; i++) {
        unsigned char ch = (unsigned char)source[i];
        dest[i] = ch >= 32 && ch < 127 ? (char)ch : '?';
    }
    dest[i] = '\0';
}

int main(int argc, char **argv) {
    const char *root = "/proc", *sort = "rss", *name_filter = NULL;
    unsigned long long limit = 25, uid_filter = 0;
    bool filter_uid = false, json = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) { usage(stdout); return 0; }
        if (strcmp(argv[i], "--json") == 0) { json = true; continue; }
        const char *option = argv[i];
        if (strcmp(option,"--sort") && strcmp(option,"--limit") && strcmp(option,"--name") && strcmp(option,"--uid") && strcmp(option,"--proc-root")) {
            fprintf(stderr, "Unknown option: %s\n", option); return 2;
        }
        if (++i >= argc) { fprintf(stderr,"Missing value for %s\n",option); return 2; }
        if (strcmp(option,"--sort") == 0) sort = argv[i];
        else if (strcmp(option,"--name") == 0) name_filter = argv[i];
        else if (strcmp(option,"--proc-root") == 0) root = argv[i];
        else if (strcmp(option,"--limit") == 0) {
            if (!parse_number(argv[i],&limit) || limit < 1 || limit > 10000) { fputs("Limit must be 1–10000\n",stderr); return 2; }
        } else {
            if (!parse_number(argv[i],&uid_filter) || uid_filter > ULONG_MAX) { fputs("Invalid UID\n",stderr); return 2; }
            filter_uid = true;
        }
    }
    int (*comparator)(const void *,const void *) = NULL;
    if (strcmp(sort,"rss") == 0) comparator = compare_rss;
    else if (strcmp(sort,"pid") == 0) comparator = compare_pid;
    else if (strcmp(sort,"name") == 0) comparator = compare_name;
    else { fputs("Sort must be rss, pid, or name\n",stderr); return 2; }

    int root_fd = open(root, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (root_fd < 0) { perror("Cannot open proc root"); return 2; }
    DIR *directory = fdopendir(root_fd);
    if (directory == NULL) { perror("Cannot enumerate proc root"); close(root_fd); return 2; }
    size_t count = 0, capacity = 128, skipped = 0, inspected = 0;
    bool capped = false;
    Process *items = calloc(capacity,sizeof *items);
    if (items == NULL) { closedir(directory); fputs("Allocation failed\n",stderr); return 2; }
    struct dirent *entry;
    bool enumeration_failed = false;
    while (true) {
        errno = 0;
        entry = readdir(directory);
        if (entry == NULL) { enumeration_failed = errno != 0; break; }
        unsigned long long pid;
        if (!parse_number(entry->d_name,&pid) || pid == 0 || pid > ULONG_MAX) continue;
        if (inspected >= MAX_PROCESSES) { capped = true; break; }
        inspected++;
        int process_fd = openat(root_fd, entry->d_name, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        if (process_fd < 0) { skipped++; continue; }
        int status_fd = openat(process_fd,"status",O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK);
        close(process_fd);
        if (status_fd < 0) { skipped++; continue; }
        struct stat metadata;
        if (fstat(status_fd,&metadata) != 0 || !S_ISREG(metadata.st_mode)) { close(status_fd); skipped++; continue; }
        FILE *stream = fdopen(status_fd,"r");
        if (stream == NULL) { close(status_fd); skipped++; continue; }
        Process process;
        bool valid = read_status(stream,(unsigned long)pid,&process);
        fclose(stream);
        if (!valid) { skipped++; continue; }
        if (name_filter != NULL && strstr(process.name,name_filter) == NULL) continue;
        if (filter_uid && process.uid != uid_filter) continue;
        if (count == capacity) {
            size_t next = capacity * 2;
            Process *grown = realloc(items,next*sizeof *items);
            if (grown == NULL) { free(items); closedir(directory); fputs("Allocation failed\n",stderr); return 2; }
            items = grown; capacity = next;
        }
        items[count++] = process;
    }
    closedir(directory);
    if (enumeration_failed) { free(items); fputs("Proc directory enumeration failed\n",stderr); return 2; }
    qsort(items,count,sizeof *items,comparator);
    size_t shown = count < limit ? count : (size_t)limit;
    if (json) {
        printf("{\"schema_version\":1,\"matched\":%zu,\"shown\":%zu,\"skipped\":%zu,\"scan_capped\":%s,\"name_encoding\":\"proc bytes escaped as Latin-1 code points\",\"processes\":[",count,shown,skipped,capped?"true":"false");
        for (size_t i=0;i<shown;i++) {
            Process *item=&items[i];
            if(i) fputc(',',stdout);
            printf("{\"pid\":%lu,\"uid\":%lu,\"name\":",item->pid,item->uid); json_string(stdout,item->name);
            fputs(",\"state\":",stdout); json_string(stdout,item->state);
            if(item->has_rss) printf(",\"rss_kib\":%llu}",item->rss_kib);
            else fputs(",\"rss_kib\":null}",stdout);
        }
        fputs("]}\n",stdout);
    } else {
        puts("LINUX PROCESS INSPECTOR");
        printf("%7s %7s %12s %-26s %s\n","PID","UID","RSS KiB","NAME","STATE");
        for(size_t i=0;i<shown;i++) {
            char name[256],state[80],rss[32]; Process *item=&items[i];
            terminal_text(item->name,name,sizeof name); terminal_text(item->state,state,sizeof state);
            if(item->has_rss) snprintf(rss,sizeof rss,"%llu",item->rss_kib); else strcpy(rss,"-");
            printf("%7lu %7lu %12s %-26.26s %s\n",item->pid,item->uid,rss,name,state);
        }
        printf("\n%zu shown / %zu matched | %zu skipped | scan capped: %s\n",shown,count,skipped,capped?"yes":"no");
        puts("One read-only sample. RSS is resident memory, not CPU usage or unique physical memory.");
    }
    free(items);
    if (ferror(stdout)) return 2;
    return skipped || capped ? 1 : 0;
}
