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
const char* fileName9 = "GC_sublk_record.txt";
const char* fileName10 = "sublock_request.txt";
const char* fileName11 = "subblock_line.txt";
const char* fileName12 = "ssd_write.txt";
const char* fileName13 = "test.txt";
const char* fileName14 = "page_record.txt";
const char* fileName15 = "finder_init.txt";
const char* fileName16 = "add_link.txt";
const char* fileName17 = "mark_pg_invalid.txt";
const char* fileName18 = "Finder_record.txt";
const char* fileName19 = "vic_node.txt";

FILE *outfile = NULL;
FILE *outfile2 = NULL;
FILE *outfile3 = NULL;
FILE *outfile4 = NULL;
FILE *outfile5 = NULL;
FILE *outfile6 = NULL;
FILE *outfile7 = NULL;
FILE *outfile8 = NULL;
FILE *outfile9 = NULL;
FILE *outfile10 = NULL;
FILE *outfile11 = NULL;
FILE *outfile12 = NULL;
FILE *outfile13 = NULL;
FILE *outfile14 = NULL;
FILE *outfile15 = NULL;
FILE *outfile16 = NULL;
FILE *outfile17 = NULL;
FILE *outfile18 = NULL;
FILE *outfile19 = NULL;

//#define FEMU_DEBUG_FTL

// static int table[1048576] = {0};
static float table_cnt_avg = 0;
// static bool wp_table[1048576] = {false};
static bool wp_2 = false;
static uint64_t read_cnt = 0;
static uint64_t write_cnt = 0;
// static uint64_t erase_cnt = 0;
static uint64_t live_page_copy_cnt = 0;
// static uint64_t gc_cnt = 0;
static uint64_t gcc_count = 0;
static uint64_t page_write_count = 0;
static uint64_t page_trim_count = 0;
static int mark_page_invalid_count = 0;

/* Empty Page Queue */
TAILQ_HEAD(ppa_queue, ppa);
struct ppa_queue *Empty_page_queue = NULL; 

/* Finder */
static struct Finder *finder;
static int tt_ipc =0;
static int tt_vpc =0;
static int test_count=0;

static void *ftl_thread(void *arg);


/*
static inline bool should_gc(struct ssd *ssd)
{
    // printf("should_gc %d, %d\n",ssd->lm.free_line_cnt, ssd->sp.gc_thres_lines);
    return (ssd->lm.free_line_cnt <= ssd->sp.gc_thres_lines);
}

static inline bool should_gc_high(struct ssd *ssd)
{
    //printf("fshould_gc_high %d, %d\n",ssd->lm.free_line_cnt, ssd->sp.gc_thres_lines_high);
    return (ssd->lm.free_line_cnt <= ssd->sp.gc_thres_lines_high);
}
*/

