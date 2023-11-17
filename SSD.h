#include <stdio.h>
#include <stdlib.h>

#ifndef SSD_H
#define SSD_H

/*LinkedList的種類*/
#define NonEmpty_id 0
#define Empty_id 1

/*Block.position的位置*/
#define IN_NonEmpty 0
#define IN_Empty 1

/*Page.state的狀態*/
#define INVALID -1
#define VALID 1
#define EMPTY 0

/*用於maplba*/
#define UNMAPPED 0
#define MAPPED 1

/*用於初始化*/
#define NO_SETTING -1

/*Page.type Page的類型*/
#define General 0
#define Sensitive 1

/*Sublock.state*/
#define Victim 0
#define NonVictim 1

/*GC的行為*/
#define Stop_GC 0
#define Finish_GC 1

#define TRUE 1
#define FALSE 0

/*用於Debug or 控制是否GC*/
int Total_Empty_Block = 0;
int Total_vpc = 0;
int Total_ipc = 0;
int Total_epc = 0;

/*SSD的參數*/ 
const int pgs_per_sublk = 2;
const int sublks_per_blk = 3;
const int blks_per_pl = 24;
const int pls_per_lun = 1;
const int luns_per_ch = 1;
const int nchs = 1;
const int nblks = nchs*luns_per_ch*pls_per_lun*blks_per_pl; 

const int pgs_per_blk = sublks_per_blk*pgs_per_sublk;
const int pgs_per_pl = pgs_per_blk*blks_per_pl;
const int pgs_per_lun = pgs_per_pl*pls_per_lun;
const int pgs_per_ch = pgs_per_lun * luns_per_ch;

/*Finder1的參數*/
const int nLayers_Finder1 = sublks_per_blk;

/*Finder2的參數*/
const int Total_Block = nblks;
const int nHotLevel = 8;
const int Blocks_per_linkedList = (Total_Block / nHotLevel);
const int pgs_per_linkedList = Blocks_per_linkedList * pgs_per_blk;
int Current_Block_Count = 0;

/*設定sensitive的range*/
const int boundary_1 = 36;
const int boundary_2 = 72;


struct ppa
{
    int ch;
    int lun;
    int pl;
    int blk;
    int sublk;
    int pg;
    int state;
};

struct addr
{
    int ch;
    int lun;
    int pl;
    int blk;
    int sublk;
    int pg;
};

struct Page
{
    struct addr addr;
    int id;
    int state;
    int LBA_HotLevel;
    int type;
};

struct Sublock
{
    struct Page *pg;
    int id;
    int vpc;
    int ipc;
    int epc;
    int state;
};

struct Block
{
    struct Sublock *sublk;
    int id;
    int block_id; //用於辨識BLock存在哪個Finder2->ArrayList，這是唯一的id
    int vpc;
    int ipc;
    int epc;
    int victim_sublk_count;
    int Nonvictim_sublk_count;
    int position;
};

struct Plane
{
    int id;
    struct Block *blk;
};

struct Lun
{
    int id;
    struct Plane *pl;
};

struct Channel
{
    int id;
    struct Lun *lun;
};

struct SSD
{
    struct Channel *ch;
};

struct Node
{
    struct Block *blk;
    struct Node *next;
};

struct LinkedList_1
{
    struct Node head;
    int id;
};

struct ArrayList_1
{
    struct LinkedList_1 list;
    int id;
};

struct Finder1
{
    struct ArrayList_1 *Array;
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

void Print_SSD_State(void);
#endif