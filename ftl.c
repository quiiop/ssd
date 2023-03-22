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
const char* fileName20 = "log.txt";
const char* fileName21 = "lba.txt";
const char* fileName22 = "hot_cold_record.txt";
const char* fileName23 = "test_case_finder.txt";

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
FILE *outfile20 = NULL;
FILE *outfile21 = NULL;
FILE *outfile22 = NULL;
FILE *outfile23 = NULL;

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


static inline bool should_gc_sublk(struct ssd *ssd)
{
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
    spp->sublk_gc_thres_percent = 0.7;
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
    pg->attribute = PG_COLD;
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
    subblk->was_full = sublk_not_full;
    subblk->was_victim = sublk_not_victim;
}

static void ssd_init_nand_blk(struct nand_block *blk, struct ssdparams *spp)
{
    blk->nsubblks = spp->subblks_per_blk;
    blk->subblk = g_malloc0(sizeof(struct nand_subblock) * blk->nsubblks);
    blk->pos = -1;
    blk->full_sublk = 0;
    blk->invalid_sublk = 0;

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
   
   for (int blk=0; blk<blks; blk++){
        for (int sublk=0; sublk<sublks; sublk++){
            for (int page=0; page<pages; page++){
                for (int lun=0; lun<luns; lun++){
                    for (int ch=0; ch<chs; ch++){

                        struct ppa *ppa = malloc(sizeof(struct ppa));
                        ppa->g.pg = page;
                        ppa->g.subblk = sublk;
                        ppa->g.blk = blk;
                        ppa->g.pl = 0;
                        ppa->g.lun = lun;
                        ppa->g.ch = ch;
                        
                        /*
                        if (ch==0 || ch==1){
                            if (lun==0 || lun==1){
                                if (blk==0){
                                    ppa->was_hot_data = 1; // 0 is cold data, 1 is hot data
                                }
                            }
                        }
                        */

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
    // printf("630 node \n");
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

static void print_link(struct link *list, FILE *temp){ 
    struct node *current = list->head;
    current = list->head;
    // printf("659\n");
    fprintf(temp, "Link id %d : ", list->id);

    if (list->head == NULL){
        fprintf(temp, "list empty\n");
    }else{
        while (1){
            if (current->next == NULL){
            break;
            }
            fprintf(temp, "[ Block id %d , addr %p ] -> ", current->blk->blk, current->blk);
            current = current->next;
        }
        fprintf(temp, "[ Block id %d , addr %p ] -> NULL\n", current->blk->blk, current->blk);
        // fprintf(temp, "---------------------------\n");
    }
}

static void Finder_record(FILE *temp) {
    // printf("678\n");
    fprintf(temp, "Finder Record :\n");
    for (int i=0; i<16; i++){
        print_link(finder->list[i], temp);
    }
    fprintf(temp, " \n");
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
    // print_link(list);
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
    // printf("add Finder, pos= %d\n", pos);
    fprintf(outfile17, "blk %p add Finder, pos= %d\n", blk, blk->pos);
    if (pos == 0){ // block first time add Finder
        struct node *n = init_node(blk);
        // printf("744 \n");
        add_link(finder->list[pos], n);
        // printf("746 \n");
    }else{
        struct node **target = malloc(sizeof(struct node *));
        struct node *n;
        int last_pos = pos-1;

        // printf("751 \n");
        from_link_get_node(finder->list[last_pos], blk, target);
        // printf("754 \n");
        n = *target;
        // printf("756 \n");
        add_link(finder->list[pos], n);
        // printf("758 \n");
    }
    
}

static struct node* get_victim_node_from_Finder(void){
    struct node *victim_node = NULL;
    int pos = -1;

    for (int i=finder->size-1; i>=0; i--){
        if (finder->list[i]->head != NULL){
            pos = i;
            break;
        }
    }
    
    if (pos == -1){
        printf("err Finder is empty\n");
        fprintf(outfile9, "err Finder is empty\n");
        return NULL;
    }else{
        victim_node = get_victim_node(finder->list[pos]);
        fprintf(outfile9, "GC from link %d , victim blk %p\n", pos, victim_node->blk);
        return victim_node;
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

    /* init Empty_page_queue */
    Empty_page_queue = malloc(sizeof(struct ppa_queue));
    TAILQ_INIT(Empty_page_queue);
    init_Empty_page_queue(spp);
    
    /* init Finder */
    int nsublks = spp->subblks_per_blk;
    outfile15  = fopen(fileName15, "wb");
    init_Finder(nsublks);
    fclose(outfile15);

    outfile20 = fopen(fileName20, "wb");
    fprintf(outfile20, "ssd tt pg %d , gc thres pg %d\n",ssd->sp.tt_pgs ,ssd->sp.sublk_gc_thres_pgs);
    fprintf(outfile20, "ssd tt_blks %d , tt_luns %d , tt chs %d\n",ssd->sp.tt_blks , ssd->sp.tt_luns, ssd->sp.nchs);
    fprintf(outfile20, "ssd pgs_per_pl %d , pgs_per_lun %d , pgs_per_ch %d\n",ssd->sp.pgs_per_pl , ssd->sp.pgs_per_lun, ssd->sp.pgs_per_ch);
    fclose(outfile20);

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
    struct ssdparams *spp = &ssd->sp;
    struct nand_subblock *subblk = NULL;
    struct nand_page *pg = NULL;
    struct nand_block *blk = get_blk(ssd, ppa);

    /* update corresponding page status */
    pg = get_pg(ssd, ppa);
    ftl_assert(pg->status == PG_VALID);
    pg->status = PG_INVALID;
    
    ssd->sp.invalid_page++;
    tt_ipc = tt_ipc +1;

    ssd->sp.valid_page--;
    tt_vpc = tt_vpc -1;
    
    // printf("1021\n");
    /* update corresponding block status */ 
    subblk = get_subblk(ssd, ppa);
    ftl_assert(subblk->ipc >= 0 && subblk->ipc < spp->pgs_per_subblk);
    subblk->ipc = subblk->ipc +1;

    ftl_assert(subblk->vpc > 0 && subblk->vpc <= spp->pgs_per_subblk);
    subblk->vpc = subblk->vpc -1;
    int sublk_total_page = subblk->ipc + subblk->vpc; 
    // printf("mark page invalid , sublk %p\n", subblk);
    // printf("ch %d , lun %d , pl %d , blk %d , sublk %d , pg %d\n", ppa->g.ch, ppa->g.lun, ppa->g.pl, ppa->g.blk, ppa->g.subblk, ppa->g.pg);
    // printf("ipc %d vpc %d tt %d \n", subblk->ipc, subblk->vpc, sublk_total_page);
    // printf("blk invalid sublk %d\n", blk->invalid_sublk);
    // printf("------------------\n");

    /*
    if (req->cmd.opcode == NVME_CMD_DSM){
        fprintf(outfile17, "Trim mark page invalid , sublk %p\n", subblk);
        fprintf(outfile17, "ch %d , lun %d , pl %d , blk %d , sublk %d , pg %d\n", ppa->g.ch, ppa->g.lun, ppa->g.pl, ppa->g.blk, ppa->g.subblk, ppa->g.pg);
        fprintf(outfile17, "ipc %d vpc %d tt %d \n", subblk->ipc, subblk->vpc, sublk_total_page);
        fprintf(outfile17, "blk invalid sublk %d\n", blk->invalid_sublk);
    }else{
        fprintf(outfile17, "mark page invalid , sublk %p\n", subblk);
        fprintf(outfile17, "ch %d , lun %d , pl %d , blk %d , sublk %d , pg %d\n", ppa->g.ch, ppa->g.lun, ppa->g.pl, ppa->g.blk, ppa->g.subblk, ppa->g.pg);
        fprintf(outfile17, "ipc %d vpc %d tt %d \n", subblk->ipc, subblk->vpc, sublk_total_page);
        fprintf(outfile17, "blk invalid sublk %d\n", blk->invalid_sublk);
    }
    */
    fprintf(outfile17, "mark page invalid , sublk %p\n", subblk);
    fprintf(outfile17, "ch %d , lun %d , pl %d , blk %d , sublk %d , pg %d\n", ppa->g.ch, ppa->g.lun, ppa->g.pl, ppa->g.blk, ppa->g.subblk, ppa->g.pg);
    fprintf(outfile17, "ipc %d vpc %d tt %d \n", subblk->ipc, subblk->vpc, sublk_total_page);
    fprintf(outfile17, "blk invalid sublk %d\n", blk->invalid_sublk);

    // sublk add Finder , if need
    if (sublk_total_page == spp->pgs_per_subblk){
        int nsublk = spp->subblks_per_blk;
        int count = 0;
        subblk->was_full = sublk_full; // sublk was full

        for (int i=0; i<nsublk; i++){
            if (blk->subblk[i].was_full == sublk_full){
                count = count + 1;
            }
        }
        blk->full_sublk = count;
        fprintf(outfile17, "1092 blk %p , full sublk %d\n", blk, blk->full_sublk);
        fprintf(outfile17, "1093 sublk %p,  ipc %d , vpc %d\n", subblk, subblk->ipc, subblk->vpc);

        if (subblk->ipc >= 10){
            int count2 = 0;
            subblk->was_victim = sublk_victim; // sublk was victim

            for (int i=0; i<nsublk; i++){
                if (blk->subblk[i].was_victim == sublk_victim){
                    count2 = count2 +1;
                }
            }
            blk->invalid_sublk = count2;
            int temp = blk->invalid_sublk -1;
            fprintf(outfile17, "1106 blk pos %d , invalid sublk %d , temp %d\n", blk->pos, blk->invalid_sublk, temp);

            if (blk->pos == temp){
                /* continus */
            }else{
                blk->ch = ppa->g.ch;
                blk->lun = ppa->g.lun;
                blk->pl = ppa->g.pl;
                blk->blk = ppa->g.blk;
                blk->pos = blk->invalid_sublk -1;
                // printf("1059\n");
                add_Finder(blk, blk->pos);
                // printf("1061\n");
            }
        }
    }
    fprintf(outfile17, "------------------\n");
}

static void mark_page_valid(struct ssd *ssd, struct ppa *ppa)
{
    struct nand_subblock *subblk = NULL;
    struct nand_page *pg = NULL;
    struct ssdparams *spp = &ssd->sp;
    struct nand_block *blk = get_blk(ssd, ppa);
    // struct line *line;
    // printf("1073\n");

    /* update page status */
    pg = get_pg(ssd, ppa);
    // printf("vaild pg %d\n", ppa->g.pg);
    ftl_assert(pg->status == PG_FREE);
    pg->status = PG_VALID;

    /* update corresponding block status */
    subblk = get_subblk(ssd, ppa);
    ftl_assert(subblk->vpc >= 0 && subblk->vpc < ssd->sp.pgs_per_subblk);
    subblk->vpc = subblk->vpc +1;

    int sublk_total_page = subblk->ipc + subblk->vpc; 
    fprintf(outfile17, "mark page valid , sublk %p\n", subblk);
    fprintf(outfile17, "ch %d , lun %d , pl %d , blk %d , sublk %d , pg %d\n", ppa->g.ch, ppa->g.lun, ppa->g.pl, ppa->g.blk, ppa->g.subblk, ppa->g.pg);
    fprintf(outfile17, "ipc %d vpc %d tt %d \n", subblk->ipc, subblk->vpc, sublk_total_page);

    // sublk add Finder , if need
    if (sublk_total_page == spp->pgs_per_subblk){
        int nsublk = spp->subblks_per_blk;
        int count = 0;
        subblk->was_full = sublk_full; // sublk was full

        for (int i=0; i<nsublk; i++){
            if (blk->subblk[i].was_full == sublk_full){
                count = count + 1;
            }
        }
        blk->full_sublk = count;
        fprintf(outfile17, "1162 blk %p , full sublk %d\n", blk, blk->full_sublk);
        fprintf(outfile17, "1163 sublk %p,  ipc %d , vpc %d\n", subblk, subblk->ipc, subblk->vpc);

        if (subblk->ipc >= 10){
            int count2 = 0;
            subblk->was_victim = sublk_victim; // sublk was victim

            for (int i=0; i<nsublk; i++){
                if (blk->subblk[i].was_victim == sublk_victim){
                    count2 = count2 +1;
                }
            }
            blk->invalid_sublk = count2;
            int temp = blk->invalid_sublk -1;
            fprintf(outfile17, "1176 blk pos %d , invalid sublk %d , temp %d\n", blk->pos, blk->invalid_sublk, temp);

            if (blk->pos == temp){
                /* continus */
            }else{
                blk->ch = ppa->g.ch;
                blk->lun = ppa->g.lun;
                blk->pl = ppa->g.pl;
                blk->blk = ppa->g.blk;
                blk->pos = blk->invalid_sublk -1;
                // printf("1059\n");
                add_Finder(blk, blk->pos);
                // printf("1061\n");
            }
        }
    }
    fprintf(outfile17, "------------------\n");
    ssd->sp.valid_page++;
    tt_vpc = tt_vpc +1;
    
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
        pg->attribute = PG_COLD;
        insert_page_in_queue(ppa);
    }

    /* reset block status */
    ftl_assert(subblk->npgs == spp->pgs_per_subblk);
    subblk->ipc = 0;
    subblk->vpc = 0;
    subblk->erase_cnt++;
    subblk->was_full = sublk_not_full;
    subblk->was_victim = sublk_not_victim;
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
static uint64_t gc_write_page(struct ssd *ssd, struct ppa *old_ppa, struct ppa *save_hot_data_ppa)
{
    struct ppa new_ppa;
    struct ppa *first;
    uint64_t lpn = get_rmap_ent(ssd, old_ppa);

    ftl_assert(valid_lpn(ssd, lpn));
    if (save_hot_data_ppa == NULL){
        first = get_empty_page();
    }else{
        first = save_hot_data_ppa;
    }
    new_ppa = *first;

    /* update maptbl */
    set_maptbl_ent(ssd, lpn, &new_ppa);
    /* update rmap */
    set_rmap_ent(ssd, lpn, &new_ppa);

    mark_page_valid(ssd, &new_ppa);

    /* need to advance the write pointer here */
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

static struct nand_block *find_block(struct ssd *ssd) // save hot data
{
    struct ssdparams *spp = &ssd->sp;

    for (int i=finder->size-2; i>=0; i--){
        if (finder->list[i]->head != NULL){
            struct node *current = finder->list[i]->head;

            while (1){
                struct nand_block *blk = current->blk;

                if (blk->full_sublk < spp->subblks_per_blk){
                    return blk;
                }
                if (current->next == NULL){
                    break;
                }
                current = current->next;
            }
        }
    }
    return NULL;
}

static struct ppa *find_ppa(struct ssd *ssd, struct nand_block *blk)
{
    struct ssdparams *spp = &ssd->sp;
    struct ppa *ppa = malloc(sizeof(struct ppa));
    ppa->g.ch = blk->ch;
    ppa->g.lun = blk->lun;
    ppa->g.pl = blk->pl;
    ppa->g.blk = blk->blk;

    for (int i=spp->subblks_per_blk-1; i>=0; i--){
        ppa->g.subblk = i;
        struct nand_subblock *sublk = get_subblk(ssd, ppa);
        if (sublk->was_full != sublk_full){
            for (int j=spp->pgs_per_subblk-1; j>=0; j--){
                ppa->g.pg = j;
                struct nand_page *pg = get_pg(ssd, ppa);
                if (pg->status == PG_FREE){
                    return ppa;
                }
            }
        }

    }
    return NULL;
}

/* here ppa identifies the block we want to clean */
static void clean_one_subblock(struct ssd *ssd, struct ppa *ppa, NvmeRequest *req)
{
    struct ssdparams *spp = &ssd->sp;
    struct nand_page *pg_iter = NULL;
    int cnt = 0; // count one block vaild page
    // int invaild_cnt = 0; // count one block invaild page
    
    for (int pg = 0; pg < spp->pgs_per_subblk; pg++) {
        ppa->g.pg = pg;
        pg_iter = get_pg(ssd, ppa);
        /* there shouldn't be any free page in victim blocks */
        ftl_assert(pg_iter->status != PG_FREE);

        if (pg_iter->status == PG_VALID) {
            fprintf(outfile22, "pg atrribute %d\n", pg_iter->attribute);
            if (pg_iter->attribute == PG_HOT){
                printf("1190\n");
                struct nand_block *blk = find_block(ssd);
                fprintf(outfile22, "pg hot , attribute %d\n", pg_iter->attribute);
                fprintf(outfile22, "find blk %p , id %d\n", blk, blk->blk);
                printf("1193\n");

                if (blk==NULL){
                    printf("1192 blk err\n");
                    fprintf(outfile22, "blk is NULL\n");
                }
                struct ppa *new_ppa = find_ppa(ssd, blk);
                fprintf(outfile22, "find ppa : ch %d , lun %d , pl %d , blk %d , sublk %d , pg %d\n", new_ppa->g.ch, new_ppa->g.lun, new_ppa->g.pl, new_ppa->g.blk, new_ppa->g.subblk, new_ppa->g.pg);
                printf("1191\n");
                
                gc_read_page(ssd, ppa);
                gc_write_page(ssd, ppa, new_ppa);
            }else{ 
                struct ppa *new_ppa = NULL;
                fprintf(outfile22, "pg cold , attribute %d\n", pg_iter->attribute);

                gc_read_page(ssd, ppa);
                gc_write_page(ssd, ppa, new_ppa);
            }
            cnt = cnt+1;
            live_page_copy_cnt++;
            ssd->sp.valid_page--;
            tt_vpc = tt_vpc -1;
        }else{
            ssd->sp.invalid_page--;
            tt_ipc = tt_ipc -1;
        }
    }
    fprintf(outfile9, "gc additional writer pg %d\n", cnt);
    // printf("1192 clean one sublk ipc %d\n", tt_ipc);
    fprintf(outfile9, "1192 tt_ipc %d\n", tt_ipc);

    // invaild_cnt = spp->pgs_per_subblk - cnt;
    // fprintf(outfile4,"%d %d %d\n",req->cmd.opcode,cnt,invaild_cnt);// valid , inalid
    ftl_assert(get_subblk(ssd, ppa)->vpc == cnt);
}

static int do_gc(struct ssd *ssd, bool force, NvmeRequest *req)
{
    struct ssdparams *spp = &ssd->sp;
    struct ppa ppa;
    int sublk;
    struct node *victim_node = NULL;
    struct nand_block *victim_blk = NULL;
    struct nand_subblock *victim_sublk = NULL;
    int vic_sublk_count = 0;

    printf("do_gc\n");
    Finder_record(outfile18);
    victim_node = get_victim_node_from_Finder();

    if (victim_node == NULL){
        printf("do gc no victim node\n");
        fprintf(outfile9, "do gc no victim node\n");
        fprintf(outfile9, " \n");
        return -1;
    };
    // printf("do gc\n");

    victim_blk = victim_node->blk;
    ppa.g.ch = victim_blk->ch;
    ppa.g.lun = victim_blk->lun;
    ppa.g.pl = victim_blk->pl;
    ppa.g.blk = victim_blk->blk;

    /* copy back valid data */
    fprintf(outfile9, "GC blk %p : pos= %d , invalid sub= %d , full sub= %d\n",victim_blk ,victim_blk->pos ,victim_blk->invalid_sublk, victim_blk->full_sublk);

    for (sublk=0; sublk<spp->subblks_per_blk; sublk++){
        ppa.g.subblk = sublk;
        victim_sublk = get_subblk(ssd, &ppa);
        fprintf(outfile9, "GC sublk %p : id %d ipc %d , vpc %d\n",victim_sublk ,sublk ,victim_sublk->ipc ,victim_sublk->vpc);

        if (victim_sublk->ipc >= 10){
            clean_one_subblock(ssd, &ppa, req);
            mark_subblock_free(ssd, &ppa);
            vic_sublk_count = vic_sublk_count +1;
        }
    }

    victim_blk->invalid_sublk = victim_blk->invalid_sublk - vic_sublk_count;
    victim_blk->full_sublk = victim_blk->full_sublk - vic_sublk_count;
    victim_blk->pos = victim_blk->pos -vic_sublk_count;

    fprintf(outfile9, "GC over blk %p : invalid sublk= %d , full sub= %d\n",victim_blk ,victim_blk->invalid_sublk ,victim_blk->full_sublk);
    fprintf(outfile9, " \n");
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
            struct nand_page *pg2 = get_pg(ssd, &ppa);
            if (pg2->status != PG_INVALID){
                mark_page_invalid(ssd, &ppa, req);
            }

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
               struct nand_page *pg2 = get_pg(ssd, &ppa);
                if (pg2->status != PG_INVALID){
                    mark_page_invalid(ssd, &ppa, req);
                }
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

/*
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
*/

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

/**
static void test_case()
{
    printf("1560\n");
    outfile18 = fopen(fileName18, "wb");

    struct ppa *test_pg = get_empty_page();
    printf("1564\n");
    struct nand_block *test_blk = get_blk(ssd, test_pg);
    printf("1566\n");
    struct ppa ppa;
        
    test_blk->ch = test_pg->g.ch;
    test_blk->lun = test_pg->g.lun;
    test_blk->pl = test_pg->g.pl;
    test_blk->blk = test_pg->g.blk;
    printf("1573\n");

    ppa.g.ch = test_blk->ch;
    ppa.g.lun = test_blk->lun;
    ppa.g.pl = test_blk->pl;
    ppa.g.blk = test_blk->blk;
    printf("1573 \n");
    int cnt=0;

    for(int i=0; i<16; i++) {
        ppa.g.subblk = i;
        // struct nand_subblock *temp_sublk = get_subblk(ssd, &ppa);
            
        if (i<10){
            for(int j=0; j<16; j++) {
                ppa.g.pg = j;
                mark_page_valid(ssd, &ppa);
                cnt = cnt+1;
                if (j < 13){
                    mark_page_invalid(ssd, &ppa, req);
                    cnt = cnt+1;
                }
            }
        }else{
            for(int j=0; j<16; j++) {
                ppa.g.pg = j;
                mark_page_valid(ssd, &ppa);
                cnt = cnt+1;
            }
        }
    }

    test_pg = get_empty_page();
    printf("1564\n");
    test_blk = get_blk(ssd, test_pg);
    printf("1566\n");
        
    test_blk->ch = test_pg->g.ch;
    test_blk->lun = test_pg->g.lun;
    test_blk->pl = test_pg->g.pl;
    test_blk->blk = test_pg->g.blk;
    printf("1573\n");

    ppa.g.ch = test_blk->ch;
    ppa.g.lun = test_blk->lun;
    ppa.g.pl = test_blk->pl;
    ppa.g.blk = test_blk->blk;
    printf("1573 \n");
        
    for(int i=0; i<16; i++) {
        ppa.g.subblk = i;
        // struct nand_subblock *temp_sublk = get_subblk(ssd, &ppa);
            
        if (i<5){
            for(int j=0; j<16; j++) {
                ppa.g.pg = j;
                mark_page_valid(ssd, &ppa);
                cnt = cnt+1;
                if (j < 13){
                    mark_page_invalid(ssd, &ppa, req);
                    cnt = cnt+1;
                }
            }
        }else{
            for(int j=0; j<16; j++) {
                ppa.g.pg = j;
                mark_page_valid(ssd, &ppa);
                cnt = cnt+1;
            }
        }
    }

    fprintf(outfile9, "total pg %d\n", cnt);
    Finder_record(outfile18);

    // struct node *victim_node = NULL;
    printf("1572\n");
    // victim_node = get_victim_node_from_Finder();
    // fprintf(outfile16, "victim blk %p\n", victim_node->blk);
    // printf("1575\n");
    printf("1587\n");
    int r = do_gc(ssd, true, req);
    printf("1590 r = %d\n", r);
    Finder_record(outfile18);
    r = do_gc(ssd, true, req);
    printf("1730 r = %d\n", r);
    Finder_record(outfile18);
}
*/


static void test_case_2(struct ssd *ssd, NvmeRequest *req)
{
    
    outfile22 = fopen(fileName22, "wb");
    outfile23 = fopen(fileName23, "wb");

    struct ppa *test_pg = get_empty_page();
    struct nand_block *test_blk = get_blk(ssd, test_pg);
    printf("1566\n");
    test_blk->ch = test_pg->g.ch;
    test_blk->lun = test_pg->g.lun;
    test_blk->pl = test_pg->g.pl;
    test_blk->blk = test_pg->g.blk;
    printf("1573\n");
    struct ppa ppa;
    ppa.g.ch = test_blk->ch;
    ppa.g.lun = test_blk->lun;
    ppa.g.pl = test_blk->pl;
    ppa.g.blk = test_blk->blk;
    printf("1666\n");

    for(int i=0; i<16; i++) {
        ppa.g.subblk = i;
        printf("1670\n");
        if (i<10){
            for(int j=0; j<16; j++) {
                printf("1673\n");
                ppa.g.pg = j;
                struct nand_page *pg = get_pg(ssd, &ppa);

                mark_page_valid(ssd, &ppa);
                pg->attribute = PG_HOT;
                if (j < 13){
                    printf("1681\n");
                    mark_page_invalid(ssd, &ppa, req);
                    printf("1683\n");
                }
            }
        }else{
            for(int j=0; j<16; j++) {
                printf("1685\n");
                ppa.g.pg = j;
                struct nand_page *pg = get_pg(ssd, &ppa);

                if (j < 8){
                    printf("1688\n");
                    pg->attribute = PG_HOT;
                }else{
                    printf("1691\n");
                    pg->attribute = PG_COLD;
                }
                mark_page_valid(ssd, &ppa);
            }
        }
    }
    printf("1693\n");
    fprintf(outfile22, "blk 1 , invalid sublk= %d , full sublk= %d\n", test_blk->invalid_sublk, test_blk->full_sublk);

    test_pg = get_empty_page();
    test_blk = get_blk(ssd, test_pg);
    printf("1698\n");
    test_blk->ch = test_pg->g.ch;
    test_blk->lun = test_pg->g.lun;
    test_blk->pl = test_pg->g.pl;
    test_blk->blk = test_pg->g.blk;
    printf("1703\n");
    ppa.g.ch = test_blk->ch;
    ppa.g.lun = test_blk->lun;
    ppa.g.pl = test_blk->pl;
    ppa.g.blk = test_blk->blk;
    printf("1708\n");
    
    for(int i=0; i<16; i++) {
        ppa.g.subblk = i;
        printf("1670\n");
        if (i<10){
            for(int j=0; j<16; j++) {
                printf("1673\n");
                ppa.g.pg = j;
                struct nand_page *pg = get_pg(ssd, &ppa);

                mark_page_valid(ssd, &ppa);
                pg->attribute = PG_HOT;
                if (j < 13){
                    printf("1681\n");
                    mark_page_invalid(ssd, &ppa, req);
                    printf("1683\n");
                }
            }
        }else{
            for(int j=0; j<16; j++) {
                printf("1685\n");
                ppa.g.pg = j;
                struct nand_page *pg = get_pg(ssd, &ppa);
                
                if (j<=14){
                    pg->attribute = PG_HOT;
                    mark_page_valid(ssd, &ppa);
                }
            }
        }
    }
    printf("1745\n");
    fprintf(outfile22, "blk 2 , invalid sublk= %d , full sublk= %d\n", test_blk->invalid_sublk, test_blk->full_sublk);

    test_pg = get_empty_page();
    test_blk = get_blk(ssd, test_pg);
    printf("1698\n");
    test_blk->ch = test_pg->g.ch;
    test_blk->lun = test_pg->g.lun;
    test_blk->pl = test_pg->g.pl;
    test_blk->blk = test_pg->g.blk;
    printf("1703\n");
    ppa.g.ch = test_blk->ch;
    ppa.g.lun = test_blk->lun;
    ppa.g.pl = test_blk->pl;
    ppa.g.blk = test_blk->blk;
    printf("1708\n");

    for(int i=0; i<16; i++) {
        ppa.g.subblk = i;

        if (i<6){
            for(int j=0; j<16; j++) {
                ppa.g.pg = j;
                struct nand_page *pg = get_pg(ssd, &ppa);
                mark_page_valid(ssd, &ppa);

                pg->attribute = PG_HOT;
                if (j < 13){
                    pg->attribute = PG_COLD;
                    mark_page_invalid(ssd, &ppa, req);
                }
            }
        }else{
            for(int j=0; j<16; j++) { // not full 
                if (j < 2){
                    ppa.g.pg = j;
                    struct nand_page *pg = get_pg(ssd, &ppa);
                    mark_page_valid(ssd, &ppa);

                    pg->attribute = PG_HOT;
                }else{
                    
                }
            }
        }
    }
    fprintf(outfile22, "blk 3 , invalid sublk= %d , full sublk= %d\n", test_blk->invalid_sublk, test_blk->full_sublk);

    Finder_record(outfile23);

    /*
    printf("1756\n");
    struct nand_block *t_blk = find_block(ssd);
    printf("1757\n");
    fprintf(outfile22, "find blk %p , id %d\n", t_blk, t_blk->blk);
    printf("1760\n");
    struct ppa *new_ppa = find_ppa(ssd, t_blk);
    fprintf(outfile22, "find ppa : ch %d , lun %d , pl %d , blk %d , sublk %d , pg %d\n", new_ppa->g.ch, new_ppa->g.lun, new_ppa->g.pl, new_ppa->g.blk, new_ppa->g.subblk, new_ppa->g.pg);
    
    struct nand_page *new_pg = get_pg(ssd, new_ppa);
    if (new_pg->status == PG_FREE){
        fprintf(outfile22, "pg free\n");
    }else{
        fprintf(outfile22, "pg not free\n");
    }
    */

    int r;
    r = do_gc(ssd, true, req);
    printf("r %d\n", r);

    fclose(outfile22);
    fclose(outfile23);
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
    outfile10 = fopen(fileName10, "wb");
    outfile11 = fopen(fileName11, "wb");
    outfile12 = fopen(fileName12, "wb");
    outfile13 = fopen(fileName13, "wb");
    outfile16 = fopen(fileName16, "wb");
    outfile17 = fopen(fileName17, "wb");
    outfile18 = fopen(fileName18, "wb");
    outfile19 = fopen(fileName19, "wb");
    outfile21 = fopen(fileName21, "wb");
    
    

    while (!*(ssd->dataplane_started_ptr)) {
        usleep(100000);
    }

    /* FIXME: not safe, to handle ->to_ftl and ->to_poller gracefully */
    ssd->to_ftl = n->to_ftl;
    ssd->to_poller = n->to_poller;

    /* test Finder */ 
    if (test_count == 0){
        test_case_2(ssd, req);
        test_count = test_count +1;
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
                fprintf(outfile21, "write : slba= %lu , offset= %d\n", req->slba, req->nlb);
                lat = ssd_write(ssd, req);
                fprintf(outfile10, "cmd write, lat= %ld\n",lat);
                request_trim = false;
                break;
            case NVME_CMD_READ:
                fprintf(outfile21, "read : slba= %lu , offset= %d\n", req->slba, req->nlb);
                lat = ssd_read(ssd, req);
                fprintf(outfile10, "cmd read, lat= %ld\n",lat);
                request_trim = false;
                break;
            case NVME_CMD_DSM:
                fprintf(outfile21, "trim : slba= %lu , offset= %d\n", req->slba, req->nlb);
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
    fclose(outfile21);

    return NULL;
}

