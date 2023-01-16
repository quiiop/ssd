#include "ftl.h"

# include <stdlib.h>
# include <stdio.h>

const char* fileName = "output.txt";
const char* fileName2 = "output2.txt";
const char* fileName3 = "output3.txt";
const char* fileName4 = "write_trim_gc.txt"; //
const char* fileName5 = "record.txt";
const char* fileName6 = "count.txt";
const char* fileName7 = "write_gc_count.txt"; //
const char* fileName8 = "subblock_ppa.txt";
const char* fileName9 = "clean_subblock.txt";

FILE *outfile = NULL;
FILE *outfile2 = NULL;
FILE *outfile3 = NULL;
FILE *outfile4 = NULL;
FILE *outfile5 = NULL;
FILE *outfile6 = NULL;
FILE *outfile7 = NULL;
FILE *outfile8 = NULL;
FILE *outfile9 = NULL;

//#define FEMU_DEBUG_FTL

// static int table[1048576] = {0};
static float table_cnt_avg = 0;
// static bool wp_table[1048576] = {false};
static bool wp_2 = false;
static uint64_t read_cnt = 0;
static uint64_t write_cnt = 0;
static uint64_t erase_cnt = 0;
static uint64_t live_page_copy_cnt = 0;
static uint64_t gc_cnt = 0;
static uint64_t gcc_count = 0;
static uint64_t page_write_count = 0;
static uint64_t page_trim_count = 0;
static int mark_page_invalid_count = 0;

// static write_pointer_table *wp_table = NULL;

static void *ftl_thread(void *arg);

static inline bool should_gc(struct ssd *ssd)
{
    //printf("should_gc %d, %d\n",ssd->lm.free_line_cnt, ssd->sp.gc_thres_lines);
    return (ssd->lm.free_line_cnt <= ssd->sp.gc_thres_lines);
}

static inline bool should_gc_high(struct ssd *ssd)
{
    //printf("fshould_gc_high %d, %d\n",ssd->lm.free_line_cnt, ssd->sp.gc_thres_lines_high);
    return (ssd->lm.free_line_cnt <= ssd->sp.gc_thres_lines_high);
}

static inline struct ppa get_maptbl_ent(struct ssd *ssd, uint64_t lpn)
{
    return ssd->maptbl[lpn];
}

static inline void set_maptbl_ent(struct ssd *ssd, uint64_t lpn, struct ppa *ppa)
{
    ftl_assert(lpn < ssd->sp.tt_pgs);
    ssd->maptbl[lpn] = *ppa;
}

static uint64_t ppa2pgidx(struct ssd *ssd, struct ppa *ppa) /* kuo */
{
    struct ssdparams *spp = &ssd->sp;
    uint64_t pgidx;

    pgidx = ppa->g.ch  * spp->pgs_per_ch  + \
            ppa->g.lun * spp->pgs_per_lun + \
            ppa->g.pl  * spp->pgs_per_pl  + \
            ppa->g.blk * spp->pgs_per_blk + \
            ppa->g.subblk * spp->pgs_per_subblk + \
            ppa->g.pg;

    ftl_assert(pgidx < spp->tt_pgs);

    fprintf(outfile8, "pgidx %lu\n", pgidx);
    return pgidx;
}

static inline uint64_t get_rmap_ent(struct ssd *ssd, struct ppa *ppa)
{
    uint64_t pgidx = ppa2pgidx(ssd, ppa);

    return ssd->rmap[pgidx];
}

/* set rmap[page_no(ppa)] -> lpn */
static inline void set_rmap_ent(struct ssd *ssd, uint64_t lpn, struct ppa *ppa)
{
    uint64_t pgidx = ppa2pgidx(ssd, ppa);

    ssd->rmap[pgidx] = lpn;
}

static inline int victim_line_cmp_pri(pqueue_pri_t next, pqueue_pri_t curr)
{
    return (next > curr);
}

static inline pqueue_pri_t victim_line_get_pri(void *a)
{
    return ((struct line *)a)->vpc;
}

static inline void victim_line_set_pri(void *a, pqueue_pri_t pri)
{
    ((struct line *)a)->vpc = pri;
}

static inline size_t victim_line_get_pos(void *a)
{
    return ((struct line *)a)->pos;
}

static inline void victim_line_set_pos(void *a, size_t pos)
{
    ((struct line *)a)->pos = pos;
}

static void ssd_init_lines(struct ssd *ssd)
{
    struct ssdparams *spp = &ssd->sp;
    struct line_mgmt *lm = &ssd->lm;
    struct line *line;

    lm->tt_lines = spp->blks_per_pl;
    ftl_assert(lm->tt_lines == spp->tt_lines);
    lm->lines = g_malloc0(sizeof(struct line) * lm->tt_lines);

    QTAILQ_INIT(&lm->free_line_list);
    lm->victim_line_pq = pqueue_init(spp->tt_lines, victim_line_cmp_pri,
            victim_line_get_pri, victim_line_set_pri,
            victim_line_get_pos, victim_line_set_pos);
    QTAILQ_INIT(&lm->full_line_list);

    lm->free_line_cnt = 0;
    for (int i = 0; i < lm->tt_lines; i++) {
        line = &lm->lines[i];
        line->id = i;
        line->ipc = 0;
        line->vpc = 0;
        line->pos = 0;
        line->was_line_in_victim_pq = false;
        /* initialize all the lines as free lines */
        QTAILQ_INSERT_TAIL(&lm->free_line_list, line, entry);
        lm->free_line_cnt++;
    }

    ftl_assert(lm->free_line_cnt == lm->tt_lines);
    lm->victim_line_cnt = 0;
    lm->full_line_cnt = 0;
}

static void ssd_init_write_pointer(struct ssd *ssd) /* kuo */
{
    struct write_pointer *wpp = &ssd->wp;
    struct write_pointer *wpp_2 = &ssd->wp_2;
    struct line_mgmt *lm = &ssd->lm;
    struct line *curline = NULL;
    struct line *curline_2 = NULL;

    ssd->wp_table = g_malloc0(sizeof(struct write_pointer_table) * ssd->sp.tt_pgs);

    struct write_pointer_table *wp_table = ssd->wp_table;
    for( int i = 0; i < ssd->sp.tt_pgs; i++ ) {
        wp_table[i].use_wp_2 = false;
    }

    curline = QTAILQ_FIRST(&lm->free_line_list);
    QTAILQ_REMOVE(&lm->free_line_list, curline, entry);
    lm->free_line_cnt--;
    // printf("curline->id : %d,",curline->id);

    /* wpp->curline is always our next-to-write super-block */
    wpp->curline = curline;
    wpp->ch = 0;
    wpp->lun = 0;
    wpp->pg = 0;
    wpp->blk = 0;
    wpp->subblk = 0;
    wpp->pl = 0;

    curline_2 = QTAILQ_FIRST(&lm->free_line_list);
    QTAILQ_REMOVE(&lm->free_line_list, curline_2, entry);
    lm->free_line_cnt--;

    // curline_2 = curline;
    wpp_2->curline = curline_2;
    // wpp_2->curline = curline;
    wpp_2->ch = 0;
    wpp_2->lun = 0;
    wpp_2->pg = 0;
    wpp_2->blk = curline_2->id;
    wpp_2->subblk = 0;
    wpp_2->pl = 0;

}