static inline bool should_gc_sublk(struct ssd *ssd)
{
    /*
    if (ssd->sp.invalid_page > ssd->sp.sublk_gc_thres_pgs){
        return true;
    }else{
        return false;
    }
    */
    if (tt_ipc > ssd->sp.sublk_gc_thres_pgs){
        return true;
    }else{
        return false;
    }

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

/*
static struct line *get_next_free_line(struct ssd *ssd)
{
    struct line_mgmt *lm = &ssd->lm;
    struct line *curline = NULL;

    //printf("free line cnt %lu\n", lm->free_line_cnt);
    curline = QTAILQ_FIRST(&lm->free_line_list);
    if (!curline) {
        printf("221 error\n");
        ftl_err("No free lines left in [%s] !!!!\n", ssd->ssdname);
        return NULL;
    }

    QTAILQ_REMOVE(&lm->free_line_list, curline, entry);
    lm->free_line_cnt--;
    return curline;
}
*/

/*
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
        //in this case, we should go to next lun 
        // printf("wpp->lun :%d\n",wpp->lun);
        if (wpp->lun == spp->luns_per_ch) { // 8
            wpp->lun = 0;
            //go to next page in the block 
            check_addr(wpp->pg, spp->pgs_per_subblk);
            wpp->pg++;
            if (wpp->pg == spp->pgs_per_subblk) { // 8
                wpp->pg = 0;

                // set subblock 
                check_addr(wpp->subblk, spp->subblks_per_blk);
                wpp->subblk++;
                if (wpp->subblk == spp->subblks_per_blk) { // 16
                    wpp->subblk = 0;   
                    // move current line to {victim,full} line list 
                    // printf("subblock change\n");
                    fprintf(outfile12, "subblock change, line_total_page %d, line_valid_page %d\n", spp->pgs_per_line, wpp->curline->vpc);
                    if (wpp->curline->vpc == spp->pgs_per_line) {
                        //all pgs are still valid, move to full line list
                        ftl_assert(wpp->curline->ipc == 0);
                        QTAILQ_INSERT_TAIL(&lm->full_line_list, wpp->curline, entry);
                        // printf("277\n");
                        lm->full_line_cnt++;
                    } else {
                        ftl_assert(wpp->curline->vpc >= 0 && wpp->curline->vpc < spp->pgs_per_line);
                        //there must be some invalid pages in this line 
                        ftl_assert(wpp->curline->ipc > 0);
                        //printf("283 %d pqueue insert \n", wpp->curline->id);
                        pqueue_insert(lm->victim_line_pq, wpp->curline);
                        lm->victim_line_cnt++;
                    }
                    //current line is used up, pick another empty line 
                    check_addr(wpp->blk, spp->blks_per_pl);
                    fprintf(outfile11, "curline id %d, free line %d\n", wpp->curline->id, lm->free_line_cnt);
                    wpp->curline = NULL;
                    wpp->curline = get_next_free_line(ssd);
                    fprintf(outfile11, "newline id %d, free line %d\n", wpp->curline->id, lm->free_line_cnt);
                    // printf("curline id %d\n", wpp->curline->id);
                    //printf("282: line= %d, threshold= %d \n", ssd->lm.free_line_cnt, ssd->sp.gc_thres_lines);
                    //printf("283 curline= %d\n", (wpp->curline)->id); 
                    if (!wpp->curline) {
                        // TODO 
                        printf("282\n");
                        abort();
                    }   

                    // set block 
                    wpp->blk = wpp->curline->id;
                    check_addr(wpp->blk, spp->blks_per_pl);
                    // make sure we are starting from page 0 in the super block 
                    ftl_assert(wpp->pg == 0);
                    ftl_assert(wpp->lun == 0);
                    ftl_assert(wpp->ch == 0);
                    // TODO: assume # of pl_per_lun is 1, fix later 
                    ftl_assert(wpp->pl == 0);
                }
            }
        }
    }
    fprintf(outfile8,"wpp ch= %d, lun= %d, pl=%d, blk=%d, subblk=%d, pg=%d\n", wpp->ch, wpp->lun, wpp->pl, wpp->blk, wpp->subblk, wpp->pg);
}
*/

/*
static struct ppa get_new_page(struct ssd *ssd, bool wp_2) 
{
    struct write_pointer *wpp = &ssd->wp;

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
*/

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
    
    spp->blks_per_pl = 64; /* 16GB */  //senior set 128 ,256->64
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
    spp->gc_thres_pcent = 0.75; // 0.75
    spp->gc_thres_lines = (int)((1 - spp->gc_thres_pcent) * spp->tt_lines);
    spp->gc_thres_pcent_high = 0.95;
    spp->gc_thres_lines_high = (int)((1 - spp->gc_thres_pcent_high) * spp->tt_lines);
    spp->enable_gc_delay = true;

    /* sublk */
    spp->valid_page = 0;
    spp->invalid_page = 0;
    spp->sublk_gc_thres_percent = 0.9;
    spp->sublk_gc_thres_pgs = (int)( (1 - spp->sublk_gc_thres_percent) * spp->tt_pgs);

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
    blk->pos = -1;

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


/*
static void page_record(struct ppa_queue *pq) {

    
    outfile14 = fopen(fileName14, "wb");
    
    while (!TAILQ_EMPTY(pq)){
        
        struct ppa *first = TAILQ_FIRST(pq);
        
        struct ppa ppa = *first;

        TAILQ_REMOVE(pq, first, next);
        
        free(first);
        
        fprintf(outfile14, "ch= %d lun= %d pl= %d blk= %d sublk= %d pg= %d\n", ppa.g.ch, ppa.g.lun, ppa.g.pl, ppa.g.blk, ppa.g.subblk, ppa.g.pg);
    }
    fprintf(outfile14, "OK\n");
    
}
*/

static void init_Empty_page_queue(struct ssdparams *spp) {

   
    int pages = spp->pgs_per_subblk;
    int sublks = spp->subblks_per_blk;
    int blks = spp->blks_per_pl;
    int luns = spp->luns_per_ch;
    int chs = spp->nchs;
   
    
    for (int page=0; page<pages; page++){
        for (int sublk=0; sublk<sublks; sublk++){
            for (int blk=0; blk<blks; blk++){
                for (int lun=0; lun<luns; lun++){
                    for (int ch=0; ch<chs; ch++){
                        
                        struct ppa *ppa = malloc(sizeof(struct ppa));
                        ppa->g.pg = page;
                        ppa->g.subblk = sublk;
                        ppa->g.blk = blk;
                        ppa->g.pl = 0;
                        ppa->g.lun = lun;
                        ppa->g.ch = ch;

                        TAILQ_INSERT_TAIL(Empty_page_queue, ppa, next);
                        
                    }
                }
            }
        }
    }

    // page_record(Empty_page_queue);
}


static struct ppa *get_empty_page(void){
    struct ppa *first = TAILQ_FIRST(Empty_page_queue);    
    TAILQ_REMOVE(Empty_page_queue, first, next);
    return first;
}

static void insert_page_in_queue(struct ppa *ppa){
    
    struct ppa *node = malloc(sizeof(struct ppa));
    *node = *ppa;
    // printf("node blk %d sublk %d\n", node->g.blk, node->g.subblk);
    TAILQ_INSERT_TAIL(Empty_page_queue, node, next);
    // printf("insert complete \n");
} 

/* sublk Finder */

static struct node* init_node(struct nand_block *blk)
{
    struct node *n = malloc(sizeof(struct node));
    n->blk = blk;
    n->next = NULL;
    printf("630 node \n");
    return n;
}

static struct link* init_link(int id){
    struct link* list = malloc(sizeof(struct link));
    list->id = id;
    list->head = NULL;
    list->tail = NULL;
    return list;
}

static void init_Finder(int size) {
    finder = malloc(sizeof(struct Finder));
    finder->size = size;
    finder->list = (struct link **)malloc(sizeof(struct link)*size);
    for (int i=0; i<size; i++){
        finder->list[i] = init_link(i);
    }
}

static void print_link(struct link *list){ 
    struct node *current = list->head;
    current = list->head;
    // printf("Link id %d : ", list->id);
    fprintf(outfile15, "Link id %d : ", list->id);

    if (list->head == NULL){
        // printf("list empty\n");
        fprintf(outfile15, "list empty\n");
        //fprintf(outfile18, "list empty\n");
    }else{
        fprintf(outfile16, "Link id %d : ", list->id);
        fprintf(outfile18, "Link id %d : ", list->id);
        while (1){
            if (current->next == NULL){
            break;
            }
            // printf("[ Block id %d , addr %p ] -> ", current->blk->id, current->blk);
            fprintf(outfile16, "[ Block id %d , addr %p ] -> ", current->blk->blk, current->blk);
            fprintf(outfile18, "[ Block id %d , addr %p ] -> ", current->blk->blk, current->blk);
            current = current->next;
        }
        // printf("[ Block id %d , addr %p ]\n", current->blk->id, current->blk);
        fprintf(outfile16, "[ Block id %d , addr %p ] -> NULL\n", current->blk->blk, current->blk);
        fprintf(outfile16, "---------------------------\n");

        fprintf(outfile18, "[ Block id %d , addr %p ] -> NULL\n", current->blk->blk, current->blk);
        fprintf(outfile18, "---------------------------\n");
    }
}


static void Finder_record(void) {
    printf("688\n");
    fprintf(outfile18, "Finder Record :\n");
    for (int i=0; i<16; i++){
        print_link(finder->list[i]);
    }
}


static void add_link(struct link *list, struct node *n){
    struct node *current = NULL;
    if (list->head == NULL){
        list->head = n;
    }else{
        current = list->head;
        while (1){
            if (current->next == NULL){
                break;
            }
            current = current->next;
        }
        current->next = n;
        list->tail = n;
    }
    print_link(list);
}

static struct node* get_victim_node(struct link *list){
    struct node *victim_node;

    if (list->head == NULL){
        printf("142 err list is empty\n");
        return NULL;
    }else {
        // printf("146 target : node %p , blk %p\n", list->head, list->head->blk);
        if (list->head->next == NULL){
            victim_node = list->head;
            list->head = NULL;
        } else{
            victim_node = list->head;
            list->head = list->head->next;
        }
    }
    // printf("155 victim_node : node %p , blk %p\n", victim_node, victim_node->blk);
    return victim_node;
}

static void from_link_get_node(struct link *list, struct nand_block *blk, struct node **target) {
    struct node *current;
    struct node *temp = NULL;

    if (list->head == NULL){
        printf("156 err link is empty\n");
    }else{
        if (list->head->blk == blk){
            temp = list->head;
            list->head = list->head->next;
        }else{
            for (current=list->head; current!=NULL; current=current->next){
                if (current->next->blk == blk){
                    temp = current->next;
                    current->next = current->next->next;
                    break;
                }
            }
        }
    }
    temp->next = NULL;
    *target = temp;
}

static void add_Finder(struct nand_block *blk, int pos){
    // pos = Which link you want to add
    
    //int last_pos = pos-1;
    //int result;
    printf("add Finder, pos= %d\n", pos);
    if (pos == 0){ // block first time add Finder
        struct node *n = init_node(blk);
        printf("744 \n");
        add_link(finder->list[pos], n);
        printf("746 \n");
    }else{
        struct node **target = malloc(sizeof(struct node *));
        struct node *n;
        int last_pos = pos-1;

        printf("751 \n");
        from_link_get_node(finder->list[last_pos], blk, target);
        printf("754 \n");
        n = *target;
        printf("756 \n");
        add_link(finder->list[pos], n);
        printf("758 \n");
    }
    
}

static struct node* get_victim_node_from_Finder(void){
    struct node *victim_node = NULL;
    //struct node **target;
    //struct node *n;
    int pos = -1;

    for (int i=finder->size-1; i>=0; i--){
        if (finder->list[i]->head != NULL){
            pos = i;
            break;
        }
    }
    
    if (pos == -1){
        // printf("err Finder is empty\n");
        return NULL;
    }else{
        victim_node = get_victim_node(finder->list[pos]);
        printf("744 %d", victim_node->blk->blk);
        return victim_node;
    }
}

/*
static void print_Finder_one_list_node(int pos) {
    print_link(finder->list[pos]);
    fprintf(outfile15, "------------------\n");
}
*/

static void print_Finder_all_list(void) {
    int size = finder->size;
    // printf("Finder :\n");
    fprintf(outfile15, "Finder :\n");

    for (int i=0; i<size; i++){
        // printf("link id %d\n", finder->list[i]->id);
        print_link(finder->list[i]);
        // printf("\n");
    }
    // printf("---------------\n");
}

/*
static struct ppa *test_Finder(void) {
    struct ppa *empty_page = get_empty_page();
    struct nand_subblock *sublk = NULL;

    for (int i=0; i<10; i++){
        empty_page->g.subblk = i;

        sublk = get_subblk(ssd, ppa);
        sublk->was_full = 1;
    }
    return empty_page;
}

static int print_test_Finder(struct ppa *test_pg) {
    int count = 0;
    struct nand_subblock *sublk = NULL;

    for (int i=0; i<16; i++){
        test_pg->g.subblk = i;

        sublk = get_subblk(ssd, test_pg);
        if (sublk->was_full == 1){
            count = count +1;
        }
    }
    return count;
}
*/

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

    /* init Empty_page_queue */
    Empty_page_queue = malloc(sizeof(struct ppa_queue));
    TAILQ_INIT(Empty_page_queue);
    init_Empty_page_queue(spp);
    
    /* init Finder */
    int nsublks = spp->subblks_per_blk;
    outfile15  = fopen(fileName15, "wb");
    outfile18 = fopen(fileName18, "wb");
    init_Finder(nsublks);
    print_Finder_all_list();
    fclose(outfile15);


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
        printf("707 error\n");
        ftl_err("Unsupported NAND command: 0x%x\n", c);
    }

    return lat;
}

