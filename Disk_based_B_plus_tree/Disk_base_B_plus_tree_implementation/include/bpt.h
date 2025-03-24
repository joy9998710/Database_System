#ifndef __BPT_H__
#define __BPT_H__

// Uncomment the line below if you are compiling on Windows.
// #define WINDOWS
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <inttypes.h>
#include <string.h>
/*
    진짜 중요!!!!!!!!!
    leaf node의 key 총 개수는 31개
    leaf node의 pointer 총 개수는 31 + 1(next_offset)
    Internal node의 key 총 개수는 248개
    Internal node의 pointer 총 개수는 248 + 1(next_offset)
*/
#define LEAF_MAX 31
#define INTERNAL_MAX 248

//record는 총 128 bytes
typedef struct record{
    int64_t key;
    char value[120];
}record;

//inter_record는 총 16 bytes
//internal node 에서만 사용
typedef struct inter_record {
    int64_t key;
    off_t p_offset;
}I_R;

//Page는 총 4096 bytes => 31개의 record가 들어가거나 inter_record 248개가 들어갈 수 있음
typedef struct Page{
    //8 bytes
    off_t parent_page_offset;
    //4 bytes
    int is_leaf;
    //4 bytes
    int num_of_keys;
    //104 bytes
    char reserved[104];
    //8 bytes
    off_t next_offset;
    //3968 bytes
    //leaf면 records를 저장하고 있으므로 records를 사용함
    //non-leaf (internal node) 에 대해서는 I_R을 사용
    union{
        I_R b_f[248];
        record records[31];
    };
}page;

//Header Page는 총 4096 bytes로 일반적인 page 크기와 동일
typedef struct Header_Page{
    //Free Page offset [0~7] : 첫 번째 free page를 가리킴 / free page가 없으면 0 값을 가짐 (8 bytes)
    off_t fpo;
    //Root page offset [8~15] : Root page를 가리킴 (8 bytes)
    off_t rpo;
    //Number of Pages [16~23] : 현재 파일에 몇 개의 page가 존재하는 지 나타냄 (8 bytes)
    int64_t num_of_pages;
    //Reserved : unused 4072 bytes
    char reserved[4072];
}H_P;


extern int fd;

extern page * rt;

extern H_P * hp;
// FUNCTION PROTOTYPES.
int open_table(char * pathname);
H_P * load_header(off_t off);
page * load_page(off_t off);

void reset(off_t off);
off_t new_page();
void freetouse(off_t fpo);
int cut(int length);
int parser();
void start_new_file(record rec);

char * db_find(int64_t key);
int db_insert(int64_t key, char * value);
void update_parent(off_t parent_offset, int64_t old_key, int64_t new_key);
void insert_in_leaf(off_t L, int64_t key, char* value);
int insert_in_parent(off_t parent_offset, off_t left_child, int64_t key, off_t right_child);
int db_delete(int64_t key);
// void print_keys(off_t page_offset, int depth);
// void print_bpt();
// void print_parent(off_t child_offset);
int delete_entry(off_t node_offset, int64_t key);
off_t get_neighbor_offset (off_t node_offset, off_t parent_offset, int node_num_of_keys, int* is_left_sibling, int* k_prime);
off_t get_neighbor_for_redistribution(off_t node_offset, off_t parent_offset, int* is_left_sibling, int* k_prime);
// void print_leaf();

#endif /* __BPT_H__*/


