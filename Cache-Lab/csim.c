#include "cachelab.h"
#include <stdio.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>


int verbose = 0;
int s_bit, E_line, b_block;
char filename[30];

typedef struct {
    bool vaild;
    uint32_t tag;
    uint32_t timestamp;
} CacheLine;

typedef struct {
    int s;
    int E;
    int b;
    CacheLine **line;
} Cache;

typedef Cache *Cache_t;

int *set_capacity;

enum Cache_behavior {HIT, MISS, EVICTION};
typedef enum Cache_behavior CacheBehavior_t;

int status[3] = {0}; // Cache status (hit, miss, eviction)
const char *behavior_str[3] = {"HIT", "MISS", "EVICTION"};

uint32_t tick = 0;

Cache_t Init_cache(void)
{
    int set_num = 1 << s_bit;
    Cache_t LRUCache = malloc(sizeof(LRUCache));
    if (LRUCache == NULL) {
        printf("LRUCache Malloc failed\n");
        exit(-1);
    }

    LRUCache->s = s_bit;
    LRUCache->E = E_line;
    LRUCache->b = b_block;
    LRUCache->line = (CacheLine **) malloc(set_num * sizeof(CacheLine *));
    if (LRUCache->line == NULL) {
        printf("LRUCache->line Malloc failed\n");
        exit(-1);
    }
    set_capacity = (int *) malloc(set_num * sizeof(int));
    if (set_capacity == NULL) {
        printf("set_capacity Malloc failed\n");
        exit(-1);
    }
    for (int i = 0; i < set_num; i++) {
        set_capacity[i] = 0;
        LRUCache->line[i] = (CacheLine *) malloc(E_line * sizeof(CacheLine));
        for (int j = 0; j < E_line; j++) {
            LRUCache->line[i][j].vaild = 0;
            LRUCache->line[i][j].tag = 0;
            LRUCache->line[i][j].timestamp = 0;
        }
    }
    return LRUCache;
}

void free_cache(Cache_t Cache)
{
    int set_num = 1 << s_bit;
    for (int i = 0; i < set_num; i++) {
        free(Cache->line[i]);
    }
    free(Cache);
}

void update_cache(CacheLine *Cache_line, CacheBehavior_t behavior, int line_idx, uint32_t tag)
{
    Cache_line[line_idx].vaild = 1;
    Cache_line[line_idx].tag = tag;
    Cache_line[line_idx].timestamp = ++tick;
    
    status[behavior]++;
    if (verbose) {
        printf("%s ", behavior_str[behavior]);
    }
}

int LRU_replace(CacheLine *Cache_line)
{
    int min_T = Cache_line[0].timestamp;
    int replace_idx = 0;
    for (int i = 1; i < E_line; i++) {
        if (Cache_line[i].timestamp < min_T) {
            replace_idx = i;
            min_T = Cache_line[i].timestamp;
        }
    }
    return replace_idx;
}

void cache_movement(Cache_t LRUCache, int set_index, uint32_t tag)
{
    int is_hit = 0;
    int line_idx = 0;
    int line_num = LRUCache->E;
    for (int i = 0; i < line_num; i++) {
        if (LRUCache->line[set_index][i].vaild == 1 && LRUCache->line[set_index][i].tag == tag) {
            line_idx = i;
            is_hit = 1;
            update_cache(LRUCache->line[set_index], HIT, line_idx, tag);
            break;
        }
    }
    if (!is_hit) {
        // cache is not full
        if (set_capacity[set_index] != line_num) {
            set_capacity[set_index]++;
            for (int j = 0; j < line_num; j++) {
                if (LRUCache->line[set_index][j].vaild == 0) { // is empty
                    line_idx = j;
                    update_cache(LRUCache->line[set_index], MISS, line_idx, tag);
                    break;
                }
            }
        }
        else { // is full
            line_idx = LRU_replace(LRUCache->line[set_index]);
            update_cache(LRUCache->line[set_index], MISS, line_idx, tag);
            update_cache(LRUCache->line[set_index], EVICTION, line_idx, tag);
        }
    }

}

void get_trace(Cache_t LRUCache)
{
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        perror("Error: Can't open file ");
        exit(1);
    }

    char operation;
    unsigned long addr;
    int size;

    int set_index;
    uint32_t tag;

    while (fscanf(fp, " %c %lx,%d", &operation, &addr, &size) == 3) {
        set_index = (addr >> LRUCache->b) & (uint32_t)((1 << LRUCache->s) - 1);
        tag = addr >> (LRUCache->b + LRUCache->s);

        if (verbose) {
            printf(" %c %lx,%d: ", operation, addr, size);
        }
        switch (operation)
        {
            case 'L':
                cache_movement(LRUCache, set_index, tag);
                if (verbose) printf("\n");
                break;
            case 'S':
                cache_movement(LRUCache, set_index, tag);
                if (verbose) printf("\n");
                break;
            case 'M':
                cache_movement(LRUCache, set_index, tag);
                cache_movement(LRUCache, set_index, tag);
                if (verbose) printf("\n");
                break;
            default:
                break;
        }
    }
    fclose(fp);
}

void print_help(void)
{
    printf("Usage: ./csim-ref [-hv] -s <s> -E <E> -b <b> -t <tracefile>\n");
}

void read_arg(int argc, char* argv[])
{
    // Usage: ./csim-ref [-hv] -s <s> -E <E> -b <b> -t <tracefile>
    if (argc < 5){
        return;
    }


    char opt;
    while ((opt = getopt(argc, argv, "hvs:E:b:t:")) != -1) {
        switch (opt)
        {
            case 'h':
                print_help();
                break;
            case 'v':
                verbose = 1;
                break;
            case 's':
                s_bit = atoi(optarg);
                break;
            case 'E':
                E_line = atoi(optarg);
                break;
            case 'b':
                b_block = atoi(optarg);
                break;
            case 't':
                // size_t len = strlen(optarg);
                // strncpy(filename, optarg, len);
                // filename[len - 1] = '\0';
                strcpy(filename, optarg);
                break;
            default:
                printf("Unknown option %c !!!\n", opt);
                print_help();
                exit(-1);
        }
    }
}

int main(int argc, char* argv[])
{
    read_arg(argc, argv);

    Cache_t LRUCache = Init_cache();
    get_trace(LRUCache);
    free_cache(LRUCache);

    printSummary(status[HIT], status[MISS], status[EVICTION]);
    return 0;
}