static inline void check_addr(int a, int max)
{
    ftl_assert(a >= 0 && a < max);
}

static struct line *get_next_free_line(struct ssd *ssd)
{
    struct line_mgmt *lm = &ssd->lm;
    struct line *curline = NULL;

    curline = QTAILQ_FIRST(&lm->free_line_list);
    if (!curline) {
        ftl_err("No free lines left in [%s] !!!!\n", ssd->ssdname);
        return NULL;
    }

    QTAILQ_REMOVE(&lm->free_line_list, curline, entry);
    lm->free_line_cnt--;
    return curline;
}

static void ssd_advance_write_pointer(struct ssd *ssd, bool wp_2)
{
    struct ssdparams *spp = &ssd->sp;
    struct write_pointer *wpp = &ssd->wp;
    // struct write_pointer *wpp = NULL;
    struct line_mgmt *lm = &ssd->lm;

    if ( wp_2 == false) {
        wpp = &ssd->wp;
    }
    else {
        wpp = &ssd->wp_2;
    }

    // printf("ssd_advance_write_pointer begin\n");

    check_addr(wpp->ch, spp->nchs);
    wpp->ch++;
    // printf("wpp->ch :%d\n",wpp->ch);
    if (wpp->ch == spp->nchs) {
        wpp->ch = 0;
        check_addr(wpp->lun, spp->luns_per_ch);
        wpp->lun++;
        /* in this case, we should go to next lun */
        // printf("wpp->lun :%d\n",wpp->lun);
        if (wpp->lun == spp->luns_per_ch) { // 8
            wpp->lun = 0;
            /* go to next page in the block */
            check_addr(wpp->pg, spp->pgs_per_subblk);
            wpp->pg++;
            if (wpp->pg == spp->pgs_per_subblk) { // 8
                wpp->pg = 0;

                /* set subblock */
                check_addr(wpp->subblk, spp->subblks_per_blk);
                wpp->subblk++;
                if (wpp->subblk == spp->subblks_per_blk) { // 16
                    wpp->subblk = 0;   
                    /* move current line to {victim,full} line list */
                    if (wpp->curline->vpc == spp->pgs_per_line) {
                        /* all pgs are still valid, move to full line list */
                        ftl_assert(wpp->curline->ipc == 0);
                        QTAILQ_INSERT_TAIL(&lm->full_line_list, wpp->curline, entry);
                        lm->full_line_cnt++;
                    } else {
                        ftl_assert(wpp->curline->vpc >= 0 && wpp->curline->vpc < spp->pgs_per_line);
                        /* there must be some invalid pages in this line */
                        ftl_assert(wpp->curline->ipc > 0);
                        pqueue_insert(lm->victim_line_pq, wpp->curline);
                        lm->victim_line_cnt++;
                    }
                    /* current line is used up, pick another empty line */
                    check_addr(wpp->blk, spp->blks_per_pl);
                    wpp->curline = NULL;
                    wpp->curline = get_next_free_line(ssd);
                    // printf("curline id %d\n", wpp->curline->id);
                    //printf("282: line= %d, threshold= %d \n", ssd->lm.free_line_cnt, ssd->sp.gc_thres_lines);
                    //printf("283 curline= %d\n", (wpp->curline)->id); 
                    if (!wpp->curline) {
                        /* TODO */
                        printf("282\n");
                        abort();
                    }   

                    /* set block */
                    wpp->blk = wpp->curline->id;
                    check_addr(wpp->blk, spp->blks_per_pl);
                    /* make sure we are starting from page 0 in the super block */
                    ftl_assert(wpp->pg == 0);
                    ftl_assert(wpp->lun == 0);
                    ftl_assert(wpp->ch == 0);
                    /* TODO: assume # of pl_per_lun is 1, fix later */
                    ftl_assert(wpp->pl == 0);
                }
            }
        }
    }
    fprintf(outfile8,"wpp ch= %d, lun= %d, pl=%d, blk=%d, subblk=%d, pg=%d\n", wpp->ch, wpp->lun, wpp->pl, wpp->blk, wpp->subblk, wpp->pg);
}

static struct ppa get_new_page(struct ssd *ssd, bool wp_2) /* kuo */
{
    struct write_pointer *wpp = &ssd->wp;
    // struct write_pointer *wpp = NULL;


    // struct ppa ppa_high;
    // ppa_high.ppa = 0;
    // ppa_high.g.ch = wpp->ch;
    // ppa_high.g.lun = wpp->lun;
    // ppa_high.g.pg = wpp->pg;
    // ppa_high.g.blk = wpp->blk;
    // ppa_high.g.pl = wpp->pl;
    // ftl_assert(ppa_high.g.pl == 0);

    // struct ppa ppa_low;
    // ppa_low.ppa = 0;
    // ppa_low.g.ch = wpp_low->ch;
    // ppa_low.g.lun = wpp_low->lun;
    // ppa_low.g.pg = wpp_low->pg;
    // ppa_low.g.blk = wpp_low->blk;
    // ppa_low.g.pl = wpp_low->pl;
    // ftl_assert(ppa_low.g.pl == 0);

    // static uint64_t lpn_high_pre = 0;
    // static uint64_t lpn_low_pre = 0;
    // uint64_t lpn_high = get_rmap_ent(ssd, &ppa_high);
    // uint64_t lpn_low = get_rmap_ent(ssd, &ppa_low);

    // if ( lpn_high_pre == 0 || lpn_high_pre != lpn_high) {
    //     lpn_high_pre = lpn_high;
    //     // printf("lpn_high : %ld , high cnt : %d\n", lpn_high, table[lpn_high]);
    // }

    // if ( lpn_low_pre == 0 || lpn_low_pre != lpn_low) {
    //     lpn_low_pre = lpn_low;
    //     // printf("lpn_low : %ld, low cnt : %d\n", lpn_low, table[lpn_low]);
    // }

    
    // // printf("high cnt : %d, low cnt : %d\n", table[lpn_high], table[lpn_low]);

    if ( wp_2 == false) {
        wpp = &ssd->wp;
    }
    else {
        wpp = &ssd->wp_2;
    }


    struct ppa ppa;
    ppa.ppa = 0;
    ppa.g.ch = wpp->ch;
    ppa.g.lun = wpp->lun;
    ppa.g.pg = wpp->pg;
    ppa.g.subblk = wpp->subblk;
    ppa.g.blk = wpp->blk;
    ppa.g.pl = wpp->pl;
    ftl_assert(ppa.g.pl == 0);

