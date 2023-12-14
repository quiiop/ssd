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
const char* fileName26 = "Test_Case_Log.txt";
const char* fileName27 = "Report_File.txt";
const char* fileName28 = "Workload_Lba.txt";
const char* fileName29 = "WA_Cnt_Record.txt";
const char* fileName30 = "Write_Cnt_Record.txt";
const char* fileName31 = "workload.txt";
const char* fileName32 = "Test_Record.txt";
const char* fileName33 = "space.txt";
const char* fileName34 = "finder_record.txt";
const char* fileName35 = "victim_sublk_record.txt";
const char* fileName36 = "GC_Sublk_Record.txt";
const char* fileName37 = "latency.txt";
const char* fileName38 = "clock.txt";
const char* fileName39 = "LBA_39.txt";
const char* fileName40 = "debug_finder1.txt";
const char* fileName41 = "debug_finder2.txt";

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
FILE *outfile27 = NULL;
FILE *outfile28 = NULL;
FILE *outfile29 = NULL;
FILE *outfile30 = NULL;
FILE *outfile31 = NULL;
FILE *outfile32 = NULL;
FILE *outfile33 = NULL;
FILE *outfile34 = NULL;
FILE *outfile35 = NULL;
FILE *outfile36 = NULL;
FILE *outfile37 = NULL;
FILE *outfile38 = NULL;
FILE *outfile39 = NULL;
FILE *outfile40 = NULL;
FILE *outfile41 = NULL;
//#define FEMU_DEBUG_FTL

//static bool wp_2 = false;
static uint64_t read_cnt = 0;
static uint64_t gcc_count = 0;
//static uint64_t page_write_count = 0;
//static uint64_t page_trim_count = 0;
static int mark_page_invalid_count = 0;

/* Finder */
static struct Finder *finder=NULL;
static int tt_ipc =0;
static int tt_vpc =0;

/* Finder2 */
static struct Finder2 *finder2=NULL;

/* Free_Block_Management */
static struct Queue *Free_Block_Management=NULL;
static struct Queue *Temp_Block_Management=NULL; //用來暫存尚未存到Finder2的Block

static void *ftl_thread(void *arg);

static int Test_Count = 0;
static uint64_t Total_Page = 0;
static uint64_t Free_Page = 0;
static uint64_t Valid_Page = 0;
static uint64_t Invalid_Page = 0;
uint64_t current_block_cnt = 0;
// static uint64_t Write_Lpn_Cnt = 0;

static inline bool should_gc_sublk(struct ssd *ssd)
{
    /*
    if (tt_ipc > ssd->sp.sublk_gc_thres_pgs){
        return true;
    }else{
        return false;
    }
    */
   /*當可用的Free Block數量 < 總Blk數 * GC_Threshold，做GC動作*/
   //int GC_Threshold = 0.5;
   //int GC_Threshold_Blk_Count = (ssd->sp.nchs * ssd->sp.blks_per_ch) * GC_Threshold;
   // printf("Queue_Size = %d\n", Free_Block_Management->Queue_Size);
   
   //printf("120\n");
   double threshold = 4096 * 0.5;
   if (Free_Block_Management->Queue_Size < threshold){ //total 4096 blks
        return true;
   }else{
        return false;
   }
   //printf("127\n");
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

    //fprintf(outfile8, "pgidx %lu\n", pgidx);
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
    struct Finder *temp_Finder = malloc(sizeof(struct Finder));
    temp_Finder->id = 1;
    temp_Finder->size = size;
    temp_Finder->list = malloc(sizeof(struct link) * size);
    for (int i=0; i<size; i++){
        temp_Finder->list[i] = init_link(i);
    }
    return temp_Finder;
}

