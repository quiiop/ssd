#include "ftl.h"
#include <stdlib.h>
#include <stdio.h>

//#define FEMU_DEBUG_FTL
/*Record File*/
const char* fileName1 = "Test_Init.txt";

/*Output File*/
FILE *outfile1 = NULL;

/*SSD的參數*/ 
int pgs_per_sublk = NO_SETTING;
int sublks_per_blk = NO_SETTING;
int blks_per_pl = NO_SETTING;
int pls_per_lun = NO_SETTING;
int luns_per_ch = NO_SETTING;
int nchs = NO_SETTING;
int nblks = NO_SETTING;
int npgs = NO_SETTING;
int pgs_per_blk = NO_SETTING;
int pgs_per_pl = NO_SETTING;
int pgs_per_lun = NO_SETTING;
int pgs_per_ch = NO_SETTING;

/*Finder1的參數*/
int nLayers_Finder1 = NO_SETTING;

/*Finder2的參數*/
int Total_Block = NO_SETTING; //如果total block數量有改變這裡記得要改
int nHotLevel = NO_SETTING;
int Blocks_per_linkedList = NO_SETTING;
int pgs_per_linkedList = NO_SETTING;
int Current_Block_Count = NO_SETTING;

/*用於Debug or 控制是否GC*/
int Total_Empty_Block = 0;
int Total_vpc = 0;
int Total_ipc = 0;
int Total_epc = 0;

/* Global Struct */
struct Finder1 finder1;
struct Finder2 finder2;
struct Over_Provisioning OP; 

/* WA的參數 */
double move_pg_cnt = 0;
double write_pg_cnt = 0;

/*預宣告*/
static int Enforce_Clean_Block(struct ssd *ssd, struct nand_block *victim_blk);
static void *ftl_thread(void *arg);
static struct nand_page *Get_Empty_Page_From_Finder1(void);

static inline bool should_gc(struct ssd *ssd)
{
    return (ssd->lm.free_line_cnt <= ssd->sp.gc_thres_lines);
}

static inline bool should_gc_high(struct ssd *ssd)
{
    return (ssd->lm.free_line_cnt <= ssd->sp.gc_thres_lines_high);
}

// maplba的操作
static struct ppa *get_maplba(struct ssd *ssd, uint64_t lpn)
{
    return &ssd->maptbl[lpn];
}

static void set_maplba(struct ssd *ssd, uint64_t lpn, struct ppa *ppa)
{
    ftl_assert(lpn < ssd->sp.tt_pgs);
    ssd->maptbl[lpn] = *ppa;
    ssd->maptbl[lpn].state = MAPPED;
}

static void clear_maplba(struct ssd *ssd, uint64_t lpn)
{
    ssd->maptbl[lpn].state = UNMAPPED;
}

static int mapped(struct ssd *ssd,uint64_t lpn)
{
    if (ssd->maptbl[lpn].state == MAPPED){
        return MAPPED;
    }
    return UNMAPPED;
}

static uint64_t ppa2pgidx(struct ssd *ssd, struct ppa *ppa)
{
    struct ssdparams *spp = &ssd->sp;
    uint64_t pgidx;

    pgidx = ppa->g.ch  * spp->pgs_per_ch  + \
            ppa->g.lun * spp->pgs_per_lun + \
            ppa->g.pl  * spp->pgs_per_pl  + \
            ppa->g.blk * spp->pgs_per_blk + \
            ppa->g.sublk * spp->pgs_per_sublk + \
            ppa->g.pg;

    ftl_assert(pgidx < spp->tt_pgs);

    return pgidx;
}

static uint64_t get_rmap(struct ssd *ssd, struct ppa *ppa)
{
    uint64_t pgidx = ppa2pgidx(ssd, ppa);

    return ssd->rmap[pgidx];
}

/* set rmap[page_no(ppa)] -> lpn */
static void set_rmap(struct ssd *ssd, struct ppa *ppa, uint64_t lpn)
{
    uint64_t pgidx = ppa2pgidx(ssd, ppa);

    ssd->rmap[pgidx] = lpn;
}

static void ssd_init_params(struct ssdparams *spp)
{
    spp->secsz = 512;
    spp->secs_per_pg = 8;
    spp->pgs_per_sublk = 16;
    spp->sublks_per_blk = 16;
    spp->pgs_per_blk = spp->pgs_per_sublk * spp->sublks_per_blk;
    spp->blks_per_pl = 64; /* 4GB */
    spp->pls_per_lun = 1;
    spp->luns_per_ch = 8;
    spp->nchs = 8;

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

    spp->gc_thres_pcent = 0.75;
    spp->gc_thres_lines = (int)((1 - spp->gc_thres_pcent) * spp->tt_lines);
    spp->gc_thres_pcent_high = 0.95;
    spp->gc_thres_lines_high = (int)((1 - spp->gc_thres_pcent_high) * spp->tt_lines);
    spp->enable_gc_delay = true;

    /*基礎的參數設定*/
    pgs_per_sublk = spp->pgs_per_sublk;
    sublks_per_blk = spp->sublks_per_blk;
    blks_per_pl = spp->blks_per_pl;
    pls_per_lun = spp->pls_per_lun;
    luns_per_ch = spp->luns_per_ch;
    nchs = spp->nchs;
    nblks = spp->tt_blks;
    npgs = spp->tt_pgs;
    
    pgs_per_blk = spp->pgs_per_blk;
    pgs_per_pl = spp->pgs_per_pl;
    pgs_per_lun = spp->pgs_per_lun;
    pgs_per_ch = spp->pgs_per_ch;
}

// 初始化Node 
static struct Node* init_node(struct nand_block *blk)
{
    struct Node *node = (struct Node*)malloc(sizeof(struct Node));
    node->blk = blk;
    node->next = NULL;
    return node;
}

// Finder1的操作
static void Add_Node_Finder1(int Victim_Sublk_Cnt, struct Node *node)
{
    if (node == NULL){
        printf("node create false\n");
    }

    int index = Victim_Sublk_Cnt-1;
    if (index < 0){
        printf("350 err\n");
        abort();
    }
    struct LinkedList_1 *List = &finder1.Array[index].list;
    
    if (List->head.next == NULL){
        List->head.next = node;
    }else{
        struct Node *current =NULL;
        for (current=List->head.next; current->next!=NULL; current=current->next){
            // idle 
        }
        current->next = node;
    }
}

static void Add_Block_Finder1(int Victim_Sublk_Cnt, struct nand_block *blk)
{
    struct Node *node = init_node(blk);
    Add_Node_Finder1(Victim_Sublk_Cnt, node);
}

static void Remove_Block_Finder1(int Victim_Sublk_Cnt, struct nand_block *blk)
{
    int index = Victim_Sublk_Cnt-1;
    if (index < 0){
        printf("376 err\n");
        abort();
    }
    struct LinkedList_1 *List = &finder1.Array[index].list;
    if (List->head.next == NULL){
        printf("381 err, Finder1 Array %d\n", index);
        abort();
    }

    struct Node *current = NULL;
    if (List->head.next->blk == blk){
        current = List->head.next;
        List->head.next = current->next;
    }else{
        struct Node *previous = List->head.next; 
        for (current=List->head.next; current!=NULL; current=current->next){
            if (current->blk == blk){
                previous->next = current->next;
                break;
            }
            previous = current;
        }
    }
    
    if (current == NULL){
        printf("401 err Not find remove blk \n");
        abort();
    }else{
        current->next = NULL;
        free(current);
    }
}

static void Change_Block_Position_Finder1(int Old_Victim_Sublk_Cnt, int New_Victim_Sublk_Cnt, struct nand_block *blk)
{   
    Remove_Block_Finder1(Old_Victim_Sublk_Cnt, blk);
    Add_Block_Finder1(New_Victim_Sublk_Cnt, blk);
}

static void init_finder1(void)
{
    /*Finder1的參數*/
    nLayers_Finder1 = sublks_per_blk;
    
    finder1.Array = (struct ArrayList_1*)malloc(sizeof(struct ArrayList_1) * nLayers_Finder1);
    for (int i=0; i<sublks_per_blk; i++){
        finder1.Array[i].id = i;
        finder1.Array[i].list.id = 0;
        finder1.Array[i].list.head.blk = NULL;
        finder1.Array[i].list.head.next = NULL;
    }
}

static struct nand_block *Get_Victim_Block_From_Finder1(void)
{
    struct Node *current = NULL;
    
    for (int i=nLayers_Finder1-1; i>=0; i--){
        struct ArrayList_1 *Array = &finder1.Array[i];
        struct LinkedList_1 *list = &Array->list;
        if (list->head.next != NULL){
            current = list->head.next;
            list->head.next = current->next;
            break;
        }
    }

    if (current == NULL){
        printf("444 Warning !! Not Find Victim Blk\n");
        // 沒有找到Victim Block不一定是錯誤的，因為有時候連續大批資料寫入寫會導致觸發了should_do_gc，但是所有的資料都是valid page，這時候就會發現沒有Victim Block可以處理
        return NULL;
    }

    struct nand_block *blk = current->blk;
    current->next = NULL;
    free(current);

    return blk;
}

// Finder2的操作
static void Add_Node_Finder2(int HotLevel, struct Node *node)
{
    if (node == NULL){
        printf("node create false\n");
    }
    // printf("init node %d\n", node->blk->id);

    struct LinkedList *List = &finder2.Array[HotLevel].Empty;

    if (List->head.next == NULL){
        // printf("list first add node \n");
        List->head.next = node;
    }else{
        struct Node *current =NULL;
        for (current=List->head.next; current->next!=NULL; current=current->next){
            /* idle */
        }
        current->next = node;
        // printf("add over\n");
    }
    List->size++;
}

static void init_finder2(void)
{
    /*Finder2的參數*/
    Total_Block = nblks;
    nHotLevel = 8;
    Blocks_per_linkedList = (Total_Block / nHotLevel);
    pgs_per_linkedList = Blocks_per_linkedList * pgs_per_blk;
    Current_Block_Count = 0;

    finder2.Array = (struct ArrayList *)malloc(sizeof(struct ArrayList) * nHotLevel);
    for (int i=0; i<nHotLevel; i++){
        finder2.Array[i].array_id = i; // LinkList ID
        finder2.Array[i].Empty.list_id = Empty_id;
        /* 記得這裡初始化時，這裡要配置NULL，不然有可能出錯 */
        finder2.Array[i].Empty.head.blk = NULL;
        finder2.Array[i].Empty.head.next = NULL;
        finder2.Array[i].Empty.size = 0;
        finder2.Array[i].Empty.vpc = 0;
        finder2.Array[i].Empty.ipc = 0;
        finder2.Array[i].Empty.epc = 0;
        
        finder2.Array[i].NonEmpty.list_id = NonEmpty_id;
        finder2.Array[i].NonEmpty.head.blk = NULL;
        finder2.Array[i].NonEmpty.head.next = NULL;
        finder2.Array[i].NonEmpty.size = 0;
        finder2.Array[i].NonEmpty.vpc = 0;
        finder2.Array[i].NonEmpty.ipc = 0;
        finder2.Array[i].NonEmpty.epc = 0;
    }
}