    return ppa;
}

static void check_params(struct ssdparams *spp)
{
    /*
     * we are using a general write pointer increment method now, no need to
     * force luns_per_ch and nchs to be power of 2
     */

    //ftl_assert(is_power_of_2(spp->luns_per_ch));
    //ftl_assert(is_power_of_2(spp->nchs));
}

static void ssd_init_params(struct ssdparams *spp)
{
    spp->secsz = 512;
    spp->secs_per_pg = 8;

    spp->pgs_per_subblk = 16; /* kuo */
    spp->subblks_per_blk = 16; /* kuo */
    spp->pgs_per_blk = spp->pgs_per_subblk * spp->subblks_per_blk; /* kuo */
    
    spp->blks_per_pl = 256; /* 16GB */  //senior set 128 ,256->64
    spp->pls_per_lun = 1;
    spp->luns_per_ch = 8;//8->2
    spp->nchs = 8;//8->4

    spp->pg_rd_lat = NAND_READ_LATENCY;
    spp->pg_wr_lat = NAND_PROG_LATENCY;
    spp->blk_er_lat = NAND_ERASE_LATENCY;
    spp->ch_xfer_lat = 0;

    /* calculated values */
    spp->secs_per_blk = spp->secs_per_pg * spp->pgs_per_blk;
    spp->secs_per_pl = spp->secs_per_blk * spp->blks_per_pl;
    spp->secs_per_lun = spp->secs_per_pl * spp->pls_per_lun;
    spp->secs_per_ch = spp->secs_per_lun * spp->luns_per_ch;
    spp->tt_secs = spp->secs_per_ch * spp->nchs;

    spp->pgs_per_pl = spp->pgs_per_blk * spp->blks_per_pl;
    spp->pgs_per_lun = spp->pgs_per_pl * spp->pls_per_lun;
    spp->pgs_per_ch = spp->pgs_per_lun * spp->luns_per_ch;
    spp->tt_pgs = spp->pgs_per_ch * spp->nchs;

    spp->blks_per_lun = spp->blks_per_pl * spp->pls_per_lun;
    spp->blks_per_ch = spp->blks_per_lun * spp->luns_per_ch;
    spp->tt_blks = spp->blks_per_ch * spp->nchs;

    spp->pls_per_ch =  spp->pls_per_lun * spp->luns_per_ch;
    spp->tt_pls = spp->pls_per_ch * spp->nchs;

    spp->tt_luns = spp->luns_per_ch * spp->nchs;

    /* line is special, put it at the end */
    spp->blks_per_line = spp->tt_luns; /* TODO: to fix under multiplanes */
    spp->pgs_per_line = spp->blks_per_line * spp->pgs_per_blk;
    spp->secs_per_line = spp->pgs_per_line * spp->secs_per_pg;
    spp->tt_lines = spp->blks_per_lun; /* TODO: to fix under multiplanes */

    printf("total line %d\n", spp->tt_lines);
    spp->gc_thres_pcent = 0.1; // 0.75
    spp->gc_thres_lines = (int)((1 - spp->gc_thres_pcent) * spp->tt_lines);
    spp->gc_thres_pcent_high = 0.95;
    spp->gc_thres_lines_high = (int)((1 - spp->gc_thres_pcent_high) * spp->tt_lines);
    spp->enable_gc_delay = true;


    check_params(spp);
}

static void ssd_init_nand_page(struct nand_page *pg, struct ssdparams *spp)
{
    pg->nsecs = spp->secs_per_pg;
    pg->sec = g_malloc0(sizeof(nand_sec_status_t) * pg->nsecs);
    for (int i = 0; i < pg->nsecs; i++) {
        pg->sec[i] = SEC_FREE;
    }
    pg->status = PG_FREE;
}

static void ssd_init_nand_subblk(struct nand_subblock *subblk, struct ssdparams *spp)
{
    subblk->npgs = spp->pgs_per_subblk;
    subblk->pg = g_malloc0(sizeof(struct nand_page) * subblk->npgs);
    for (int i = 0; i < subblk->npgs; i++) {
        ssd_init_nand_page(&subblk->pg[i], spp);
    }
    subblk->ipc = 0;
    subblk->vpc = 0;
    subblk->erase_cnt = 0;
    subblk->wp = 0;
}

static void ssd_init_nand_blk(struct nand_block *blk, struct ssdparams *spp)
{
    blk->nsubblks = spp->subblks_per_blk;
    blk->subblk = g_malloc0(sizeof(struct nand_subblock) * blk->nsubblks);
    for (int i = 0; i < blk->nsubblks; i++) {
        ssd_init_nand_subblk(&blk->subblk[i], spp);
    }
}

static void ssd_init_nand_plane(struct nand_plane *pl, struct ssdparams *spp)
{
    pl->nblks = spp->blks_per_pl;
    pl->blk = g_malloc0(sizeof(struct nand_block) * pl->nblks);
    for (int i = 0; i < pl->nblks; i++) {
        ssd_init_nand_blk(&pl->blk[i], spp);
    }
}

static void ssd_init_nand_lun(struct nand_lun *lun, struct ssdparams *spp)
{
    lun->npls = spp->pls_per_lun;
    lun->pl = g_malloc0(sizeof(struct nand_plane) * lun->npls);
    for (int i = 0; i < lun->npls; i++) {
        ssd_init_nand_plane(&lun->pl[i], spp);
    }
    lun->next_lun_avail_time = 0;
    lun->busy = false;
}

static void ssd_init_ch(struct ssd_channel *ch, struct ssdparams *spp)
{
    ch->nluns = spp->luns_per_ch;
    ch->lun = g_malloc0(sizeof(struct nand_lun) * ch->nluns);
    for (int i = 0; i < ch->nluns; i++) {
        ssd_init_nand_lun(&ch->lun[i], spp);
    }
    ch->next_ch_avail_time = 0;
    ch->busy = 0;
}

static void ssd_init_maptbl(struct ssd *ssd)
{
    struct ssdparams *spp = &ssd->sp;

    ssd->maptbl = g_malloc0(sizeof(struct ppa) * spp->tt_pgs);
    for (int i = 0; i < spp->tt_pgs; i++) {
        ssd->maptbl[i].ppa = UNMAPPED_PPA;
    }
}

static void ssd_init_rmap(struct ssd *ssd)
{
    struct ssdparams *spp = &ssd->sp;

    ssd->rmap = g_malloc0(sizeof(uint64_t) * spp->tt_pgs);
    for (int i = 0; i < spp->tt_pgs; i++) {
        ssd->rmap[i] = INVALID_LPN;
    }
}

