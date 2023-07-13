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
const char* fileName24 = "free_sublk.txt";
const char* fileName25 = "testA.txt";
const char* fileName26 = "test_case_3.txt";

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
FILE *outfile24 = NULL;
FILE *outfile25 = NULL;
FILE *outfile26 = NULL;

//#define FEMU_DEBUG_FTL

static float table_cnt_avg = 0;
static bool wp_2 = false;
static uint64_t read_cnt = 0;
static uint64_t write_cnt = 0;
static uint64_t live_page_copy_cnt = 0;
static uint64_t gcc_count = 0;
static uint64_t page_write_count = 0;
static uint64_t page_trim_count = 0;
static int mark_page_invalid_count = 0;

/* Empty Sublk Queue */
TAILQ_HEAD(sublk_queue, ppa);
struct sublk_queue *Empty_sublk_queue = NULL; 

/* Empty Sublk Queue */
TAILQ_HEAD(sublk_queue_2, ppa);
struct sublk_queue *Temp_sublk_queue = NULL; 

/* Finder */
static struct Finder *finder=NULL;
static int tt_ipc =0;
static int tt_vpc =0;
static int test_count=0;

/* Finder2 */
struct Finder2 *finder2=NULL;

/* Free_Block_Management */
struct Queue *Free_Block_Management=NULL;
struct Queue *Temp_Block_Management=NULL; //用來暫存尚未存到Finder2的Block


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

/* Queue Operation */
static int Push(struct Queue *queue, struct nand_block *blk)
{
    struct node *n = init_node(blk);

    if (queue->head == NULL){
        queue->head = n;
        queue->tail = n;
        queue->Queue_Size++;
        return Successful;
    }else{
        queue->tail->next = n;
        queue->tail = n;
        queue->Queue_Size++;
        return Successful;
    }
}

static int Pop(struct Queue *queue)
{
    struct node *victim_node = NULL;
    if (queue->head == NULL){
        return Fail;
    }else{
        victim_node = queue->head;
        queue->head = queue->head->next;
        queue->Queue_Size--;
        free(victim_node);
        return Successful;
    }
}

static int Size(struct Queue *queue)
{
    return queue->Queue_Size;
}

static struct nand_block *Peek(struct Queue *queue)
{
    struct node *victim_node = NULL;
    if (queue->head == NULL){
        return NULL;
    }else{
        victim_node = queue->head;
        return victim_node->blk;
    }
}

static void show(struct Queue *queue)
{
    struct node *current;
    printf("Queue Size %d \n", queue->Queue_Size);
    printf("Queue id %d :", queue->id);
    for (current=queue->head; current!=NULL; current=current->next){
        printf(" Block %d , ", current->blk->id);
    }
    printf("\n");
}

struct Queue *init_Queue(int id)
{
    struct Queue *queue = malloc(sizeof(struct Queue));
    queue->id = id;
    queue->Queue_Size = 0;
    queue->head = NULL;
    queue->tail = NULL;
    return queue;
}
/* Queue Operation Over */

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
    pg->Hot_level = Hot_level_0;
    pg->pg_type = PG_Empty;
}

static void ssd_init_nand_subblk(struct nand_subblock *subblk, struct ssdparams *spp, int ch_id, int lun_id, int pl_id, int blk_id, int sublk_id, FILE *outfile)
{
    subblk->npgs = spp->pgs_per_subblk;
    subblk->pg = g_malloc0(sizeof(struct nand_page) * subblk->npgs);
    subblk->ipc = 0;
    subblk->vpc = 0;
    subblk->epc = spp->pgs_per_subblk;
    subblk->erase_cnt = 0;
    subblk->wp = 0;
    subblk->was_full = SUBLK_NOT_FULL;
    subblk->was_victim = SUBLK_NOT_VICTIM;
    subblk->Current_Hot_Level = Hot_level_0;
    subblk->current_page_id = 0;
    
    for (int i = 0; i < subblk->npgs; i++) {
        ssd_init_nand_page(&subblk->pg[i], spp);
    }
    
    subblk->ch = ch_id;
    subblk->lun = lun_id;
    subblk->pl = pl_id;
    subblk->blk = blk_id;
    subblk->sublk = sublk_id;
}

static void ssd_init_nand_blk(struct nand_block *blk, struct ssdparams *spp, int ch_id, int lun_id, int pl_id, int blk_id, FILE *outfile)
{
    /* Set Blk Status */
    blk->nsubblks = spp->subblks_per_blk;
    blk->subblk = g_malloc0(sizeof(struct nand_subblock) * blk->nsubblks);
    blk->current_sublk_id = 0;
    blk->GC_Sublk_Count = 0;
    blk->Not_GC_Sublk_Count = blk->nsubblks;
    blk->In_Finder1_Position = Blk_Not_In_Finder1;
    blk->In_Finder2_Position = Blk_Not_in_Finder2;
    /* Set Blk Info */
    blk->ch = ch_id;
    blk->lun = lun_id;
    blk->pl = pl_id;
    blk->blk = blk_id;

    for (int i = 0; i < blk->nsubblks; i++) {
        ssd_init_nand_subblk(&blk->subblk[i], spp, ch_id, lun_id, pl_id, blk_id, i, outfile);
    }

    /* Set Free Block Management */
    Push(Free_Block_Management, blk);
}

