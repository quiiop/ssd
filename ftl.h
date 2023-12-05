#ifndef __FEMU_FTL_H
#define __FEMU_FTL_H

#include "../nvme.h"

#define INVALID_PPA     (~(0ULL))
#define INVALID_LPN     (~(0ULL))
#define UNMAPPED_PPA    (~(0ULL))

enum {
    NAND_READ =  0,
    NAND_WRITE = 1,
    NAND_ERASE = 2,

    NAND_READ_LATENCY = 40000,
    NAND_PROG_LATENCY = 200000,
    NAND_ERASE_LATENCY = 2000000,
};

enum {
    USER_IO = 0,
    GC_IO = 1,
};

enum {
    SEC_FREE = 0,
    SEC_INVALID = 1,
    SEC_VALID = 2,

    PG_FREE = 0,
    PG_INVALID = 1,
    PG_VALID = 2
};

enum {
    FEMU_ENABLE_GC_DELAY = 1,
    FEMU_DISABLE_GC_DELAY = 2,

    FEMU_ENABLE_DELAY_EMU = 3,
    FEMU_DISABLE_DELAY_EMU = 4,

    FEMU_RESET_ACCT = 5,
    FEMU_ENABLE_LOG = 6,
    FEMU_DISABLE_LOG = 7,
};


/*----------------------------自定義的----------------------------*/
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

#define FTL_TRUE 1
#define FTL_FALSE 0

/*OP的參數*/
#define OP_ID -1
#define OP_SUCCESSFUL 1
/*--------------------------------------------------------------*/

#define SUBLK_BITS  (8)
#define BLK_BITS    (8)
#define PG_BITS     (16)
#define SEC_BITS    (8)
#define PL_BITS     (8)
#define LUN_BITS    (8)
#define CH_BITS     (7)

/* describe a physical page addr */
struct ppa {
    union {
        struct {
            uint64_t sublk : SUBLK_BITS;
            uint64_t blk : BLK_BITS;
            uint64_t pg  : PG_BITS;
            uint64_t sec : SEC_BITS;
            uint64_t pl  : PL_BITS;
            uint64_t lun : LUN_BITS;
            uint64_t ch  : CH_BITS;
            uint64_t rsv : 1;
        } g;

        uint64_t ppa;
    };
    int state;
};

typedef int nand_sec_status_t;

struct addr
{
    uint64_t ch;
    uint64_t lun;
    uint64_t pl;
    uint64_t blk;
    uint64_t sublk;
    uint64_t pg;
};

struct nand_page {
    struct addr addr; 
    nand_sec_status_t *sec;
    int nsecs;
    int id;
    int state;
    int LBA_HotLevel;
    int type;
};

struct nand_sublock
{
    struct nand_page *pg;
    int npgs;
    int id;
    int vpc;
    int ipc;
    int epc;
    int state;
    int have_invalid_sensitive_page;
};

struct nand_block {
    struct nand_sublock *sublk;
    int nsublks;
    int id;
    int block_id; 
    int vpc; 
    int ipc;
    int epc;
    int victim_sublk_count;
    int Nonvictim_sublk_count;
    int position;
};

struct nand_plane {
    struct nand_block *blk;
    int id;
    int nblks;
};

struct nand_lun {
    struct nand_plane *pl;
    int id;
    int npls;
    uint64_t next_lun_avail_time;
    bool busy;
    uint64_t gc_endtime;
};

struct ssd_channel {
    struct nand_lun *lun;
    int id;
    int nluns;
    uint64_t next_ch_avail_time;
    bool busy;
    uint64_t gc_endtime;
};

struct Over_Provisioning
{
    struct nand_block *blk;
    int size;
};

struct ssdparams {
    int secsz;        /* sector size in bytes */
    int secs_per_pg;  /* # of sectors per page */
    int pgs_per_sublk;
    int sublks_per_blk;  /* # of NAND pages per block */
    int pgs_per_blk;
    int blks_per_pl;  /* # of blocks per plane */
    int pls_per_lun;  /* # of planes per LUN (Die) */
    int luns_per_ch;  /* # of LUNs per channel */
    int nchs;         /* # of channels in the SSD */

