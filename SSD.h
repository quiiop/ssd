#include <stdio.h>
#include <stdlib.h>

#ifndef SSD_H
#define SSD_H

#define NonEmpty_id 0
#define Empty_id 1

#define IN_NonEmpty 0
#define IN_Empty 1

#define INVALID -1
#define VALID 1
#define EMPTY 0

#define UNMAPPED 0
#define MAPPED 1

#define NO_SETTING -1

#define TRUE 1
#define FALSE 0

const int nHotLevel = 8;
const int Total_Block = 40; //4096
const int pgs_per_blk = 3;
int Block_Count = 0;

struct ppa
{
    int block_id;
    int pg_id;
};

struct Page
{
    int id;
    int statue;
    int LBA_HotLevel;
};

struct Block
{
    struct Page *page;
    int id;
    int vpc;
    int ipc;
    int position;
};

struct Node
{
    struct Block *blk;
    struct Node *next;
};

struct LinkedList
{
    struct Node head;
    int size;
    int list_id;
};

struct ArrayList
{
    struct LinkedList Empty;
    struct LinkedList NonEmpty;
    int array_id;
};

struct Finder2
{
    struct ArrayList *Array;
};

struct Node* init_node(struct Block *blk);
void add_Node(int HotLevel, struct Node *node);
void init_block(int id);
void init_finder2(void);
void print_list(int ArrayList_Position, int position);
struct Block *Get_Empty_Block(int HotLevel);
#endif