void ssd_init(FemuCtrl *n)
{
    struct ssd *ssd = n->ssd;
    struct ssdparams *spp = &ssd->sp;

    ftl_assert(ssd);

    ssd_init_params(spp);

    /* initialize ssd internal layout architecture */
    ssd->ch = g_malloc0(sizeof(struct ssd_channel) * spp->nchs);
    for (int i = 0; i < spp->nchs; i++) {
        ssd_init_ch(&ssd->ch[i], spp);
    }

    /* initialize maptbl */
    ssd_init_maptbl(ssd);

    /* initialize rmap */
    ssd_init_rmap(ssd);

    /* initialize all the lines */
    ssd_init_lines(ssd);

    // initialize ssd trim cnt table 
    ssd->trim_table = g_malloc0(sizeof(struct trim_table) * spp->tt_pgs);
    for (int i = 0; i < spp->tt_pgs; i++) {
        ssd->trim_table[i].cnt = 0;
    }

    /* initialize write pointer, this is how we allocate new pages for writes */
    ssd_init_write_pointer(ssd);

    qemu_thread_create(&ssd->ftl_thread, "FEMU-FTL-Thread", ftl_thread, n,
                       QEMU_THREAD_JOINABLE);
}

static inline bool valid_ppa(struct ssd *ssd, struct ppa *ppa) /* kuo */
{
    struct ssdparams *spp = &ssd->sp;
    int ch = ppa->g.ch;
    int lun = ppa->g.lun;
    int pl = ppa->g.pl;
    int blk = ppa->g.blk;
    int subblk = ppa->g.subblk; /* kuo */
    int pg = ppa->g.pg;
    int sec = ppa->g.sec;

    if (ch >= 0 && ch < spp->nchs && lun >= 0 && lun < spp->luns_per_ch && pl >= /* kuo */
        0 && pl < spp->pls_per_lun && blk >= 0 && blk < spp->blks_per_pl && subblk >=0 && 
        subblk < spp->subblks_per_blk && pg >= 0 && pg < spp->pgs_per_subblk && sec >= 0 && 
        sec < spp->secs_per_pg)
        return true;

    return false;
}

static inline bool valid_lpn(struct ssd *ssd, uint64_t lpn)
{
    return (lpn < ssd->sp.tt_pgs);
}

static inline bool mapped_ppa(struct ppa *ppa)
{
    return !(ppa->ppa == UNMAPPED_PPA);
}

static inline struct ssd_channel *get_ch(struct ssd *ssd, struct ppa *ppa)
{
    return &(ssd->ch[ppa->g.ch]);
}

static inline struct nand_lun *get_lun(struct ssd *ssd, struct ppa *ppa)
{
    struct ssd_channel *ch = get_ch(ssd, ppa);
    // if ( ssd->wp.curline->id >= 55 ) {
    //     printf("get_ch ok\n");
    // }
    return &(ch->lun[ppa->g.lun]);
}

static inline struct nand_plane *get_pl(struct ssd *ssd, struct ppa *ppa)
{
    struct nand_lun *lun = get_lun(ssd, ppa);
    return &(lun->pl[ppa->g.pl]);
}

static inline struct nand_block *get_blk(struct ssd *ssd, struct ppa *ppa)
{
    struct nand_plane *pl = get_pl(ssd, ppa);
    return &(pl->blk[ppa->g.blk]);
}

static inline struct line *get_line(struct ssd *ssd, struct ppa *ppa)
{
    return &(ssd->lm.lines[ppa->g.blk]);
}

static inline struct nand_subblock *get_subblk(struct ssd *ssd, struct ppa *ppa)
{
    struct nand_block *blk = get_blk(ssd, ppa);
    return &(blk->subblk[ppa->g.subblk]);
}

static inline struct nand_page *get_pg(struct ssd *ssd, struct ppa *ppa)
{
    struct nand_subblock *subblk = get_subblk(ssd, ppa);
    return &(subblk->pg[ppa->g.pg]);
}

static uint64_t ssd_advance_status(struct ssd *ssd, struct ppa *ppa, struct
        nand_cmd *ncmd)
{
    int c = ncmd->cmd;
    uint64_t cmd_stime = (ncmd->stime == 0) ? \
        qemu_clock_get_ns(QEMU_CLOCK_REALTIME) : ncmd->stime;
    uint64_t nand_stime;
    struct ssdparams *spp = &ssd->sp;
    // if ( ssd->wp.curline->id >= 55 ) {
    //     printf("get_lun begin\n");
    // }
    struct nand_lun *lun = get_lun(ssd, ppa);
    uint64_t lat = 0;

    // if ( ssd->wp.curline->id >= 55 ) {
    //     printf("lun->npls : %d\n",lun->npls);
    // }

    switch (c) {
    case NAND_READ:
        /* read: perform NAND cmd first */
        nand_stime = (lun->next_lun_avail_time < cmd_stime) ? cmd_stime : \
                     lun->next_lun_avail_time;
        lun->next_lun_avail_time = nand_stime + spp->pg_rd_lat;
        lat = lun->next_lun_avail_time - cmd_stime;
#if 0
        lun->next_lun_avail_time = nand_stime + spp->pg_rd_lat;

        /* read: then data transfer through channel */
        chnl_stime = (ch->next_ch_avail_time < lun->next_lun_avail_time) ? \
            lun->next_lun_avail_time : ch->next_ch_avail_time;
        ch->next_ch_avail_time = chnl_stime + spp->ch_xfer_lat;

        lat = ch->next_ch_avail_time - cmd_stime;
#endif
        break;

    case NAND_WRITE:
        /* write: transfer data through channel first */
        nand_stime = (lun->next_lun_avail_time < cmd_stime) ? cmd_stime : \
                     lun->next_lun_avail_time;
        if (ncmd->type == USER_IO) {
            lun->next_lun_avail_time = nand_stime + spp->pg_wr_lat;
        } else {
            lun->next_lun_avail_time = nand_stime + spp->pg_wr_lat;
        }
        lat = lun->next_lun_avail_time - cmd_stime;

#if 0
        chnl_stime = (ch->next_ch_avail_time < cmd_stime) ? cmd_stime : \
                     ch->next_ch_avail_time;
        ch->next_ch_avail_time = chnl_stime + spp->ch_xfer_lat;

        /* write: then do NAND program */
        nand_stime = (lun->next_lun_avail_time < ch->next_ch_avail_time) ? \
            ch->next_ch_avail_time : lun->next_lun_avail_time;
        lun->next_lun_avail_time = nand_stime + spp->pg_wr_lat;

        lat = lun->next_lun_avail_time - cmd_stime;
#endif
        break;

    case NAND_ERASE:
        /* erase: only need to advance NAND status */
        nand_stime = (lun->next_lun_avail_time < cmd_stime) ? cmd_stime : \
                     lun->next_lun_avail_time;
        lun->next_lun_avail_time = nand_stime + spp->blk_er_lat;

        lat = lun->next_lun_avail_time - cmd_stime;
        break;

    default:
        ftl_err("Unsupported NAND command: 0x%x\n", c);
    }

    return lat;
}