static void ssd_init_nand_page(struct nand_page *pg, struct ssdparams *spp, int id, uint64_t cid, uint64_t lid, uint64_t pid, uint64_t bid, uint64_t sublkid)
{
    pg->nsecs = spp->secs_per_pg;
    pg->sec = g_malloc0(sizeof(nand_sec_status_t) * pg->nsecs);
    for (int i = 0; i < pg->nsecs; i++) {
        pg->sec[i] = SEC_FREE;
    }
    pg->id = id;
    pg->state = EMPTY;
    pg->LBA_HotLevel = NO_SETTING;
    pg->type = NO_SETTING;
    // 設定Page addr
    pg->addr.ch = cid;
    pg->addr.lun = lid;
    pg->addr.pl = pid;
    pg->addr.blk = bid;
    pg->addr.sublk = sublkid;
    pg->addr.pg = id;
}

static void ssd_init_nand_sublock(struct nand_sublock *sublk, struct ssdparams *spp, int id, uint64_t cid, uint64_t lid, uint64_t pid, uint64_t bid)
{
    sublk->npgs = spp->pgs_per_sublk;
    sublk->id = id;
    sublk->pg = (struct nand_page*)g_malloc0(sizeof(struct nand_page) * pgs_per_sublk);
    if (sublk->pg == NULL){
        printf("537 err\n");
    }
    for (int i=0; i<pgs_per_sublk; i++){
        ssd_init_nand_page(&sublk->pg[i], spp, i, cid, lid, pid, bid, sublk->id);    
    }
    sublk->vpc = 0;
    sublk->ipc = 0;
    sublk->epc = pgs_per_sublk;
    sublk->state = NonVictim;
    sublk->have_invalid_sensitive_page = FALSE;
}
static void ssd_init_nand_blk(struct nand_block *blk, struct ssdparams *spp, int id, uint64_t cid, uint64_t lid, uint64_t pid)
{
    blk->nsublks = spp->sublks_per_blk;
    blk->id = id;

    blk->sublk = g_malloc0(sizeof(struct nand_sublock) * blk->nsublks);
    if (blk->sublk == NULL){
        printf("557 err\n");
    }
    for (int i = 0; i < blk->nsublks; i++) {
        ssd_init_nand_sublock(&blk->sublk[i], spp, i, cid, lid, pid, blk->id);
    }
    blk->block_id = Current_Block_Count;
    blk->vpc = 0;
    blk->ipc = 0;
    blk->epc = pgs_per_blk;
    blk->victim_sublk_count = 0;
    blk->Nonvictim_sublk_count = sublks_per_blk;
    blk->position = IN_Empty;

    // 創建Node，把Node加入Finder2
    int Current_HotLevel = Current_Block_Count / Blocks_per_linkedList;
    struct Node *node = init_node(blk);
    if (node == NULL){
        printf("574 err\n");
    }

    Add_Node_Finder2(Current_HotLevel, node);
    Current_Block_Count++;

    // 更新Empty List的狀態
    struct LinkedList *list = &finder2.Array[Current_HotLevel].Empty;
    list->epc += (sublks_per_blk * pgs_per_sublk); 

    // 更新整體統計的資料
    Total_Empty_Block++;
    Total_vpc = 0;
    Total_ipc = 0;
    Total_epc += (sublks_per_blk * pgs_per_sublk);
}

static void ssd_init_nand_plane(struct nand_plane *pl, struct ssdparams *spp, int id, uint64_t cid, uint64_t lid)
{
    pl->id = id;
    pl->nblks = spp->blks_per_pl;
    pl->blk = g_malloc0(sizeof(struct nand_block) * pl->nblks);
    if (pl->blk == NULL){
        printf("601 err\n");
    }
    for (int i = 0; i < pl->nblks; i++) {
        ssd_init_nand_blk(&pl->blk[i], spp, i, cid, lid, pl->id);
    }
}

static void ssd_init_nand_lun(struct nand_lun *lun, struct ssdparams *spp, int id, uint64_t cid)
{
    lun->id = id;
    lun->npls = spp->pls_per_lun;
    lun->pl = g_malloc0(sizeof(struct nand_plane) * lun->npls);
    for (int i = 0; i < lun->npls; i++) {
        ssd_init_nand_plane(&lun->pl[i], spp, i, cid, lun->id);
    }
    lun->next_lun_avail_time = 0;
    lun->busy = false;
}

static void ssd_init_ch(struct ssd_channel *ch, struct ssdparams *spp, int id)
{
    ch->id = id;
    ch->nluns = spp->luns_per_ch;
    ch->lun = g_malloc0(sizeof(struct nand_lun) * ch->nluns);
    for (int i = 0; i < ch->nluns; i++) {
        ssd_init_nand_lun(&ch->lun[i], spp, i, ch->id);
    }
    ch->next_ch_avail_time = 0;
    ch->busy = 0;
}

static void ssd_init_maptbl(struct ssd *ssd)
{
    struct ssdparams *spp = &ssd->sp;

    ssd->maptbl = g_malloc0(sizeof(struct ppa) * spp->tt_pgs);
    for (int i = 0; i < spp->tt_pgs; i++) {
        ssd->maptbl[i].g.ch = NO_SETTING;
        ssd->maptbl[i].g.lun = NO_SETTING;
        ssd->maptbl[i].g.pl = NO_SETTING;
        ssd->maptbl[i].g.blk = NO_SETTING;
        ssd->maptbl[i].g.sublk = NO_SETTING;
        ssd->maptbl[i].g.pg = NO_SETTING;
        
        ssd->maptbl[i].state = UNMAPPED;
    }
}

static void ssd_init_rmap(struct ssd *ssd)
{
    struct ssdparams *spp = &ssd->sp;

    ssd->rmap = g_malloc0(sizeof(uint64_t) * spp->tt_pgs);
    for (int i = 0; i < spp->tt_pgs; i++) {
        ssd->rmap[i] = NO_SETTING;
    }
}

// 初始化OP 額外空間
static void init_OP(void){
    // 初始化OP
    OP.size = 1;
    OP.blk = (struct nand_block *)malloc(OP.size * sizeof(struct nand_block));
    // 初始化Block
    struct nand_block *blk = &OP.blk[0];
    blk->sublk = (struct nand_sublock *)malloc(sublks_per_blk * sizeof(struct nand_sublock));
    blk->id = OP_ID;
    blk->block_id = OP_ID;
    blk->vpc = 0;
    blk->ipc = 0;
    blk->epc = pgs_per_blk;
    blk->victim_sublk_count = 0;
    blk->Nonvictim_sublk_count = sublks_per_blk;
    blk->position = NO_SETTING;
    // 初始化Sublock
    for (int s=0; s<sublks_per_blk; s++){
        struct nand_sublock *sublk = &blk->sublk[s];
        sublk->pg = (struct nand_page *)malloc(pgs_per_sublk * sizeof(struct nand_page));
        sublk->id = s;
        sublk->vpc = 0;
        sublk->ipc = 0;
        sublk->epc = pgs_per_sublk;
        sublk->state = NonVictim;
        sublk->have_invalid_sensitive_page = FALSE;
        for (int p=0; p<pgs_per_sublk; p++){
            struct nand_page *pg = &sublk->pg[p];
            pg->addr.ch = NO_SETTING;
            pg->addr.lun = NO_SETTING;
            pg->addr.pl = NO_SETTING;
            pg->addr.blk = NO_SETTING;
            pg->addr.sublk = NO_SETTING;
            pg->addr.pg = NO_SETTING;
            pg->id = p;
            pg->state = NO_SETTING;
            pg->LBA_HotLevel = NO_SETTING;
            pg->type = NO_SETTING;
        }
    }

    printf("Finish Op init\n");
}

/*DeBug用的*/
static void Print_List_Finder1(struct LinkedList_1 *list, FILE *outfile)
{
    struct Node *current = NULL;
    if (list->head.next == NULL){
        fprintf(outfile, "NULL \n");
    }else{
        for (current=list->head.next; current!=NULL; current=current->next){
            fprintf(outfile, "%d -> ", current->blk->id);
        }
        fprintf(outfile, "NULL \n");
    }
}

static void Print_Finder1(FILE *outfile)
{
    fprintf(outfile, "Finder1 !!\n");
    for (int i=0; i<sublks_per_blk; i++){
        fprintf(outfile, "ArrayList (%d) List -> ", finder1.Array[i].id);
        Print_List_Finder1(&finder1.Array[i].list, outfile);
    }
}

static void print_list(int ArrayList_Position, int position, FILE *outfile)
{
    struct ArrayList *Temp_Array = &finder2.Array[ArrayList_Position];
    struct LinkedList *Temp_List = NULL;
    
    if (position == Empty_id){
        Temp_List = &Temp_Array->Empty;
        fprintf(outfile, "Empty List vpc %d, ipc %d, epc %d\n", Temp_List->vpc, Temp_List->ipc, Temp_List->epc);
    }
    if (position == NonEmpty_id){
        Temp_List = &Temp_Array->NonEmpty;
        fprintf(outfile, "NonEmpty List vpc %d, ipc %d, epc %d\n", Temp_List->vpc, Temp_List->ipc, Temp_List->epc);
    }

    if (Temp_List->head.next == NULL){
        if (Temp_List->list_id == NonEmpty_id){
            fprintf(outfile, "NonEmpty List : NULL\n");
        }else if (Temp_List->list_id == Empty_id){
            fprintf(outfile, "Empty List : NUL\n");
        }else{
            printf("804 err\n");
        }
    }else{    
        if (Temp_List->list_id == NonEmpty_id){
            fprintf(outfile, "NonEmpty List : ");
        }else if (Temp_List->list_id == Empty_id){
            fprintf(outfile, "Empty List : ");
        }else{
            printf("812 err\n");
        }

        struct Node *current =NULL;
        for (current=Temp_List->head.next; current->next!=NULL; current=current->next){
            if (current->blk->position != Temp_List->list_id){  
                printf("818 blk position err \n");
                abort();
            }
            fprintf(outfile, "(Blk(%d), vpc %d, ipc %d, epc %d) -> ", current->blk->block_id, current->blk->vpc, current->blk->ipc, current->blk->epc); // 這裡不能用blk->id
        }
        fprintf(outfile, "(Blk(%d), vpc %d, ipc %d, epc %d) -> NULL\n", current->blk->block_id, current->blk->vpc, current->blk->ipc, current->blk->epc);
    }
}

static void Print_Finder2(FILE *outfile)
{
    fprintf(outfile, "Finder2 !!\n");

    for (int i=0; i<nHotLevel; i++){
        struct ArrayList *Temp_Array = &finder2.Array[i];
        fprintf(outfile, "ArrayList (%d)\n", Temp_Array->array_id);
        fprintf(outfile, "EmptyList size = %d, NonEmptyList size = %d\n", Temp_Array->Empty.size, Temp_Array->NonEmpty.size);

        print_list(i, NonEmpty_id, outfile);
        print_list(i, Empty_id, outfile);
        fprintf(outfile, "\n");
    }
}

static void Print_SSD_State(void)
{
    outfile1 = fopen( fileName1, "wb" );

    fprintf(outfile1, "Start Print Finder2------------\n");
    Print_Finder2(outfile1);
    
    //printf("Start Print All Block------------\n");
    //Print_All_Block();
    
    fprintf(outfile1, "Start Print Finder1------------\n");
    Print_Finder1(outfile1);
    
    fprintf(outfile1, "Total Info---------------------\n");
    fprintf(outfile1, "Total Empty Block %d\n", Total_Empty_Block);
    fprintf(outfile1, "Total vpc %d, Total ipc %d, Total epc %d\n", Total_vpc, Total_ipc, Total_epc);

    fclose(outfile1);
}

