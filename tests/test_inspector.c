#include "inspector.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool parse(const char *text, Process *process) {
    FILE *stream=tmpfile(); assert(stream != NULL);
    fputs(text,stream); rewind(stream);
    bool result=read_status(stream,42,process); fclose(stream); return result;
}

int main(void) {
    unsigned long long number=0;
    assert(parse_number("123",&number) && number==123);
    assert(!parse_number("-1",&number)); assert(!parse_number("12x",&number));
    assert(!parse_number("",&number)); assert(!parse_number("184467440737095516160",&number));
    Process process;
    assert(parse("Name:\tdemo worker\nState:\tS (sleeping)\nUid:\t1000\t1000\t1000\t1000\nVmRSS:\t2048 kB\n",&process));
    assert(process.pid==42 && process.uid==1000 && process.rss_kib==2048 && process.has_rss);
    assert(strcmp(process.name,"demo worker")==0);
    assert(parse("Name:\tkthread\nState:\tI (idle)\nUid:\t0\t0\t0\t0\n",&process));
    assert(!process.has_rss);
    assert(!parse("Name:\tincomplete\nState:\tR\n",&process));
    assert(!parse("Name:\tdemo\nState:\tS\nUid:\t-1\n",&process));
    assert(!parse("Name:\tdemo\nState:\tS\nUid:\t0\nVmRSS:\t12 MB\n",&process));
    Process items[3]={{.pid=10,.rss_kib=50,.has_rss=true},{.pid=2,.rss_kib=50,.has_rss=true},{.pid=1,.has_rss=false}};
    qsort(items,3,sizeof *items,compare_rss);
    assert(items[0].pid==2 && items[1].pid==10 && items[2].pid==1);
    qsort(items,3,sizeof *items,compare_pid); assert(items[0].pid==1);
    FILE *stream=tmpfile(); assert(stream != NULL); json_string(stream,"a\"b\\c\n\033"); rewind(stream);
    char output[128]={0}; assert(fgets(output,sizeof output,stream)!=NULL); fclose(stream);
    assert(strcmp(output,"\"a\\\"b\\\\c\\u000a\\u001b\"")==0);
    puts("PASS: numeric validation, status parsing, missing RSS, malformed data, sorting, and JSON escaping");
    return 0;
}