/* update SSD status about one page from PG_VALID -> PG_VALID */
static void mark_page_invalid(struct ssd *ssd, struct ppa *ppa, NvmeRequest *req)
{
    mark_page_invalid_count = mark_page_invalid_count + 1;
    //printf("%d %d mark_page_invalid begin\n",mark_page_invalid_count ,req->cmd.opcode);
    struct line_mgmt *lm = &ssd->lm;
    struct ssdparams *spp = &ssd->sp;
    // struct nand_block *blk = NULL;
    struct nand_subblock *subblk = NULL;
    struct nand_page *pg = NULL;
    bool was_full_line = false;
    bool all_page_valid = false;
    struct line *line;

    /* update corresponding page status */
    pg = get_pg(ssd, ppa);
    ftl_assert(pg->status == PG_VALID);
    pg->status = PG_INVALID;
    
    
    /* uint64_t lpn = get_rmap_ent(ssd, ppa);
    if(req->cmd.opcode == NVME_CMD_DSM){
    	printf("%d slba %"PRIu64"\n",req->cmd.opcode, req->slba);
    	printf("lpn %"PRIu64" status %d\n",lpn,pg->status);
    } */
    
    /* update corresponding block status */ 
    subblk = get_subblk(ssd, ppa);
    ftl_assert(subblk->ipc >= 0 && subblk->ipc < spp->pgs_per_subblk);
    subblk->ipc++;
    ftl_assert(subblk->vpc > 0 && subblk->vpc <= spp->pgs_per_subblk);
    subblk->vpc--;

    /* update corresponding line status */
    line = get_line(ssd, ppa);
    ftl_assert(line->ipc >= 0 && line->ipc < spp->pgs_per_line);
    int cond1 = line->vpc + line->ipc;

    if (line->vpc == spp->pgs_per_line) {
        ftl_assert(line->ipc == 0);
        all_page_valid = true;
        was_full_line = true;
        //printf("726 %d %d %d %d\n",req->cmd.opcode,line->vpc,line->ipc,spp->pgs_per_line);
    }
    if (cond1 == spp->pgs_per_line){
    	int cond2 = (int)(spp->pgs_per_line/4);
    	if(line->ipc >= cond2) {
    		was_full_line = true;
        	//printf("730 %d %d %d %d\n",req->cmd.opcode,line->vpc,line->ipc,spp->pgs_per_line);
    	}
    }
    
    line->ipc++;
    ftl_assert(line->vpc > 0 && line->vpc <= spp->pgs_per_line);
    /* Adjust the position of the victime line in the pq under over-writes */
    if (line->pos) {
        //printf("742 %d %d\n",line->id,line->vpc); 
        pqueue_change_priority(lm->victim_line_pq, line->vpc - 1, line);
        //printf("744 %d %d\n",line->id,line->vpc); 
    } else {
    	line->vpc--;
    }
    
    if (was_full_line) {
        if (all_page_valid) {
        	QTAILQ_REMOVE(&lm->full_line_list, line, entry);
 		    lm->full_line_cnt--;
        }
        bool was_line_in_victim_pq = line->was_line_in_victim_pq;
        if (!was_line_in_victim_pq){
        	pqueue_insert(lm->victim_line_pq, line);
        	line->was_line_in_victim_pq = true;
        	lm->victim_line_cnt++; 
        }
    }
}

static void mark_page_valid(struct ssd *ssd, struct ppa *ppa)
{
    struct nand_subblock *subblk = NULL;
    struct nand_page *pg = NULL;
    struct line *line;

    /* update page status */
    pg = get_pg(ssd, ppa);
    ftl_assert(pg->status == PG_FREE);
    pg->status = PG_VALID;

    /* update corresponding block status */
    subblk = get_subblk(ssd, ppa);
    ftl_assert(subblk->vpc >= 0 && subblk->vpc < ssd->sp.pgs_per_subblk);
    subblk->vpc++;

    /* update corresponding line status */
    line = get_line(ssd, ppa);
    ftl_assert(line->vpc >= 0 && line->vpc < ssd->sp.pgs_per_line);
    line->vpc++;
}

static void mark_subblock_free(struct ssd *ssd, struct ppa *ppa)
{
    struct ssdparams *spp = &ssd->sp;
    struct nand_subblock *subblk = get_subblk(ssd, ppa);
    struct nand_page *pg = NULL;

    for (int i = 0; i < spp->pgs_per_subblk; i++) {
        /* reset page status */
        pg = &subblk->pg[i];
        ftl_assert(pg->nsecs == spp->secs_per_pg);
        pg->status = PG_FREE;
    }

    /* reset block status */
    ftl_assert(subblk->npgs == spp->pgs_per_subblk);
    subblk->ipc = 0;
    subblk->vpc = 0;
    subblk->erase_cnt++;
}

static void gc_read_page(struct ssd *ssd, struct ppa *ppa)
{
    /* advance ssd status, we don't care about how long it takes */
    if (ssd->sp.enable_gc_delay) {
        struct nand_cmd gcr;
        gcr.type = GC_IO;
        gcr.cmd = NAND_READ;
        gcr.stime = 0;
        ssd_advance_status(ssd, ppa, &gcr);
    }
}

/* move valid page data (already in DRAM) from victim line to a new page */
static uint64_t gc_write_page(struct ssd *ssd, struct ppa *old_ppa)
{
    struct ppa new_ppa;
    struct nand_lun *new_lun;
    uint64_t lpn = get_rmap_ent(ssd, old_ppa);
    bool old_use_wp_2 = ssd->wp_table[lpn].use_wp_2; // mapping the lpn with corresponding which write pointer we used!

    ftl_assert(valid_lpn(ssd, lpn));
    new_ppa = get_new_page(ssd, old_use_wp_2);
    /* update maptbl */
    set_maptbl_ent(ssd, lpn, &new_ppa);
    /* update rmap */
    set_rmap_ent(ssd, lpn, &new_ppa);

    mark_page_valid(ssd, &new_ppa);

    /* need to advance the write pointer here */
    ssd_advance_write_pointer(ssd, old_use_wp_2);

    if (ssd->sp.enable_gc_delay) {
        struct nand_cmd gcw;
        gcw.type = GC_IO;
        gcw.cmd = NAND_WRITE;
        gcw.stime = 0;
        ssd_advance_status(ssd, &new_ppa, &gcw);
    }

    /* advance per-ch gc_endtime as well */
#if 0
    new_ch = get_ch(ssd, &new_ppa);
    new_ch->gc_endtime = new_ch->next_ch_avail_time;
#endif

    new_lun = get_lun(ssd, &new_ppa);
    new_lun->gc_endtime = new_lun->next_lun_avail_time;

    return 0;
}

static struct line *select_victim_line(struct ssd *ssd, bool force)
{
    struct line_mgmt *lm = &ssd->lm;
    struct line *victim_line = NULL;