void ssd_init(FemuCtrl *n)
{
    struct ssd *ssd = n->ssd;
    struct ssdparams *spp = &ssd->sp;

    ftl_assert(ssd);

    /*初始化SSD參數*/
    ssd_init_params(spp);

    /*初始化 Finder*/
    init_finder1();
    init_finder2();

    /* initialize ssd internal layout architecture */
    ssd->ch = g_malloc0(sizeof(struct ssd_channel) * spp->nchs);
    for (int i = 0; i < spp->nchs; i++) {
        ssd_init_ch(&ssd->ch[i], spp, i); // channel 這邊初始化有問題 加油 11/28 !!
    }

    /* initialize maptbl */
    ssd_init_maptbl(ssd);

    /* initialize rmap */
    ssd_init_rmap(ssd);

    /* initialize op */
    init_OP();

    qemu_thread_create(&ssd->ftl_thread, "FEMU-FTL-Thread", ftl_thread, n,
                       QEMU_THREAD_JOINABLE);
    
    Print_SSD_State();
    printf("Init Finish \n");
}

static inline bool valid_ppa(struct ssd *ssd, struct ppa *ppa)
{
    struct ssdparams *spp = &ssd->sp;
    int ch = ppa->g.ch;
    int lun = ppa->g.lun;
    int pl = ppa->g.pl;
    int blk = ppa->g.blk;
    int sublk = ppa->g.sublk;
    int pg = ppa->g.pg;
    int sec = ppa->g.sec;

    if (ch >= 0 && ch < spp->nchs && lun >= 0 && lun < spp->luns_per_ch && pl >=
        0 && pl < spp->pls_per_lun && blk >= 0 && blk < spp->blks_per_pl && sublk>=0 && sublk < spp->sublks_per_blk 
        && pg >= 0 && pg < spp->pgs_per_blk && sec >= 0 && sec < spp->secs_per_pg)
        return true;

    return false;
}

static inline bool valid_lpn(struct ssd *ssd, uint64_t lpn)
{
    return (lpn < ssd->sp.tt_pgs);
}

static inline struct ssd_channel *get_ch(struct ssd *ssd, struct ppa *ppa)
{
    return &(ssd->ch[ppa->g.ch]);
}