/* update SSD status about one page from PG_VALID -> PG_VALID */
static void mark_page_invalid(struct ssd *ssd, struct ppa *ppa, NvmeRequest *req)
{
    mark_page_invalid_count = mark_page_invalid_count + 1;
    // struct line_mgmt *lm = &ssd->lm;
    struct ssdparams *spp = &ssd->sp;
    struct nand_subblock *subblk = NULL;
    struct nand_page *pg = NULL;
    // bool was_full_line = false;
    // struct line *line;

    /* update corresponding page status */
    pg = get_pg(ssd, ppa);
    ftl_assert(pg->status == PG_VALID);
    pg->status = PG_INVALID;
    
    ssd->sp.invalid_page++;
    tt_ipc = tt_ipc +1;

    ssd->sp.valid_page--;
    tt_vpc = tt_vpc -1;
    
    /* uint64_t lpn = get_rmap_ent(ssd, ppa);
    if(req->cmd.opcode == NVME_CMD_DSM){
    	printf("%d slba %"PRIu64"\n",req->cmd.opcode, req->slba);
    	printf("lpn %"PRIu64" status %d\n",lpn,pg->status);
    } */
    
    /* update corresponding block status */ 
    subblk = get_subblk(ssd, ppa);
    ftl_assert(subblk->ipc >= 0 && subblk->ipc < spp->pgs_per_subblk);
    subblk->ipc = subblk->ipc +1;
    ftl_assert(subblk->vpc > 0 && subblk->vpc <= spp->pgs_per_subblk);
    subblk->vpc = subblk->vpc -1;
    // printf("mark page invalid\n");
    fprintf(outfile17, "mark page invalid\n");

    int sublk_total_page = subblk->ipc + subblk->vpc; 
    //printf("sublk ipc %d vpc %d tt %d", subblk->ipc, subblk->vpc, sublk_total_page);
    //printf("sublk ipc %d vpc %d tt %d \n", subblk->ipc, subblk->vpc, sublk_total_page);
    fprintf(outfile17, "sublk ipc %d vpc %d tt %d \n", subblk->ipc, subblk->vpc, sublk_total_page);
    fprintf(outfile17, "------------------\n");

    // spp->pgs_per_subblk
    if (sublk_total_page == spp->pgs_per_subblk){
        printf("sublk full %d\n", subblk->ipc);
        subblk->was_full = 1;

        if (subblk->ipc >= 10){
            struct nand_block *blk = get_blk(ssd, ppa);
            int nsublk = spp->subblks_per_blk;
            int count=0;

            for (int i=0; i<nsublk; i++){
                if (blk->subblk[i].was_full == 1){
                    count++;
                }
            }
            blk->invalid_sublk = count;

            if (blk->pos == blk->invalid_sublk){

            }else{
                blk->ch = ppa->g.ch;
                blk->lun = ppa->g.lun;
                blk->pl = ppa->g.pl;
                blk->blk = ppa->g.blk;
                blk->pos = blk->invalid_sublk -1;
                add_Finder(blk, blk->pos);
            }
        }
    }
    


    /* update corresponding line status */
    
    /*
    line = get_line(ssd, ppa);
    ftl_assert(line->ipc >= 0 && line->ipc < spp->pgs_per_line);
    fprintf(outfile12, "Page invalid, line %d, vpc %d, ipc %d\n", line->id, line->vpc, line->ipc);
    */

    /*
    if (line->vpc == spp->pgs_per_line) {
        ftl_assert(line->ipc == 0);
        was_full_line = true;
    }
    */
    
    /*
    
    line->ipc++;
    ftl_assert(line->vpc > 0 && line->vpc <= spp->pgs_per_line);
    */
    
    
    // Adjust the position of the victime line in the pq under over-writes 
    /*
    if (line->pos) {
        pqueue_change_priority(lm->victim_line_pq, line->vpc - 1, line);
    } else {
    	line->vpc--;
    }
    
   if (was_full_line) {
        move line: "full" -> "victim" 
        QTAILQ_REMOVE(&lm->full_line_list, line, entry);
        lm->full_line_cnt--;
        pqueue_insert(lm->victim_line_pq, line);
        lm->victim_line_cnt++;
    }
    */
}