    //printf("851\n");
    victim_line = pqueue_peek(lm->victim_line_pq);
    //printf("853\n");
    if (!victim_line) {
    	//printf("855\n");
        return NULL;
    }

    if (!force && victim_line->ipc < ssd->sp.pgs_per_line / 8) {
    	//printf("860\n");
        return NULL;
    }
    //printf("select_victim_line return not NULL\n");

    //printf("865\n");
    pqueue_pop(lm->victim_line_pq);
    //printf("867\n");
    victim_line->pos = 0;
    lm->victim_line_cnt--;

    /* victim_line is a danggling node now */
    return victim_line;
}

/* here ppa identifies the block we want to clean */
static void clean_one_subblock(struct ssd *ssd, struct ppa *ppa, NvmeRequest *req)
{
    struct ssdparams *spp = &ssd->sp;
    struct nand_page *pg_iter = NULL;
    int cnt = 0; // count one block vaild page
    int invaild_cnt = 0; // count one block invaild page
    fprintf(outfile9, "gc ch= %d, lun= %d, blk= %d, subblk= %d\n", ppa->g.ch, ppa->g.lun, ppa->g.blk, ppa->g.subblk);
    for (int pg = 0; pg < spp->pgs_per_subblk; pg++) {
        ppa->g.pg = pg;
        pg_iter = get_pg(ssd, ppa);
        /* there shouldn't be any free page in victim blocks */
        ftl_assert(pg_iter->status != PG_FREE);
        if (pg_iter->status == PG_VALID) {
            gc_read_page(ssd, ppa);
            /* delay the maptbl update until "write" happens */
            gc_write_page(ssd, ppa);
            cnt++;
            live_page_copy_cnt++;
        }  
    }
    invaild_cnt = spp->pgs_per_subblk - cnt;
    fprintf(outfile4,"%d %d %d\n",req->cmd.opcode,cnt,invaild_cnt);// valid , inalid
    ftl_assert(get_subblk(ssd, ppa)->vpc == cnt);
}



static void mark_line_free(struct ssd *ssd, struct ppa *ppa)
{
    struct line_mgmt *lm = &ssd->lm;
    struct line *line = get_line(ssd, ppa);
    // printf("947 make free line id= %d\n", line->id);
    line->ipc = 0;
    line->vpc = 0;
    line->was_line_in_victim_pq = false;
    /* move this line to free line list */
    QTAILQ_INSERT_TAIL(&lm->free_line_list, line, entry);
    lm->free_line_cnt++;
    // printf("954 now free_line_cnt= %d\n", lm->free_line_cnt);
}

static int do_gc(struct ssd *ssd, bool force, NvmeRequest *req)
{
    struct line *victim_line = NULL;
    struct ssdparams *spp = &ssd->sp;
    struct nand_lun *lunp;
    struct ppa ppa;
    int ch, lun, subblk;
    int before_free_line_cnt = ssd->lm.free_line_cnt;

    victim_line = select_victim_line(ssd, force);
    if (!victim_line) {
        return -1;
    }
    //printf("970 victim line id= %d\n", victim_line->id);
    gc_cnt++;

    ppa.g.blk = victim_line->id;
    ftl_debug("GC-ing line:%d,ipc=%d,victim=%d,full=%d,free=%d\n", ppa.g.blk,
              victim_line->ipc, ssd->lm.victim_line_cnt, ssd->lm.full_line_cnt,
              ssd->lm.free_line_cnt);
    
    /*record*/
    // printf("%d %d %d %d\n",req->cmd.opcode,ssd->lm.free_line_cnt,ssd->sp.gc_thres_lines,ssd->sp.gc_thres_lines_high);
    /* copy back valid data */
    fprintf(outfile9, "gc, curline %d, victim line %d\n",ssd->wp.curline->id ,victim_line->id);
    fprintf(outfile9, "gc free line %d, gc thres %d\n", ssd->lm.free_line_cnt, ssd->sp.gc_thres_lines);
    for (ch = 0; ch < spp->nchs; ch++) {
        for (lun = 0; lun < spp->luns_per_ch; lun++) {
            ppa.g.ch = ch;
            ppa.g.lun = lun;
            ppa.g.pl = 0;
            lunp = get_lun(ssd, &ppa);
            for (subblk=0; subblk < spp->subblks_per_blk; subblk++) {
                ppa.g.subblk = subblk; /* kuo */
                clean_one_subblock(ssd, &ppa, req);
                mark_subblock_free(ssd, &ppa);
            }
            if (spp->enable_gc_delay) {
                    erase_cnt++;
                    struct nand_cmd gce;
                    gce.type = GC_IO;
                    gce.cmd = NAND_ERASE;
                    gce.stime = 0;
                    ssd_advance_status(ssd, &ppa, &gce);
            }   
            lunp->gc_endtime = lunp->next_lun_avail_time;
        }
    }
    fprintf(outfile9, "  \n");
    /* update line status */
    mark_line_free(ssd, &ppa);
    fprintf(outfile5,"%d %d %d %d %d\n", req->cmd.opcode, before_free_line_cnt ,ssd->lm.free_line_cnt, ssd->sp.gc_thres_lines, ssd->sp.gc_thres_lines_high);	
    return 0;
}

static uint64_t ssd_read(struct ssd *ssd, NvmeRequest *req)
{
    struct ssdparams *spp = &ssd->sp;
    uint64_t lba = req->slba;
    int nsecs = req->nlb;
    struct ppa ppa;
    uint64_t start_lpn = lba / spp->secs_per_pg;
    uint64_t end_lpn = (lba + nsecs - 1) / spp->secs_per_pg;
    uint64_t lpn;
    uint64_t sublat, maxlat = 0;

    // fprintf(outfile,"R ");
    // printf("R\n");
    if (end_lpn >= spp->tt_pgs) {
        ftl_err("start_lpn=%"PRIu64",tt_pgs=%d\n", start_lpn, ssd->sp.tt_pgs);
    }

    // fprintf(outfile,"start_lpn %ld ",start_lpn);
    // fprintf(outfile,"end_lpn %ld\n",end_lpn);
    /* normal IO read path */
    for (lpn = start_lpn; lpn <= end_lpn; lpn++) {
        ppa = get_maptbl_ent(ssd, lpn);
        // if(ppa.g.ch != 127) {
        //     printf("ppa.g.blk:%d pg:%d sec:%d pl:%d lun:%d ch:%d\n", ppa.g.blk, ppa.g.pg, ppa.g.sec, ppa.g.pl, ppa.g.lun, ppa.g.ch);

        //     //  ppa2pgidx(ssd, &ppa);
        //     // printf("page index : %d\n",pgindex);
        //     uint64_t pgindex = get_rmap_ent(ssd, &ppa);
        //     printf("page index : %ld\n",pgindex);
        //     printf("lpn : %ld\n",lpn);
        // }
        
        if (!mapped_ppa(&ppa) || !valid_ppa(ssd, &ppa)) {
            //printf("%s,lpn(%" PRId64 ") not mapped to valid ppa\n", ssd->ssdname, lpn);
            //printf("Invalid ppa,ch:%d,lun:%d,blk:%d,pl:%d,pg:%d,sec:%d\n",
            //ppa.g.ch, ppa.g.lun, ppa.g.blk, ppa.g.pl, ppa.g.pg, ppa.g.sec);
            continue;
        }

        read_cnt++;

        struct nand_cmd srd;
        srd.type = USER_IO;
        srd.cmd = NAND_READ;
        srd.stime = req->stime;
        sublat = ssd_advance_status(ssd, &ppa, &srd);
        maxlat = (sublat > maxlat) ? sublat : maxlat;
    }

    return maxlat;
}