static inline struct nand_lun *get_lun(struct ssd *ssd, struct ppa *ppa)
{
    struct ssd_channel *ch = get_ch(ssd, ppa);
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

static inline struct nand_sublock *get_sublk(struct ssd *ssd, struct ppa *ppa)
{
    struct nand_block *blk = get_blk(ssd, ppa);
    return &(blk->sublk[ppa->g.sublk]);
}

static inline struct line *get_line(struct ssd *ssd, struct ppa *ppa)
{
    return &(ssd->lm.lines[ppa->g.blk]);
}

static inline struct nand_page *get_pg(struct ssd *ssd, struct ppa *ppa)
{
    struct nand_sublock *sublk = get_sublk(ssd, ppa);
    return &(sublk->pg[ppa->g.pg]);
}

static uint64_t ssd_advance_status(struct ssd *ssd, struct ppa *ppa, struct
        nand_cmd *ncmd)
{
    int c = ncmd->cmd;
    uint64_t cmd_stime = (ncmd->stime == 0) ? \
        qemu_clock_get_ns(QEMU_CLOCK_REALTIME) : ncmd->stime;
    uint64_t nand_stime;
    struct ssdparams *spp = &ssd->sp;
    struct nand_lun *lun = get_lun(ssd, ppa);
    uint64_t lat = 0;

    switch (c) {
    case NAND_READ:
        // read: perform NAND cmd first 
        nand_stime = (lun->next_lun_avail_time < cmd_stime) ? cmd_stime : \
                     lun->next_lun_avail_time;
        lun->next_lun_avail_time = nand_stime + spp->pg_rd_lat;
        lat = lun->next_lun_avail_time - cmd_stime;
#if 0
        lun->next_lun_avail_time = nand_stime + spp->pg_rd_lat;

        // read: then data transfer through channel 
        chnl_stime = (ch->next_ch_avail_time < lun->next_lun_avail_time) ? \
            lun->next_lun_avail_time : ch->next_ch_avail_time;
        ch->next_ch_avail_time = chnl_stime + spp->ch_xfer_lat;

        lat = ch->next_ch_avail_time - cmd_stime;
#endif
        break;

    case NAND_WRITE:
        // write: transfer data through channel first 
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

        // write: then do NAND program
        nand_stime = (lun->next_lun_avail_time < ch->next_ch_avail_time) ? \
            ch->next_ch_avail_time : lun->next_lun_avail_time;
        lun->next_lun_avail_time = nand_stime + spp->pg_wr_lat;

        lat = lun->next_lun_avail_time - cmd_stime;
#endif
        break;

    case NAND_ERASE:
        // erase: only need to advance NAND status
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

// Block在Finder2搬移的操作
static void Remove_Block_From_EmptyList(int ArrayList_Position, struct nand_block *blk)
{
    //把blk從Empty List搬到NonEmpty List
    struct ArrayList *Array = &finder2.Array[ArrayList_Position];
    struct LinkedList *Empty = &Array->Empty;
    struct LinkedList *NonEmpty = &Array->NonEmpty;

    struct Node *First_Node = Empty->head.next; 
    struct Node *current = NULL;
    struct Node *Previous = NULL;

    //把current node從Empty List移除 
    for (current=Empty->head.next; current != NULL; current=current->next){
        if (current->blk == blk){
            if (current == First_Node){
                Empty->head.next = current->next;
                current->next = NULL;
            }else{
                Previous->next = current->next;
                current->next = NULL;
            }
            Empty->size--;
            // printf("Move Node %d, vpc %d, ipc %d, epc %d\n", current->blk->id, current->blk->vpc, current->blk->ipc, current->blk->epc);
            break;
        }
        Previous = current;
    }
    //更新Empty List的狀態
    // printf("576 Before EmptyList(%d) state : vpc %d, ipc %d, epc %d\n",Array->array_id, Empty->vpc, Empty->ipc, Empty->epc);
    Empty->vpc = Empty->vpc - current->blk->vpc;
    Empty->ipc = Empty->ipc - current->blk->ipc;
    Empty->epc = Empty->epc - current->blk->epc;
    // printf("576 After EmptyList(%d) state : vpc %d, ipc %d, epc %d\n",Array->array_id, Empty->vpc, Empty->ipc, Empty->epc);

    //把current node加入NonEmpty List
    if (NonEmpty->head.next == NULL){
        NonEmpty->head.next = current;
        NonEmpty->size++;
    }else{
        struct Node *current2 = NULL;
        for (current2=NonEmpty->head.next; current2->next!=NULL; current2=current2->next){
            //idle
        }
        current2->next = current;
        NonEmpty->size++;
    }
    blk->position = IN_NonEmpty;
    //更新NonEmpty List的狀態
    // printf("596 Before NonEmptyList(%d) state : vpc %d, ipc %d, epc %d\n",Array->array_id, NonEmpty->vpc, NonEmpty->ipc, NonEmpty->epc);
    NonEmpty->vpc = NonEmpty->vpc + current->blk->vpc;
    NonEmpty->ipc = NonEmpty->ipc + current->blk->ipc;
    NonEmpty->epc = NonEmpty->epc + current->blk->epc;
    // printf("600 After NonEmptyList(%d) state : vpc %d, ipc %d, epc %d\n",Array->array_id, NonEmpty->vpc, NonEmpty->ipc, NonEmpty->epc);

    //更新統計的資料
    Total_Empty_Block--;

    // 自我檢測
    if (Empty->vpc < 0 || NonEmpty->vpc < 0){
        printf("605 err\n");
        abort();
    }
    if (Empty->ipc < 0 || NonEmpty->ipc < 0){
        printf("609 err\n");
        abort();
    }
    if (Empty->epc < 0 || NonEmpty->epc < 0){
        printf("613 err\n");
        abort();
    }
}

static void Rmove_Block_To_NonEmpty(struct nand_block *blk)
{
    int ArrayList_Position = blk->block_id / Blocks_per_linkedList;
    
    Remove_Block_From_EmptyList(ArrayList_Position, blk);
    // printf("Remove Block %d, position %d\n", blk->id, blk->position);
}

static void Remove_Block_To_Empty(struct nand_block *blk) //int victim_blk_vpc, int victim_blk_ipc, int victim_blk_epc
{
    /* 
    這裡要注意一點，從NonEmpty list移除block時，NonEmpty list vpc、ipc、epc是要減去gc前victim block的vpc、ipc、epc。
    而從Empty List加入Block時，Empty List vpc ipc epc 是要加gc後victim block的vpc、ipc、epc。
    victim_blk_vpc、victim_blk_ipc、victim_blk_epc 是gc前victim block的vpc、ipc、epc。
    blk.vpc、blk.ipc、blk.epc 是gc後blk的vpc、ipc、epc。
    */

    //把blk從NonEmpty List搬到Empty List
    if (blk->position == IN_Empty){
        //printf("blk (%d), position %d, vpc %d, ipc %d, epc %d\n", blk->block_id, blk->position, blk->vpc, blk->ipc, blk->epc);
        printf("606 err\n");
        abort();
    }

    int ArrayList_Position = blk->block_id / Blocks_per_linkedList;
    struct ArrayList *Array = &finder2.Array[ArrayList_Position];
    struct LinkedList *Empty = &Array->Empty;
    struct LinkedList *NonEmpty = &Array->NonEmpty;

    struct Node *First_Node = NonEmpty->head.next; 
    struct Node *current = NULL;
    struct Node *Previous = NULL;

    //把current node從NonEmpty List移除 
    for (current=NonEmpty->head.next; current != NULL; current=current->next){
        if (current->blk == blk){
            if (current == First_Node){
                NonEmpty->head.next = current->next;
                current->next = NULL;
            }else{
                Previous->next = current->next;
                current->next = NULL;
            }
            NonEmpty->size--;
            // printf("Move Node %d\n", current->blk->id);
            break;
        }
        Previous = current;
    }
    //更新NonEmpty List的狀態
    // printf("662 Before NonEmptyList(%d) state : vpc %d, ipc %d, epc %d\n",Array->array_id, NonEmpty->vpc, NonEmpty->ipc, NonEmpty->epc);
    NonEmpty->vpc = NonEmpty->vpc - blk->vpc;
    NonEmpty->ipc = NonEmpty->ipc - blk->ipc;
    NonEmpty->epc = NonEmpty->epc - blk->epc;
    // printf("666 After NonEmptyList(%d) state : vpc %d, ipc %d, epc %d\n",Array->array_id, NonEmpty->vpc, NonEmpty->ipc, NonEmpty->epc);

    //把current node加入Empty List
    if (Empty->head.next == NULL){
        Empty->head.next = current;
        Empty->size++;
    }else{
        struct Node *current2 = NULL;
        for (current2=Empty->head.next; current2->next!=NULL; current2=current2->next){
            //idle
        }
        current2->next = current;
        Empty->size++;
    }
    blk->position = IN_Empty;
    //更新Empty List的狀態
    // printf("682 Before EmptyList(%d) state : vpc %d, ipc %d, epc %d\n",Array->array_id, Empty->vpc, Empty->ipc, Empty->epc);
    Empty->vpc = Empty->vpc + blk->vpc;
    Empty->ipc = Empty->ipc + blk->ipc;
    Empty->epc = Empty->epc + blk->epc;
    // printf("686 After EmptyList(%d) state : vpc %d, ipc %d, epc %d\n",Array->array_id, Empty->vpc, Empty->ipc, Empty->epc);

    //更新統計的資料
    Total_Empty_Block++;

    // 自我檢測
    if (Empty->vpc < 0 || NonEmpty->vpc < 0){
        // printf("688 err\n");
        abort();
    }
    if (Empty->ipc < 0 || NonEmpty->ipc < 0){
        // printf("692 err\n");
        abort();
    }
    if (Empty->epc < 0 || NonEmpty->epc < 0){
        // printf("696 err\n");
        abort();
    }
}

/*static void Update_EmptyList(struct nand_block *blk) //, int total_victim_sublk_vpc, int total_victim_sublk_ipc
{
    int ArrayList_Position = blk->block_id / Blocks_per_linkedList;
    struct ArrayList *Array = &finder2.Array[ArrayList_Position];
    struct LinkedList *Empty = &Array->Empty;
    struct LinkedList *NonEmpty = &Array->NonEmpty;

    Empty->vpc = Empty->vpc - total_victim_sublk_vpc;
    Empty->ipc = Empty->ipc - total_victim_sublk_ipc;
    Empty->epc = Empty->epc + (total_victim_sublk_vpc+total_victim_sublk_ipc);
    printf("724 Update EmptyList vpc %d, ipc %d, epc %d\n", Empty->vpc, Empty->ipc, Empty->epc);
}*/

static void Move_Block_Position(struct nand_block *blk, int victim_blk_vpc, int victim_blk_ipc, int victim_blk_epc, int total_victim_sublk_vpc, int total_victim_sublk_ipc)
{
    /*
    Rmove_Block_To_NonEmpty()是當Mark Page Valid時，發現blk no empty page時，把blk從EmptyList移到NonEmptyList，所以在搬移前block.position一定在EmptyList。
    Remove_Block_To_Empty()是由do_gc進行觸發，這邊需要假設兩種狀況 :
        1. block position在Empty List，假如 block vpc 2、ipc 2、epc 2，此時block在finder1[0]找的到，所以有可能成為victim block，如果block position本來就在EmptyList那就不需要搬移，但是仍然需要更新EmptyList的資訊。
        2. 假如 block vpc 2、ipc 4、epc 0，此時block在finder1[1]找的到，而且block正常已經在NonEmpty List裡，所以當block進行過do_gc後一定會產生empty pg，所以block需要從NonEmpty List搬到Empty List。
    */

    //printf("759 blk(%d): position %d\n", blk->block_id, blk->position);
    //printf("760 處理前 blk(%d) vpc %d, ipc %d, epc %d\n",blk->block_id, victim_blk_vpc, victim_blk_ipc, victim_blk_epc);
    //printf("761 處理後 blk(%d) vpc %d, ipc %d, epc %d\n",blk->block_id, blk->vpc, blk->ipc, blk->epc);
    if (blk->position == IN_NonEmpty){
        Remove_Block_To_Empty(blk); //, victim_blk_vpc, victim_blk_ipc, victim_blk_epc
    }else{ // blk->position == IN_Empty
        // skip
        // Update_EmptyList(blk); //, total_victim_sublk_vpc, total_victim_sublk_ipc
    }
    //Remove_Block_To_Empty(blk, victim_blk_vpc, victim_blk_ipc, victim_blk_epc);
}

// Mark Page Valid 的操作
static void Mark_Page_Valid(struct ssd *ssd, struct ppa *ppa)
{
    struct nand_block *blk = get_blk(ssd, ppa);
    if (blk == NULL){
        printf("1029 err\n");
        abort();
    }

    struct nand_sublock *sublk = get_sublk(ssd, ppa);
    if (sublk == NULL){
        printf("1035 err\n");
        abort();
    }
    
    struct nand_page *pg = get_pg(ssd, ppa);
    if (pg == NULL){
        printf("1041 err\n");
        abort();
    }

    struct LinkedList *list = NULL;
    int ArrayList_Position = blk->block_id / Blocks_per_linkedList;
    if (blk->position == IN_Empty){
        list = &finder2.Array[ArrayList_Position].Empty;
    }else if (blk->position == IN_NonEmpty){
        list = &finder2.Array[ArrayList_Position].NonEmpty;
    }else{
        printf("1053 err\n");
    }
    
    // 更新Page狀態
    pg->state = VALID;
    
    // 更新Sublock狀態
    sublk->vpc++;
    sublk->epc--;
    if (sublk->epc < 0 || sublk->epc > pgs_per_sublk){
        printf("1062 err\n");
        abort();
    }

    // 更新Block狀態
    blk->vpc++;
    blk->epc--;
    if (blk->epc < 0 || blk->epc > pgs_per_blk){
        printf("1070 err\n");
        abort();
    }

    // 更新List狀態
    list->vpc++;
    list->epc--;
    if (list->epc < 0){
        printf("1078 err, ArrayList Position %d\n", ArrayList_Position);
        // Print_Finder2();
        abort();
    }

    // 更新統計資料
    Total_vpc++;
    Total_epc--;
    if (Total_epc < 0){
        printf("1087 err, ArrayList Position %d\n", ArrayList_Position);
        abort();
    }

    // 確認Block是否要從Empty List移到NonEmpty List
    if (blk->epc == 0){
        if ( (blk->vpc+blk->ipc!=pgs_per_blk) ){
            printf("844 err\n");
            abort();
        }
        // printf("Block %d is full\n", blk->block_id);
        Rmove_Block_To_NonEmpty(blk);
    }
}

// Mark Page Invalid 的操作
static int Check_Sublk_Victim(struct nand_sublock *sublk)
{
    if (sublk->ipc == pgs_per_sublk){
        return Victim;
    }

    double n = (sublk->vpc+sublk->ipc) / sublk->vpc;
    // printf("866 n = %f\n", n);
    if (n >= 2){
        return Victim;
    }else{
        return NonVictim;
    }
}

static void Mark_Page_Invalid(struct ssd *ssd, struct ppa *ppa)
{
    struct nand_block *blk = get_blk(ssd, ppa);
    if (blk == NULL){
        printf("1106 err\n");
        abort();
    }

    struct nand_sublock *sublk = get_sublk(ssd, ppa);
    if (sublk == NULL){
        printf("1112 err\n");
        abort();
    }
    
    struct nand_page *pg = get_pg(ssd, ppa);
    if (pg == NULL){
        printf("1118 err\n");
        abort();
    }

    struct LinkedList *list = NULL;
    int ArrayList_Position = blk->block_id / Blocks_per_linkedList;
    if (blk->position == IN_Empty){
        list = &finder2.Array[ArrayList_Position].Empty;
    }else if (blk->position == IN_NonEmpty){
        list = &finder2.Array[ArrayList_Position].NonEmpty;
    }else{
        printf("1129 err\n");
        abort();
    }

    // 更新Page
    pg->state = INVALID;
    
    // 更新sublk
    sublk->ipc++;
    sublk->vpc--;
    
    // 更新Block
    blk->ipc++;
    blk->vpc--;

    // 更新List
    list->ipc++;
    list->vpc--;

    // 更新統計的資料
    Total_ipc++;
    Total_vpc--;

    // 確認Sublk在Finder1的位置
    if (sublk->state == NonVictim){
        if ((sublk->vpc+sublk->ipc) == pgs_per_sublk){
            int previous_victim_sublk_cnt = blk->victim_sublk_count;
            int state = Check_Sublk_Victim(sublk);
            if (state == Victim){
                sublk->state = Victim;
                blk->victim_sublk_count++;
                blk->Nonvictim_sublk_count--;
                if (previous_victim_sublk_cnt == 0 && previous_victim_sublk_cnt != blk->victim_sublk_count){
                    Add_Block_Finder1(blk->victim_sublk_count, blk);
                }else{
                    if (blk->victim_sublk_count > nLayers_Finder1){
                        printf("664 err blk victim sublk count over Finder1 layer \n");
                        abort();
                    }
                    Change_Block_Position_Finder1(previous_victim_sublk_cnt, blk->victim_sublk_count, blk);
                }
            }
        }
    }
}

// 找Empty Page
static struct nand_page *Get_Empty_Page(struct nand_block *blk)
{
    struct nand_page *pg = NULL;
    
    for (int i=0; i<sublks_per_blk; i++){
        struct nand_sublock *sublk = &blk->sublk[i];

        if (sublk->epc > 0){
            // printf("sublk (%d): vpc %d, ipc %d, epc %d\n", sublk->id, sublk->vpc, sublk->ipc, sublk->epc);
            for (int j=0; j<pgs_per_sublk; j++){
                pg = &sublk->pg[j];
                // printf("pg %d, state %d\n", pg->id, pg->state);
                if (pg->state == EMPTY){
                    return pg;
                }       
            }
            printf("728 err sublk not find empty pg \n");
            abort();
        }
    }

    printf("727 err\n");
    return NULL;
}

static struct nand_block *Get_Empty_Block(int HotLevel)
{
    // 統一回傳第一個Block 
    struct LinkedList *List = &finder2.Array[HotLevel].Empty;
    if (List == NULL){
        printf("137 err\n");
    }
    // printf("896 ArrayList(%d) EmptyList vpc %d, ipc %d, epc %d\n", finder2.Array[HotLevel].array_id, List->vpc, List->ipc, List->epc);

    struct Node *node = NULL;
    if (List->head.next == NULL){
        // printf("142 ArrayList (%d) EmptyList is NULL\n", HotLevel);
    }else{
        node = List->head.next;
        if (node == NULL){
            printf("904 err\n");
            abort();
        }
        struct nand_block *blk = node->blk;
        // printf("908 blk (%d), vpc %d, ipc %d, epc %d\n", blk->block_id, blk->vpc, blk->ipc, blk->epc);
        if (blk == NULL){
            printf("175 blk NULL err\n");
            abort();
        }
        if ((blk->vpc+blk->ipc) == pgs_per_blk){
            printf("178 blk is full\n");
            abort();
        }
        if (blk->epc == 0){
            printf("913 err blk no empty pg\n");
            abort();
        }
        return blk;
    }
    return NULL;
}

static void HotLevel_Order(int *array, int HotLevel)
{
    if (HotLevel == nHotLevel-1){
        int index = 0;
        for (int i=HotLevel; i>=0; i--){
            array[index] = i;
            index++;
        }
    }else{
        for (int i=0; i<nHotLevel; i++){
            array[i] = (HotLevel+i) % nHotLevel;
        }
    }
}

static struct ppa Get_Empty_PPA(int HotLevel) // For General LBA
{
    if (Total_Empty_Block == 0){
        /*int r = Enforce_Clean_Block();
        if (r == OP_SUCCESSFUL){
            if (Total_Empty_Block == 0){
                printf("1087 err\n");
                abort();
            }
        }else{
            printf("1091 No Empty Block err\n");
            abort();
        }*/
        // Print_All_Block();
        printf("1091 No Empty Block err\n");
        abort();
    }

    // printf("lba original HotLevel %d\n", HotLevel);
    struct ppa ppa;

    int array[nHotLevel];
    HotLevel_Order(array, HotLevel);
    
    struct nand_block *blk = NULL;
    for (int i=0; i<nHotLevel; i++){
        blk = Get_Empty_Block(array[i]);
        if (blk != NULL){
            break;
        }
    }
    if (blk == NULL){
        printf("1302 No Empty Blk err\n");
        abort();
    }
    
    struct nand_page *pg = Get_Empty_Page(blk);
    if (pg == NULL){
        printf("1308 err\n");
        abort();
    }

    // 設定ppa
    ppa.g.ch = pg->addr.ch;
    ppa.g.lun = pg->addr.lun;
    ppa.g.pl = pg->addr.pl;
    ppa.g.blk = pg->addr.blk;
    ppa.g.sublk = pg->addr.sublk;
    ppa.g.pg = pg->addr.pg;
    return ppa;
}

static struct nand_page *Get_Empty_Page_From_Finder1(void)
{
    for (int n=0; n<nLayers_Finder1; n++){
        
        struct ArrayList_1 *Array = &finder1.Array[n];
        if (Array == NULL){
            printf("1063 err\n");
            abort();
        }
        //printf("Array(%d) ", Array->id);
        
        struct LinkedList_1 *List = &Array->list;
        //printf("List(%d) -> ", List->id);
        if (List == NULL){
            printf("1071 err\n");
            abort();
        }

        if (List->head.next == NULL){
            printf("NULL\n");
        }else{
            struct Node *current = NULL;
            for (current=List->head.next; current!=NULL; current=current->next){
                //printf("blk(%d) vpc %d, ipc %d, epc %d -> ", current->blk->block_id, current->blk->vpc, current->blk->ipc, current->blk->epc);
                if (current->blk->epc < 0){
                    printf("1072 err\n");
                    abort();
                }
                if (current->blk->epc > 0){
                    //printf("1087 Find\n");
                    struct nand_block *blk = current->blk;
                    for (int k=0; k<sublks_per_blk; k++){
                        struct nand_sublock *sublk = &blk->sublk[k];
                        if (sublk->epc < 0){
                            printf("1091 err\n");
                            abort();
                        }
                        if (sublk->epc > 0){
                            struct nand_page *pg = NULL;
                            for (int p=0; p<pgs_per_sublk; p++){
                                pg = &sublk->pg[p];
                                if (pg->state == EMPTY){
                                    return pg;
                                } 
                            }
                            if (pg == NULL){
                                printf("1103 err \n");
                                abort();
                            }
                        }
                    }
                }
            }
        }
    }
    return NULL;
}

static struct ppa Get_Empty_PPA_For_Senitive_LBA(int HotLevel) // For Sensitive LBA
{
    /*一開始先去Finder1找找看有沒有Empty Page，如果Finder1找不到，才用Get_Empty_PPA找Empty Page*/
    struct ppa ppa;
    struct nand_page *pg = Get_Empty_Page_From_Finder1();
    
    if (pg == NULL){
        ppa = Get_Empty_PPA(HotLevel);
    }else{
        ppa.g.ch = pg->addr.ch;
        ppa.g.lun = pg->addr.lun;
        ppa.g.pl = pg->addr.pl;
        ppa.g.blk = pg->addr.blk;
        ppa.g.sublk = pg->addr.sublk;
        ppa.g.pg = pg->addr.pg;
    }

    return ppa;
}

/*
int Is_Block_Full(struct nand_block *blk)
{
    if (blk->epc == 0){
        return TRUE;
    }else{
        return FALSE;
    }
}
*/

// GC的相關操作
static int should_do_gc(void)
{
    double threshold_percent = 0.5;
    double threshold = Total_Block * (1-threshold_percent);
    if (Total_Empty_Block < threshold){
        return TRUE;
    }else{
        return FALSE;
    }
}

static int Enforce_do_gc(void)
{
    double threshold_percent = 0.8;
    double threshold = Total_Block * (1-threshold_percent);
    if (Total_Empty_Block < threshold){
        return TRUE;
    }else{
        return FALSE;
    }
}

// GC的相關操作
static uint64_t GC_Write(struct ssd *ssd, struct nand_page *pg)
{
    // 取得valid pge的lba
    struct ppa old_ppa;
    old_ppa.g.ch = pg->addr.ch;
    old_ppa.g.lun = pg->addr.lun;
    old_ppa.g.pl = pg->addr.pl;
    old_ppa.g.blk = pg->addr.blk;
    old_ppa.g.sublk = pg->addr.sublk;
    old_ppa.g.pg = pg->addr.pg;
    uint64_t lpn = get_rmap(ssd, &old_ppa);
    //Mark_Page_Invalid(&old_ppa);
    set_rmap(ssd, &old_ppa, NO_SETTING); //把old_ppa指向non setting

    // 取得新的ppa
    // printf("1259 GC Write\n");
    int lba_HotLevel = pg->LBA_HotLevel;
    struct ppa new_ppa = Get_Empty_PPA(lba_HotLevel);
    set_maplba(ssd, lpn, &new_ppa);
    set_rmap(ssd, &new_ppa, lpn);

    struct nand_block *blk = get_blk(ssd, &new_ppa);
    if (blk == NULL){
        printf("948 err\n");
    }

    struct nand_page *new_pg = get_pg(ssd, &new_ppa);
    if (new_pg == NULL){
        printf("953 err\n");
    }

    Mark_Page_Valid(ssd, &new_ppa);
    new_pg->LBA_HotLevel = lba_HotLevel;

    if ( (blk->ipc+blk->vpc)>pgs_per_blk ){
        printf("965 err \n");
        abort();
    }
    move_pg_cnt++;   

    /*if (ssd->sp.enable_gc_delay) {
        struct nand_cmd gcw;
        gcw.type = GC_IO;
        gcw.cmd = NAND_WRITE;
        gcw.stime = 0;
        ssd_advance_status(ssd, &new_ppa, &gcw);
    }*/

    // advance per-ch gc_endtime as well 
#if 0
    new_ch = get_ch(ssd, &new_ppa);
    new_ch->gc_endtime = new_ch->next_ch_avail_time;
#endif
    return 0;
}

static int Clean_One_Sublock(struct ssd *ssd, struct nand_block *blk ,struct nand_sublock *sublk)
{   
    if (sublk == NULL){
        printf("935 err\n");
        abort();
    }

    int enforce_clean_block_id = NO_SETTING;
    for (int i=0; i<pgs_per_sublk; i++){
        struct nand_page *pg = &sublk->pg[i];
        
        if (pg == NULL){
            printf("942 err\n");
            abort();
        }
        
        if (pg->state == VALID){
            // clean sublk 時因為會需要先搬移valid pg，所以有可能導致ssd space不足
            if (Total_epc == 0){ 
                // printf("1309 enforce clean block\n");
                enforce_clean_block_id = Enforce_Clean_Block(ssd, blk);
                if (enforce_clean_block_id == NO_SETTING){ // 應該要有enforce block id
                    printf("1311 err\n");
                    abort();
                }
            }
            if (enforce_clean_block_id != blk->block_id){ // 如果Enforce clean blk不是現在的blk，才需要做GC Write
                GC_Write(ssd, pg);
            }
        }
    }
    return enforce_clean_block_id;
}

static void Mark_Sublock_Free(struct nand_block *blk, struct nand_sublock *sublk)
{
    if (sublk == NULL){
        printf("993 err\n");
    }
    if (blk == NULL){
        printf("996 err\n");
    }

    // 更新block資訊，因為sublk之後的資料會被更新調，所以記得先記錄下來
    int sublk_vpc = sublk->vpc;
    int sublk_ipc = sublk->ipc;
    int used_pg = sublk_vpc + sublk_ipc;
    
    blk->vpc = blk->vpc - sublk_vpc;
    if (blk->vpc<0){
        printf("1003 err\n");
        abort();
    }
    blk->ipc = blk->ipc - sublk_ipc;
    if (blk->ipc<0){
        printf("1008 err\n");
        abort();
    }
    blk->epc = blk->epc + used_pg;
    if (blk->epc < 0 || blk->epc > pgs_per_blk){
        printf("1013 err\n");
        abort();
    }

    // 更新統計資料
    Total_vpc = Total_vpc - sublk->vpc;
    Total_ipc = Total_ipc - sublk->ipc;
    Total_epc = Total_epc + used_pg;

    // 更新sublk的資訊
    sublk->vpc = 0;
    sublk->ipc = 0;
    sublk->epc = pgs_per_sublk;
    sublk->state = NonVictim;
    sublk->have_invalid_sensitive_page = FALSE;

    // 更新page的資訊
    for (int i=0; i<pgs_per_sublk; i++){
        struct nand_page *pg = &sublk->pg[i];
        if (pg == NULL){
            printf("1027 err\n");
            abort();
        }
        pg->state = EMPTY;
        pg->LBA_HotLevel = NO_SETTING;
        pg->type = NO_SETTING;
    }

    // 更新Block所在的List資訊
    int ArrayList_Position = blk->block_id / Blocks_per_linkedList;
    struct ArrayList *Array = &finder2.Array[ArrayList_Position];
    struct LinkedList *List = NULL;

    if (blk->position == IN_Empty){
        List = &Array->Empty;
    }else{
        List = &Array->NonEmpty;
    }
    List->vpc = List->vpc - sublk_vpc;
    List->ipc = List->ipc - sublk_ipc;
    List->epc = List->epc + used_pg;
}

static int do_gc(struct ssd *ssd, struct nand_block *secure_deletion_blk, int need_secure_deletion)
{
    //printf("do gc !!\n");
    // Print_Finder1();
    struct nand_block *victim_blk = NULL;
    if (need_secure_deletion == TRUE){
        //printf("1315 do secure deletion\n");
        victim_blk = secure_deletion_blk;
        if (victim_blk == NULL){
            printf("1314 err\n");
            abort();
        }
    }else{
        victim_blk = Get_Victim_Block_From_Finder1();
        if (victim_blk == NULL){
            printf("1323 No Victim Block\n");
            return Stop_GC;
        }
    }

    //printf("Victim Block %d\n", victim_blk->block_id);
    //printf("Victim Blokc vpc %d, ipc %d, epc %d\n", victim_blk->vpc, victim_blk->ipc, victim_blk->epc);
    //printf("Victim Block victim count %d, Nonvictim count %d, position %d\n", victim_blk->victim_sublk_count, victim_blk->Nonvictim_sublk_count, victim_blk->position);

    //int have_victim_sublk = FALSE;
    int victim_blk_vpc = victim_blk->vpc;
    int victim_blk_ipc = victim_blk->ipc;
    int victim_blk_epc = victim_blk->epc;

    int total_victim_sublk_vpc = 0;
    int total_victim_sublk_ipc = 0;

    // 確認enforce clean block 跟 GC blk，這兩個blk是否相同
    int enforce_clean_block_id = NO_SETTING;

    for (int i=0; i<sublks_per_blk; i++){
        struct nand_sublock *victim_sublk = &victim_blk->sublk[i];
        if (victim_sublk->state == Victim || victim_sublk->have_invalid_sensitive_page == TRUE){
            if (victim_sublk->have_invalid_sensitive_page == TRUE){
                // printf("1343 victim sublk(%d) have invalid pg, vpc %d, ipc %d, epc %d\n", victim_sublk->id, victim_sublk->vpc, victim_sublk->ipc, victim_sublk->epc);
            }

            // 將victim sublk vpc、ipc資訊記下來
            total_victim_sublk_vpc += victim_sublk->vpc;
            total_victim_sublk_ipc += victim_sublk->ipc;

            // have_victim_sublk = TRUE;
            if (enforce_clean_block_id != victim_blk->block_id){ // 檢查看看如果有觸發enforce clean blk，強制清除的blk跟現在GC的blk是否相同，如果相同本次GC就不用做了
                enforce_clean_block_id = Clean_One_Sublock(ssd, victim_blk, victim_sublk);
            }/*else{
                printf("1442 enforce blk id %d, victim blk id %d\n", enforce_clean_block_id, victim_blk->block_id);
                printf("1443 victim_blk(%d), vpc %d, ipc %d, epc %d", victim_blk->block_id, victim_blk->vpc, victim_blk->ipc, victim_blk->epc);
            }*/
            // printf("1237 After Clean Blk(%d), vpc %d, ipc %d, epc %d\n", victim_blk->block_id, victim_blk->vpc, victim_blk->ipc, victim_blk->epc);
            // printf("1238 After Clean Sublk(%d), vpc %d, ipc %d, epc %d\n", victim_sublk->id, victim_sublk->vpc, victim_sublk->ipc, victim_sublk->epc);            
            
            // 更新block資訊
            if (victim_sublk->state == Victim){ // 有可能gc victim sublk不是victim state，但是sublk have invalid sensitive page, 如果是因為有invalid sensitive pg的話，就不用更新blk info
                victim_blk->victim_sublk_count--;
                if (victim_blk->victim_sublk_count < 0){
                    printf("1363 err\n");
                    abort();
                }
                victim_blk->Nonvictim_sublk_count++;
                if (victim_blk->Nonvictim_sublk_count > sublks_per_blk){
                    printf("1368 err\n");
                    abort();
                }
            }

            if (enforce_clean_block_id != victim_blk->block_id){
                Mark_Sublock_Free(victim_blk, victim_sublk);
            }
            // printf("1241 After MarkFree Blk(%d), vpc %d, ipc %d, epc %d\n", victim_blk->block_id, victim_blk->vpc, victim_blk->ipc, victim_blk->epc);
            // printf("1242 After MarkFree Sublk(%d), vpc %d, ipc %d, epc %d\n", victim_sublk->id, victim_sublk->vpc, victim_sublk->ipc, victim_sublk->epc);

            // 更新block position
            if (enforce_clean_block_id != victim_blk->block_id){
                Move_Block_Position(victim_blk, victim_blk_vpc, victim_blk_ipc, victim_blk_epc, total_victim_sublk_vpc, total_victim_sublk_ipc);
            }
        }
    }

    /* 不需要擔心block在finder1的位置，finder1是根據block的victim sublk cnt來決定blk的存放位置，Get_Victim_Block_From_Finder1()回傳victim block後，victim block是已經從finder1移除了，而經過do_gc()處理後  
    victim block所有的victim sublk都會被清除，所以victim block經過do_gc()後也就不應該再finder1了。*/

    // printf("Clean blk(%d), vpc %d, ipc %d, epc %d, position %d\n", victim_blk->block_id, victim_blk->vpc, victim_blk->ipc, victim_blk->epc, victim_blk->position);
    // Print_Finder1();
    // printf("do gc over\n");

    return Finish_GC;
}

static void do_secure_deletion(struct ssd *ssd, struct ppa *secure_deletion_table, int index)
{
    for (int i=0; i<index; i++){
        struct nand_block *victim_blk = get_blk(ssd, &secure_deletion_table[i]);
        if (victim_blk == NULL){
            printf("1392 err\n");
            abort();
        }
        // 要先把victim blk從finder1移除
        if (victim_blk->victim_sublk_count > 0){
            // printf("1403 Blk(%d) vpc %d ipc %d epc %d vsubc %d\n", victim_blk->block_id, victim_blk->vpc, victim_blk->ipc, victim_blk->epc, victim_blk->victim_sublk_count);
            Remove_Block_Finder1(victim_blk->victim_sublk_count, victim_blk);
        }
        do_gc(ssd, victim_blk, TRUE);
    }
}

static void ssd_trim(struct ssd *ssd, NvmeRequest *req)
{
    uint64_t lba = req->slba;
    struct ssdparams *spp = &ssd->sp;
    int len = req->nlb;
    uint64_t start_lpn = lba / spp->secs_per_pg;
    uint64_t end_lpn = (lba + len - 1) / spp->secs_per_pg;  
    
    // secure deletion會用到的變數
    int need_do_secure_deletion = FALSE;
    int *secure_deletion_map = (int *)malloc(Total_Block * sizeof(int));
    for (int block_id=0; block_id<Total_Block; block_id++){
            secure_deletion_map[block_id] = 0;
    }
    int index = 0;
    struct ppa *secure_deletion_table = (struct ppa *)malloc(len * sizeof(struct ppa));

    for (uint64_t lpn=start_lpn; lpn<=end_lpn; lpn++){
        
        if (mapped(ssd, lpn) == MAPPED){
            //printf("start trim !!\n");

            struct ppa *ppa = get_maplba(ssd, lpn);
            set_rmap(ssd, ppa, NO_SETTING);

            // 找出Block 
            struct nand_block *blk = get_blk(ssd, ppa);
            if (blk == NULL){
                printf("1442 err\n");
                abort();
            }

            // 找出Sublock
            struct nand_sublock *sublk = get_sublk(ssd, ppa);
            if (sublk == NULL){
                printf("1448 err\n");
                abort();
            }

            // 找出Page 
            struct nand_page *pg = get_pg(ssd, ppa);
            if (pg == NULL){ 
                printf("1454 err\n"); 
                abort();
            }

            // 確認要不要做secure deletion
            if (pg->type == Sensitive){
                need_do_secure_deletion = TRUE;
                if (secure_deletion_map[blk->block_id] == 0){ // block尚未紀錄
                    
                    secure_deletion_map[blk->block_id] = 1;
                    struct ppa blk_ppa;
                    blk_ppa.g.ch = pg->addr.ch;
                    blk_ppa.g.lun = pg->addr.lun;
                    blk_ppa.g.pl = pg->addr.pl;
                    blk_ppa.g.blk = pg->addr.blk;
                    secure_deletion_table[index] = blk_ppa;
                    index++;
                }
            }

            // Mark Page Invalid 
            Mark_Page_Invalid(ssd, ppa);

            // 如果Page是Sensitive Type的話，需要把Page的Sublock->have_invalid_sensitive_page變成TRUE
            if (pg->type == Sensitive){
                sublk->have_invalid_sensitive_page = TRUE;
            }

            // 更新ppa
            clear_maplba(ssd, lpn);
        }
    }

    // 執行secure deletion
   if (need_do_secure_deletion == TRUE){
        do_secure_deletion(ssd, secure_deletion_table, index);
   }
   free(secure_deletion_table);
}

// Over Provisioning 的操作
static void Move_VictimBlk_Vpc_To_OP(struct ssd *ssd, struct nand_page *pg, struct nand_page *temp_pg)
{
    // 取得valid pge的lba
    struct ppa old_ppa;
    old_ppa.g.ch = pg->addr.ch;
    old_ppa.g.lun = pg->addr.lun;
    old_ppa.g.pl = pg->addr.pl;
    old_ppa.g.blk = pg->addr.blk;
    old_ppa.g.sublk = pg->addr.sublk;
    old_ppa.g.pg = pg->addr.pg;
    uint64_t lpn = get_rmap(ssd, &old_ppa);

    // 交換資料
    // 把victim blk的pg addr記在OP temp pg，是因為當從OP把pg存回清除過後的victim blk時，需要知道valid pg原本對應的lba是哪個
    temp_pg->addr.ch = pg->addr.ch;
    temp_pg->addr.lun = pg->addr.lun;
    temp_pg->addr.pl = pg->addr.pl;
    temp_pg->addr.blk = pg->addr.blk;
    temp_pg->addr.sublk = pg->addr.sublk;
    temp_pg->addr.pg = pg->addr.pg;
    
    temp_pg->state = pg->state;
    temp_pg->LBA_HotLevel = pg->LBA_HotLevel;
    temp_pg->type = pg->type;


    // 清除lba和ppa之間的關係
    clear_maplba(ssd, lpn);

    // 更新OP Block的資料
    struct nand_block *op_blk = &OP.blk[0];
    op_blk->vpc++;
    op_blk->epc--;
    if (op_blk->vpc > pgs_per_blk){
        printf("1581 err\n");
        abort();
    }
    if (op_blk->epc < 0){
        printf("1587 err\n");
        abort();
    }

    // 更新WA資料
    move_pg_cnt++;
}

static struct Node* OP_remove_block_from_emptyList(struct nand_block *victim_blk)
{
    // 把victim blk從Empty List移除
    int ArrayList_Position = victim_blk->block_id / Blocks_per_linkedList;
    struct ArrayList *Array = &finder2.Array[ArrayList_Position];
    struct LinkedList *Empty = &Array->Empty;
    struct LinkedList *NonEmpty = &Array->NonEmpty;
    struct Node *First_Node = Empty->head.next; 
    struct Node *current = NULL;
    struct Node *Previous = NULL;

    if (First_Node == NULL){
        printf("1584 err\n");
        abort();
    }

    for (current=Empty->head.next; current!=NULL; current=current->next){
        if (current->blk == victim_blk){
            if (current == First_Node){
                Empty->head.next = current->next;
                current->next = NULL;
            }else{
                Previous->next = current->next;
                current->next = NULL;
            }
            Empty->size--;
            // printf("1610 OP Move Node %d, vpc %d, ipc %d, epc %d\n", current->blk->id, current->blk->vpc, current->blk->ipc, current->blk->epc);
            break;
        }
        Previous = current;
    }

    // 更新Empty List的info
    // printf("1617 OP Before EmptyList(%d) state : vpc %d, ipc %d, epc %d\n",Array->array_id, Empty->vpc, Empty->ipc, Empty->epc);
    Empty->vpc = Empty->vpc - victim_blk->vpc;
    Empty->ipc = Empty->ipc - victim_blk->ipc;
    Empty->epc = Empty->epc - victim_blk->epc;
    // printf("1621 OP After EmptyList(%d) state : vpc %d, ipc %d, epc %d\n",Array->array_id, Empty->vpc, Empty->ipc, Empty->epc);

    //更新統計的資料
    Total_Empty_Block--;

    // 自我檢測
    if (Empty->vpc < 0 || NonEmpty->vpc < 0){
        printf("1658 err\n");
        abort();
    }
    if (Empty->ipc < 0 || NonEmpty->ipc < 0){
        printf("1662 err\n");
        abort();
    }
    if (Empty->epc < 0 || NonEmpty->epc < 0){
        printf("1666 err\n");
        abort();
    }

    return current;
}

static struct Node *OP_remove_block_from_nonemptyList(struct nand_block *victim_blk)
{
    // 把victim blk從nonEmpty list移除
    int ArrayList_Position = victim_blk->block_id / Blocks_per_linkedList;
    struct ArrayList *Array = &finder2.Array[ArrayList_Position];
    struct LinkedList *Empty = &Array->Empty;
    struct LinkedList *NonEmpty = &Array->NonEmpty;

    struct Node *First_Node = NonEmpty->head.next; 
    struct Node *current = NULL;
    struct Node *Previous = NULL;

    if (First_Node == NULL){
        printf("1623 err \n");
        abort();
    }

    //把current node從NonEmpty List移除 
    for (current=NonEmpty->head.next; current != NULL; current=current->next){
        if (current->blk == victim_blk){
            if (current == First_Node){
                NonEmpty->head.next = current->next;
                current->next = NULL;
            }else{
                Previous->next = current->next;
                current->next = NULL;
            }
            NonEmpty->size--;
            // printf("1667 OP Move Node %d, vpc %d, ipc %d, epc %d\n", current->blk->id, current->blk->vpc, current->blk->ipc, current->blk->epc);
            break;
        }
        Previous = current;
    }

    //更新NonEmpty List的狀態
    // printf("1674 OP Before NonEmptyList(%d) state : vpc %d, ipc %d, epc %d\n",Array->array_id, NonEmpty->vpc, NonEmpty->ipc, NonEmpty->epc);
    NonEmpty->vpc = NonEmpty->vpc - victim_blk->vpc;
    NonEmpty->ipc = NonEmpty->ipc - victim_blk->ipc;
    NonEmpty->epc = NonEmpty->epc - victim_blk->epc;
    // printf("1678 OP After NonEmptyList(%d) state : vpc %d, ipc %d, epc %d\n",Array->array_id, NonEmpty->vpc, NonEmpty->ipc, NonEmpty->epc);

    // 自我檢測
    if (Empty->vpc < 0 || NonEmpty->vpc < 0){
        printf("1685 err\n");
        abort();
    }
    if (Empty->ipc < 0 || NonEmpty->ipc < 0){
        printf("1689 err\n");
        abort();
    }
    if (Empty->epc < 0 || NonEmpty->epc < 0){
        printf("1693 err\n");
        abort();
    }

    return current;
}

static void OP_Mark_Free_Block(struct nand_block *victim_block)
{
    // 更新統計資訊
    Total_vpc = Total_vpc - victim_block->vpc;
    Total_ipc = Total_ipc - victim_block->ipc;
    Total_epc = Total_epc + (victim_block->vpc + victim_block->ipc);

    for (int s=0; s<sublks_per_blk; s++){
        struct nand_sublock *sublk = &victim_block->sublk[s];
        
        for (int p=0; p<pgs_per_sublk; p++){
            // 更新page的資料
            struct nand_page *pg = &sublk->pg[p];
            pg->state = EMPTY;
            pg->LBA_HotLevel = NO_SETTING;
            pg->type = NO_SETTING;
        }

        // 更新sublk的資訊
        sublk->vpc = 0;
        sublk->ipc = 0;
        sublk->epc = pgs_per_sublk;
        sublk->state = NonVictim;
        sublk->have_invalid_sensitive_page = FALSE;
    }

    // 更新blk的資訊
    victim_block->vpc = 0;
    victim_block->ipc = 0;
    victim_block->epc = pgs_per_blk;
    victim_block->victim_sublk_count = 0;
    victim_block->Nonvictim_sublk_count = sublks_per_blk;
    victim_block->position = IN_Empty;
}

static void OP_get_pg_to_VictimBlk(struct ssd *ssd, struct nand_page *op_pg, struct nand_page *pg, struct nand_block *blk, struct nand_sublock *sublk)
{
    // Page 之間交換資料
    pg->state = op_pg->state;
    pg->LBA_HotLevel = op_pg->LBA_HotLevel;
    pg->type = op_pg->type;

    // 設定lba 和 ppa的關係
    struct ppa old_ppa;
    old_ppa.g.ch = op_pg->addr.ch;
    old_ppa.g.lun = op_pg->addr.lun;
    old_ppa.g.pl = op_pg->addr.pl;
    old_ppa.g.blk = op_pg->addr.blk;
    old_ppa.g.sublk = op_pg->addr.sublk;
    old_ppa.g.pg = op_pg->addr.pg;
    int lba = get_rmap(ssd, &old_ppa);

    struct ppa new_ppa;
    new_ppa.g.ch = pg->addr.ch;
    new_ppa.g.lun = pg->addr.lun;
    new_ppa.g.pl = pg->addr.pl;
    new_ppa.g.blk = pg->addr.blk;
    new_ppa.g.sublk = pg->addr.sublk;
    new_ppa.g.pg = pg->addr.pg;

    // 設定lba新的對應ppa
    set_maplba(ssd, lba, &new_ppa);
    set_rmap(ssd, &new_ppa, lba);
    
    // 更新sublk
    sublk->vpc++;
    sublk->epc--;
    // 更新blk
    blk->vpc++;
    blk->epc--;
    // 更新統計資料
    Total_vpc++;
    Total_epc--;
    // 更新WA資料
    move_pg_cnt++;
}

static void OP_move_victim_blk_to_emptyList(struct Node *current)
{
    struct nand_block *victim_blk = current->blk;
    int ArrayList_Position = victim_blk->block_id / Blocks_per_linkedList;
    struct ArrayList *Array = &finder2.Array[ArrayList_Position];
    struct LinkedList *Empty = &Array->Empty;

    if (Empty->head.next == NULL){
        // printf("list first add node \n");
        Empty->head.next = current;
    }else{
        struct Node *Temp =NULL;
        for (Temp=Empty->head.next; Temp->next!=NULL; Temp=Temp->next){
            /* idle */
        }
        Temp->next = current;
        // printf("add over\n");
    }

    // 更新Emprt List的資訊
    Empty->vpc = Empty->vpc + current->blk->vpc;
    Empty->ipc = Empty->ipc + current->blk->ipc;
    Empty->epc = Empty->epc + current->blk->epc;
    Empty->size++;

    //更新統計的資料
    Total_Empty_Block++;
}

static void OP_mark_op_free(void)
{
    // Free OP block
    struct nand_block *blk = &OP.blk[0];
    blk->id = OP_ID;
    blk->block_id = OP_ID;
    blk->vpc = 0;
    blk->ipc = 0;
    blk->epc = pgs_per_blk;
    blk->victim_sublk_count = 0;
    blk->Nonvictim_sublk_count = sublks_per_blk;
    blk->position = NO_SETTING;
    // 初始化Sublock
    for (int s=0; s<sublks_per_blk; s++){
        struct nand_sublock *sublk = &blk->sublk[s];
        sublk->id = s;
        sublk->vpc = 0;
        sublk->ipc = 0;
        sublk->epc = pgs_per_sublk;
        sublk->state = NonVictim;
        sublk->have_invalid_sensitive_page = FALSE;
        for (int p=0; p<pgs_per_sublk; p++){
            struct nand_page *pg = &sublk->pg[p];
            pg->addr.ch = NO_SETTING;
            pg->addr.lun = NO_SETTING;
            pg->addr.pl = NO_SETTING;
            pg->addr.blk = NO_SETTING;
            pg->addr.sublk = NO_SETTING;
            pg->addr.pg = NO_SETTING;
            pg->id = p;
            pg->state = NO_SETTING;
            pg->LBA_HotLevel = NO_SETTING;
            pg->type = NO_SETTING;
        }
    }
}

static int Enforce_Clean_Block(struct ssd *ssd, struct nand_block *victim_blk)
{
    int need_check_finder = TRUE;
    if (victim_blk != NULL){
        // 因為如果victim blk有指定，說明是從Clean_One_Sublock()觸發的，而Clean_One_Sublock()地victim blk是從do_gc傳來的，do_gc在傳來前就把victim blk在finedr1移除了
        need_check_finder = FALSE; 
    }
    
    // 有兩種方法和OP做資料交換, 1. 有指定victim_blk, 只會由Clean_One_Sublock()觸發 2. 不指定 NULL 就需要自己找victim blk
    // printf("1904 start enforce clean block\n");
    if (victim_blk == NULL){
        // 找出invalid pg最多的Block
        struct ppa ppa;
        int Max_ipc = 0;
        for (uint64_t ch=0; ch<nchs; ch++){
            for (uint64_t lun=0; lun<luns_per_ch; lun++){
                for (uint64_t pl=0; pl<pls_per_lun; pl++){
                    for (uint64_t blk=0; blk<blks_per_pl; blk++){
                        ppa.g.ch = ch;
                        ppa.g.lun = lun;
                        ppa.g.pl = pl;
                        ppa.g.blk = blk;

                        struct nand_block *blk = get_blk(ssd, &ppa);
                        if (Max_ipc < blk->ipc){
                            victim_blk = blk;
                            Max_ipc = blk->ipc;
                        }
                    }
                }
            }
        }
    }

    if (victim_blk == NULL){
        printf("2100 err\n");
        // Print_All_Block();
        if (Total_ipc == 0 && Total_epc == 0){
            printf("2103 err SSD Space no enough !!\n");
        }
        abort();
    }

    if (victim_blk->ipc == 0){
        printf("2109 err\n");
        abort();
    }
    // printf("1582 Enforce clean victim blk(%d) : vpc %d, ipc %d, epc %d\n", victim_blk->block_id, victim_blk->vpc, victim_blk->ipc, victim_blk->epc);
    
    // 判斷victim blk是否有需要從Finder1移除
    if (need_check_finder == TRUE){
        int victim_sublk_cnt = 0;
        for (int i=0 ;i<sublks_per_blk; i++){
            struct nand_sublock *sublk = &victim_blk->sublk[i];
            if (sublk->state == Victim){
                victim_sublk_cnt++;
            }
        }

        if (victim_sublk_cnt!=0){
            // printf("1918 OP victim blk have %d victim sublk. \n", victim_sublk_cnt);
            Remove_Block_Finder1(victim_sublk_cnt, victim_blk);
        }
    }
    
    // 先把 victim blk從Empty List or NonEmpty List 移除，記得list的狀態要更新
    struct Node *current = NULL;
    if (victim_blk->position == IN_Empty){
        current = OP_remove_block_from_emptyList(victim_blk);
    }else if(victim_blk->position == IN_NonEmpty){
        current = OP_remove_block_from_nonemptyList(victim_blk);
    }else{
        printf("2137 err\n");
        abort();
    }
    if (current == NULL){
        printf("2141 err\n");
        abort();
    }
    // printf("1876 OP Move Node %d, vpc %d, ipc %d, epc %d\n", current->blk->id, current->blk->vpc, current->blk->ipc, current->blk->epc);
    
    // 把victim blk的valid pg移到 OP
    int OP_sublk_index = 0;
    int OP_pg_index = 0;
    for (int s =0; s<sublks_per_blk; s++){
        struct nand_sublock *sublk = &victim_blk->sublk[s];
        /*if (sublk->state == Victim){
            Print_All_Block();
            printf("1609 warning, Have sublk is victim state .\n");
            abort();
        }*/

        for (int p=0; p<pgs_per_sublk; p++){
            struct nand_page *pg = &sublk->pg[p];
            if (pg->state == VALID){
                struct nand_page *temp_pg = &OP.blk[0].sublk[OP_sublk_index].pg[OP_pg_index];
                Move_VictimBlk_Vpc_To_OP(ssd, pg, temp_pg);
                
                OP_pg_index++;
                if (OP_pg_index == pgs_per_sublk){
                    OP_sublk_index++;
                    OP_pg_index = 0;
                    
                    if (OP_sublk_index == sublks_per_blk){
                        printf("2169 err\n");
                        abort();
                    }
                }
            }
        }
    }
    
    // Mark Free victim blk
    OP_Mark_Free_Block(victim_blk);

    // 把OP的pg移回victim blk
    int sublk_id = 0;
    int pg_id = 0;
    struct nand_block *op_blk = &OP.blk[0];
    int stop = FALSE;

    for (int s=0; s<sublks_per_blk; s++){
        
        struct nand_sublock *op_sublk = &op_blk->sublk[s];
        for (int p=0; p<pgs_per_sublk; p++){
            
            struct nand_page *op_pg = &op_sublk->pg[p];
            if (op_pg->state == NO_SETTING){
                stop = TRUE;
                break;
            }

            struct nand_sublock *victim_sublk = &victim_blk->sublk[sublk_id];
            struct nand_page *victim_pg = &victim_blk->sublk[sublk_id].pg[pg_id];
            OP_get_pg_to_VictimBlk(ssd, op_pg, victim_pg, victim_blk, victim_sublk);
            
            pg_id++;
            if (pg_id == pgs_per_sublk){
                sublk_id++;
                pg_id = 0;
            }
        }
        
        if (stop == TRUE){
            break;
        } 
    }
    //printf("1966 sublk id %d, page id %d\n", sublk_id, pg_id);

    // 把victim blk移到對應的Empty list裡
    OP_move_victim_blk_to_emptyList(current);
    // Mark Free OP
    OP_mark_op_free();

    printf("OP finish, victim blk(%d) state vpc %d, ipc %d, epc %d\n", victim_blk->block_id, victim_blk->vpc, victim_blk->ipc, victim_blk->epc);
    //Print_Finder2();
    return victim_blk->block_id;
}

static int Check_whether_enforce_clean_blcok(void)
{
    if (Total_epc == 0){
        printf("2227 warning, Total epc = %d !!\n", Total_epc);
        /*if (Total_ipc == 0){
            printf("2007 err, ssd space no enough\n");
            abort();
        }*/
        return TRUE;   
    }
    return FALSE;
}

static uint64_t ssd_read(struct ssd *ssd, NvmeRequest *req)
{
    struct ssdparams *spp = &ssd->sp;
    uint64_t lba = req->slba;
    int nsecs = req->nlb;
    struct ppa *ppa;
    uint64_t start_lpn = lba / spp->secs_per_pg;
    uint64_t end_lpn = (lba + nsecs - 1) / spp->secs_per_pg;
    uint64_t lpn;
    uint64_t sublat, maxlat = 0;

    if (end_lpn >= spp->tt_pgs) {
        ftl_err("start_lpn=%"PRIu64",tt_pgs=%d\n", start_lpn, ssd->sp.tt_pgs);
    }

    // normal IO read path 
    for (lpn = start_lpn; lpn <= end_lpn; lpn++) {
        ppa = get_maplba(ssd, lpn);
        if (mapped(ssd, lpn) == UNMAPPED) {
            //printf("%s,lpn(%" PRId64 ") not mapped to valid ppa\n", ssd->ssdname, lpn);
            //printf("Invalid ppa,ch:%d,lun:%d,blk:%d,pl:%d,pg:%d,sec:%d\n",
            //ppa.g.ch, ppa.g.lun, ppa.g.blk, ppa.g.pl, ppa.g.pg, ppa.g.sec);
            continue;
        }

        struct nand_cmd srd;
        srd.type = USER_IO;
        srd.cmd = NAND_READ;
        srd.stime = req->stime;
        sublat = ssd_advance_status(ssd, ppa, &srd);
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
    uint64_t lpn;
    uint64_t curlat = 0, maxlat = 0;

    // secure deletion會用到的變數
   int secure_deletion_boundary_1 = 7500;
   int secure_deletion_boundary_2 = 8500;
   int need_do_secure_deletion = FALSE;
   int *secure_deletion_map = (int *)malloc(Total_Block * sizeof(int));
   for (int block_id=0; block_id<Total_Block; block_id++){
        secure_deletion_map[block_id] = 0;
   }
   int index = 0;
   struct ppa *secure_deletion_table = (struct ppa *)malloc(len * sizeof(struct ppa));


    if (end_lpn >= spp->tt_pgs) {
        ftl_err("start_lpn=%"PRIu64",tt_pgs=%d\n", start_lpn, ssd->sp.tt_pgs);
    }

    while (Enforce_do_gc() == TRUE){
        printf("2299\n");
        int r2 = do_gc(ssd, NULL, FALSE);
        
        if (r2 == Stop_GC){
            //Print_SSD_State();
            printf("2304 Warning Enforce GC Not Find Victim Blk\n");
            //printf("Total vpc %d, ipc %d, epc %d\n", Total_vpc, Total_ipc, Total_epc);
            if (Check_whether_enforce_clean_blcok() == TRUE){
                Enforce_Clean_Block(ssd, NULL);
            }
            break;
        }
    }

    for (lpn = start_lpn; lpn <= end_lpn; lpn++) {
        
        if (Check_whether_enforce_clean_blcok() == TRUE){
            Enforce_Clean_Block(ssd, NULL);
        }

        int is_new_lba = 1;
        int lba_HotLevel = 0;

        //printf("write lba %d\n", lba);
        if (lpn >= (Total_Block * pgs_per_blk)){
            printf("2324 err\n");
            abort();
        }

        if (mapped(ssd, lpn) == MAPPED){ // lba不是第一次寫入
            printf("mapped \n");
            // 設定lba、ppa的轉換
            struct ppa *ppa = get_maplba(ssd, lpn);
            set_rmap(ssd, ppa, NO_SETTING); // 因為lba會指向新的ppa，所以舊的ppa指向NO_SETTING

            // 找出Block 
            struct nand_block *blk = get_blk(ssd, ppa);
            if (blk == NULL){
                printf("2337 err\n");
            }

            // 找出Sublock
            struct nand_sublock *sublk = get_sublk(ssd, ppa);
            if (sublk == NULL){
                printf("2349 err\n");
            }

            // 找出Page 
            struct nand_page *pg = get_pg(ssd, ppa);
            if (pg == NULL){ 
                printf("2355 err\n"); 
            }

            // 確認要不要做secure deletion
            if (pg->type == Sensitive){
                need_do_secure_deletion = TRUE;
                if (secure_deletion_map[blk->block_id] == 0){ // block尚未紀錄
                    
                    secure_deletion_map[blk->block_id] = 1;
                    
                    struct ppa blk_ppa;
                    blk_ppa.g.ch = pg->addr.ch;
                    blk_ppa.g.lun = pg->addr.lun;
                    blk_ppa.g.pl = pg->addr.pl;
                    blk_ppa.g.blk = pg->addr.blk;
                    secure_deletion_table[index] = blk_ppa;
                    index++;
                }
            }

            // Mark Page Invalid 
            Mark_Page_Invalid(ssd, ppa);
            
            is_new_lba = 0;
            lba_HotLevel = pg->LBA_HotLevel;

            // 如果Page是Sensitive Type的話，需要把Page的Sublock->have_invalid_sensitive_page變成TRUE
            if (pg->type == Sensitive){
                sublk->have_invalid_sensitive_page = TRUE;
            }
        }

        if (is_new_lba == 0){
            if (++lba_HotLevel == nHotLevel){ // 限制lba Hotlevel上限
                lba_HotLevel = nHotLevel-1;
            }
        }

        // Sensitive、General LBA的存取
        struct ppa ppa;
        if (secure_deletion_boundary_1<=lba && lba<=secure_deletion_boundary_2){
            ppa = Get_Empty_PPA_For_Senitive_LBA(lba_HotLevel);
        }else{
            ppa = Get_Empty_PPA(lba_HotLevel);
        }
        set_maplba(ssd, lpn, &ppa);
        set_rmap(ssd, &ppa, lpn);

        struct nand_block *blk = get_blk(ssd, &ppa);
        if (blk == NULL){
            printf("2340 err\n");
        }

        struct nand_page *pg = get_pg(ssd, &ppa);
        if (pg == NULL){
            printf("2405 err\n");
        }

        Mark_Page_Valid(ssd, &ppa);
        // 設定page的屬性
        pg->LBA_HotLevel = lba_HotLevel;
        if (secure_deletion_boundary_1<=lba && lba<=secure_deletion_boundary_2){
            pg->type = Sensitive;
        }else{
            pg->type = General;
        }

        if ( (blk->ipc+blk->vpc)>pgs_per_blk ){
            printf("2418 err \n");
            abort();
        }
        //printf("\n");

        write_pg_cnt++;

        struct nand_cmd swr;
        swr.type = USER_IO;
        swr.cmd = NAND_WRITE;
        swr.stime = req->stime;
        // get latency statistics 
        curlat = ssd_advance_status(ssd, &ppa, &swr);
        maxlat = (curlat > maxlat) ? curlat : maxlat;
    }
    
    if (need_do_secure_deletion == TRUE){
        do_secure_deletion(ssd, secure_deletion_table, index);
    }
    free(secure_deletion_table);

    return maxlat;
}

static void *ftl_thread(void *arg)
{
    FemuCtrl *n = (FemuCtrl *)arg;
    struct ssd *ssd = n->ssd;
    NvmeRequest *req = NULL;
    uint64_t lat = 0;
    int rc;
    int i;

    while (!*(ssd->dataplane_started_ptr)) {
        usleep(100000);
    }

    // FIXME: not safe, to handle ->to_ftl and ->to_poller gracefully 
    ssd->to_ftl = n->to_ftl;
    ssd->to_poller = n->to_poller;

    while (1) {
        for (i = 1; i <= n->num_poller; i++) {
            if (!ssd->to_ftl[i] || !femu_ring_count(ssd->to_ftl[i]))
                continue;

            rc = femu_ring_dequeue(ssd->to_ftl[i], (void *)&req, 1);
            if (rc != 1) {
                printf("FEMU: FTL to_ftl dequeue failed\n");
            }

            ftl_assert(req);
            switch (req->cmd.opcode) {
            case NVME_CMD_WRITE:
                lat = ssd_write(ssd, req);
                if (should_do_gc() == TRUE){
                    do_gc(ssd, NULL, FALSE);
                }
                break;
            case NVME_CMD_READ:
                lat = ssd_read(ssd, req);
                break;
            case NVME_CMD_DSM:
                ssd_trim(ssd, req);
                break;
            default:
                //ftl_err("FTL received unkown request type, ERROR\n");
                ;
            }

            req->reqlat = lat;
            req->expire_time += lat;

            rc = femu_ring_enqueue(ssd->to_poller[i], (void *)&req, 1);
            if (rc != 1) {
                ftl_err("FTL to_poller enqueue failed\n");
            }
        }
    }
    return NULL;
}