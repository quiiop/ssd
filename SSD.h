#include <stdio.h>
#include <stdlib.h>

#ifndef SSD_H
#define SSD_H

/*LinkedList的種類*/
#define NonEmpty_id 0
#define Empty_id 1

/*Block->position的位置*/
#define IN_NonEmpty 0
#define IN_Empty 1

/*Page.statue的狀態*/
#define INVALID -1
#define VALID 1
#define EMPTY 0

/*LB是新的or舊的*/
#define UNMAPPED 0
#define MAPPED 1

/*用於初始化*/
#define NO_SETTING -1

/*Page->type Page的類型*/
#define General 0
#define Sensitive 1

/*Sublock->statue*/
#define Victim 0
#define NonVictim 1

#define TRUE 1
#define FALSE 0

const int nHotLevel = 8;
const int Total_Block = 4096; //24
const int pgs_per_blk = 3; //3
int Block_Count = 0;

/*用於Debug or 控制是否GC*/
int Total_Empty_Block = 0;
int Total_vpc = 0;
int Total_ipc = 0;
int Total_epc = 0;


struct ppa
{
    //int ch_id;
    //int lun_id;
    //int pl_id;
    int block_id;
    //int sublk_id;
    int pg_id;
};

struct Page
{
    int id;
    int statue;
    int LBA_HotLevel;
    int type;
};

/*
struct Sublock
{
    struct Page *page;
    int vpc;
    int ipc;
    int epc;
    int statue;
}
*/

struct Block
{
    struct Page *page;
    int id;
    int vpc;
    int ipc;
    int epc;
    int position;
};

/*
struct Plane
{
    struct Block *blk;
};

struct Lun
{
    struct Lun
}
*/

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
    int vpc;
    int ipc;
    int epc;
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