static void mark_page_valid(struct ssd *ssd, struct ppa *ppa)
{
    struct nand_subblock *subblk = NULL;
    struct nand_page *pg = NULL;
    // struct line *line;

    /* update page status */
    pg = get_pg(ssd, ppa);
    // printf("vaild pg %d\n", ppa->g.pg);
    ftl_assert(pg->status == PG_FREE);
    pg->status = PG_VALID;

    /* update corresponding block status */
    subblk = get_subblk(ssd, ppa);
    ftl_assert(subblk->vpc >= 0 && subblk->vpc < ssd->sp.pgs_per_subblk);
    subblk->vpc++;

    ssd->sp.valid_page++;
    tt_vpc = tt_vpc +1;

    /* update corresponding line status */
    /*
    line = get_line(ssd, ppa);
    ftl_assert(line->vpc >= 0 && line->vpc < ssd->sp.pgs_per_line);
    line->vpc++;
    */
    
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

        ppa->g.pg = i;
        insert_page_in_queue(ppa);
    }

    /* reset block status */
    ftl_assert(subblk->npgs == spp->pgs_per_subblk);
    subblk->ipc = 0;
    subblk->vpc = 0;
    subblk->erase_cnt++;
    subblk->was_full = -1;
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
    struct ppa *first;
    //struct nand_lun *new_lun;
    uint64_t lpn = get_rmap_ent(ssd, old_ppa);
    // bool old_use_wp_2 = ssd->wp_table[lpn].use_wp_2; // mapping the lpn with corresponding which write pointer we used!

    ftl_assert(valid_lpn(ssd, lpn));
    // new_ppa = get_new_page(ssd, old_use_wp_2);
    first = get_empty_page();
    new_ppa = *first;

    /* update maptbl */
    set_maptbl_ent(ssd, lpn, &new_ppa);
    /* update rmap */
    set_rmap_ent(ssd, lpn, &new_ppa);

    mark_page_valid(ssd, &new_ppa);

    /* need to advance the write pointer here */
    // ssd_advance_write_pointer(ssd, old_use_wp_2);

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

    //new_lun = get_lun(ssd, &new_ppa);
    //new_lun->gc_endtime = new_lun->next_lun_avail_time;

    return 0;
}

