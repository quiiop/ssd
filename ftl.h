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


#define BLK_BITS    (8)
#define SUBBLK_BITS (8) /* kuo */
#define PG_BITS     (16)
#define SEC_BITS    (8)
#define PL_BITS     (8)
#define LUN_BITS    (8)
#define CH_BITS     (7)

/* describe a physical page addr */
struct ppa { /* kuo */
    union {
        struct {
            uint64_t blk : BLK_BITS;
            uint64_t pg  : PG_BITS;
            uint64_t subblk : SUBBLK_BITS;
            uint64_t sec : SEC_BITS;
            uint64_t pl  : PL_BITS;
            uint64_t lun : LUN_BITS;
            uint64_t ch  : CH_BITS;
            uint64_t rsv : 1;
        } g;
        uint64_t ppa;
    };
};

struct write_pointer_table {
    bool use_wp_2;
};

struct trim_table {
    int cnt;
};

typedef int nand_sec_status_t;

/*Page的屬性*/
#define PG_HOT 1 
#define PG_COLD 0
#define PG_Empty     0
#define PG_General   1
#define PG_Sensitive 2
/*Hot Level分級*/
#define Hot_level_0 0
#define Hot_level_1 1
#define Hot_level_2 2
#define Hot_level_3 3
#define Hot_level_4 4 
#define Max_Level 4
#define nHotLevel 16 //5
/*Sublk的屬性*/
#define SUBLK_VICTIM 0
#define OLD_LPN 0
#define NEW_LPN 1
/*請求Empty Page的目的*/
#define DO_CopyBack 0
#define DO_Write 1
#define Sensitive_Write 2
#define General_Write 3
#define Lived_Page_General 4
#define Lived_Page_Sensitive 5
#define No_CopyWrite 6
/*自訂義*/
#define False 0
#define True 1


struct nand_page {
    nand_sec_status_t *sec;
    int nsecs;
    int status;
    int attribute;  // 停用
    int Hot_level; // 紀錄這筆Page lba的Hot Level   
    int pg_type;  // 紀錄這筆Page存的是Genernal or Sensitive lba
};


#define SUBLK_FULL 0
#define SUBLK_NOT_FULL 1
#define SUBLK_VICTIM 0
#define SUBLK_NOT_VICTIM 1
#define SUBLK_NOT_IN_FINDER1 -1
#define SUBLK_NOT_IN_FINDER2 -1

struct nand_subblock { /* kuo */
    struct nand_page *pg;
    int npgs;
    int ipc; /* invalid page */
    int vpc; /* valid page*/
    int epc; /* empty page */
    int erase_cnt;
    int wp; /* current write pointer */
    int was_full; // sublk是否寫滿了
    int was_victim; // sublk是否符合GC的條件
    int Current_Hot_Level; // sublk現在Hot Level
    uint64_t current_page_id;// 現在在使用哪個Page

    uint64_t ch;
    uint64_t lun;
    uint64_t pl;
    uint64_t blk;
    uint64_t sublk;
};

/*-1 表示沒有在Finder裡*/
#define Blk_Not_In_Finder1 -1
#define Blk_Not_in_Finder2 -1
struct nand_block { /* kuo */
    struct nand_subblock *subblk;
    int nsubblks; // blk所擁有的sublk總數
    uint64_t current_sublk_id; // blk現在使用哪個sublk
    int GC_Sublk_Count; // blk現在有多少符合GC條件的sublk
    int Free_Sublk_Count; // nsubblks - GC_Sublk_Count
    
    int In_Finder1_Position; // blk在Finder1的位置
    int In_Finder2_Position; // blk在Finder2的位置
    int invalid_sublk; //停用
    int full_sublk; //停用
    
    uint64_t ch;
    uint64_t lun;
    uint64_t pl;
    uint64_t blk;
};

struct nand_plane {
    struct nand_block *blk;
    int nblks;
};

struct nand_lun {
    struct nand_plane *pl;
    int npls;
    uint64_t next_lun_avail_time;
    bool busy;
    uint64_t gc_endtime;
};

struct ssd_channel {
    struct nand_lun *lun;
    int nluns;
    uint64_t next_ch_avail_time;
    bool busy;
    uint64_t gc_endtime;
};

struct ssdparams {
    int secsz;        /* sector size in bytes */
    int secs_per_pg;  /* # of sectors per page */

    int pgs_per_subblk; /* kuo */
    int subblks_per_blk; /* kuo */
    
    int pgs_per_blk;  /* # of NAND pages per block */
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

    
    /* sublk gc */
    int valid_page;
    int invalid_page;

    double sublk_gc_thres_percent;
    int sublk_gc_thres_pgs;

};

typedef struct line {
    int id;  /* line id, the same as corresponding block id */
    int ipc; /* invalid page count in this line */
    int vpc; /* valid page count in this line */
    QTAILQ_ENTRY(line) entry; /* in either {free,victim,full} list */
    /* position in the priority queue for victim lines */
    size_t                  pos;
    bool was_line_in_victim_pq; // all page have valid or invalid one of them
} line;

/* wp: record next write addr */
struct write_pointer {
    struct line *curline;
    int ch;
    int lun;
    int pg;
    int subblk; /* kuo */
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

struct nand_cmd {
    int type;
    int cmd;
    int64_t stime; /* Coperd: request arrival time */
};

struct node{
    struct nand_block *blk;
    struct node *next;
};

struct link
{
    int id;
    struct node *head;
    struct node *tail;
};

#define Finder1_ID 1
struct Finder{
    int id;
    int size;
    struct link *list;
    void (*Show_Finder)(int);
};

#define Finder2_ID 2
struct Finder2{
    int id;
    int size;
    struct link *list;
    void (*Show_Finder)(int);
};

#define Fail 0;
#define Successful 1
struct Queue{
    int id;
    int Queue_Size;
    struct node *head;
    struct node *tail;

    /*
    int (*Push)(struct nand_block*);
    int (*Pop)();
    int (*Size)();
    struct nand_block* (*Peek)();
    void (*Show)();
    */ 
};


struct ssd {
    char *ssdname;
    struct ssdparams sp;
    struct ssd_channel *ch;
    struct ppa *maptbl; /* page level mapping table */
    uint64_t *rmap;     /* reverse mapptbl, assume it's stored in OOB */
    struct write_pointer wp; // more or equal than average
    struct write_pointer wp_2; // less than average
    struct line_mgmt lm;
    struct write_pointer_table *wp_table; // record the lpn which write pointer we used
    struct trim_table *trim_table;

    /* lockless ring for communication with NVMe IO thread */
    struct rte_ring **to_ftl;
    struct rte_ring **to_poller;
    bool *dataplane_started_ptr;
    QemuThread ftl_thread;
};

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