/* Finder2 init */
static struct Finder2 *init_Finder2(int size)
{
    struct Finder2 *temp_Finder = malloc(sizeof(struct Finder2));
    temp_Finder->id = 2;
    temp_Finder->size = size;
    temp_Finder->list = malloc(sizeof(struct link) * size);
    for(int i=0; i<size; i++){
        temp_Finder->list[i] = init_link(i);
    }
    return temp_Finder;
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

static int Remove_Node(struct link *list, struct nand_block *blk)
{
    struct node *current;
    struct node *prevoius; 

    int is_find = 0;
    if(list->head == NULL){
        return is_find;
    }
    prevoius = list->head;
    for(current=list->head; current!=NULL; current=current->next){
        if(current->blk == blk){
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
        Remove_Node(&finder->list[Old_Position], blk);
        Add_Link(&finder->list[New_Position], n);
    }
    return 1;
}

static int Add_Finder2(struct nand_block *blk, int Old_Hot_Level, int New_Hot_Level)
{
    struct node *n = init_node(blk);
    /* Old_Hot_Level等於-1，表示Block新加入Finder2新加入Finder2 */
    //fprintf(outfile27, "258 Old Hot Level %d, New Hot Level %d\n", Old_Hot_Level, New_Hot_Level);
    if(Old_Hot_Level == Blk_Not_in_Finder2){
        Add_Link(&finder2->list[New_Hot_Level], n);
    }else{
        Remove_Node(&finder2->list[Old_Hot_Level], blk);
        Add_Link(&finder2->list[New_Hot_Level], n);
    }
    return 1;
}


static void Remove_Blk_InFinder2(struct nand_block *blk, int Hot_Level)
{
    Remove_Node(&finder2->list[Hot_Level], blk);
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

/*
static int Size(struct Queue *queue)
{
    return queue->Queue_Size;
}
*/

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

/*
static void show(struct Queue *queue)
{
    struct node *current;
    printf("Queue Size %d \n", queue->Queue_Size);
    printf("Queue id %d :", queue->id);
    for (current=queue->head; current!=NULL; current=current->next){
        printf(" Block %d , ", current->blk->blk);
    }
    printf("\n");
}
*/

static void clean_Temp_Block_Management(void){
    while(1){
        if (Temp_Block_Management->Queue_Size == 0){
            break;
        }
        Pop(Temp_Block_Management);
    }
}

static struct Queue *init_Queue(int id)
{
    struct Queue *queue = malloc(sizeof(struct Queue));
    queue->id = id;
    queue->Queue_Size = 0;
    queue->head = NULL;
    queue->tail = NULL;
    return queue;
}
/* Queue Operation Over */

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

    //printf("total line %d\n", spp->tt_lines);
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

    /* Total Page */
    Total_Page = spp->nchs * spp->blks_per_ch * spp->pgs_per_blk;
    Free_Page = Total_Page;
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

static void ssd_init_nand_subblk(struct nand_subblock *subblk, struct ssdparams *spp, uint64_t ch_id, uint64_t lun_id, uint64_t pl_id, uint64_t blk_id, uint64_t sublk_id, FILE *outfile)
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
    subblk->Current_Hot_Level = SUBLK_NOT_IN_FINDER2;
    subblk->current_page_id = 0;
    
    for (uint64_t i = 0; i < subblk->npgs; i++) {
        ssd_init_nand_page(&subblk->pg[i], spp);
    }
    
    subblk->ch = ch_id;
    subblk->lun = lun_id;
    subblk->pl = pl_id;
    subblk->blk = blk_id;
    subblk->sublk = sublk_id;
}

static void ssd_init_nand_blk(struct nand_block *blk, struct ssdparams *spp, uint64_t ch_id, uint64_t lun_id, uint64_t pl_id, uint64_t blk_id, FILE *outfile)
{
    /* Set Blk Status */
    blk->id = current_block_cnt;
    blk->nsubblks = spp->subblks_per_blk;
    blk->subblk = g_malloc0(sizeof(struct nand_subblock) * blk->nsubblks);
    blk->current_sublk_id = 0;
    blk->GC_Sublk_Count = 0;
    blk->Free_Sublk_Count = blk->nsubblks;
    blk->In_Finder1_Position = Blk_Not_In_Finder1;
    blk->In_Finder2_Position = Blk_Not_in_Finder2;
    /* Set Blk Info */
    blk->ch = ch_id;
    blk->lun = lun_id;
    blk->pl = pl_id;
    blk->blk = blk_id;

    for (uint64_t i = 0; i < blk->nsubblks; i++) {
        ssd_init_nand_subblk(&blk->subblk[i], spp, ch_id, lun_id, pl_id, blk_id, i, outfile);
    }

    /* Set Free Block Management */
    Push(Free_Block_Management, blk);
    current_block_cnt++;
}

static void ssd_init_nand_plane(struct nand_plane *pl, struct ssdparams *spp, uint64_t ch_id, uint64_t lun_id, uint64_t pl_id, FILE *outfile)
{
    pl->nblks = spp->blks_per_pl;
    pl->blk = g_malloc0(sizeof(struct nand_block) * pl->nblks);
    for (uint64_t i = 0; i < pl->nblks; i++) {
        ssd_init_nand_blk(&pl->blk[i], spp, ch_id, lun_id, pl_id, i, outfile);
    }
}

static void ssd_init_nand_lun(struct nand_lun *lun, struct ssdparams *spp, uint64_t ch_id, uint64_t lun_id, FILE *outfile)
{
    lun->npls = spp->pls_per_lun;
    lun->pl = g_malloc0(sizeof(struct nand_plane) * lun->npls);
    for (uint64_t i = 0; i < lun->npls; i++) {
        ssd_init_nand_plane(&lun->pl[i], spp, ch_id, lun_id, i, outfile);
    }
    lun->next_lun_avail_time = 0;
    lun->busy = false;
}

static void ssd_init_ch(struct ssd_channel *ch, struct ssdparams *spp, uint64_t ch_id, FILE *outfile)
{
    ch->nluns = spp->luns_per_ch;
    ch->lun = g_malloc0(sizeof(struct nand_lun) * ch->nluns);
    for (uint64_t i = 0; i < ch->nluns; i++) {
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

static int Change_Blk_Position_InFinder1(struct nand_block *blk, int New_Position){
    int Old_Position = blk->In_Finder1_Position;
    int r =Add_Finder1(blk, Old_Position, New_Position);
    if(r == 1){
        blk->In_Finder1_Position = New_Position;
    }
    return r;
}

static int Change_Blk_Position_InFinder2(struct nand_block *blk, int New_Hot_Level)
{
    //fprintf(outfile27 ,"582 blk %lu, Finder2 Position %d, New Hot Level %d\n", blk->blk, blk->In_Finder2_Position, New_Hot_Level);
    int r = Add_Finder2(blk, blk->In_Finder2_Position, New_Hot_Level);
    if (r==1){
        blk->In_Finder2_Position = New_Hot_Level;
    }
    return r;
}

static struct nand_block *Get_Victim_Block(struct ssd *ssd)
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

/*
static void Remove_Block_Finder1(struct nand_block *target_block)
{
    struct node *n = init_node(target_block);
    int Old_Position = target_block->In_Finder1_Position;
    Remove_Node(&finder->list[Old_Position], n);
    int New
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
*/

/*
static void show_info(struct nand_block blk)
{
    printf("Block id %d , ",  blk.id);
    printf("pos %d\n", blk.In_Finder1_Position);
}
*/
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

    /* init Finder */
    int nsublks = spp->subblks_per_blk;
    outfile15  = fopen(fileName15, "wb");
    finder = init_Finder(nsublks);
    fclose(outfile15);

    outfile20 = fopen(fileName20, "wb");
    //fprintf(outfile20, "ssd tt pg %d , gc thres pg %d\n",ssd->sp.tt_pgs ,ssd->sp.sublk_gc_thres_pgs);
    //fprintf(outfile20, "ssd tt_blks %d , tt_luns %d , tt chs %d\n",ssd->sp.tt_blks , ssd->sp.tt_luns, ssd->sp.nchs);
    //fprintf(outfile20, "ssd pgs_per_pl %d , pgs_per_lun %d , pgs_per_ch %d\n",ssd->sp.pgs_per_pl , ssd->sp.pgs_per_lun, ssd->sp.pgs_per_ch);
    fclose(outfile20);

    /* init Finder2 */
    finder2 = init_Finder2(nHotLevel);

    // initialize ssd trim cnt table 
    ssd->trim_table = g_malloc0(sizeof(struct trim_table) * spp->tt_pgs);
    for (int i = 0; i < spp->tt_pgs; i++) {
        ssd->trim_table[i].cnt = 0;
    }

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

/*
static int Calculate_Sublk_Hot_Level(struct ssd *ssd, struct nand_subblock *sublk, struct ppa *sublk_info)
{
    struct ssdparams *spp = &ssd->sp;
    struct nand_page *pg_iter = NULL;
    struct ppa temp_sublk = *sublk_info;

    int *array = malloc(sizeof(int) * nHotLevel); //儲存統計結果
    for (int Hot_level=0; Hot_level<nHotLevel; Hot_level++){   
        array[Hot_level] = 0;
    }

    for(int i=0; i<spp->pgs_per_subblk; i++){
        temp_sublk.g.pg = i;
        pg_iter = get_pg(ssd, &temp_sublk);
        if(pg_iter->status == PG_VALID){
            for (int Hot_level=0; Hot_level<nHotLevel; Hot_level++){
                if(pg_iter->Hot_level == Hot_level){
                    array[Hot_level] = array[Hot_level] +1;
                }
            }
        }
    }

    int r = 0;
    int temp = array[Hot_level_0];
    
    for (int Hot_level=1; Hot_level<nHotLevel; Hot_level++){
        if (temp < array[Hot_level]){
            temp = array[Hot_level];
            r = Hot_level;
        }
    }
    
    return r;
}
*/

static struct blk_result blk_info(struct ssd *ssd, struct nand_block *blk)
{
    struct ssdparams *spp = &ssd->sp;
    int vpc = 0;
    int ipc = 0;
    int epc = 0;

    for (int i=0; i<spp->subblks_per_blk; i++){
        struct nand_subblock *sublk = &blk->subblk[i];
        vpc = vpc + sublk->vpc;
        ipc = ipc + sublk->ipc;
        epc = epc + sublk->epc;
    }

    struct blk_result info;
    info.vpc = vpc;
    info.ipc = ipc;
    info.epc = epc;

    return info;
}



static void Print_Link(struct ssd *ssd, FILE *outfile, struct link *list)
{
    struct node *current;
    fprintf(outfile ,"Link %d : ", list->id);
    if(list->head==NULL){
        fprintf(outfile, "Empty \n");
    }else{
        for(current=list->head; current->next!=NULL; current=current->next){
            struct blk_result info = blk_info(ssd, current->blk);
            fprintf(outfile, "Blk %lu (vpc %d, ipc %d, epc %d) -> ", current->blk->id, info.vpc, info.ipc, info.epc);
        }
        struct blk_result info = blk_info(ssd, current->blk);
        fprintf(outfile, "Blk %lu (vpc %d, ipc %d, epc %d) -> NULL \n", current->blk->id, info.vpc, info.ipc, info.epc);
    } 
}


static void Print_Finder(struct ssd *ssd, FILE *outfile, int finder_id)
{
    fprintf(outfile, "\n");
    if(finder_id == Finder1_ID){
        fprintf(outfile, "Finder1 : \n");
        for(int i=0; i<finder->size; i++){
            Print_Link(ssd, outfile ,&finder->list[i]);
        }
    }else{
        fprintf(outfile, "Finder2 : \n");
        for(int i=0; i<finder2->size; i++){
            Print_Link(ssd, outfile ,&finder2->list[i]);
        }
    }
}

static int Calculate_GC_Sublk(struct nand_subblock *sublk)
{
    // printf("902\n");
    double n = 0;
    if (sublk == NULL){
        printf("905 error\n");
    }
    // printf("903 ipc %d\n", sublk->ipc);
    // printf("904 vpc %d\n", sublk->vpc);
    if (sublk->vpc == 0){
        n = (sublk->ipc + sublk->vpc) / 1;
    }else{
        n = (sublk->ipc + sublk->vpc) / sublk->vpc;
    }

    // printf("n %f\n", n);
    if (n>4){
        return 1; //do_gc
    }else{
        return 0;
    }
}

static double Calculate_GC_Sublk_2(struct nand_subblock *sublk)
{
    // printf("902\n");
    double n = 0;
    if (sublk == NULL){
        printf("905 error\n");
    }
    // printf("903 ipc %d\n", sublk->ipc);
    // printf("904 vpc %d\n", sublk->vpc);
    if (sublk->vpc == 0){
        n = (sublk->ipc + sublk->vpc) / 1;
    }else{
        n = (sublk->ipc + sublk->vpc) / sublk->vpc;
    }

    return n;
}

/* update SSD status about one page from PG_VALID -> PG_VALID */
static void mark_page_invalid(struct ssd *ssd, struct ppa *ppa, NvmeRequest *req)
{
    // mark_page_invalid 只需要注意blk在Finder1的位置
    mark_page_invalid_count = mark_page_invalid_count + 1;
    struct ssdparams *spp = &ssd->sp;
    struct nand_subblock *subblk = NULL;
    struct nand_page *pg = NULL;
    struct nand_block *blk = get_blk(ssd, ppa);

    /* update corresponding page status */
    pg = get_pg(ssd, ppa);
    ftl_assert(pg->status == PG_VALID);
    pg->status = PG_INVALID;
    //fprintf(outfile26, "invalid page : ch= %d, lun= %d, pl= %d, blk= %d, sublk= %d, pg= %d\n",ppa->g.ch, ppa->g.lun, ppa->g.pl, ppa->g.blk, ppa->g.subblk, ppa->g.pg);
    
    ssd->sp.invalid_page++;
    tt_ipc = tt_ipc +1;

    ssd->sp.valid_page--;
    tt_vpc = tt_vpc -1;
    
    /* update corresponding block status */ 
    subblk = get_subblk(ssd, ppa);
    subblk->ipc++;
    if (subblk->ipc > spp->pgs_per_subblk){
        printf("1019 err\n");
        abort();
    }

    subblk->vpc--;
    if (subblk->vpc < 0){
        printf("1024 err\n");
        abort();
    }

    Invalid_Page++;
    Valid_Page--;

    /* invalid page後 要從新計算sublk的Hot Level調整Block在Finder2的位置 */
    if (subblk->ipc + subblk->vpc == spp->pgs_per_subblk){
        if (subblk->was_full != SUBLK_FULL){
            subblk->was_full = SUBLK_FULL;
            blk->Free_Sublk_Count--;
        }
    }

    if (subblk->was_full == SUBLK_FULL){
        if (subblk->was_victim != SUBLK_VICTIM){
            int n = Calculate_GC_Sublk(subblk);
            if (n==1){
                subblk->was_victim = SUBLK_VICTIM;
                int Need_GC_Sublk_Count = 0;
                for (int i=0; i<spp->subblks_per_blk; i++){
                    if (blk->subblk[i].was_victim == SUBLK_VICTIM){
                        Need_GC_Sublk_Count = Need_GC_Sublk_Count +1;
                    }
                }
                blk->GC_Sublk_Count = Need_GC_Sublk_Count;
                int New_Position = blk->GC_Sublk_Count -1;
                if(blk->In_Finder1_Position != New_Position){
                    blk->ch = ppa->g.ch;
                    blk->lun = ppa->g.lun;
                    blk->pl = ppa->g.pl;
                    blk->blk = ppa->g.blk;
                    Change_Blk_Position_InFinder1(blk, New_Position);
                }
            }
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
    //fprintf(outfile26, "valid page : ch= %d, lun= %d, pl= %d, blk= %d, sublk= %d, pg= %d\n",ppa->g.ch, ppa->g.lun, ppa->g.pl, ppa->g.blk, ppa->g.subblk, ppa->g.pg);

    /* update corresponding block status */
    subblk = get_subblk(ssd, ppa);
    ftl_assert(subblk->vpc >= 0 && subblk->vpc < ssd->sp.pgs_per_subblk);
    subblk->Current_Hot_Level = pg->Hot_level;
    
    subblk->vpc++;
    if (subblk->vpc > spp->pgs_per_subblk){
        printf("1085 err\n");
        abort();
    } 
    subblk->epc--;
    if (subblk->epc < 0){
        printf("1090 err\n");
        abort();
    }

    Valid_Page++;

    /* 紀錄blk現在使用的sublk id */
    blk->current_sublk_id = ppa->g.subblk;
    subblk->current_page_id = ppa->g.pg;

    /* blk尚未加入Finder2，把blk加入Finder2 */
    if (blk->In_Finder2_Position == Blk_Not_in_Finder2){
        Change_Blk_Position_InFinder2(blk, pg->Hot_level);
    }

    /* 如果現在這個sublk滿了，blk下一次寫入就要用新的sublk，而用新的sublk就要把blk先搬移到Finder2的Hot_level_0 */
    if (subblk->ipc + subblk->vpc == spp->pgs_per_subblk){
        if (subblk->was_full != SUBLK_FULL){
            subblk->was_full = SUBLK_FULL;
            blk->Free_Sublk_Count--;
        }
    }

    if (subblk->was_full == SUBLK_FULL){
        if (subblk->was_victim != SUBLK_VICTIM){
            int n = Calculate_GC_Sublk(subblk);
            if (n==1){
                subblk->was_victim = SUBLK_VICTIM;
                int Need_GC_Sublk_Count = 0;
                for (int i=0; i<spp->subblks_per_blk; i++){
                    if (blk->subblk[i].was_victim == SUBLK_VICTIM){
                        Need_GC_Sublk_Count = Need_GC_Sublk_Count +1;
                    }
                }
                blk->GC_Sublk_Count = Need_GC_Sublk_Count;
                int New_Position = blk->GC_Sublk_Count -1;
                if(blk->In_Finder1_Position != New_Position){
                    blk->ch = ppa->g.ch;
                    blk->lun = ppa->g.lun;
                    blk->pl = ppa->g.pl;
                    blk->blk = ppa->g.blk;
                    Change_Blk_Position_InFinder1(blk, New_Position);
                }
            }
        }
    }

    ssd->sp.valid_page++;
    tt_vpc = tt_vpc +1;
    Free_Page--;
}

static int Check_All_Sublk_IsEmpty(struct ssd *ssd, struct nand_block *blk)
{
    struct ssdparams *spp = &ssd->sp;
    for (int i=0; i<spp->subblks_per_blk; i++){
        if (blk->subblk[i].epc != spp->pgs_per_subblk){
            return 0;
        }
    }
    return 1;
}

static void mark_subblock_free(struct ssd *ssd, struct ppa *ppa)
{
    struct ssdparams *spp = &ssd->sp;
    struct nand_subblock *sublk = get_subblk(ssd, ppa);
    //fprintf(outfile26, "1037 Mark Free sublk %p\n", sublk);
    //fprintf(outfile26, "1037 blk=%d, sublk=%d, pg=%d\n",  ppa->g.blk, ppa->g.subblk, ppa->g.pg);
    struct nand_page *pg = NULL;

    for (int i = 0; i < spp->pgs_per_subblk; i++) {
        /* 重置 Page Status */
        pg = &sublk->pg[i];
        pg->status = PG_FREE;
        pg->Hot_level = Hot_level_0;
        pg->pg_type = PG_Empty;
        Free_Page++;
    }
    struct nand_block *blk = get_blk(ssd, ppa);
    int Sublk_Full_count = 0;
    int Victim_Sublk_Count = 0;
    for (int i=0; i<spp->subblks_per_blk; i++){
        if ((&blk->subblk[i])->was_full == SUBLK_FULL){
            Sublk_Full_count++;
        }
        if ((&blk->subblk[i])->was_victim == SUBLK_VICTIM){
            Victim_Sublk_Count++;
        }
    }

    /* 重置 sublock Status */
    sublk->vpc =0;
    sublk->ipc =0;
    sublk->epc = spp->pgs_per_subblk;
    sublk->erase_cnt++;
    sublk->was_full = SUBLK_NOT_FULL;
    sublk->was_victim = SUBLK_NOT_VICTIM;
    sublk->Current_Hot_Level = SUBLK_NOT_IN_FINDER2;
    sublk->current_page_id = 0;

    /* 更新blk */
    blk->GC_Sublk_Count--;
    blk->Free_Sublk_Count++;
    
    /*計算blk在Finder1的位置*/
    if (Victim_Sublk_Count == 0){
        //struct node *n = init_node(blk);
        int Old_Position = blk->In_Finder1_Position;
        Remove_Node(&finder->list[Old_Position], blk);
        //free(n);
        //fprintf(outfile26, "1094\n"); 
    }else{
         int New_Position = Victim_Sublk_Count -1;
         Change_Blk_Position_InFinder1(blk, New_Position);
         //fprintf(outfile26, "1098 blk id %lu, Finder1 position= %d\n", blk->blk, blk->In_Finder1_Position); 
    }
    
    /* 如果blk所有的sublk都是empty sublk，那blk要從Finder2移除，加入到Free_Block_Management */
    int n = Check_All_Sublk_IsEmpty(ssd, blk);
    if (n==1){ //blk要從Finder2移除，加入到Free_Block_Management
        if (blk->In_Finder2_Position != Blk_Not_in_Finder2){
            Remove_Blk_InFinder2(blk, blk->In_Finder2_Position);
        }
        Push(Free_Block_Management, blk);
    }
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
static uint64_t gc_write_page(struct ssd *ssd, struct ppa *old_ppa, struct ppa *empty_ppa, int Hot_Level)
{
    //printf("1182\n");
    struct ppa new_ppa;
    //printf("1184\n");
    uint64_t lpn = get_rmap_ent(ssd, old_ppa);
    //printf("1186\n");

    /* update maptbl */
    //printf("1189\n");
    set_maptbl_ent(ssd, lpn, empty_ppa);
    //printf("1191\n");
    /* update rmap */
    //printf("1193\n");
    set_rmap_ent(ssd, lpn, empty_ppa);
    //printf("1195\n");

    struct nand_page *new_pg = get_pg(ssd, empty_ppa);
    new_pg->Hot_level = Hot_Level;
    //printf("1199\n");
    mark_page_valid(ssd, empty_ppa);
    //printf("1201\n");
    
    /* need to advance the write pointer here */
    if (ssd->sp.enable_gc_delay) {
        struct nand_cmd gcw;
        gcw.type = GC_IO;
        gcw.cmd = NAND_WRITE;
        gcw.stime = 0;
        ssd_advance_status(ssd, &new_ppa, &gcw);
    }
    // printf("1211\n");

    /* advance per-ch gc_endtime as well */
#if 0
    new_ch = get_ch(ssd, &new_ppa);
    new_ch->gc_endtime = new_ch->next_ch_avail_time;
#endif

    //new_lun = get_lun(ssd, &new_ppa);
    //new_lun->gc_endtime = new_lun->next_lun_avail_time;
    // printf("1212\n");
    return 0;
}

/* Use Free_Block_Management */
static struct nand_subblock *find(struct ssd *ssd, int Temp_Level)
{
    //struct ssdparams *spp = &ssd->sp;
    struct node *current = finder2->list[Temp_Level].head;
    struct ssdparams *spp = &ssd->sp;

    if (current == NULL){
        // printf("current NULL\n");
        return NULL;
    }else{
        for (current=finder2->list[Temp_Level].head; current->next != NULL; current=current->next){
            struct nand_block *blk = current->blk;
            for (int i=0; i<spp->pgs_per_subblk; i++){
                if (blk->subblk[i].epc != 0){
                    // printf("find sublk\n");
                    return &blk->subblk[i];
                }
            }
        }
        // printf("Not find\n");
        return NULL;
    }
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

static struct ppa *get_Empty_pg_from_Finder2(struct ssd *ssd, int Hot_Level)
{
    struct ssdparams *spp = &ssd->sp;
    struct nand_subblock *sublk = NULL;
    int *array = malloc(sizeof(int)*nHotLevel);
    struct ppa *empty_ppa = malloc(sizeof(struct ppa));

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
        // printf("Sublk : ipc %d, vpc %d, epc %d, Hot_Level %d, Page_id %ld\n", sublk->ipc, sublk->vpc, sublk->epc, sublk->Current_Hot_Level, sublk->current_page_id);
        for (int i=0; i<spp->pgs_per_subblk; i++){
            // printf("page (%d) state %d\n", i, sublk->pg[i].status);
            if (sublk->pg[i].status == PG_FREE){
                empty_ppa->g.ch = sublk->ch;
                empty_ppa->g.lun = sublk->lun;
                empty_ppa->g.pl = sublk->pl;
                empty_ppa->g.blk = sublk->blk;
                empty_ppa->g.subblk = sublk->sublk;
                empty_ppa->g.pg = i;
                free(array);
                return empty_ppa;
            }
        }
        printf("1338 err\n");
        abort();
    }else{
        //printf("not find \n");
        free(array);
        free(empty_ppa);
        empty_ppa = NULL;
        // printf("1323 not find\n");
    }
    return NULL;
}

static struct nand_block *find_blk_from_Finder1(struct ssd *ssd)
{
    // printf("1250\n");
    struct ssdparams *spp = &ssd->sp;
    int nlinks = spp->subblks_per_blk; //16
    struct nand_block *blk = NULL;
    double Max = 0;
    int is_find_blk = 0;

    for(int i=nlinks-1; i>=0; i--){
        // printf("1254\n");
        struct link *list = &finder->list[i];
        // printf("1256\n");
        if (list->head!=NULL){
            struct node *current;
            for (current=list->head; current->next!=NULL; current=current->next){
                // printf("1259\n");
                struct nand_block *target_block = current->blk;
                is_find_blk = 0;
                // printf("1261\n");
                for (int k=0; k<spp->subblks_per_blk; k++){
                    struct nand_subblock *sublk = &(target_block->subblk[k]);
                    if (sublk->was_full != SUBLK_FULL){
                        for (int k2=0; k2<spp->pgs_per_subblk; k2++){
                            struct nand_page *pg = &sublk->pg[k2];
                            if (pg->status == PG_FREE){
                                double n = Calculate_GC_Sublk_2(sublk);
                                if (n > Max || n == Max){
                                    blk = target_block;
                                    Max = n;
                                    is_find_blk = 1;
                                }
                                break;
                            }
                        }
                    }
                    if (is_find_blk == 1){
                        break;
                    }
                }
            }
            if (blk!=NULL){
                return blk;
            }
        }
    }
    // printf("1263\n");
    return NULL;
}

static struct ppa *Get_Empty_Page_For_General_LPN(struct ssd *ssd, int Hot_Level)
{
    //printf("1356\n");
    struct ppa *temp_ppa = NULL;
    struct ppa *empty_pg = malloc(sizeof(struct ppa));

    temp_ppa = get_Empty_pg_from_Finder2(ssd, Hot_Level);

    if (temp_ppa == NULL){
        if (Free_Block_Management->Queue_Size!=0){
                struct nand_block *blk = Peek(Free_Block_Management);
                Pop(Free_Block_Management);
                if (blk==NULL){
                    printf("1490 err\n");
                }
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

                //printf("1423\n");
                return empty_pg;
        }else{
                free(empty_pg);
                
                //printf("1426\n");
                return NULL;
        }
    }else{
        *empty_pg = *temp_ppa;
        
        //printf("1431\n");
        return empty_pg;
    }
    //printf("1391\n");
}

static struct ppa *Get_Empty_Page_For_Sensitive_LPN(struct ssd *ssd, int Hot_Level)
{
    //printf("1396\n");
    struct nand_block *blk = find_blk_from_Finder1(ssd);
    //printf("1474\n");
    struct ppa *empty_pg = malloc(sizeof(struct ppa));
    struct ssdparams *spp = &ssd->sp;
    //printf("1476\n");
    if (blk!=NULL){
        //printf("blk id %lu, finder1 pos %d\n", blk->blk, blk->In_Finder1_Position);
        empty_pg->g.ch = blk->ch;
        empty_pg->g.lun = blk->lun;
        empty_pg->g.pl = blk->pl;
        empty_pg->g.blk = blk->blk;
        for (int k=0; k<spp->subblks_per_blk; k++){
            //printf("sublk id %lu, ipc %d, vpc %d\n", blk->subblk[k].sublk, blk->subblk[k].ipc, blk->subblk[k].vpc);
            if (blk->subblk[k].was_full != SUBLK_FULL){
                //printf("1484\n");
                struct nand_subblock *sublk = &blk->subblk[k];
                //printf("1486\n");
                for (int m=0 ;m<spp->pgs_per_subblk; m++){
                    struct nand_page *pg = &sublk->pg[m];
                    //printf("1349 pg statue %d\n", pg->status);
                    if (pg->status == PG_FREE){
                        empty_pg->g.subblk = sublk->sublk;
                        empty_pg->g.pg = m;
                        return empty_pg;
                    }
                }
                //printf("1494\n");
            }
        }
        printf("1493 err\n");
        return NULL;
    }else{
        //printf("1500\n");
        struct ppa *temp_ppa = Get_Empty_Page_For_General_LPN(ssd, Hot_Level);
        if (temp_ppa == NULL){
            free(empty_pg);
            printf("1494 err\n");
            return NULL;
        }else{
            *empty_pg = *temp_ppa;
            free(temp_ppa);
            return empty_pg;
        }
        //printf("1511\n");
    }
    //printf("1442\n");
}

/* here ppa identifies the block we want to clean */
static int clean_one_subblock(struct ssd *ssd, struct ppa *ppa, NvmeRequest *req)
{
    //printf("1446\n");
    struct ssdparams *spp = &ssd->sp;
    struct nand_page *pg_iter = NULL;
    int cnt = 0; //計算sublk有多少valid pg

    for (int pg = 0; pg < spp->pgs_per_subblk; pg++){
        ppa->g.pg = pg;
        pg_iter = get_pg(ssd, ppa);

        if (pg_iter->status == PG_VALID){ 
            //printf("1458\n");
            gc_read_page(ssd, ppa);
            struct ppa *empty_ppa;
            if (pg_iter->pg_type == PG_Sensitive){
                //printf("1459\n");
                empty_ppa = Get_Empty_Page_For_Sensitive_LPN(ssd, pg_iter->Hot_level);
                //printf("1461\n");
            }else{
                //printf("1463\n");
                empty_ppa = Get_Empty_Page_For_General_LPN(ssd, pg_iter->Hot_level);
                //printf("1465\n");
            }
            //printf("1469\n");
            gc_write_page(ssd, ppa, empty_ppa, pg_iter->Hot_level);
            //printf("1471\n");
            cnt++;
        }else{
            Invalid_Page--;
        }
    }
    fprintf(outfile29, "%d\n", cnt);
    ftl_assert(get_subblk(ssd, ppa)->vpc == cnt)
    //printf("1477\n");
    return 0;
}


static int do_gc(struct ssd *ssd, bool force, NvmeRequest *req)
{
    struct ssdparams *spp = &ssd->sp;
    struct ppa ppa;
    int sublk;
    struct nand_block *victim_blk = NULL;
    struct nand_subblock *victim_sublk = NULL;
    //int GC_Sublk_Count = 0;

    //printf("do gc\n");
    victim_blk = Get_Victim_Block(ssd);
    if (victim_blk == NULL){
        //printf("No Victim Blk\n");
        //fprintf(outfile32, "No Victim Blk\n");
        return -1;
    }else{
        //fprintf(outfile32, "GC blk= %lu\n", victim_blk->blk);
    }

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
            //printf("1513\n");
            mark_subblock_free(ssd, &ppa);
        }
    }
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

static int do_secure_deletion(struct ssd *ssd, struct ppa *secure_deletion_table, int sensitive_lpn_count, int temp_lpn_count)
{
    struct ssdparams *spp = &ssd->sp; 
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
                if( (target_ppa.g.ch==check_ppa.g.ch)&&(target_ppa.g.lun==check_ppa.g.lun)&&(target_ppa.g.pl==check_ppa.g.pl)&&(target_ppa.g.blk==check_ppa.g.blk) ){
                    key = 1; // 有重複
                }
            }
            if(key == 0){
                table[index] = check_ppa;
                index = index +1;
            }
        }
    }
    
    for (int i=0; i<index; i++){
        struct nand_block *blk = get_blk(ssd, &table[i]);
        if (blk!=NULL){
            int count = 0;
            for (int k=0; k<spp->subblks_per_blk; k++){
                if (blk->subblk[k].was_victim == SUBLK_VICTIM){  
                    count++;
                }
            }

            struct ppa sublk_ppa;
            sublk_ppa.g.ch = blk->ch;
            sublk_ppa.g.lun = blk->lun;
            sublk_ppa.g.pl = blk->pl;
            sublk_ppa.g.blk = blk->blk;
        
            if (count == spp->subblks_per_blk){
                for (int k=0; k<spp->subblks_per_blk; k++){
                    sublk_ppa.g.subblk = k;
                    //printf("1609\n");
                    clean_one_subblock(ssd, &sublk_ppa, NULL);
                    //printf("1611\n");
                }
            }else{
                for (int k=0; k<spp->subblks_per_blk; k++){
                    sublk_ppa.g.subblk = k;
                    if (blk->subblk[k].was_victim == SUBLK_VICTIM){
                        //printf("1617\n");
                        clean_one_subblock(ssd, &sublk_ppa, NULL);
                        //printf("1619\n");
                    }
                }
            }
            for (int k=0; k<spp->subblks_per_blk; k++){
                sublk_ppa.g.subblk = k;
                if (blk->subblk[k].was_victim == SUBLK_VICTIM){
                    //printf("1626\n");
                    mark_subblock_free(ssd, &sublk_ppa);
                   //printf("1628\n");
                }
            }
        }
    }

    free(table);
    return 0;
}

static uint64_t ssd_dsm(struct ssd *ssd, NvmeRequest *req)
{
    printf("trim start\n");
    struct ssdparams *spp = &ssd->sp;
    uint64_t lba = req->slba;
    int len = req->nlb;
    uint64_t start_lpn = lba / spp->secs_per_pg;
    uint64_t end_lpn = (lba + len - 1) / spp->secs_per_pg;
    
    if (end_lpn >= ssd->sp.tt_pgs){
        return 0;
    }

    struct ppa ppa;
    uint64_t lpn;
    int is_need_secure_deletion = -1;
    int sensitive_lpn_count = 0;
    struct ppa *secure_deletion_table = (struct ppa *)malloc(sizeof(struct ppa) * (end_lpn-start_lpn+1));
    
    // printf("trim slba %lu, len %d\n", lba, len);
    // printf("trim sLpn %lu , eLpn %lu, ttPg %d\n", start_lpn, end_lpn, ssd->sp.tt_pgs);
    for (lpn = start_lpn; lpn <= end_lpn; lpn++) {
        if (lpn >= ssd->sp.tt_pgs) {
            // printf("table out of range!\n");
            continue;                
        }
        else{
            ssd->trim_table[lpn].cnt++;
        }
        ppa = get_maptbl_ent(ssd, lpn);
        
        if (mapped_ppa(&ppa)) {
            struct nand_page *pg2 = get_pg(ssd, &ppa);
            if (pg2 == NULL){
                printf("pg2 Null !!\n");
            }else{
                if (pg2->status == PG_VALID){
                    mark_page_invalid(ssd, &ppa, req);
                    if (pg2->pg_type == PG_Sensitive){
                        secure_deletion_table[sensitive_lpn_count] = ppa;
                        sensitive_lpn_count++;
                        is_need_secure_deletion = 1;
                    }
                }
                set_rmap_ent(ssd, INVALID_LPN, &ppa);
            }
        }
    }

    if (is_need_secure_deletion == 1){
        do_secure_deletion(ssd, secure_deletion_table, sensitive_lpn_count, (end_lpn-start_lpn+1));
    }
    
    free(secure_deletion_table);
    printf("trim over\n");
return 0;
}

static uint64_t ssd_write(struct ssd *ssd, NvmeRequest *req)
{
    printf("write start\n");
    int boundary_1 = 750000;
    int boundary_2 = 1250000;

    clock_t start_t,finish_t;
    double total_t = 0;
    start_t = clock();

    uint64_t lba = req->slba;
    struct ssdparams *spp = &ssd->sp;
    int len = req->nlb;
    //fprintf(outfile39, "%lu\n", lba);

    //printf("1621\n");
    uint64_t start_lpn = lba / spp->secs_per_pg;
    uint64_t end_lpn = (lba + len - 1) / spp->secs_per_pg;
    //fprintf(outfile32, "start= %lu, end= %lu\n", start_lpn, end_lpn);
    struct ppa ppa;
    uint64_t lpn;
    uint64_t curlat = 0, maxlat = 0;
	int is_need_secure_deletion = -1; // new
	struct ppa *secure_deletion_table = malloc(sizeof(struct ppa) * (end_lpn-start_lpn+1)); //用來記錄哪些lpn是需要secure deletion的
    // printf("1629\n");
    int sensitive_lpn_count = 0;
    int check =-1; // 拿來確認Lba是不是sensitive lba

    // printf("1633\n");
    if (end_lpn >= spp->tt_pgs) {
        printf("1084 error\n");
        ftl_err("start_lpn=%"PRIu64",tt_pgs=%d\n", start_lpn, ssd->sp.tt_pgs);
    }
    //printf("1719\n");

    // printf("ssd ipc %d , gc ipc %d\n", tt_ipc, spp->sublk_gc_thres_pgs);
    while (should_gc_sublk(ssd)) {
        /* perform GC here until !should_gc(ssd) */
        // printf("1643\n");
        int r = do_gc(ssd, true, req);
        // printf("1645\n");
        //fprintf(outfile8,"gc r= %d\n", r);
        // printf("1647\n");
        if (r == -1){
            // printf("1409 not find\n");
            break;
        }
        gcc_count++;
    }
    //printf("1735\n");
    
    
    // printf("Wrie Lba= %lu , len= %d\n", lba, len);
    //fprintf(outfile28, "%lu %d\n", lba, len);
    //fprintf(outfile27, "lba= %lu, size= %d\n", lba, len);
    fprintf(outfile30, "%lu\n", (end_lpn-start_lpn+1));
    printf("1835\n");

    if (boundary_1<=lba && lba<=boundary_2){
        check = 1;
    }else{
        check = 0;
    }

    //printf("Free Block %d\n", Free_Block_Management->Queue_Size);
    //printf("Free Page %lu\n", Free_Page);
    //printf("start_lpn %lu, end_lpn %lu\n", start_lpn, end_lpn);

    for (lpn = start_lpn; lpn <= end_lpn; lpn++){
        // fprintf(outfile26, "LBA %lu\n", lba);
        int original_hot_level = -1; //lba原本的Hot Level
		int is_new_Lpn = -1; //lba是不是新寫入的

        //printf("1759\n");
        ppa = get_maptbl_ent(ssd, lpn);
        // printf("1678\n");
    
        if (mapped_ppa(&ppa)){ // lba 不是新的
            //printf("1681\n");
            /* update old page information first */
            struct nand_page *pg = get_pg(ssd, &ppa);
            // printf("1684\n");
            if (pg->status == PG_VALID){
                //printf("1686\n");
                mark_page_invalid(ssd, &ppa, req);
                //printf("1688\n");
            }
            //printf("1773\n");
            set_rmap_ent(ssd, INVALID_LPN, &ppa);
			// printf("1692\n");
            original_hot_level = pg->Hot_level; // 紀錄原本Lpn的hot level
		    //printf("1777\n");
            is_new_Lpn = OLD_LPN; // lpn不是第一次寫入
			if (pg->pg_type == PG_Sensitive){ // 此page是sensitive
                //printf("1780\n");
                secure_deletion_table[sensitive_lpn_count] = ppa;
                //fprintf(outfile32, "invalid sensitive lpn %lu, ch= %d, lun= %d, pl= %d, blk= %d, sublk= %d, pg= %d\n", lpn, ppa.g.ch, ppa.g.lun, ppa.g.pl, ppa.g.blk, ppa.g.subblk, ppa.g.pg);
                //printf("1699\n");
                struct nand_subblock *sublk = get_subblk(ssd, &ppa);
                //printf("1785\n");
                sublk->was_victim = SUBLK_VICTIM;
                sensitive_lpn_count++;
                is_need_secure_deletion = 1;
			}
        }else{
			is_new_Lpn = NEW_LPN;
		}
       printf("1736\n");

        /*1. 先申請一個Empty PPA*/
        int New_Hot_Level = original_hot_level+1;
        struct ppa *empty_ppa = NULL;
        if (check == 1){ // sensitive LPN
            // printf("1799\n");
            empty_ppa = Get_Empty_Page_For_Sensitive_LPN(ssd, New_Hot_Level);
            //printf("1801\n");
            //fprintf(outfile32, "Sensitive LPN %lu, Empty Page: ch= %d, lun= %d, pl= %d, blk= %d, sublk= %d, pg= %d\n", lpn, empty_ppa->g.ch, empty_ppa->g.lun, empty_ppa->g.pl, empty_ppa->g.blk, empty_ppa->g.subblk, empty_ppa->g.pg);
        }else{
            // printf("1804\n");
            empty_ppa = Get_Empty_Page_For_General_LPN(ssd, New_Hot_Level);
            //printf("1806\n");
            //fprintf(outfile32, "General LPN %lu, Empty Page: ch= %d, lun= %d, pl= %d, blk= %d, sublk= %d, pg= %d\n", lpn, empty_ppa->g.ch, empty_ppa->g.lun, empty_ppa->g.pl, empty_ppa->g.blk, empty_ppa->g.subblk, empty_ppa->g.pg);
        }
        //printf("1744\n");
        if (empty_ppa == NULL){
            printf("no empty page\n");
            printf("Free Block %d, Free Page %lu, Valid Page %lu, Invalid Page %lu\n",Free_Block_Management->Queue_Size, Free_Page, Valid_Page, Invalid_Page);
            
            outfile40 = fopen(fileName40, "wb");
            outfile41 = fopen(fileName41, "wb");
            
            Print_Finder(ssd, outfile40, 1);
            Print_Finder(ssd, outfile41, 2);
            
            fclose(outfile40);
            fclose(outfile41);
        }
        ppa = *empty_ppa;
        free(empty_ppa);
		//printf("1819\n");
        set_maptbl_ent(ssd, lpn, &ppa);
        // printf("1724\n");
		set_rmap_ent(ssd, lpn, &ppa);
        //printf("1823\n");

		/*2. 設定Page資料*/
        // printf("1729\n");
		struct nand_page *new_pg = get_pg(ssd, &ppa);
        // printf("1731\n");
		if (check == 1){
			new_pg->pg_type = PG_Sensitive;
		}else{
			new_pg->pg_type = PG_General;
		}

        /*3. 更新Lba的Hot level*/
		if(is_new_Lpn == NEW_LPN){ // new lpn
            //fprintf(outfile27, "New lpn %lu\n", lpn);
			new_pg->Hot_level = Hot_level_0;
		}else{
            //fprintf(outfile27, "Old lpn %lu\n", lpn);
			if (New_Hot_Level <= Max_Level){
                new_pg->Hot_level = New_Hot_Level;
            }else{
                new_pg->Hot_level = Max_Level;
            }		
		}

        /*4. mark page valid*/
        //printf("1752\n");
        mark_page_valid(ssd, &ppa);
        //printf("1754\n");
        //fprintf(outfile27, "New Page : ch %d, lun %d, pl %d, blk %d, sublk %d, pg %d, Hot_Level %d, Page Type %d\n", ppa.g.ch, ppa.g.lun, ppa.g.pl, ppa.g.blk, ppa.g.subblk, ppa.g.pg, new_pg->Hot_level, new_pg->pg_type);

        struct nand_cmd swr;
        swr.type = USER_IO;
        swr.cmd = NAND_WRITE;
        swr.stime = req->stime;
        /* get latency statistics */
        curlat = ssd_advance_status(ssd, &ppa, &swr);
        maxlat = (curlat > maxlat) ? curlat : maxlat;
        
        /* calucate write pda*/
        // uint64_t pba = ppa2pgidx(ssd, &ppa);
    }

    if (is_need_secure_deletion == 1){
        //printf("1770\n");
        //fprintf(outfile32, "--------------------------------------\n");
        //Print_Finder(outfile32, 1);
        //Print_Finder(outfile32, 2);
        //fprintf(outfile32, "--------------------------------------\n");
        printf("1872\n");
        do_secure_deletion(ssd, secure_deletion_table, sensitive_lpn_count, (end_lpn-start_lpn+1));
        //printf("1874\n");
    }

    //fprintf(outfile27 ,"Free Block Management= %d\n", Free_Block_Management->Queue_Size);
    //fprintf(outfile27 ,"Temp Block Management= %d\n", Temp_Block_Management->Queue_Size);
    //Print_Finder(outfile27, 1);
    //Print_Finder(outfile27, 2);

    free(secure_deletion_table);
    printf("1883\n");
    clean_Temp_Block_Management();
    //printf("1885\n");

    finish_t = clock();
    total_t = (double)(finish_t - start_t);
    fprintf(outfile38, "%f \n", total_t);
    printf("write over\n");
    return maxlat;
}

/* 7/17 工作日誌: 目前在確認secure deletion有沒有問題，確認無誤後，就可以確認do_gc和實作should_do_gc()，之後就可以開始跑fio測試 */
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
    outfile26 = fopen(fileName26, "wb");
    outfile27 = fopen(fileName27, "wb");
    outfile28 = fopen(fileName28, "wb");
    outfile29 = fopen(fileName29, "wb");
    outfile30 = fopen(fileName30, "wb");
    outfile31 = fopen(fileName31, "wb");
    outfile32 = fopen(fileName32, "wb");
    outfile33 = fopen(fileName33, "wb");
    outfile34 = fopen(fileName34, "wb");
    outfile35 = fopen(fileName35, "wb");
    outfile36 = fopen(fileName36, "wb");
    outfile37 = fopen(fileName37, "wb");
    outfile38 = fopen(fileName38, "wb");
    outfile39 = fopen(fileName39, "wb");

    while (!*(ssd->dataplane_started_ptr)) {
        usleep(100000);
    }

    /* FIXME: not safe, to handle ->to_ftl and ->to_poller gracefully */
    ssd->to_ftl = n->to_ftl;
    ssd->to_poller = n->to_poller;

    /* Test Case */ 
    if (Test_Count == 0){
        Test_Count++;
    }

    while (1) {
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
                request_trim = false;
                //fprintf(outfile37, "%lu \n", lat);
                break;
            case NVME_CMD_READ:
                //printf("1925\n");
                lat = ssd_read(ssd, req);
                //printf("1927\n");
                request_trim = false;
                break;
            case NVME_CMD_DSM:
                //Test3(ssd, outfile32);
                lat = ssd_dsm(ssd, req);
                // lat = 0;
                request_trim = true;
                break;
            default:
            	request_trim = false;
                //ftl_err("FTL received unkown request type, ERROR\n");
                ;
            }
	    //printf("i = %d\n",i);
            //printf("1944\n");
            req->reqlat = lat;
            req->expire_time += lat;
            rc = femu_ring_enqueue(ssd->to_poller[i], (void *)&req, 1);
            //fprintf(outfile10, "after rc = %d\n", rc);
            if (rc != 1) {
                printf("1364 error\n");
                ftl_err("FTL to_poller enqueue failed\n");
            }

            /* clean one line if needed (in the background) */
            if(!request_trim) {
            	//printf("1956\n");
                if (should_gc_sublk(ssd)) {
            		//fprintf(outfile7,"%"PRIu64" %d %d\n",req->slba, req->cmd.opcode, ssd->lm.free_line_cnt);
                	do_gc(ssd, false, req); // default false
            	}
                //printf("1961\n");
            }
            //printf("1963\n");    
        }
    }
    //printf("1966\n");
    //fprintf(outfile2,"page write count %"PRIu64,page_write_count);
    //fprintf(outfile2,"page trim count %"PRIu64,page_trim_count);
    
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
    fclose(outfile26);
    fclose(outfile27);
    fclose(outfile28);
    fclose(outfile29);
    fclose(outfile30);
    fclose(outfile31);
    fclose(outfile32);
    fclose(outfile33);
    fclose(outfile35);
    fclose(outfile36);
    fclose(outfile37);
    fclose(outfile38);
    fclose(outfile39);

    return NULL;
}