/*
static struct line *select_victim_line(struct ssd *ssd, bool force)
{
    struct line_mgmt *lm = &ssd->lm;
    struct line *victim_line = NULL;

    //printf("851\n");
    victim_line = pqueue_peek(lm->victim_line_pq);
    if (!victim_line) {
    	// printf("855 no victim line\n");
        return NULL;
    }
    if (!force && victim_line->ipc < ssd->sp.pgs_per_line / 8) {
    	// printf("860\n");
        return NULL;
    }
    //printf("victim line id %d , ipc %d\n", victim_line->id, victim_line->ipc);
    fprintf(outfile12, "victim line id %d , ipc %d\n", victim_line->id, victim_line->ipc);

    pqueue_pop(lm->victim_line_pq);
    victim_line->pos = 0;
    lm->victim_line_cnt--;

    victim_line is a danggling node now 
    return victim_line;
}
*/

/* here ppa identifies the block we want to clean */
static void clean_one_subblock(struct ssd *ssd, struct ppa *ppa, NvmeRequest *req)
{
    struct ssdparams *spp = &ssd->sp;
    struct nand_page *pg_iter = NULL;
    int cnt = 0; // count one block vaild page
    int invaild_cnt = 0; // count one block invaild page
    
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
            ssd->sp.valid_page--;
            tt_vpc = tt_vpc -1;
        }else{
            ssd->sp.invalid_page--;
            tt_ipc = tt_ipc -1;
            printf("clean one sublk ipc %d\n", tt_ipc);
        }
    }

    invaild_cnt = spp->pgs_per_subblk - cnt;
    fprintf(outfile4,"%d %d %d\n",req->cmd.opcode,cnt,invaild_cnt);// valid , inalid
    ftl_assert(get_subblk(ssd, ppa)->vpc == cnt);
}