static uint64_t ssd_write(struct ssd *ssd, NvmeRequest *req)
{
    //printf("ssd write\n");
    uint64_t lba = req->slba;
    //printf("ssd_write req->slba  : %"PRIu64"\n",req->slba);
    //printf("lba :%ld\n",lba);
    struct ssdparams *spp = &ssd->sp;
    //printf("spp->secs_per_pg  %d\n",spp->secs_per_pg);
    int len = req->nlb;
    //printf("ssd_write req->nlb :%d\n",req->nlb);
    uint64_t start_lpn = lba / spp->secs_per_pg;
    //printf("ssd_write start_lpn  %"PRIu64"\n",start_lpn);
    uint64_t end_lpn = (lba + len - 1) / spp->secs_per_pg;
    //printf("ssd_write end_lpn  %"PRIu64"\n",end_lpn);
    struct ppa ppa;
    uint64_t lpn;
    uint64_t curlat = 0, maxlat = 0;
    int r;

    if (end_lpn >= spp->tt_pgs) {
        ftl_err("start_lpn=%"PRIu64",tt_pgs=%d\n", start_lpn, ssd->sp.tt_pgs);
    }

    while (should_gc_high(ssd)) {
        /* perform GC here until !should_gc(ssd) */
        r = do_gc(ssd, true, req);
        // gc_cnt++;
        fprintf(outfile8,"gc r= %d\n", r);
        if (r == -1)
            break;
        gcc_count++;
    }
    
    
    fprintf(outfile,"start_lpn %ld ",start_lpn);
    fprintf(outfile,"end_lpn %ld\n",end_lpn);
    // printf("start_lpn %ld ",start_lpn);
    // printf("end_lpn %ld\n",end_lpn);
    for (lpn = start_lpn; lpn <= end_lpn; lpn++) {
        
        ppa = get_maptbl_ent(ssd, lpn);

        if (mapped_ppa(&ppa)) {
            /* update old page information first */
            mark_page_invalid(ssd, &ppa, req);
            set_rmap_ent(ssd, INVALID_LPN, &ppa);
        }

        // only use to debug

        /*static int last_id = 0;
        static int last_id_2 = 0;

        if( last_id != ssd->wp.curline->id) {
            last_id = ssd->wp.curline->id;
            printf("now_lpn : %ld ",lpn);
            printf("curline->id : %d ",ssd->wp.curline->id);
            printf("wp_2.curline->id : %d\n",ssd->wp_2.curline->id);

            // printf("table_cnt_avg : %.4f\n", table_cnt_avg);
            // printf("ssd->trim_table[lpn].cnt : %d\n", ssd->trim_table[lpn].cnt);
            // if(wp_2) {
            //     printf("wp_2 is true\n");
            // }
            // else {
            //     printf("wp_2 is false\n");
            // }
        }

        // if ( ssd->wp.curline->id >= 55) {
        //     printf("get ppa end\n");
        // }

        if( last_id_2 != ssd->wp_2.curline->id) {
            last_id_2 = ssd->wp_2.curline->id;
            printf("now_lpn : %ld ",lpn);
            printf("curline->id : %d ",ssd->wp.curline->id);
            printf("wp_2.curline->id : %d\n",ssd->wp_2.curline->id);

            // printf("table_cnt_avg : %.4f\n", table_cnt_avg);
            // printf("ssd->trim_table[lpn].cnt : %d\n", ssd->trim_table[lpn].cnt);
            // if(wp_2) {
            //     printf("wp_2 is true\n");
            // }
            // else {
            //     printf("wp_2 is false\n");
            // }
        }*/

        

        if ( (float)ssd->trim_table[lpn].cnt > table_cnt_avg) {
            wp_2 = false; 
            ssd->wp_table[lpn].use_wp_2 = false;
        }
        else {
            wp_2 = true; // true
            ssd->wp_table[lpn].use_wp_2 = true;// true
        }

        
        // actually what we write
        /* new write */
        ppa = get_new_page(ssd, wp_2);
        /* update maptbl */
        set_maptbl_ent(ssd, lpn, &ppa);
        /* update rmap */
        set_rmap_ent(ssd, lpn, &ppa);

        mark_page_valid(ssd, &ppa);

        /* need to advance the write pointer here */
        ssd_advance_write_pointer(ssd, wp_2);

        write_cnt++;
        page_write_count++;
        // fprintf(outfile2 ,"write page_write_count %"PRIu64"\n", page_write_count);

        struct nand_cmd swr;
        swr.type = USER_IO;
        swr.cmd = NAND_WRITE;
        swr.stime = req->stime;
        /* get latency statistics */
        curlat = ssd_advance_status(ssd, &ppa, &swr);
        maxlat = (curlat > maxlat) ? curlat : maxlat;
        
        /* calucate write pda*/
        uint64_t pba = ppa2pgidx(ssd, &ppa);
        //printf("write %"PRIu64"\n",pba);
	    fprintf(outfile3, "write %"PRIu64"\n",pba);

    }

    return maxlat;
}