    int pg_rd_lat;    /* NAND page read latency in nanoseconds */
    int pg_wr_lat;    /* NAND page program latency in nanoseconds */
    int blk_er_lat;   /* NAND block erase latency in nanoseconds */
    int ch_xfer_lat;  /* channel transfer latency for one page in nanoseconds
                       * this defines the channel bandwith
                       */

    double gc_thres_pcent;
    int gc_thres_lines;
    double gc_thres_pcent_high;
    int gc_thres_lines_high;
    bool enable_gc_delay;

    /* below are all calculated values */
    int secs_per_blk; /* # of sectors per block */
    int secs_per_pl;  /* # of sectors per plane */
    int secs_per_lun; /* # of sectors per LUN */
    int secs_per_ch;  /* # of sectors per channel */
    int tt_secs;      /* # of sectors in the SSD */

    int pgs_per_pl;   /* # of pages per plane */
    int pgs_per_lun;  /* # of pages per LUN (Die) */
    int pgs_per_ch;   /* # of pages per channel */
    int tt_pgs;       /* total # of pages in the SSD */

    int blks_per_lun; /* # of blocks per LUN */
    int blks_per_ch;  /* # of blocks per channel */
    int tt_blks;      /* total # of blocks in the SSD */

    int secs_per_line;
    int pgs_per_line;
    int blks_per_line;
    int tt_lines;

    int pls_per_ch;   /* # of planes per channel */
    int tt_pls;       /* total # of planes in the SSD */

    int tt_luns;      /* total # of LUNs in the SSD */
};

/*----------------struct line 我們不會用到----------------*/
typedef struct line {
    int id;  /* line id, the same as corresponding block id */
    int ipc; /* invalid page count in this line */
    int vpc; /* valid page count in this line */
    QTAILQ_ENTRY(line) entry; /* in either {free,victim,full} list */
    /* position in the priority queue for victim lines */
    size_t                  pos;
} line;

/* wp: record next write addr */
struct write_pointer {
    struct line *curline;
    int ch;
    int lun;
    int pg;
    int blk;
    int pl;
};

struct line_mgmt {
    struct line *lines;
    /* free line list, we only need to maintain a list of blk numbers */
    QTAILQ_HEAD(free_line_list, line) free_line_list;
    pqueue_t *victim_line_pq;
    //QTAILQ_HEAD(victim_line_list, line) victim_line_list;
    QTAILQ_HEAD(full_line_list, line) full_line_list;
    int tt_lines;
    int free_line_cnt;
    int victim_line_cnt;
    int full_line_cnt;
};
/*-----------------------------------------------------*/

struct nand_cmd {
    int type;
    int cmd;
    int64_t stime; /* Coperd: request arrival time */
};

struct ssd {
    char *ssdname;
    struct ssdparams sp;
    struct ssd_channel *ch;
    struct ppa *maptbl; /* page level mapping table */
    uint64_t *rmap;     /* reverse mapptbl, assume it's stored in OOB */
    struct write_pointer wp;
    struct line_mgmt lm;

    /* lockless ring for communication with NVMe IO thread */
    struct rte_ring **to_ftl;
    struct rte_ring **to_poller;
    bool *dataplane_started_ptr;
    QemuThread ftl_thread;
};

/*---------------------Finder 1、2的struct---------------------*/
struct Node
{
    struct nand_block *blk;
    struct Node *next;
};

struct LinkedList_1 // For Finder1
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

const int boundary_1 = 7500;
const int boundary_2 = 8500;

//void Print_SSD_State(void);
static int Enforce_Clean_Block(struct ssd *ssd, struct nand_block *victim_blk);
/*------------------------------------------------------------*/

void ssd_init(FemuCtrl *n);

#ifdef FEMU_DEBUG_FTL
#define ftl_debug(fmt, ...) \
    do { printf("[FEMU] FTL-Dbg: " fmt, ## __VA_ARGS__); } while (0)
#else
#define ftl_debug(fmt, ...) \
    do { } while (0)
#endif

#define ftl_err(fmt, ...) \
    do { fprintf(stderr, "[FEMU] FTL-Err: " fmt, ## __VA_ARGS__); } while (0)

#define ftl_log(fmt, ...) \
    do { printf("[FEMU] FTL-Log: " fmt, ## __VA_ARGS__); } while (0)


/* FEMU assert() */
#ifdef FEMU_DEBUG_FTL
#define ftl_assert(expression) assert(expression)
#else
#define ftl_assert(expression)
#endif

#endif