static void ssd_init_nand_plane(struct nand_plane *pl, struct ssdparams *spp, int ch_id, int lun_id, int pl_id, FILE *outfile)
{
    pl->nblks = spp->blks_per_pl;
    pl->blk = g_malloc0(sizeof(struct nand_block) * pl->nblks);
    for (int i = 0; i < pl->nblks; i++) {
        ssd_init_nand_blk(&pl->blk[i], spp, ch_id, lun_id, pl_id, i, outfile);
    }
}

static void ssd_init_nand_lun(struct nand_lun *lun, struct ssdparams *spp, int ch_id, int lun_id, FILE *outfile)
{
    lun->npls = spp->pls_per_lun;
    lun->pl = g_malloc0(sizeof(struct nand_plane) * lun->npls);
    for (int i = 0; i < lun->npls; i++) {
        ssd_init_nand_plane(&lun->pl[i], spp, ch_id, lun_id, i, outfile);
    }
    lun->next_lun_avail_time = 0;
    lun->busy = false;
}

static void ssd_init_ch(struct ssd_channel *ch, struct ssdparams *spp, int ch_id, FILE *outfile)
{
    ch->nluns = spp->luns_per_ch;
    ch->lun = g_malloc0(sizeof(struct nand_lun) * ch->nluns);
    for (int i = 0; i < ch->nluns; i++) {
        ssd_init_nand_lun(&ch->lun[i], spp, ch_id, i, outfile);
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

/* Finder1 init */
static struct node* init_node(struct nand_block *blk)
{
    struct node *n = malloc(sizeof(struct node));
    n->blk = blk;
    n->next = NULL;
    return n;
}

static struct link init_link(int id)
{
    struct link *list = malloc(sizeof(struct link));
    list->id = id;
    list->head = NULL;
    list->tail = NULL;
    return *list;
}

static struct Finder* init_Finder(int size) 
{
    struct Finder *finder = malloc(sizeof(struct Finder));
    
    finder->list = malloc(sizeof(struct link) * size);
    for (int i=0; i<size; i++){
        finder->list[i] = init_link(i);
    }
    finder->Show_Finder = Print_Finder;
    return finder;
}

/* Finder2 init */
static struct Finder2 *init_Finder2(int size)
{
    struct Finder2 *finder2 = malloc(sizeof(struct Finder2));
    finder2->list = malloc(sizeof(struct link) * size);
    for(int i=0; i<size; i++){
        finder2->list[i] = init_link(i);
    }
    finder2->Show_Finder = Print_Finder;
    return finder2;
}

/* Finder Operation*/
static void Add_Link(struct link *list, struct node *n)
{
    struct node *current;
    if (list->head == NULL){
        list->head = n;
    }else{
        for(current=list->head; current->next!=NULL; current=current->next){
            /* do not something */
        }
        current->next = n;
        list->tail = n;
    }
}

static int Remove_Node(struct link *list, struct node *n)
{
    struct node *current;
    struct node *prevoius; 

    int is_find = 0;
    if(list->head == NULL){
        return is_find;
    }
    prevoius = list->head;
    for(current=list->head; current!=NULL; current=current->next){
        if(current->blk == n->blk){
            is_find = 1;
            break;
        }else{
            prevoius = current;
        }
    }
    if(is_find==1){
        if(current==list->head){
            list->head = list->head->next;
            current->next = NULL;
        }else{
            prevoius->next = current->next;
            current->next = NULL;
        }
        free(current);
    }
    return is_find;
}

static int Add_Finder1(struct nand_block *blk, int Old_Position, int New_Position)
{
    struct node *n = init_node(blk);

    if(Old_Position==-1 && New_Position==0){
        Add_Link(&finder->list[New_Position], n);
    }else{
        int r = Remove_Node(&finder->list[Old_Position], n);
        Add_Link(&finder->list[New_Position], n);
    }
    return 1;
}

static int Add_Finder2(struct nand_block *blk, int Old_Hot_Level, int New_Hot_Level)
{
    struct node *n = init_node(blk);
    /* Old_Hot_Level等於-1，表示Block新加入Finder2新加入Finder2 */
    if(Old_Hot_Level==-1){
        Add_Link(&finder2->list[New_Hot_Level], n);
    }else{
        int r = Remove_Node(&finder2->list[Old_Hot_Level], n);
        Add_Link(&finder2->list[New_Hot_Level], n);
    }
    return 1;
}

static void Print_Link(struct link *list)
{
    struct node *current;
    printf("Link %d : ", list->id);
    if(list->head==NULL){
        printf("Empty \n");
    }else{
        for(current=list->head; current->next!=NULL; current=current->next){
            printf("Blk %d -> ", current->blk->id);
        }
        printf("Blk %d -> NULL\n", current->blk->id);  
    }
     
}

static void Print_Finder(int n)
{
    printf("Finder%d :\n", n);
    if(n==1){
        for(int i=0; i<nlinks; i++){
            Print_Link(&finder->list[i]);
        }
    }else{
        for(int i=0; i<nHotLevel; i++){
            Print_Link(&finder2->list[i]);
        }
    }
}

static int Change_Blk_Position_InFinder1(struct nand_block *blk, int New_Position){
    int Old_Position = blk->In_Finder1_Position;
    int r = Add_Finder1(blk, Old_Position, New_Position);
    if(r == 1){
        blk->In_Finder1_Position = New_Position;
    }
}

static int Change_Blk_Position_InFinder2(struct nand_block *blk, int New_Hot_Level)
{
    int Old_Hot_Level = blk->In_Finder2_Position;
    int r = Add_Finder2(blk, Old_Hot_Level, New_Hot_Level);
    if(r==1){
        blk->In_Finder2_Position = New_Hot_Level;
    }
    return r;
}

struct nand_block *Get_Victim_Block(struct ssd *ssd)
{
    struct ssdparams *spp = &ssd->sp;
    int nlinks = spp->subblks_per_blk; //16
    for(int i=nlinks-1; i>=0; i--){
        struct link *list = &finder->list[i];
        if(list->head != NULL){
            struct node *target = list->head;
            struct nand_block *target_block = target->blk;
            list->head = list->head->next;
            free(target);
            target = NULL;
            target_block->In_Finder1_Position = Blk_Not_In_Finder1;
            return target_block;
        }
    }
    return NULL;
}

static int Remove_Block_Finder1(struct nand_block *target_block)
{
    struct node *n = init_node(target_block);
    int Old_Position = target_block->In_Finder1_Position;
    printf("Old_Position %d\n", Old_Position);
    int r = Remove_Node(&finder->list[Old_Position], n);
    printf("r = %d\n", r);
    free(n);
    return r;
}

static int Remove_Block_Finder2(struct nand_block *target_block)
{
    struct node *n = init_node(target_block);
    int Old_Position = target_block->In_Finder2_Position;
    printf("Finder2 Old_Position %d\n", Old_Position);
    int r = Remove_Node(&finder2->list[Old_Position], n);
    printf("r = %d\n", r);
    free(n);
    return r;
}

static int Set_Test(int n)
{
    return n;
}

static void show_info(struct nand_block blk)
{
    printf("Block id %d , ",  blk.id);
    printf("pos %d\n", blk.In_Finder1_Position);
}
/* Finder Operation over */

void ssd_init(FemuCtrl *n)
{
    struct ssd *ssd = n->ssd;
    struct ssdparams *spp = &ssd->sp;

    ftl_assert(ssd);

    ssd_init_params(spp);
    /* init Free Block Management */
    Free_Block_Management = init_Queue(1);

    /* init Temp Block Management */
    Temp_Block_Management = init_Queue(2);

    /* initialize ssd internal layout architecture */
    ssd->ch = g_malloc0(sizeof(struct ssd_channel) * spp->nchs);
    outfile24 = fopen(fileName24, "wb");
    for (int i = 0; i < spp->nchs; i++) {
        ssd_init_ch(&ssd->ch[i], spp, i, outfile24);
    }
    fclose(outfile24);

    /* initialize maptbl */
    ssd_init_maptbl(ssd);

    /* initialize rmap */
    ssd_init_rmap(ssd);

    /* initialize all the lines */
    //ssd_init_lines(ssd);
    
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

    /* init Finder2 */
    int nHotLevels = 4;
    init_Finder2(nHotLevels);

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

static int check(struct ssd *ssd, struct ppa *ppa){
    int count = 0;
    struct nand_page *pg = NULL;
    for (int i=0; i<ssd->sp.pgs_per_subblk; i++){
        ppa->g.pg = i;
        pg = get_pg(ssd, ppa);
        if (pg->status != PG_FREE){
            count = count+1;
        }
    }
    if(count==(ssd->sp.pgs_per_subblk-2)){
        return 0; // remain one pg is free .
    }else{
        return 1;
    }
}

static int Calculate_Sublk_Hot_Level(struct ssd *ssd, struct nand_subblock *sublk, struct ppa *sublk_info)
{
    struct ssdparams *spp = &ssd->sp;
    struct nand_page *pg_iter = NULL;
    struct ppa *temp_ppa = sublk_info;
    int Hot_Level_0_count = 0;
    int Hot_Level_1_count = 0;
    int Hot_Level_2_count = 0;
    int Hot_Level_3_count = 0;

    for(int i=0; i<spp->pgs_per_subblk; i++){
        temp_ppa->g.pg = i;
        pg_iter = get_pg(ssd, temp_ppa);
        if(pg_iter->status == PG_VALID){
            if(pg_iter->Hot_level == Hot_level_0){
                Hot_Level_0_count++;
            }else if(pg_iter->Hot_level == Hot_level_1){
                Hot_Level_1_count++;
            }else if(pg_iter->Hot_level == Hot_level_2){
                Hot_Level_2_count++;
            }else{
                Hot_Level_3_count++;
            }
        }
    }

    int r = 0;
    int temp = Hot_Level_0_count;
    if(temp < Hot_Level_1_count){
        temp = Hot_Level_1_count;
        r = 1;
    }
    if(temp < Hot_Level_2_count){
        temp = Hot_Level_2_count;
        r = 2;
    }
    if(temp < Hot_Level_3_count){
        temp = Hot_Level_3_count;
        r = 3;
    }

    return r;    
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
    
    /* update corresponding block status */ 
    subblk = get_subblk(ssd, ppa);
    subblk->ipc++;
    subblk->vpc--;
    int New_Hot_Level = Calculate_Sublk_Hot_Level(ssd, subblk, ppa);
    if(subblk->Current_Hot_Level != New_Hot_Level){
        int r = Change_Blk_Position_InFinder2(blk, New_Hot_Level);
        subblk->Current_Hot_Level = New_Hot_Level;
    }

    /* invalid page後 要從新計算sublk的Hot Level調整Block在Finder2的位置 */
    if (subblk->ipc == spp->pgs_per_subblk){
        subblk->was_victim = SUBLK_VICTIM;
    }
    if (subblk->was_victim = SUBLK_VICTIM){
        int Need_GC_Sublk_Count = 0;
        for (int i=0; i<spp->subblks_per_blk; i++){
            if (blk->subblk[i].was_victim == SUBLK_VICTIM){
                Need_GC_Sublk_Count = Need_GC_Sublk_Count +1;
            }
        }
        blk->GC_Sublk_Count = Need_GC_Sublk_Count;
        blk->Not_GC_Sublk_Count = spp->subblks_per_blk - Need_GC_Sublk_Count;    
        int New_Position = Need_GC_Sublk_Count-1;
        if(blk->In_Finder1_Position != New_Position){
            blk->ch = ppa->g.ch;
            blk->lun = ppa->g.lun;
            blk->pl = ppa->g.pl;
            blk->blk = ppa->g.blk;
            int r = Change_Blk_Position_InFinder1(blk, New_Position);
        }
    }
}

static void mark_page_valid(struct ssd *ssd, struct ppa *ppa)
{
    struct nand_subblock *subblk = NULL;
    struct nand_page *pg = NULL;
    struct ssdparams *spp = &ssd->sp;
    struct nand_block *blk = get_blk(ssd, ppa);

    /* update page status */
    pg = get_pg(ssd, ppa);
    ftl_assert(pg->status == PG_FREE);
    pg->status = PG_VALID;

    /* update corresponding block status */
    subblk = get_subblk(ssd, ppa);
    ftl_assert(subblk->vpc >= 0 && subblk->vpc < ssd->sp.pgs_per_subblk);
    subblk->vpc++;
    subblk->epc--;

    int New_Hot_Level = Calculate_Sublk_Hot_Level(ssd, subblk, ppa);
    if (subblk->Current_Hot_Level != New_Hot_Level){
        int r = Change_Blk_Position_InFinder2(blk, New_Hot_Level);
        subblk->Current_Hot_Level = New_Hot_Level;
    }

    /* 紀錄blk現在使用的sublk id */
    blk->current_sublk_id = ppa->g.subblk;
    subblk->current_page_id = ppa->g.pg;

    /* 如果現在這個sublk滿了，blk下一次寫入就要用新的sublk，而用新的sublk就要把blk先搬移到Finder2的Hot_level_0 */
    int sublk_total_page = subblk->ipc + subblk->vpc;
    if (sublk_total_page == spp->pgs_per_subblk){
        
        subblk->was_full = SUBLK_FULL;
        int count = 0;

        for (int i=0; i<spp->subblks_per_blk; i++){
            if (blk->subblk[i].was_full== SUBLK_FULL){
                count++;
            }
        }
        /*如果現在的sublk不是最後一個的話，代表我們下一次可以寫資料到new sublk，所以要先把blk搬到Finder2 Hot Level 0*/
        if (count < (spp->subblks_per_blk-1)){ 
            New_Hot_Level = 0;
            int r = Change_Blk_Position_InFinder2(blk, New_Hot_Level);
        }
    }

    ssd->sp.valid_page++;
    tt_vpc = tt_vpc +1;
}

static void mark_subblock_free(struct ssd *ssd, struct ppa *ppa)
{
    struct ssdparams *spp = &ssd->sp;
    struct nand_subblock *sublk = get_subblk(ssd, ppa);
    struct nand_page *pg = NULL;

    for (int i = 0; i < spp->pgs_per_subblk; i++) {
        /* 重置 Page Status */
        pg = &subblk->pg[i];
        pg->status = PG_FREE;
        pg->Hot_level = Hot_level_0;
        pg->pg_type = PG_Empty;
    }

    /* 重置 sublock Status */
    sublk->vpc =0;
    sublk->ipc =0;
    sublk->epc = spp->pgs_per_subblk;
    sublk->erase_cnt++;
    sublk->was_full = SUBLK_NOT_FULL;
    sublk->was_victim = SUBLK_NOT_VICTIM;
    sublk->Current_Hot_Level = Hot_level_0;
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
static uint64_t gc_write_page(struct ssd *ssd, struct ppa *old_ppa, struct ppa *empty_ppa)
{
    struct ppa new_ppa;
    uint64_t lpn = get_rmap_ent(ssd, old_ppa);

    /* update maptbl */
    set_maptbl_ent(ssd, lpn, empty_ppa);
    /* update rmap */
    set_rmap_ent(ssd, lpn, empty_ppa);

    mark_page_valid(ssd, empty_ppa);

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
    int cnt = 0; //計算sublk有多少valid pg
    
    for (int pg = 0; pg < spp->pgs_per_subblk; pg++) {
        ppa->g.pg = pg;
        pg_iter = get_pg(ssd, ppa);
        /* there shouldn't be any free page in victim blocks */
        ftl_assert(pg_iter->status != PG_FREE);

        if (pg_iter->status == PG_VALID) {
            gc_read_page(ssd, ppa);
            struct ppa *empty_ppa = get_empty_page(ssd, pg_iter->Hot_level, DO_CopyBack);
            gc_write_page(ssd, ppa, empty_ppa);
            cnt++;
        }
    }
    ftl_assert(get_subblk(ssd, ppa)->vpc == cnt);
}

/*
7/12 工作日誌 : 新增block紀錄最新使用到哪個sublk
		新增sublk紀錄最新使用到哪個page
		clean_one_block要更新下次可以寫入的位置
		get_empty_page會收尋sublk可用的Page，所以sublk之間是random write也可以
*/
/*
Block需要重新計算Finder2的位置
1. 先計算出哪個sublk的epc = npgs，表示這個sublk有經過clean_one_subblk
2. 把Block放入Finder2
*/
static void clean_one_Block(struct ssd *ssd, struct nand_block *blk, int GC_Sublk_Count)
{
    struct ssdparams *spp = &ssd->sp;
    int r;
    for (int i=0; i<spp->subblks_per_blk; i++){
        if (blk->subblk[i].epc == spp->subblks_per_blk){ /*當做最新的寫入位置*/
            r = Change_Blk_Position_InFinder2(blk, 0);
            break;
        }
    }
    blk->GC_Sublk_Count = blk->GC_Sublk_Count - GC_Sublk_Count;
    blk->Not_GC_Sublk_Count = spp->subblks_per_blk - blk->GC_Sublk_Count;
}

static int do_gc(struct ssd *ssd, bool force, NvmeRequest *req)
{
    struct ssdparams *spp = &ssd->sp;
    struct ppa ppa;
    int sublk;
    struct nand_block *victim_blk = NULL;
    struct nand_subblock *victim_sublk = NULL;
    int GC_Sublk_Count = 0;

    printf("do_gc\n");
    victim_blk = Get_Victim_Block(ssd);

    if (victim_blk == NULL){
        printf("Do gc no victim blk\n");
        return -1;
    };
    ppa.g.ch = victim_blk->ch;
    ppa.g.lun = victim_blk->lun;
    ppa.g.pl = victim_blk->pl;
    ppa.g.blk = victim_blk->blk;

    /* copy back valid data */
    for (sublk=0; sublk<spp->subblks_per_blk; sublk++){
        ppa.g.subblk = sublk;
        victim_sublk = get_subblk(ssd, &ppa);

        if (victim_sublk->was_victim == SUBLK_VICTIM){
            clean_one_subblock(ssd, &ppa, req);
            mark_subblock_free(ssd, &ppa);
            GC_Sublk_Count++;
        }
    }

    clean_one_Block(ssd, victim_blk, GC_Sublk_Count);
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

/*工作日誌 : 檢查secure_deletion_table[i]取出的值是struct ppa or struct ppa **/
static int do_secure_deletion(struct ssd *ssd, struct ppa *secure_deletion_table, int sensitive_lpn_count, int temp_lpn_count)
{
    int index = 0;
    struct ppa *table = malloc(sizeof(struct ppa) * (temp_lpn_count));
    for(int i=0; i<sensitive_lpn_count; i++){
        if(index==0){
            table[index] = secure_deletion_table[i];
            index = index+1;
        }else{
            int key = 0;
            struct ppa check_ppa = secure_deletion_table[i];
            for(int j=0; j<index; j++){
                struct ppa target_ppa = table[j];
                if( (target_ppa->g.ch==check_ppa->g.ch)&&(target_ppa->g.lun==check_ppa->g.lun)&&(target_ppa->g.pl==check_ppa->g.pl)&&(target_ppa->g.blk==check_ppa->g.blk)&&(check_ppa->g.sublk==target_ppa->g.sublk) ){
                    key = 1; // 有重複
                }
            }
            if(key == 0){
                table[index] = check_ppa;
                index = index +1;
            }
        }
    }
    for(int i=0; i<index; i++){
        clean_one_subblock(ssd, &table[i], NULL);
        mark_subblock_free(ssd, &table[i]);
    }
    free(table);
    return 0;
}

static struct nand_subblock *find(struct ssd *ssd, int Temp_Level)
{
    struct ssdparams *spp = &ssd->sp;
    struct node *current = finder2->list[Temp_Level].head;
    
    if (current == NULL){
        return NULL;
    }else{
        for (current; current!=NULL; current=current->next){
            
            struct nand_block *blk = current->blk;
            for (int i=0; i<spp->subblks_per_blk; i++){
                
                struct nand_subblock *sublk = &blk->subblk[i];
                if(sublk->epc!=0){
                    return sublk; 
                }
            }
        }
    }
    return NULL;
}

static struct nand_subblock *Find_sublk(struct ssd *ssd, int *array)
{
    for (int i=0; i<nHotLevel; i++){
        struct nand_subblock *sublk = find(ssd, array[i]);
        if (sublk!=NULL){
            return sublk;
        } 
    }
    return NULL;
}

/*
工作日誌 : get_Empty_pg_from_Finder2還需要改，且get_empty_page尚未完成
*/

static struct ppa *get_Empty_pg_from_Finder2(struct ssd *ssd, int Hot_Level)
{
    struct ssdparams *spp = &ssd->sp;
    struct nand_subblock *sublk = NULL;
    int *array = malloc(sizeof(int)*nHotLevel);
    struct ppa *empty_pg = malloc(sizeof(struct ppa));

    if (Hot_Level==0){
        for (int i=0; i<nHotLevel; i++){
            array[i] = i;
        }
       sublk = Find_sublk(ssd, array);
    }else if(Hot_Level==(nHotLevel-1)){
        int count = 0;
        for (int i=Hot_Level; i>=0; i--){
            array[count] = i;
            count++;
        }
        sublk = Find_sublk(ssd, array);
    }else{
        int count =0;
        for(int i=Hot_Level; i<nHotLevel; i++){
            array[count] = i;
            count++;
        }
        for(int i=(Hot_Level-1); i>=0 ;i--){
            array[count] = i;
            count++;
        }
        sublk = Find_sublk(ssd, array);
    }
   
    if (sublk!=NULL){
        for (int i=0; i<spp->pgs_per_subblk; i++){
            if (sublk->pg[i].status == PG_FREE){
                empty_pg->g.ch = sublk->ch;
                empty_pg->g.lun = sublk->lun;
                empty_pg->g.pl = sublk->pl;
                empty_pg->g.blk = sublk->blk;
                empty_pg->g.subblk = sublk->sublk;
                empty_pg->g.pg = i;
                return empty_pg;
            }
        }
    }else{
        printf("not find \n");
        free(empty_pg);
        empty_pg = NULL;
        return NULL;
    }
}

/* Use Free_Block_Management */
/* get_empty_page需要考慮，是因為Write還是GC需要取得empty page */
/* 7/10 工作日誌 : get_empty_page()完成，可以先在vscode測試模擬看看，sublk->epc這個變數再其它地方要新增 */
static struct ppa *get_empty_page(struct ssd *ssd, int Hot_Level, int condition)
{
    struct ssdparams *spp = &ssd->sp;
    struct ppa *empty_pg = NULL;

    if (condition==DO_Write){
        /*
        優先度
        1. 看Temp_Block_Management有沒有empty page。
        2. 看Free_Block_Management有沒有empty page。
        3. 看Finder2有沒有empty page。
        */
        int r = -1;
        struct nand_block *blk = NULL;
        empty_pg = malloc(sizeof(struct ppa));

        if (Temp_Block_Management->Queue_Size!=0){
            blk = Peek(Temp_Block_Management);
            empty_pg->g.ch = blk->ch;
            empty_pg->g.lun = blk->lun;
            empty_pg->g.pl = blk->pl;
            empty_pg->g.blk = blk->blk;
            for (int i=0; i<spp->subblks_per_blk; i++){
                int is_find = False;
                struct nand_subblock *sublk = &blk->subblk[i];
                if (sublk->epc !=0){
                    for (int j=0; j<spp->pgs_per_subblk; j++){
                        struct nand_page *pg = &sublk->pg[j];
                        if (pg->status = PG_FREE){
                            empty_pg->g.subblk = i;
                            empty_pg->g.pg = j;
                            is_find = True;
                            break;
                        }
                    }
                }
                if (is_find==True){
                    break;
                }
            }
            if(empty_pg->g.subblk == (spp->subblks_per_blk-1) && empty_pg->g.pg == (spp->pgs_per_subblk-1)){
                r = Pop(Temp_Block_Management);
            }
            /*紀錄blk現在使用哪個sublk，紀錄sublk現在使用哪個page*/
            blk->current_sublk_id = empty_pg->g.subblk;
            blk->subblk[blk->current_sublk_id].current_page_id = empty_pg->g.pg;

        }else if(Free_Block_Management->Queue_Size!=0){
            blk = Peek(Free_Block_Management);
            r = Pop(Free_Block_Management);
            /*從Free_Block_Management取得的Block裡面的sublk、page都是empty，所以取地empty page都是從sublk0、page0開始*/
            empty_pg->g.ch = blk->ch;
            empty_pg->g.lun = blk->lun;
            empty_pg->g.pl = blk->pl;
            empty_pg->g.blk = blk->blk;
            empty_pg->g.subblk = 0;
            empty_pg->g.pg = 0;
            Push(Temp_Block_Management, blk);
            /*紀錄blk現在使用哪個sublk，紀錄sublk現在使用哪個page*/
            blk->current_sublk_id = empty_pg->g.subblk;
            blk->subblk[blk->current_sublk_id].current_page_id = empty_pg->g.pg;

        }else{
            if(Hot_Level==-1){
                Hot_Level = 0;
                empty_pg = get_Empty_pg_from_Finder2(ssd, Hot_Level);
            }else{
                int New_Hot_level = Hot_Level +1;
                if (New_Hot_level>(nHotLevel-1)){
                    empty_pg = get_Empty_pg_from_Finder2(ssd, Hot_Level);
                }else{
                    empty_pg = get_Empty_pg_from_Finder2(ssd, New_Hot_level);
                }
            }
            /*紀錄blk現在使用哪個sublk，紀錄sublk現在使用哪個page*/
            blk->current_sublk_id = empty_pg->g.subblk;
            blk->subblk[blk->current_sublk_id].current_page_id = empty_pg->g.pg;
        }
    }else if(condition == DO_CopyBack){
        empty_pg = get_Empty_pg_from_Finder2(ssd, Hot_Level);
        /*紀錄blk現在使用哪個sublk，紀錄sublk現在使用哪個page*/
        struct nand_block *blk = get_blk(ssd, empty_pg);
        blk->current_sublk_id = empty_pg->g.subblk;
        blk->subblk[blk->current_sublk_id].current_page_id = empty_pg->g.pg;
    }else{
        printf("no provide condition \n");
    }

    return empty_pg;
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
	int is_need_secure_deletion = -1; // new
	struct ppa *secure_deletion_table = malloc(sizeof(struct ppa) * (end_lpn-start_lpn+1)); //用來記錄哪些lpn是需要secure deletion的
    int sensitive_lpn_count = 0;
    int check =-1; // 拿來確認Lba是不是sensitive lba

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

    check = lba % 10;
    
    for (lpn = start_lpn; lpn <= end_lpn; lpn++){
        int original_hot_level = -1; //lba原本的Hot Level
		int is_new_Lpn = -1; //lba是不是新寫入的

        ppa = get_maptbl_ent(ssd, lpn);
        if (mapped_ppa(&ppa)){ // lba 不是新的
            /* update old page information first */
            struct nand_page *pg = get_pg(ssd, &ppa);
            if (pg->status != PG_INVALID){
                mark_page_invalid(ssd, &ppa, req);
            }
            set_rmap_ent(ssd, INVALID_LPN, &ppa);
			original_hot_level = pg->Hot_level; // 紀錄原本Lpn的hot level
			is_new_Lpn = OLD_LPN; // lpn不是第一次寫入
			if (pg->pg_type == PG_Sensitive){ // 此page是sensitive
                secure_deletion_table[sensitive_lpn_count] = ppa;
                sensitive_lpn_count++;
                is_need_secure_deletion = 1;
			}
        }else{
			is_new_Lpn = NEW_LPN;
		}

        if ( (float)ssd->trim_table[lpn].cnt > table_cnt_avg) {
            wp_2 = false; 
            ssd->wp_table[lpn].use_wp_2 = false;
        }
        else {
            wp_2 = true; 
            ssd->wp_table[lpn].use_wp_2 = true;
        }

        /*1. 先申請一個Empty PPA*/
        int New_Hot_Level = original_hot_level+1;  
		struct ppa *empty_pg = get_empty_page(ssd, original_hot_level, DO_Write);
        ppa = *empty_pg;
		set_maptbl_ent(ssd, lpn, &ppa);
		set_rmap_ent(ssd, lpn, &ppa);
		/*2. 設定Page資料*/
		struct nand_page *new_pg = get_pg(ssd, empty_pg);
		if(check == 8){
			new_pg->pg_type = PG_Sensitive;
		}else{
			new_pg->pg_type = PG_General;
		}
        /*3. 更新Lba的Hot level*/
		if(is_new_Lpn == NEW_LPN){ // new lpn
			new_pg->Hot_level = Hot_level_0;
		}else{
			if (New_Hot_Level <= Hot_level_3){
                new_pg->Hot_level = New_Hot_Level;
            }else{
                new_pg->Hot_level = Hot_level_3;
            }		
		}
        /*4. mark page valid*/
        mark_page_valid(ssd, &ppa);

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
    if (is_need_secure_deletion == 1){
        int r = do_secure_deletion(ssd, secure_deletion_table, , sensitive_lpn_count, (end_lpn-start_lpn+1));
    }
    free(secure_deletion_table);
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

/*
static void test_case_2(struct ssd *ssd, NvmeRequest *req)
{
    
    outfile22 = fopen(fileName22, "wb");
    outfile23 = fopen(fileName23, "wb");
    outfile25 = fopen(fileName25, "wb");

    struct ppa *test_pg = get_empty_page(ssd);
    fprintf(outfile25, "new pg : ch= %d, lun= %d, pl= %d, blk= %d, sublk= %d, pg= %d\n", test_pg->g.ch, test_pg->g.lun, test_pg->g.pl, test_pg->g.blk, test_pg->g.subblk, test_pg->g.pg);

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
                if(j==15){
                    pg->attribute = PG_COLD;
                }
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

    test_pg = get_empty_page(ssd);
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

    test_pg = get_empty_page(ssd);
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

    int r;
    r = do_gc(ssd, true, req);
    printf("r %d\n", r);

    fclose(outfile22);
    fclose(outfile23);
    fclose(outfile25);
}
*/

static void test_case_3(struct ssd *ssd, NvmeRequest *req)
{
    outfile26 = fopen(fileName26, "wb");
    struct ppa *sublk = TAILQ_FIRST(Empty_sublk_queue);
    fprintf(outfile26, "sublk : ch %d , lun %d , blk %d , sublk %d \n", sublk->g.ch, sublk->g.lun, sublk->g.blk, sublk->g.subblk);
    TAILQ_REMOVE(Empty_sublk_queue, sublk, next);

    sublk = TAILQ_FIRST(Empty_sublk_queue);
    fprintf(outfile26, "sublk : ch %d , lun %d , blk %d , sublk %d \n", sublk->g.ch, sublk->g.lun, sublk->g.blk, sublk->g.subblk);
    TAILQ_REMOVE(Empty_sublk_queue, sublk, next);

    sublk = TAILQ_FIRST(Empty_sublk_queue);
    fprintf(outfile26, "sublk : ch %d , lun %d , blk %d , sublk %d \n", sublk->g.ch, sublk->g.lun, sublk->g.blk, sublk->g.subblk);
    TAILQ_REMOVE(Empty_sublk_queue, sublk, next);

    /*
    printf("1934\n");
    struct ppa *sublk_1 = get_empty_sublk(ssd, outfile26);
    fprintf(outfile26, "sublk : ch %d , lun %d , blk %d , sublk %d\n", sublk_1->g.ch, sublk_1->g.lun, sublk_1->g.blk, sublk_1->g.subblk);
    fprintf(outfile26, "   \n");
    if (sublk_1 == NULL){
        printf("sublk 1 NULL \n");
    }else{
        printf("sublk : ch %d , lun %d , blk %d , sublk %d \n", sublk_1->g.ch, sublk_1->g.lun, sublk_1->g.blk, sublk_1->g.subblk);
    }
    for (int i=0; i<16; i++){
        printf("1940\n");
        struct ppa *pg = get_subblk_free_pg(ssd, sublk_1, outfile26);
        printf("1942\n");
        struct nand_page *page = get_pg(ssd, pg);
        printf("1944\n");
        mark_page_valid(ssd, pg);
        printf("1946\n");
        page->attribute = PG_HOT;

        fprintf(outfile26, "pg : ch %d , lun %d , blk %d , sublk %d , pg %d\n", pg->g.ch, pg->g.lun, pg->g.blk, pg->g.subblk, pg->g.pg);
        fprintf(outfile26, "   \n");
    }

    printf("1949\n");
    struct ppa *sublk_2 = get_empty_sublk(ssd, outfile26);
    fprintf(outfile26, "sublk 2 : ch %d , lun %d , blk %d , sublk %d\n", sublk_2->g.ch, sublk_2->g.lun, sublk_2->g.blk, sublk_2->g.subblk);
    fprintf(outfile26, "   \n");
    if (sublk_2 == NULL){
        printf("sublk 2 NULL \n");
    }
    for (int i=0; i<8; i++){
        printf("1956\n");
        struct ppa *pg = get_subblk_free_pg(ssd, sublk_2, outfile26);
        printf("1958\n");
        struct nand_page *page = get_pg(ssd, pg);
        printf("1960\n");
        mark_page_valid(ssd, pg);
        printf("1962\n");
        page->attribute = PG_HOT;

        fprintf(outfile26, "pg : ch %d , lun %d , blk %d , sublk %d , pg %d\n", pg->g.ch, pg->g.lun, pg->g.blk, pg->g.subblk, pg->g.pg);
        fprintf(outfile26, "   \n");
    }

    printf("1961\n");
    struct ppa *sublk_3 = get_empty_sublk(ssd, outfile26);
    fprintf(outfile26, "sublk 3 : ch %d , lun %d , blk %d , sublk %d\n", sublk_3->g.ch, sublk_3->g.lun, sublk_3->g.blk, sublk_3->g.subblk);
    fprintf(outfile26, "   \n");
    if (sublk_3 == NULL){
        printf("sublk 3 NULL \n");
    }
    for (int i=0; i<8; i++){
        printf("1972\n");
        struct ppa *pg = get_subblk_free_pg(ssd, sublk_3, outfile26);
        printf("1974\n");
        struct nand_page *page = get_pg(ssd, pg);
        printf("1976\n");
        mark_page_valid(ssd, pg);
        printf("1978\n");
        page->attribute = PG_HOT;

        fprintf(outfile26, "pg : ch %d , lun %d , blk %d , sublk %d , pg %d\n", pg->g.ch, pg->g.lun, pg->g.blk, pg->g.subblk, pg->g.pg);
        fprintf(outfile26, "   \n");
    }
    */

    fclose(outfile26);
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
        test_case_3(ssd, req);
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