static uint64_t ssd_dsm(struct ssd *ssd, NvmeRequest *req)
{
    //printf("ssd dsm\n");
    uint64_t lba = req->slba;
    //printf("dsm req->slba  : %"PRIu64"\n",req->slba);
    fprintf(outfile,"D ");
    //printf("ssd_dsm ");
    //printf("lba :%ld \n",lba);
    struct ssdparams *spp = &ssd->sp;
    int len = req->nlb;
    //printf("len :%d\n",len);
    uint64_t start_lpn = lba / spp->secs_per_pg;
    uint64_t end_lpn = (lba + len - 1) / spp->secs_per_pg;
    struct ppa ppa;
    uint64_t lpn;

    // uint64_t curlat = 0, maxlat = 0;
    // int r;

    fprintf(outfile,"ssd_dsm start_lpn %ld ",start_lpn);
    fprintf(outfile,"end_lpn %ld\n",end_lpn);
    /*
    printf("lba = %"PRIu64,lba);
    printf(" sp.tt_pgs = %d\n",ssd->sp.tt_pgs);
    printf("start_lpn %ld",start_lpn);
    printf(" end_lpn %ld\n",end_lpn);
    */

    uint32_t dw11 = le32_to_cpu(req->cmd.cdw11);
    //printf("dw11 %u\n",dw11);
    if (dw11 & NVME_DSMGMT_AD){
        

        for (lpn = start_lpn; lpn <= end_lpn; lpn++) {

            if (lpn >= ssd->sp.tt_pgs) {
		printf("table out of range!\n");                
            }
            else {
                // table[lpn]++;
                ssd->trim_table[lpn].cnt++;
            }
            ppa = get_maptbl_ent(ssd, lpn);
            // printf("ssd_dsm lpn:%ld\n",lpn);
            if ( /*(ppa.ppa != UNMAPPED_PPA) &&*/ mapped_ppa(&ppa)) {

                /* update old page information first */
                mark_page_invalid(ssd, &ppa, req);
                // printf("mark end\n");
                set_rmap_ent(ssd, INVALID_LPN, &ppa);
            }
            /* caulucate page trim count */
            page_trim_count++;
            
            /* calucate trim pda */
            uint64_t pba = ppa2pgidx(ssd, &ppa);
            //printf("trim %"PRIu64"\n",pba);
	    fprintf(outfile3, "trim %"PRIu64"\n",pba);
        }

        int cnt = 0;
        for (int i = 0; i < ssd->sp.tt_pgs; i++) {
            cnt += ssd->trim_table[i].cnt;
        }
        //printf("cnt = %d",cnt);
        table_cnt_avg = cnt / (float)ssd->sp.tt_pgs;
        // printf("table_cnt_avg : %.4f\n", table_cnt_avg);

        cnt = 0;
        int max = 0, min = ssd->trim_table[0].cnt;
        for (int i = 0; i < ssd->sp.tt_pgs; i++) {
            if ( (float)ssd->trim_table[i].cnt > table_cnt_avg ) {
                cnt++;
            }

            if ( ssd->trim_table[i].cnt > max ) {
                max = ssd->trim_table[i].cnt;
            }

            if ( ssd->trim_table[i].cnt < min ) {
                min = ssd->trim_table[i].cnt;
            }
        }
        // printf("cnt more than average : %d\n", cnt);
        // printf("max : %d min : %d\n", max, min);

    }

    return 0;

}


static void *ftl_thread(void *arg)
{
    FemuCtrl *n = (FemuCtrl *)arg;
    struct ssd *ssd = n->ssd;
    NvmeRequest *req = NULL;
    uint64_t lat = 0;
    int rc;
    int i;
    bool request_trim = false;
    
    //printf("start !!\n");
    //fprintf(outfile2,"start !!");
    outfile = fopen( fileName, "wb" );
    outfile2 = fopen(fileName2, "wb");
    outfile3 = fopen(fileName3, "wb");
    outfile4 = fopen(fileName4, "wb");
    outfile5 = fopen(fileName5, "wb");
    outfile6 = fopen(fileName6, "wb");
    outfile7 = fopen(fileName7, "wb");
    outfile8 = fopen(fileName8, "wb");
    outfile9 = fopen(fileName9, "wb");

    while (!*(ssd->dataplane_started_ptr)) {
        usleep(100000);
    }

    /* FIXME: not safe, to handle ->to_ftl and ->to_poller gracefully */
    ssd->to_ftl = n->to_ftl;
    ssd->to_poller = n->to_poller;

    // struct ssdparams *spp = &ssd->sp;
    // printf("spp->secsz :%d, spp->secs_per_pg :%d, spp->pgs_per_blk :%d\n", spp->secsz, spp->secs_per_pg, spp->pgs_per_blk);
    // printf("spp->blks_per_pl :%d, spp->pls_per_lun :%d, spp->luns_per_ch :%d, spp->nchs :%d\n",spp->blks_per_pl, spp->pls_per_lun, spp->luns_per_ch, spp->nchs);
    // printf("spp->pgs_per_line:%d, spp->blks_per_line:%d\n",spp->pgs_per_line,spp->blks_per_line);

    while (1) {
    	// printf("n->num_poller %d\n",n->num_poller);
        for (i = 1; i <= n->num_poller; i++) {
            if (!ssd->to_ftl[i] || !femu_ring_count(ssd->to_ftl[i]))
                continue;

            rc = femu_ring_dequeue(ssd->to_ftl[i], (void *)&req, 1);
            if (rc != 1) {
                printf("FEMU: FTL to_ftl dequeue failed\n");
            }

            ftl_assert(req);

            // static int req_len = 0;
            // req_len+=req->nlb;
            // printf("req_len:%d\n", req_len);

            if (req->cmd.opcode != 2){
       		// printf("ftl.c req->cmd.opcode :%d\n",req->cmd.opcode);
            }
            
            switch (req->cmd.opcode) {
            case NVME_CMD_WRITE:
                lat = ssd_write(ssd, req);
                request_trim = false;
                break;
            case NVME_CMD_READ:
                lat = ssd_read(ssd, req);
                request_trim = false;
                break;
            case NVME_CMD_DSM:
                //printf("ftl_thread NVME_CMD_DSM\n");
                lat = ssd_dsm(ssd, req);
                lat = 0;
                request_trim = true;
                break;
            default:
            	request_trim = false;
                //ftl_err("FTL received unkown request type, ERROR\n");
                ;
            }
	    //printf("i = %d\n",i);
            req->reqlat = lat;
            req->expire_time += lat;

            rc = femu_ring_enqueue(ssd->to_poller[i], (void *)&req, 1);
            if (rc != 1) {
                ftl_err("FTL to_poller enqueue failed\n");
            }

            /* clean one line if needed (in the background) */
            if(!request_trim) {
            	if (should_gc(ssd)) {
            		fprintf(outfile7,"%"PRIu64" %d %d\n",req->slba, req->cmd.opcode, ssd->lm.free_line_cnt);
                	do_gc(ssd, false, req); // default false
            	}
            }
            // if ( req->cmd.opcode == NVME_CMD_WRITE ) {
            //     printf("read_cnt : %ld ",read_cnt);
            //     printf("write_cnt : %ld ",write_cnt);
            //     printf("erase_cnt : %ld ",erase_cnt);
            //     printf("live_page_copy_cnt : %ld ",live_page_copy_cnt);
            //     printf("gc_cnt : %ld \n",gc_cnt);
            // }
            
       } 
    }
    
    fprintf(outfile2,"page write count %"PRIu64,page_write_count);
    fprintf(outfile2,"page trim count %"PRIu64,page_trim_count);
    
    fclose( outfile );
    fclose(outfile2);
    fclose(outfile3);
    fclose(outfile4);
    fclose(outfile5);
    fclose(outfile6);
    fclose(outfile7);
    fclose(outfile8);
    fclose(outfile9);

    return NULL;
}