/*
static void mark_line_free(struct ssd *ssd, struct ppa *ppa)
{
    struct line_mgmt *lm = &ssd->lm;
    struct line *line = get_line(ssd, ppa);
    // printf("947 make free line id= %d\n", line->id);
    line->ipc = 0;
    line->vpc = 0;
    line->was_line_in_victim_pq = false;
    //move this line to free line list
    QTAILQ_INSERT_TAIL(&lm->free_line_list, line, entry);
    lm->free_line_cnt++;
    // printf("954 now free_line_cnt= %d\n", lm->free_line_cnt);
}
*/



static int do_gc(struct ssd *ssd, bool force, NvmeRequest *req)
{
    // struct line *victim_line = NULL;
    struct ssdparams *spp = &ssd->sp;
    //struct nand_lun *lunp;
    struct ppa ppa;
    int sublk;
    // int before_free_line_cnt = ssd->lm.free_line_cnt;
    struct node *victim_node = NULL;
    struct nand_block *victim_blk = NULL;
    struct nand_subblock *victim_sublk = NULL;

    // printf("do gc\n");
    victim_node = get_victim_node_from_Finder();

    if (victim_node == NULL){
        // printf("not find vic nod\n");
        return -1;
    };
    // printf("do gc\n");
    victim_blk = victim_node->blk;


    /*
    victim_line = select_victim_line(ssd, force);
    if (!victim_line) {
        return -1;
    }
    
    printf("970 victim line id= %d\n", victim_line->id);
    gc_cnt++;
    
    ppa.g.blk = victim_line->id;
    */

    ppa.g.ch = victim_blk->ch;
    ppa.g.lun = victim_blk->lun;
    ppa.g.pl = victim_blk->pl;
    ppa.g.blk = victim_blk->blk;
    
    /*
    ftl_debug("GC-ing line:%d,ipc=%d,victim=%d,full=%d,free=%d\n", ppa.g.blk,
              victim_line->ipc, ssd->lm.victim_line_cnt, ssd->lm.full_line_cnt,
              ssd->lm.free_line_cnt);
    */

    /* copy back valid data */

    for (sublk=0; sublk<spp->subblks_per_blk; sublk++){
        ppa.g.subblk = sublk;
        victim_sublk = get_subblk(ssd, &ppa);

        if (victim_sublk->ipc >= 10){
            fprintf(outfile9, "%d %d\n", victim_sublk->ipc, victim_sublk->vpc);
            clean_one_subblock(ssd, &ppa, req);
            mark_subblock_free(ssd, &ppa);
        }
    }

    /*
    for (ch = 0; ch < spp->nchs; ch++) {
        for (lun = 0; lun < spp->luns_per_ch; lun++) {
            ppa.g.ch = ch;
            ppa.g.lun = lun;
            ppa.g.pl = 0;
            lunp = get_lun(ssd, &ppa);
            int ipc = 0;
            int vpc = 0;

            for (subblk=0; subblk < spp->subblks_per_blk; subblk++) {
                ppa.g.subblk = subblk;  // kuo 
                struct nand_subblock *sublk = get_subblk(ssd, &ppa);
                ipc = ipc + sublk->ipc;
                vpc = vpc + sublk->vpc;

                clean_one_subblock(ssd, &ppa, req);
                mark_subblock_free(ssd, &ppa);
            }
            fprintf(outfile9, "%d %d\n", ipc, vpc);

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
    // update line status 
    mark_line_free(ssd, &ppa);
    fprintf(outfile5,"%d %d %d %d %d\n", req->cmd.opcode, before_free_line_cnt ,ssd->lm.free_line_cnt, ssd->sp.gc_thres_lines, ssd->sp.gc_thres_lines_high);
    */	
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
        printf("1025 error\n");
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
    uint64_t lba = req->slba;
    struct ssdparams *spp = &ssd->sp;
    int len = req->nlb;
    uint64_t start_lpn = lba / spp->secs_per_pg;
    uint64_t end_lpn = (lba + len - 1) / spp->secs_per_pg;
    struct ppa ppa;
    uint64_t lpn;
    uint64_t curlat = 0, maxlat = 0;
    int r;

    if (end_lpn >= spp->tt_pgs) {
        printf("1084 error\n");
        ftl_err("start_lpn=%"PRIu64",tt_pgs=%d\n", start_lpn, ssd->sp.tt_pgs);
    }

    // printf("ssd ipc %d , gc ipc %d\n", tt_ipc, spp->sublk_gc_thres_pgs);
    while (should_gc_sublk(ssd)) {
        /* perform GC here until !should_gc(ssd) */
        r = do_gc(ssd, true, req);
        fprintf(outfile8,"gc r= %d\n", r);
        if (r == -1){
            // printf("1409 not find\n");
            break;
        }
        gcc_count++;
    }

    for (lpn = start_lpn; lpn <= end_lpn; lpn++) {

        // printf("lpn %ld\n", lpn);
        
        ppa = get_maptbl_ent(ssd, lpn);

        if (mapped_ppa(&ppa)) {
            /* update old page information first */
            // printf("pg invalid\n");
            mark_page_invalid(ssd, &ppa, req);
            set_rmap_ent(ssd, INVALID_LPN, &ppa);
        }

        if ( (float)ssd->trim_table[lpn].cnt > table_cnt_avg) {
            wp_2 = false; 
            ssd->wp_table[lpn].use_wp_2 = false;
        }
        else {
            wp_2 = true; 
            ssd->wp_table[lpn].use_wp_2 = true;
        }

        // new write 
        struct ppa *empty_page = get_empty_page();
        ppa = *empty_page;
        // printf("new pg ch= %d, lun= %d, blk= %d, subblk= %d, page= %d\n", ppa.g.ch, ppa.g.lun, ppa.g.blk, ppa.g.subblk, ppa.g.pg);

        fprintf(outfile12, "ssd write ch= %d, lun= %d, blk= %d, subblk= %d, page= %d\n", ppa.g.ch, ppa.g.lun, ppa.g.blk, ppa.g.subblk, ppa.g.pg);
        // update maptbl 
        set_maptbl_ent(ssd, lpn, &ppa);
        /* update rmap */
        set_rmap_ent(ssd, lpn, &ppa);

        mark_page_valid(ssd, &ppa);

        /* need to advance the write pointer here */
        //ssd_advance_write_pointer(ssd, wp_2);

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

static struct ppa *test_Finder(struct ssd *ssd) {
    struct ppa *empty_page = get_empty_page();
    struct nand_subblock *sublk = NULL;

    for (int i=0; i<10; i++){
        empty_page->g.subblk = i;

        sublk = get_subblk(ssd, empty_page);
        sublk->was_full = 1;
    }

    return empty_page;
}

/*
static int print_test_Finder(struct ssd *ssd, struct ppa *test_pg) {
    int count = 0;
    struct nand_subblock *sublk = NULL;

    for (int i=0; i<16; i++){
        test_pg->g.subblk = i;

        sublk = get_subblk(ssd, test_pg);
        if (sublk->was_full == 1){
            count = count +1;
        }
    }
    return count;
}
*/

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
    outfile10 = fopen(fileName10, "wb");
    outfile11 = fopen(fileName11, "wb");
    outfile12 = fopen(fileName12, "wb");
    outfile13 = fopen(fileName13, "wb");
    outfile16 = fopen(fileName16, "wb");
    outfile17 = fopen(fileName17, "wb");
    outfile19 = fopen(fileName19, "wb");
    

    while (!*(ssd->dataplane_started_ptr)) {
        usleep(100000);
    }

    /* FIXME: not safe, to handle ->to_ftl and ->to_poller gracefully */
    ssd->to_ftl = n->to_ftl;
    ssd->to_poller = n->to_poller;

    /* test Finder */
    if (test_count == 0){
        /* test Finder */
        
        struct ppa *test_pg = test_Finder(ssd);
        struct nand_block *test_blk = get_blk(ssd, test_pg);

        fprintf(outfile16, "target blk %p\n", test_blk);
        test_blk->pos =0;
        add_Finder(test_blk, test_blk->pos);

        fprintf(outfile16, "target blk %p\n", test_blk);
        test_blk->pos =1;
        add_Finder(test_blk, test_blk->pos);

        fprintf(outfile16, "target blk %p\n", test_blk);
        test_blk->pos =2;
        add_Finder(test_blk, test_blk->pos);

        fprintf(outfile16, "target blk %p\n", test_blk);
        test_blk->pos =3;
        add_Finder(test_blk, test_blk->pos);

        struct node *victim_node = NULL;
        victim_node = get_victim_node_from_Finder();
        fprintf(outfile19, "victim blk %p\n", victim_node->blk);
        Finder_record();
    }

    while (1) {
    	// printf("n->num_poller %d\n",n->num_poller);
        for (i = 1; i <= n->num_poller; i++) {
            if (!ssd->to_ftl[i] || !femu_ring_count(ssd->to_ftl[i]))
                continue;

            rc = femu_ring_dequeue(ssd->to_ftl[i], (void *)&req, 1);
            fprintf(outfile10, "before rc = %d\n", rc);
            if (rc != 1) {
                printf("1333 error\n");
                printf("FEMU: FTL to_ftl dequeue failed\n");
            }

            ftl_assert(req);
            
            switch (req->cmd.opcode) {
            case NVME_CMD_WRITE:
                lat = ssd_write(ssd, req);
                fprintf(outfile10, "cmd write, lat= %ld\n",lat);
                request_trim = false;
                break;
            case NVME_CMD_READ:
                lat = ssd_read(ssd, req);
                fprintf(outfile10, "cmd read, lat= %ld\n",lat);
                request_trim = false;
                break;
            case NVME_CMD_DSM:
                //printf("ftl_thread NVME_CMD_DSM\n");
                lat = ssd_dsm(ssd, req);
                fprintf(outfile10, "cmd trim, lat= %ld\n",lat);
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
            fprintf(outfile10, "req->reqlat= %ld\n",req->reqlat);
            fprintf(outfile10, "req->expire_time= %ld\n",req->expire_time);
            rc = femu_ring_enqueue(ssd->to_poller[i], (void *)&req, 1);
            fprintf(outfile10, "after rc = %d\n", rc);
            if (rc != 1) {
                printf("1364 error\n");
                ftl_err("FTL to_poller enqueue failed\n");
            }

            /* clean one line if needed (in the background) */
            if(!request_trim) {
            	if (should_gc_sublk(ssd)) {
            		fprintf(outfile7,"%"PRIu64" %d %d\n",req->slba, req->cmd.opcode, ssd->lm.free_line_cnt);
                	do_gc(ssd, false, req); // default false
            	}
            }    
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
    fclose(outfile10);
    fclose(outfile11);
    fclose(outfile12);
    fclose(outfile13);
    fclose(outfile14);
    fclose(outfile16);
    fclose(outfile17);
    fclose(outfile18);
    fclose(outfile19);

    return NULL;
}

