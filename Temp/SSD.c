#include "SSD.h"

struct Finder1 finder1;
struct Finder2 finder2;
struct ppa *maplba; // maplba是lba -> ppa
int *rmap;  // rmap是ppa -> lba
struct SSD *ssd;
struct Over_Provisioning OP; 
double move_pg_cnt = 0;
double write_pg_cnt = 0;


// 初始化Node 
struct Node* init_node(struct Block *blk)
{
    struct Node *node = (struct Node*)malloc(sizeof(struct Node));
    node->blk = blk;
    node->next = NULL;
    return node;
}

// Finder1的操作
void Add_Node_Finder1(int Victim_Sublk_Cnt, struct Node *node)
{
    if (node == NULL){
        printf("node create false\n");
    }
    // printf("init node %d\n", node->blk->id);

    int index = Victim_Sublk_Cnt-1;
    if (index < 0){
        printf("49 err\n");
        abort();
    }
    struct LinkedList_1 *List = &finder1.Array[index].list;
    
    if (List->head.next == NULL){
        List->head.next = node;
    }else{
        struct Node *current =NULL;
        for (current=List->head.next; current->next!=NULL; current=current->next){
            /* idle */
        }
        current->next = node;
    }
}

void Add_Block_Finder1(int Victim_Sublk_Cnt, struct Block *blk)
{
    struct Node *node = init_node(blk);
    Add_Node_Finder1(Victim_Sublk_Cnt, node);
}

void Remove_Block_Finder1(int Victim_Sublk_Cnt, struct Block *blk)
{
    int index = Victim_Sublk_Cnt-1;
    if (index < 0){
        printf("53 err\n");
        abort();
    }
    struct LinkedList_1 *List = &finder1.Array[index].list;
    if (List->head.next == NULL){
        printf("58 err, Finder1 Array %d\n", index);
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
        printf("71 err Not find remove blk \n");
        abort();
    }else{
        current->next = NULL;
        free(current);
    }
}

void Change_Block_Position_Finder1(int Old_Victim_Sublk_Cnt, int New_Victim_Sublk_Cnt, struct Block *blk)
{   
    Remove_Block_Finder1(Old_Victim_Sublk_Cnt, blk);
    Add_Block_Finder1(New_Victim_Sublk_Cnt, blk);
}

void init_finder1(void)
{
    finder1.Array = (struct ArrayList_1*)malloc(sizeof(struct ArrayList_1) * nLayers_Finder1);
    for (int i=0; i<sublks_per_blk; i++){
        finder1.Array[i].id = i;
        finder1.Array[i].list.id = 0;
        finder1.Array[i].list.head.blk = NULL;
        finder1.Array[i].list.head.next = NULL;
    }
}

struct Block *Get_Victim_Block_From_Finder1(void)
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
        printf("118 Warning !! Not Find Victim Blk\n");
        /*沒有找到Victim Block不一定是錯誤的，因為有時候連續大批資料寫入寫會導致觸發了should_do_gc，但是所有的資料都是valid page，這時候就會發現沒有Victim Block可以處理*/
        return NULL;
    }

    struct Block *blk = current->blk;
    current->next = NULL;
    free(current);

    return blk;
}

// Finder2的操作
void Add_Node_Finder2(int HotLevel, struct Node *node)
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

void init_finder2(void)
{
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

void init_page(struct Page *pg, int id, int cid, int lid, int pid, int bid, int sublkid)
{
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

void init_sublock(struct Sublock *sublk, int id, int cid, int lid, int pid, int bid)
{
    sublk->id = id;
    sublk->pg = (struct Page*)malloc(sizeof(struct Page) * pgs_per_sublk);
    if (sublk->pg == NULL){
        printf("75 err\n");
    }
    for (int i=0; i<pgs_per_sublk; i++){
        init_page(&sublk->pg[i], i, cid, lid, pid, bid, sublk->id);    
    }
    sublk->vpc = 0;
    sublk->ipc = 0;
    sublk->epc = pgs_per_sublk;
    sublk->state = NonVictim;
    sublk->have_invalid_sensitive_page = FALSE;
}

void init_block(struct Block *blk, int id, int cid, int lid, int pid)
{
    blk->id = id;
    blk->sublk = (struct Sublock*)malloc(sizeof(struct Sublock) * sublks_per_blk);
    if (blk->sublk == NULL){
        printf("88 err\n");
    }
    for (int  i=0; i<sublks_per_blk; i++){
        init_sublock(&blk->sublk[i], i, cid, lid, pid, blk->id);
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
        printf("103 err\n");
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

void init_plane(struct Plane *pl, int id, int cid, int lid)
{
    pl->id = id;
    pl->blk = (struct Block*)malloc(sizeof(struct Block)*blks_per_pl);
    if (pl->blk == NULL){
        printf("276 err\n");
    }
    for (int i=0; i<blks_per_pl; i++){
        init_block(&pl->blk[i], i, cid, lid, pl->id);
    }
}

void init_lun(struct Lun *lun, int id, int cid)
{
    lun->id = id;
    lun->pl = (struct Plane*)malloc(sizeof(struct Plane)*pls_per_lun);
    if (lun->pl == NULL){
        printf("127 err\n");
    }
    for (int i=0; i<pls_per_lun; i++){
        init_plane(&lun->pl[i], id, cid, lun->id);
    }
}

void init_channel(struct Channel *ch, int id)
{
    ch->id = id;
    ch->lun = (struct Lun*)malloc(sizeof(struct Lun) * luns_per_ch);
    if (ch->lun == NULL){
        printf("136 err\n");
    }
    for (int i=0; i<luns_per_ch; i++){
        init_lun(&ch->lun[i], id, ch->id);
    }
}

void ssd_init(void)
{
    ssd = (struct SSD*)malloc(sizeof(struct SSD) *1);
    if (ssd == NULL){
        printf("144 err\n");
    }
    ssd->ch = (struct Channel*)malloc(sizeof(struct Channel) * nchs);
    if (ssd->ch == NULL){
        printf("148 err\n");
    }
    for (int i=0; i<nchs; i++){
        init_channel(&ssd->ch[i], i);
    }
    printf("SSD init finish \n");
}

// 初始化OP
void init_OP(void){
    // 初始化OP
    OP.size = 1;
    OP.blk = (struct Block *)malloc(OP.size * sizeof(struct Block));
    // 初始化Block
    struct Block *blk = &OP.blk[0];
    blk->sublk = (struct Sublock *)malloc(sublks_per_blk * sizeof(struct Sublock));
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
        struct Sublock *sublk = &blk->sublk[s];
        sublk->pg = (struct Page *)malloc(pgs_per_sublk * sizeof(struct Page));
        sublk->id = s;
        sublk->vpc = 0;
        sublk->ipc = 0;
        sublk->epc = pgs_per_sublk;
        sublk->state = NonVictim;
        sublk->have_invalid_sensitive_page = FALSE;
        for (int p=0; p<pgs_per_sublk; p++){
            struct Page *pg = &sublk->pg[p];
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

// maplba的操作
void set_maplba(int lba, struct ppa ppa)
{
    maplba[lba] = ppa;
    maplba[lba].state = MAPPED;
}

struct ppa *get_maplba(int lba)
{
    return &maplba[lba];
}

void clear_maplba(int lba)
{
    maplba[lba].state = UNMAPPED;
}

int mapped(int lba)
{
    if (maplba[lba].state == MAPPED){
        return MAPPED;
    }
    return UNMAPPED;
}

void init_maplba(void)
{
    int npgs = Total_Block * sublks_per_blk * pgs_per_sublk;
    maplba = (struct ppa *)malloc(sizeof(struct ppa) * npgs); 
    
    for (int i=0; i<npgs; i++){
        maplba[i].ch = NO_SETTING;
        maplba[i].lun = NO_SETTING;
        maplba[i].pl = NO_SETTING;
        maplba[i].blk = NO_SETTING;
        maplba[i].sublk = NO_SETTING;
        maplba[i].pg = NO_SETTING;
        maplba[i].state = UNMAPPED;
    }
}

void Print_Maplba(void)
{
    int npgs = Total_Block * sublks_per_blk * pgs_per_sublk;
    for (int lba=0; lba<npgs; lba++){
        printf("lba %d -> ppa ch %d, lun %d, pl %d, blk %d, sublk %d, pg %d . state %d\n", lba, maplba[lba].ch, maplba[lba].lun, maplba[lba].pl, maplba[lba].blk, maplba[lba].sublk, maplba[lba].pg, maplba[lba].state);
    }
}

// rmap的操作
int ppa2pgidx(struct ppa *ppa)
{
    if (ppa == NULL){
        printf("421 err\n");
        abort();
    }
    int index = 0;
    index = (pgs_per_ch*ppa->ch)+(pgs_per_lun*ppa->lun)+(pgs_per_pl*ppa->pl)+(pgs_per_blk*ppa->blk)+(pgs_per_sublk*ppa->sublk)+(ppa->pg);
    return index;
}

void set_rmap(struct ppa *ppa, int lba)
{
    int index = ppa2pgidx(ppa);
    rmap[index] = lba;
}

int get_rmap(struct ppa *ppa) // 回傳ppa對應的lba
{
    int index = ppa2pgidx(ppa);
    return rmap[index];
}

void init_rmap(void)
{
    int npgs = Total_Block * sublks_per_blk * pgs_per_sublk;
    rmap = (int*)malloc(sizeof(int)*npgs);

    for (int i=0 ;i<npgs; i++){
        rmap[i] = NO_SETTING;
    }
}

void Print_rmap(void)
{
    struct ppa ppa;
    for (int ch=0; ch<nchs; ch++){
        for (int lun=0; lun<luns_per_ch; lun++){
            for (int pl=0; pl<pls_per_lun; pl++){
                for (int blk=0; blk<blks_per_pl; blk++){
                    for (int sublk=0; sublk<sublks_per_blk; sublk++){
                        for (int pg=0; pg<pgs_per_sublk; pg++){
                            ppa.ch = ch;
                            ppa.lun = lun;
                            ppa.pl = pl;
                            ppa.blk = blk;
                            ppa.sublk = sublk;
                            ppa.pg = pg;
                            int lba = get_rmap(&ppa);
                            printf("ppa ch %d, lun %d, pl %d, blk %d, sublk %d, pg %d -> lba %d\n", ppa.ch, ppa.lun, ppa.pl, ppa.blk, ppa.sublk, ppa.pg, lba);
                        }
                    }
                }
            }
        }
    }
}

// 初始化
void Init(void)
{
    /*初始化*/
    init_finder1();
    init_finder2();
    ssd_init();
    // print_finder2();
    init_maplba();
    init_rmap();
    init_OP();
    printf("Init Finish \n");

}

struct Channel *get_ch(struct ppa *ppa)
{
    return &(ssd->ch[ppa->ch]);
}

struct Lun *get_lun(struct ppa *ppa)
{
    struct Channel *ch = get_ch(ppa);
    return &(ch->lun[ppa->lun]);
}

struct Plane *get_pl(struct ppa *ppa)
{
    struct Lun *lun = get_lun(ppa);
    return &(lun->pl[ppa->pl]);
}

struct Block *get_blk(struct ppa *ppa)
{
    struct Plane *pl = get_pl(ppa);
    return &(pl->blk[ppa->blk]);
}

struct Sublock *get_sublk(struct ppa *ppa)
{
    struct Block *blk = get_blk(ppa);
    return &(blk->sublk[ppa->sublk]);
}

struct Page *get_pg(struct ppa *ppa)
{
    struct Sublock *sublk = get_sublk(ppa);
    return &(sublk->pg[ppa->pg]);
}

// DeBug用的
void Print_List_Finder1(struct LinkedList_1 *list)
{
    struct Node *current = NULL;
    if (list->head.next == NULL){
        printf("NULL\n");
    }else{
        for (current=list->head.next; current!=NULL; current=current->next){
            printf("%d -> ", current->blk->id);
        }
        printf("NULL\n");
    }
}

void Print_Finder1(void)
{
    printf("Finder1 !!\n");
    for (int i=0; i<sublks_per_blk; i++){
        printf("ArrayList (%d) List -> ", finder1.Array[i].id);
        Print_List_Finder1(&finder1.Array[i].list);
    }
}

void print_list(int ArrayList_Position, int position)
{
    struct ArrayList *Temp_Array = &finder2.Array[ArrayList_Position];
    struct LinkedList *Temp_List = NULL;
    
    if (position == Empty_id){
        Temp_List = &Temp_Array->Empty;
        printf("Empty List vpc %d, ipc %d, epc %d\n", Temp_List->vpc, Temp_List->ipc, Temp_List->epc);
    }
    if (position == NonEmpty_id){
        Temp_List = &Temp_Array->NonEmpty;
        printf("NonEmpty List vpc %d, ipc %d, epc %d\n", Temp_List->vpc, Temp_List->ipc, Temp_List->epc);
    }

    if (Temp_List->head.next == NULL){
        if (Temp_List->list_id == NonEmpty_id){
            printf("NonEmpty List : NULL\n");
        }else if (Temp_List->list_id == Empty_id){
            printf("Empty List : NUL\n");
        }else{
            printf("115 err\n");
        }
    }else{    
        if (Temp_List->list_id == NonEmpty_id){
            printf("NonEmpty List : ");
        }else if (Temp_List->list_id == Empty_id){
            printf("Empty List : ");
        }else{
            printf("123 err\n");
        }

        struct Node *current =NULL;
        for (current=Temp_List->head.next; current->next!=NULL; current=current->next){
            if (current->blk->position != Temp_List->list_id){  
                printf("141 blk position err \n");
                abort();
            }
            printf("(Blk(%d), vpc %d, ipc %d, epc %d) -> ", current->blk->block_id, current->blk->vpc, current->blk->ipc, current->blk->epc); // 這裡不能用blk->id
        }
        printf("(Blk(%d), vpc %d, ipc %d, epc %d) -> NULL\n", current->blk->block_id, current->blk->vpc, current->blk->ipc, current->blk->epc);
    }
}

void Print_Finder2(void)
{
    printf("Finder2 !!\n");

    for (int i=0; i<nHotLevel; i++){
        struct ArrayList *Temp_Array = &finder2.Array[i];
        printf("ArrayList (%d)\n", Temp_Array->array_id);
        printf("EmptyList size = %d, NonEmptyList size = %d\n", Temp_Array->Empty.size, Temp_Array->NonEmpty.size);

        print_list(i, NonEmpty_id);
        print_list(i, Empty_id);
        printf("\n");
    }
}

void Print_All_Block(void)
{
    struct ppa ppa;
    for (int ch=0; ch<nchs; ch++){
        for (int lun=0; lun<luns_per_ch; lun++){
            for (int pl=0; pl<pls_per_lun; pl++){
                for (int blk=0; blk<blks_per_pl; blk++){
                    ppa.ch = ch;
                    ppa.lun = lun;
                    ppa.pl = pl;
                    ppa.blk = blk;
                    
                    struct Block *blk = get_blk(&ppa);
                    printf("Blk(%d) : vpc %d, ipc %d, epc %d, position %d : ", blk->block_id, blk->vpc, blk->ipc, blk->epc, blk->position);
                    for (int sublk=0; sublk<sublks_per_blk; sublk++){
                        ppa.sublk = sublk;
                        struct Sublock *sublk = get_sublk(&ppa);
                        printf("sublk(%d) [vpc %d, ipc %d, epc %d, state %d] -> ", sublk->id, sublk->vpc, sublk->ipc, sublk->epc, sublk->state);
                    }
                    printf("NULL\n");
                }
            }
        }
    }
}

void Print_SSD_State(void)
{
    printf("Start Print Finder2------------\n");
    Print_Finder2();
    
    printf("Start Print All Block------------\n");
    Print_All_Block();
    
    printf("Start Print Finder1------------\n");
    Print_Finder1();
    
    printf("Total Info---------------------\n");
    printf("Total Empty Block %d\n", Total_Empty_Block);
    printf("Total vpc %d, Total ipc %d, Total epc %d\n", Total_vpc, Total_ipc, Total_epc);
}

// Block在Finder2搬移的操作
void Remove_Block_From_EmptyList(int ArrayList_Position, struct Block *blk)
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
            printf("Move Node %d, vpc %d, ipc %d, epc %d\n", current->blk->id, current->blk->vpc, current->blk->ipc, current->blk->epc);
            break;
        }
        Previous = current;
    }
    //更新Empty List的狀態
    printf("576 Before EmptyList(%d) state : vpc %d, ipc %d, epc %d\n",Array->array_id, Empty->vpc, Empty->ipc, Empty->epc);
    Empty->vpc = Empty->vpc - current->blk->vpc;
    Empty->ipc = Empty->ipc - current->blk->ipc;
    Empty->epc = Empty->epc - current->blk->epc;
    printf("576 After EmptyList(%d) state : vpc %d, ipc %d, epc %d\n",Array->array_id, Empty->vpc, Empty->ipc, Empty->epc);

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
    printf("596 Before NonEmptyList(%d) state : vpc %d, ipc %d, epc %d\n",Array->array_id, NonEmpty->vpc, NonEmpty->ipc, NonEmpty->epc);
    NonEmpty->vpc = NonEmpty->vpc + current->blk->vpc;
    NonEmpty->ipc = NonEmpty->ipc + current->blk->ipc;
    NonEmpty->epc = NonEmpty->epc + current->blk->epc;
    printf("600 After NonEmptyList(%d) state : vpc %d, ipc %d, epc %d\n",Array->array_id, NonEmpty->vpc, NonEmpty->ipc, NonEmpty->epc);

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

void Rmove_Block_To_NonEmpty(struct Block *blk)
{
    int ArrayList_Position = blk->block_id / Blocks_per_linkedList;
    
    Remove_Block_From_EmptyList(ArrayList_Position, blk);
    printf("Remove Block %d, position %d\n", blk->id, blk->position);
}

void Remove_Block_To_Empty(struct Block *blk) //int victim_blk_vpc, int victim_blk_ipc, int victim_blk_epc
{
    /* 
    這裡要注意一點，從NonEmpty list移除block時，NonEmpty list vpc、ipc、epc是要減去gc前victim block的vpc、ipc、epc。
    而從Empty List加入Block時，Empty List vpc ipc epc 是要加gc後victim block的vpc、ipc、epc。
    victim_blk_vpc、victim_blk_ipc、victim_blk_epc 是gc前victim block的vpc、ipc、epc。
    blk.vpc、blk.ipc、blk.epc 是gc後blk的vpc、ipc、epc。
    */

    //把blk從NonEmpty List搬到Empty List
    if (blk->position == IN_Empty){
        printf("blk (%d), position %d, vpc %d, ipc %d, epc %d\n", blk->block_id, blk->position, blk->vpc, blk->ipc, blk->epc);
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
            printf("Move Node %d\n", current->blk->id);
            break;
        }
        Previous = current;
    }
    //更新NonEmpty List的狀態
    printf("662 Before NonEmptyList(%d) state : vpc %d, ipc %d, epc %d\n",Array->array_id, NonEmpty->vpc, NonEmpty->ipc, NonEmpty->epc);
    NonEmpty->vpc = NonEmpty->vpc - blk->vpc;
    NonEmpty->ipc = NonEmpty->ipc - blk->ipc;
    NonEmpty->epc = NonEmpty->epc - blk->epc;
    printf("666 After NonEmptyList(%d) state : vpc %d, ipc %d, epc %d\n",Array->array_id, NonEmpty->vpc, NonEmpty->ipc, NonEmpty->epc);

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
    printf("682 Before EmptyList(%d) state : vpc %d, ipc %d, epc %d\n",Array->array_id, Empty->vpc, Empty->ipc, Empty->epc);
    Empty->vpc = Empty->vpc + blk->vpc;
    Empty->ipc = Empty->ipc + blk->ipc;
    Empty->epc = Empty->epc + blk->epc;
    printf("686 After EmptyList(%d) state : vpc %d, ipc %d, epc %d\n",Array->array_id, Empty->vpc, Empty->ipc, Empty->epc);

    //更新統計的資料
    Total_Empty_Block++;

    // 自我檢測
    if (Empty->vpc < 0 || NonEmpty->vpc < 0){
        printf("688 err\n");
        abort();
    }
    if (Empty->ipc < 0 || NonEmpty->ipc < 0){
        printf("692 err\n");
        abort();
    }
    if (Empty->epc < 0 || NonEmpty->epc < 0){
        printf("696 err\n");
        abort();
    }
}

void Update_EmptyList(struct Block *blk) //, int total_victim_sublk_vpc, int total_victim_sublk_ipc
{
    int ArrayList_Position = blk->block_id / Blocks_per_linkedList;
    struct ArrayList *Array = &finder2.Array[ArrayList_Position];
    struct LinkedList *Empty = &Array->Empty;
    struct LinkedList *NonEmpty = &Array->NonEmpty;

    /*Empty->vpc = Empty->vpc - total_victim_sublk_vpc;
    Empty->ipc = Empty->ipc - total_victim_sublk_ipc;
    Empty->epc = Empty->epc + (total_victim_sublk_vpc+total_victim_sublk_ipc);*/
    printf("724 Update EmptyList vpc %d, ipc %d, epc %d\n", Empty->vpc, Empty->ipc, Empty->epc);
}

void Move_Block_Position(struct Block *blk, int victim_blk_vpc, int victim_blk_ipc, int victim_blk_epc, int total_victim_sublk_vpc, int total_victim_sublk_ipc)
{
    /*
    Rmove_Block_To_NonEmpty()是當Mark Page Valid時，發現blk no empty page時，把blk從EmptyList移到NonEmptyList，所以在搬移前block.position一定在EmptyList。
    Remove_Block_To_Empty()是由do_gc進行觸發，這邊需要假設兩種狀況 :
        1. block position在Empty List，假如 block vpc 2、ipc 2、epc 2，此時block在finder1[0]找的到，所以有可能成為victim block，如果block position本來就在EmptyList那就不需要搬移，但是仍然需要更新EmptyList的資訊。
        2. 假如 block vpc 2、ipc 4、epc 0，此時block在finder1[1]找的到，而且block正常已經在NonEmpty List裡，所以當block進行過do_gc後一定會產生empty pg，所以block需要從NonEmpty List搬到Empty List。
    */

    printf("759 blk(%d): position %d\n", blk->block_id, blk->position);
    printf("760 處理前 blk(%d) vpc %d, ipc %d, epc %d\n",blk->block_id, victim_blk_vpc, victim_blk_ipc, victim_blk_epc);
    printf("761 處理後 blk(%d) vpc %d, ipc %d, epc %d\n",blk->block_id, blk->vpc, blk->ipc, blk->epc);
    if (blk->position == IN_NonEmpty){
        Remove_Block_To_Empty(blk); //, victim_blk_vpc, victim_blk_ipc, victim_blk_epc
    }else{ // blk->position == IN_Empty
        Update_EmptyList(blk); //, total_victim_sublk_vpc, total_victim_sublk_ipc
    }
    //Remove_Block_To_Empty(blk, victim_blk_vpc, victim_blk_ipc, victim_blk_epc);
}

// Mark Page Valid 的操作
void Mark_Page_Valid(struct ppa *ppa)
{
    struct Block *blk = get_blk(ppa);
    if (blk == NULL){
        printf("590 err\n");
        abort();
    }

    struct Sublock *sublk = get_sublk(ppa);
    if (sublk == NULL){
        printf("596 err\n");
        abort();
    }
    
    struct Page *pg = get_pg(ppa);
    if (pg == NULL){
        printf("602 err\n");
        abort();
    }

    struct LinkedList *list = NULL;
    int ArrayList_Position = blk->block_id / Blocks_per_linkedList;
    if (blk->position == IN_Empty){
        list = &finder2.Array[ArrayList_Position].Empty;
    }else if (blk->position == IN_NonEmpty){
        list = &finder2.Array[ArrayList_Position].NonEmpty;
    }else{
        printf("613 err\n");
    }
    
    // 更新Page狀態
    pg->state = VALID;
    
    // 更新Sublock狀態
    sublk->vpc++;
    sublk->epc--;
    if (sublk->epc < 0 || sublk->epc > pgs_per_sublk){
        printf("740 err\n");
        abort();
    }

    // 更新Block狀態
    blk->vpc++;
    blk->epc--;
    if (blk->epc < 0 || blk->epc > pgs_per_blk){
        printf("748 err\n");
        abort();
    }

    // 更新List狀態
    list->vpc++;
    list->epc--;
    if (list->epc < 0){
        printf("756 err, ArrayList Position %d\n", ArrayList_Position);
        Print_Finder2();
        abort();
    }

    // 更新統計資料
    Total_vpc++;
    Total_epc--;
    if (Total_epc < 0){
        printf("764 err, ArrayList Position %d\n", ArrayList_Position);
        abort();
    }

    // 確認Block是否要從Empty List移到NonEmpty List
    if (blk->epc == 0){
        if ( (blk->vpc+blk->ipc!=pgs_per_blk) ){
            printf("844 err\n");
            abort();
        }
        printf("Block %d is full\n", blk->block_id);
        Rmove_Block_To_NonEmpty(blk);
    }
}

// Mark Page Invalid 的操作
int Check_Sublk_Victim(struct Sublock *sublk)
{
    if (sublk->ipc == pgs_per_sublk){
        return Victim;
    }

    double n = (sublk->vpc+sublk->ipc) / sublk->vpc;
    printf("866 n = %f\n", n);
    if (n >= 2){
        return Victim;
    }else{
        return NonVictim;
    }
}

void Mark_Page_Invalid(struct ppa *ppa)
{
    struct Block *blk = get_blk(ppa);
    if (blk == NULL){
        printf("479 err\n");
        abort();
    }

    struct Sublock *sublk = get_sublk(ppa);
    if (sublk == NULL){
        printf("485 err\n");
        abort();
    }
    
    struct Page *pg = get_pg(ppa);
    if (pg == NULL){
        printf("491 err\n");
        abort();
    }

    struct LinkedList *list = NULL;
    int ArrayList_Position = blk->block_id / Blocks_per_linkedList;
    if (blk->position == IN_Empty){
        list = &finder2.Array[ArrayList_Position].Empty;
    }else if (blk->position == IN_NonEmpty){
        list = &finder2.Array[ArrayList_Position].NonEmpty;
    }else{
        printf("502 err\n");
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
struct Page *Get_Empty_Page(struct Block *blk)
{
    struct Page *pg = NULL;
    
    for (int i=0; i<sublks_per_blk; i++){
        struct Sublock *sublk = &blk->sublk[i];

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

struct Block *Get_Empty_Block(int HotLevel)
{
    // 統一回傳第一個Block 
    struct LinkedList *List = &finder2.Array[HotLevel].Empty;
    if (List == NULL){
        printf("137 err\n");
    }
    printf("896 ArrayList(%d) EmptyList vpc %d, ipc %d, epc %d\n", finder2.Array[HotLevel].array_id, List->vpc, List->ipc, List->epc);

    struct Node *node = NULL;
    if (List->head.next == NULL){
        printf("142 ArrayList (%d) EmptyList is NULL\n", HotLevel);
    }else{
        node = List->head.next;
        if (node == NULL){
            printf("904 err\n");
            abort();
        }
        struct Block *blk = node->blk;
        printf("908 blk (%d), vpc %d, ipc %d, epc %d\n", blk->block_id, blk->vpc, blk->ipc, blk->epc);
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

void HotLevel_Order(int *array, int HotLevel)
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

struct ppa Get_Empty_PPA(int HotLevel) // For General LBA
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
        Print_All_Block();
        printf("1091 No Empty Block err\n");
        abort();
    }

    printf("lba original HotLevel %d\n", HotLevel);
    struct ppa ppa;

    int array[nHotLevel];
    HotLevel_Order(array, HotLevel);
    
    struct Block *blk = NULL;
    for (int i=0; i<nHotLevel; i++){
        blk = Get_Empty_Block(array[i]);
        if (blk != NULL){
            break;
        }
    }
    if (blk == NULL){
        printf("399 No Empty Blk err\n");
    }
    
    struct Page *pg = Get_Empty_Page(blk);
    if (pg == NULL){
        printf("805 err\n");
        abort();
    }

    // 設定ppa
    ppa.ch = pg->addr.ch;
    ppa.lun = pg->addr.lun;
    ppa.pl = pg->addr.pl;
    ppa.blk = pg->addr.blk;
    ppa.sublk = pg->addr.sublk;
    ppa.pg = pg->addr.pg;
    return ppa;
}

struct Page *Get_Empty_Page_From_Finder1()
{
    struct Page *pg = NULL;
    for (int n=0; n<nLayers_Finder1; n++){
        
        struct ArrayList_1 *Array = &finder1.Array[n];
        if (Array == NULL){
            printf("1063 err\n");
            abort();
        }
        printf("Array(%d) ", Array->id);
        
        struct LinkedList_1 *List = &Array->list;
        printf("List(%d) -> ", List->id);
        if (List == NULL){
            printf("1071 err\n");
            abort();
        }

        if (List->head.next == NULL){
            printf("NULL\n");
        }else{
            struct Node *current = NULL;
            for (current=List->head.next; current!=NULL; current=current->next){
                printf("blk(%d) vpc %d, ipc %d, epc %d -> ", current->blk->block_id, current->blk->vpc, current->blk->ipc, current->blk->epc);
                if (current->blk->epc < 0){
                    printf("1072 err\n");
                    abort();
                }
                if (current->blk->epc > 0){
                    printf("1087 Find\n");
                    struct Block *blk = current->blk;
                    for (int k=0; k<sublks_per_blk; k++){
                        struct Sublock *sublk = &blk->sublk[k];
                        if (sublk->epc < 0){
                            printf("1091 err\n");
                            abort();
                        }
                        if (sublk->epc > 0){
                            struct Page *pg = NULL;
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
            printf("NULL\n");
        }
    }
    return NULL;
}

struct ppa Get_Empty_PPA_For_Senitive_LBA(int HotLevel) // For Sensitive LBA
{
    /*一開始先去Finder1找找看有沒有Empty Page，如果Finder1找不到，才用Get_Empty_PPA找Empty Page*/
    struct ppa ppa;
    struct Page *pg = Get_Empty_Page_From_Finder1();
    
    if (pg == NULL){
        ppa = Get_Empty_PPA(HotLevel);
    }else{
        ppa.ch = pg->addr.ch;
        ppa.lun = pg->addr.lun;
        ppa.pl = pg->addr.pl;
        ppa.blk = pg->addr.blk;
        ppa.sublk = pg->addr.sublk;
        ppa.pg = pg->addr.pg;
    }

    return ppa;
}

int Is_Block_Full(struct Block *blk)
{
    if (blk->epc == 0){
        return TRUE;
    }else{
        return FALSE;
    }
}

// GC的相關操作
int should_do_gc(void)
{
    double threshold_percent = 0.5;
    double threshold = Total_Block * (1-threshold_percent);
    if (Total_Empty_Block < threshold){
        return TRUE;
    }else{
        return FALSE;
    }
}

int Enforce_do_gc(void)
{
    double threshold_percent = 0.8;
    double threshold = Total_Block * (1-threshold_percent);
    if (Total_Empty_Block < threshold){
        return TRUE;
    }else{
        return FALSE;
    }
}

void GC_Write(struct Page *pg)
{
    // 取得valid pge的lba
    struct ppa old_ppa;
    old_ppa.ch = pg->addr.ch;
    old_ppa.lun = pg->addr.lun;
    old_ppa.pl = pg->addr.pl;
    old_ppa.blk = pg->addr.blk;
    old_ppa.sublk = pg->addr.sublk;
    old_ppa.pg = pg->addr.pg;
    int lba = get_rmap(&old_ppa);
    //Mark_Page_Invalid(&old_ppa);
    set_rmap(&old_ppa, NO_SETTING); //把old_ppa指向non setting

    // 取得新的ppa
    printf("1259 GC Write\n");
    int lba_HotLevel = pg->LBA_HotLevel;
    struct ppa new_ppa = Get_Empty_PPA(lba_HotLevel);
    set_maplba(lba, new_ppa);
    set_rmap(&new_ppa, lba);

    struct Block *blk = get_blk(&new_ppa);
    if (blk == NULL){
        printf("948 err\n");
    }
    
    struct Page *new_pg = get_pg(&new_ppa);
    if (new_pg == NULL){
        printf("953 err\n");
    }
    
    Mark_Page_Valid(&new_ppa);
    new_pg->LBA_HotLevel = lba_HotLevel;
    
    printf("New lba %d -> ppa (ch %d, lun %d, pl %d, blk %d, sublk %d, pg %d)\n", lba, new_ppa.ch, new_ppa.lun, new_ppa.pl, new_ppa.blk, new_ppa.sublk, new_ppa.pg);
    printf("New lba HotLevel %d, write page state %d\n",new_pg->LBA_HotLevel, new_pg->state);
    printf("blk(%d) vpc %d, ipc %d, epc %d, position %d\n",blk->block_id, blk->vpc, blk->ipc, blk->epc, blk->position);
   
    if ( (blk->ipc+blk->vpc)>pgs_per_blk ){
        printf("965 err \n");
        abort();
    }

    move_pg_cnt++;   
}

int Clean_One_Sublock(struct Block *blk ,struct Sublock *sublk)
{   
    if (sublk == NULL){
        printf("935 err\n");
        abort();
    }

    int enforce_clean_block_id = NO_SETTING;
    for (int i=0; i<pgs_per_sublk; i++){
        struct Page *pg = &sublk->pg[i];
        
        if (pg == NULL){
            printf("942 err\n");
            abort();
        }
        
        if (pg->state == VALID){
            // clean sublk 時因為會需要先搬移valid pg，所以有可能導致ssd space不足
            if (Total_epc == 0){ 
                printf("1309 enforce clean block\n");
                enforce_clean_block_id = Enforce_Clean_Block(blk);
                if (enforce_clean_block_id == NO_SETTING){ // 應該要有enforce block id
                    printf("1311 err\n");
                    abort();
                }
            }
            if (enforce_clean_block_id != blk->block_id){ // 如果Enforce clean blk不是現在的blk，才需要做GC Write
                GC_Write(pg);
            }
        }
    }
    return enforce_clean_block_id;
}

void Mark_Sublock_Free(struct Block *blk, struct Sublock *sublk)
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
        struct Page *pg = &sublk->pg[i];
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

int do_gc(struct Block *secure_deletion_blk, int need_secure_deletion)
{
    printf("do gc !!\n");
    // Print_Finder1();
    struct Block *victim_blk = NULL;
    if (need_secure_deletion == TRUE){
        printf("1315 do secure deletion\n");
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

    printf("Victim Block %d\n", victim_blk->block_id);
    printf("Victim Blokc vpc %d, ipc %d, epc %d\n", victim_blk->vpc, victim_blk->ipc, victim_blk->epc);
    printf("Victim Block victim count %d, Nonvictim count %d, position %d\n", victim_blk->victim_sublk_count, victim_blk->Nonvictim_sublk_count, victim_blk->position);

    int have_victim_sublk = FALSE;
    int victim_blk_vpc = victim_blk->vpc;
    int victim_blk_ipc = victim_blk->ipc;
    int victim_blk_epc = victim_blk->epc;

    int total_victim_sublk_vpc = 0;
    int total_victim_sublk_ipc = 0;

    // 確認enforce clean block 跟 GC blk，這兩個blk是否相同
    int enforce_clean_block_id = NO_SETTING;

    for (int i=0; i<sublks_per_blk; i++){
        struct Sublock *victim_sublk = &victim_blk->sublk[i];
        if (victim_sublk->state == Victim || victim_sublk->have_invalid_sensitive_page == TRUE){
            if (victim_sublk->have_invalid_sensitive_page == TRUE){
                printf("1343 victim sublk(%d) have invalid pg, vpc %d, ipc %d, epc %d\n", victim_sublk->id, victim_sublk->vpc, victim_sublk->ipc, victim_sublk->epc);
            }

            // 將victim sublk vpc、ipc資訊記下來
            total_victim_sublk_vpc += victim_sublk->vpc;
            total_victim_sublk_ipc += victim_sublk->ipc;

            have_victim_sublk = TRUE;
            if (enforce_clean_block_id != victim_blk->block_id){ // 檢查看看如果有觸發enforce clean blk，強制清除的blk跟現在GC的blk是否相同，如果相同本次GC就不用做了
                enforce_clean_block_id = Clean_One_Sublock(victim_blk, victim_sublk);
            }/*else{
                printf("1442 enforce blk id %d, victim blk id %d\n", enforce_clean_block_id, victim_blk->block_id);
                printf("1443 victim_blk(%d), vpc %d, ipc %d, epc %d", victim_blk->block_id, victim_blk->vpc, victim_blk->ipc, victim_blk->epc);
            }*/
            printf("1237 After Clean Blk(%d), vpc %d, ipc %d, epc %d\n", victim_blk->block_id, victim_blk->vpc, victim_blk->ipc, victim_blk->epc);
            printf("1238 After Clean Sublk(%d), vpc %d, ipc %d, epc %d\n", victim_sublk->id, victim_sublk->vpc, victim_sublk->ipc, victim_sublk->epc);            
            
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
            printf("1241 After MarkFree Blk(%d), vpc %d, ipc %d, epc %d\n", victim_blk->block_id, victim_blk->vpc, victim_blk->ipc, victim_blk->epc);
            printf("1242 After MarkFree Sublk(%d), vpc %d, ipc %d, epc %d\n", victim_sublk->id, victim_sublk->vpc, victim_sublk->ipc, victim_sublk->epc);

            // 更新block position
            if (enforce_clean_block_id != victim_blk->block_id){
                Move_Block_Position(victim_blk, victim_blk_vpc, victim_blk_ipc, victim_blk_epc, total_victim_sublk_vpc, total_victim_sublk_ipc);
            }
        }
    }

    /* 不需要擔心block在finder1的位置，finder1是根據block的victim sublk cnt來決定blk的存放位置，Get_Victim_Block_From_Finder1()回傳victim block後，victim block是已經從finder1移除了，而經過do_gc()處理後  
    victim block所有的victim sublk都會被清除，所以victim block經過do_gc()後也就不應該再finder1了。*/

    printf("Clean blk(%d), vpc %d, ipc %d, epc %d, position %d\n", victim_blk->block_id, victim_blk->vpc, victim_blk->ipc, victim_blk->epc, victim_blk->position);
    Print_Finder1();
    printf("do gc over\n");

    return Finish_GC;
}

void do_secure_deletion(struct ppa *secure_deletion_table, int index)
{
    for (int i=0; i<index; i++){
        struct Block *victim_blk = get_blk(&secure_deletion_table[i]);
        if (victim_blk == NULL){
            printf("1392 err\n");
            abort();
        }
        // 要先把victim blk從finder1移除
        if (victim_blk->victim_sublk_count > 0){
            printf("1403 Blk(%d) vpc %d ipc %d epc %d vsubc %d\n", victim_blk->block_id, victim_blk->vpc, victim_blk->ipc, victim_blk->epc, victim_blk->victim_sublk_count);
            Remove_Block_Finder1(victim_blk->victim_sublk_count, victim_blk);
        }
        int r = do_gc(victim_blk, TRUE);
    }
}

void trim(int slba, int size)
{
    int start_lba = slba;
    int end_lba = start_lba + size -1;
    // secure deletion會用到的變數
   int need_do_secure_deletion = FALSE;
   int *secure_deletion_map = (int *)malloc(Total_Block * sizeof(int));
   for (int block_id=0; block_id<Total_Block; block_id++){
        secure_deletion_map[block_id] = 0;
   }
   int index = 0;
   struct ppa *secure_deletion_table = (struct ppa *)malloc(size * sizeof(struct ppa));

    for (int lba=start_lba; lba<=end_lba; lba++){
        
        if (mapped(lba) == MAPPED){
            printf("start trim !!\n");

            struct ppa *ppa = get_maplba(lba);
            set_rmap(ppa, NO_SETTING);

            // 找出Block 
            struct Block *blk = get_blk(ppa);
            if (blk == NULL){
                printf("1442 err\n");
            }

            // 找出Sublock
            struct Sublock *sublk = get_sublk(ppa);
            if (sublk == NULL){
                printf("1448 err\n");
            }

            // 找出Page 
            struct Page *pg = get_pg(ppa);
            if (pg == NULL){ 
                printf("1454 err\n"); 
            }

            // 確認要不要做secure deletion
            if (pg->type == Sensitive){
                need_do_secure_deletion = TRUE;
                if (secure_deletion_map[blk->block_id] == 0){ // block尚未紀錄
                    
                    secure_deletion_map[blk->block_id] = 1;
                    struct ppa blk_ppa;
                    blk_ppa.ch = pg->addr.ch;
                    blk_ppa.lun = pg->addr.lun;
                    blk_ppa.pl = pg->addr.pl;
                    blk_ppa.blk = pg->addr.blk;
                    secure_deletion_table[index] = blk_ppa;
                    index++;
                }
            }

            // Mark Page Invalid 
            Mark_Page_Invalid(ppa);

            // 如果Page是Sensitive Type的話，需要把Page的Sublock->have_invalid_sensitive_page變成TRUE
            if (pg->type == Sensitive){
                sublk->have_invalid_sensitive_page = TRUE;
            }

            // 更新ppa
            clear_maplba(lba);
        }
    }

    // 執行secure deletion
   if (need_do_secure_deletion == TRUE){
        do_secure_deletion(secure_deletion_table, index);
   }
   free(secure_deletion_table);
}

// Over Provisioning 的操作
void Move_VictimBlk_Vpc_To_OP(struct Page *pg, struct Page *temp_pg)
{
    // 取得valid pge的lba
    struct ppa old_ppa;
    old_ppa.ch = pg->addr.ch;
    old_ppa.lun = pg->addr.lun;
    old_ppa.pl = pg->addr.pl;
    old_ppa.blk = pg->addr.blk;
    old_ppa.sublk = pg->addr.sublk;
    old_ppa.pg = pg->addr.pg;
    int lba = get_rmap(&old_ppa);

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
    clear_maplba(lba);

    // 更新OP Block的資料
    struct Block *op_blk = &OP.blk[0];
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

struct Node* OP_remove_block_from_emptyList(struct Block *victim_blk)
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
            printf("1610 OP Move Node %d, vpc %d, ipc %d, epc %d\n", current->blk->id, current->blk->vpc, current->blk->ipc, current->blk->epc);
            break;
        }
        Previous = current;
    }

    // 更新Empty List的info
    printf("1617 OP Before EmptyList(%d) state : vpc %d, ipc %d, epc %d\n",Array->array_id, Empty->vpc, Empty->ipc, Empty->epc);
    Empty->vpc = Empty->vpc - victim_blk->vpc;
    Empty->ipc = Empty->ipc - victim_blk->ipc;
    Empty->epc = Empty->epc - victim_blk->epc;
    printf("1621 OP After EmptyList(%d) state : vpc %d, ipc %d, epc %d\n",Array->array_id, Empty->vpc, Empty->ipc, Empty->epc);

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

struct Node *OP_remove_block_from_nonemptyList(struct Block *victim_blk)
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
            printf("1667 OP Move Node %d, vpc %d, ipc %d, epc %d\n", current->blk->id, current->blk->vpc, current->blk->ipc, current->blk->epc);
            break;
        }
        Previous = current;
    }

    //更新NonEmpty List的狀態
    printf("1674 OP Before NonEmptyList(%d) state : vpc %d, ipc %d, epc %d\n",Array->array_id, NonEmpty->vpc, NonEmpty->ipc, NonEmpty->epc);
    NonEmpty->vpc = NonEmpty->vpc - victim_blk->vpc;
    NonEmpty->ipc = NonEmpty->ipc - victim_blk->ipc;
    NonEmpty->epc = NonEmpty->epc - victim_blk->epc;
    printf("1678 OP After NonEmptyList(%d) state : vpc %d, ipc %d, epc %d\n",Array->array_id, NonEmpty->vpc, NonEmpty->ipc, NonEmpty->epc);

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

void OP_Mark_Free_Block(struct Block *victim_block)
{
    // 更新統計資訊
    Total_vpc = Total_vpc - victim_block->vpc;
    Total_ipc = Total_ipc - victim_block->ipc;
    Total_epc = Total_epc + (victim_block->vpc + victim_block->ipc);

    for (int s=0; s<sublks_per_blk; s++){
        struct Sublock *sublk = &victim_block->sublk[s];
        
        for (int p=0; p<pgs_per_sublk; p++){
            // 更新page的資料
            struct Page *pg = &sublk->pg[p];
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

void OP_get_pg_to_VictimBlk(struct Page *op_pg, struct Page *pg, struct Block *blk, struct Sublock *sublk)
{
    // Page 之間交換資料
    pg->state = op_pg->state;
    pg->LBA_HotLevel = op_pg->LBA_HotLevel;
    pg->type = op_pg->type;

    // 設定lba 和 ppa的關係
    struct ppa old_ppa;
    old_ppa.ch = op_pg->addr.ch;
    old_ppa.lun = op_pg->addr.lun;
    old_ppa.pl = op_pg->addr.pl;
    old_ppa.blk = op_pg->addr.blk;
    old_ppa.sublk = op_pg->addr.sublk;
    old_ppa.pg = op_pg->addr.pg;
    int lba = get_rmap(&old_ppa);

    struct ppa new_ppa;
    new_ppa.ch = pg->addr.ch;
    new_ppa.lun = pg->addr.lun;
    new_ppa.pl = pg->addr.pl;
    new_ppa.blk = pg->addr.blk;
    new_ppa.sublk = pg->addr.sublk;
    new_ppa.pg = pg->addr.pg;

    // 設定lba新的對應ppa
    set_maplba(lba, new_ppa);
    set_rmap(&new_ppa, lba);
    
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

void OP_move_victim_blk_to_emptyList(struct Node *current)
{
    struct Block *victim_blk = current->blk;
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

void OP_mark_op_free(void)
{
    // Free OP block
    struct Block *blk = &OP.blk[0];
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
        struct Sublock *sublk = &blk->sublk[s];
        sublk->id = s;
        sublk->vpc = 0;
        sublk->ipc = 0;
        sublk->epc = pgs_per_sublk;
        sublk->state = NonVictim;
        sublk->have_invalid_sensitive_page = FALSE;
        for (int p=0; p<pgs_per_sublk; p++){
            struct Page *pg = &sublk->pg[p];
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

void Print_OP_State(void)
{
    struct Block *blk = &OP.blk[0];
    printf("OP state : vpc %d, ipc %d, epc %d\n", blk->vpc, blk->ipc, blk->epc);
    
    for (int s=0; s<sublks_per_blk; s++){
        struct Sublock *sublk = &blk->sublk[s];

        for (int p=0; p<pgs_per_sublk; p++){
            struct Page *pg = &sublk->pg[p];
            printf("s= %d, p= %d, pg state %d\n", s, p, pg->state);
        }
    }
}

int Enforce_Clean_Block(struct Block *victim_blk)
{
    int need_check_finder = TRUE;
    if (victim_blk != NULL){
        // 因為如果victim blk有指定，說明是從Clean_One_Sublock()觸發的，而Clean_One_Sublock()地victim blk是從do_gc傳來的，do_gc在傳來前就把victim blk在finedr1移除了
        need_check_finder = FALSE; 
    }
    
    // 有兩種方法和OP做資料交換, 1. 有指定victim_blk, 只會由Clean_One_Sublock()觸發 2. 不指定 NULL 就需要自己找victim blk
    printf("1904 start enforce clean block\n");
    if (victim_blk == NULL){
        // 找出invalid pg最多的Block
        struct ppa ppa;
        int Max_ipc = 0;
        for (int ch=0; ch<nchs; ch++){
            for (int lun=0; lun<luns_per_ch; lun++){
                for (int pl=0; pl<pls_per_lun; pl++){
                    for (int blk=0; blk<blks_per_pl; blk++){
                        ppa.ch = ch;
                        ppa.lun = lun;
                        ppa.pl = pl;
                        ppa.blk = blk;

                        struct Block *blk = get_blk(&ppa);
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
        printf("1572 err\n");
        Print_All_Block();
        abort();
    }

    if (victim_blk->ipc == 0){
        printf("1577 err\n");
        abort();
    }
    printf("1582 Enforce clean victim blk(%d) : vpc %d, ipc %d, epc %d\n", victim_blk->block_id, victim_blk->vpc, victim_blk->ipc, victim_blk->epc);
    
    // 判斷victim blk是否有需要從Finder1移除
    if (need_check_finder == TRUE){
        int victim_sublk_cnt = 0;
        for (int i=0 ;i<sublks_per_blk; i++){
            struct Sublock *sublk = &victim_blk->sublk[i];
            if (sublk->state == Victim){
                victim_sublk_cnt++;
            }
        }

        if (victim_sublk_cnt!=0){
            printf("1918 OP victim blk have %d victim sublk. \n", victim_sublk_cnt);
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
        printf("1615 err\n");
        abort();
    }
    if (current == NULL){
        printf("1755 err\n");
        abort();
    }
    printf("1876 OP Move Node %d, vpc %d, ipc %d, epc %d\n", current->blk->id, current->blk->vpc, current->blk->ipc, current->blk->epc);
    
    // 把victim blk的valid pg移到 OP
    int OP_sublk_index = 0;
    int OP_pg_index = 0;
    for (int s =0; s<sublks_per_blk; s++){
        struct Sublock *sublk = &victim_blk->sublk[s];
        /*if (sublk->state == Victim){
            Print_All_Block();
            printf("1609 warning, Have sublk is victim state .\n");
            abort();
        }*/

        for (int p=0; p<pgs_per_sublk; p++){
            struct Page *pg = &sublk->pg[p];
            if (pg->state == VALID){
                struct Page *temp_pg = &OP.blk[0].sublk[OP_sublk_index].pg[OP_pg_index];
                Move_VictimBlk_Vpc_To_OP(pg, temp_pg);
                
                OP_pg_index++;
                if (OP_pg_index == pgs_per_sublk){
                    OP_sublk_index++;
                    OP_pg_index = 0;
                    
                    if (OP_sublk_index == sublks_per_blk){
                        printf("1625 err\n");
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
    struct Block *op_blk = &OP.blk[0];
    int stop = FALSE;

    for (int s=0; s<sublks_per_blk; s++){
        
        struct Sublock *op_sublk = &op_blk->sublk[s];
        for (int p=0; p<pgs_per_sublk; p++){
            
            struct Page *op_pg = &op_sublk->pg[p];
            if (op_pg->state == NO_SETTING){
                stop = TRUE;
                break;
            }

            struct Sublock *victim_sublk = &victim_blk->sublk[sublk_id];
            struct Page *victim_pg = &victim_blk->sublk[sublk_id].pg[pg_id];
            OP_get_pg_to_VictimBlk(op_pg, victim_pg, victim_blk, victim_sublk);
            
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
    printf("1966 sublk id %d, page id %d\n", sublk_id, pg_id);

    // 把victim blk移到對應的Empty list裡
    OP_move_victim_blk_to_emptyList(current);
    // Mark Free OP
    OP_mark_op_free();

    printf("OP finish, victim blk(%d) state vpc %d, ipc %d, epc %d\n", victim_blk->block_id, victim_blk->vpc, victim_blk->ipc, victim_blk->epc);
    Print_Finder2();
    return victim_blk->block_id;
}

int Check_whether_enforce_clean_blcok(void)
{
    if (Total_epc == 0){
        printf("1996 warning, Total epc = %d !!\n", Total_epc);
        /*if (Total_ipc == 0){
            printf("2007 err, ssd space no enough\n");
            abort();
        }*/
        return TRUE;   
    }
    return FALSE;
}

void ssd_write(int slba, int size)
{  
   int start_lba = slba;
   int end_lba = start_lba + size -1;
   // secure deletion會用到的變數
   int need_do_secure_deletion = FALSE;
   int *secure_deletion_map = (int *)malloc(Total_Block * sizeof(int));
   for (int block_id=0; block_id<Total_Block; block_id++){
        secure_deletion_map[block_id] = 0;
   }
   int index = 0;
   struct ppa *secure_deletion_table = (struct ppa *)malloc(size * sizeof(struct ppa));

    while (Enforce_do_gc() == TRUE){
        printf("2028\n");
        int r = do_gc(NULL, FALSE);
        
        if (r == Stop_GC){
            Print_SSD_State();
            printf("1508 Warning Enforce GC Not Find Victim Blk\n");
            printf("Total vpc %d, ipc %d, epc %d\n", Total_vpc, Total_ipc, Total_epc);
            if (Check_whether_enforce_clean_blcok() == TRUE){
                Enforce_Clean_Block(NULL);
            }
            break;
        }
    }

   for (int lba=start_lba; lba<=end_lba; lba++){
        // 有時候一口氣寫很大量的檔案，在寫入的過程中才產生空間不足的情況，所以每氣血入前先檢查看看
        if (Check_whether_enforce_clean_blcok() == TRUE){
            Enforce_Clean_Block(NULL);
        }

        int is_new_lba = 1;
        int lba_HotLevel = 0;

        printf("write lba %d\n", lba);
        if (lba >= (Total_Block * pgs_per_blk)){
            printf("837 err\n");
            abort();
        }

        if (mapped(lba) == MAPPED){ // lba不是第一次寫入
            printf("mapped \n");
            // 設定lba、ppa的轉換
            struct ppa *ppa = get_maplba(lba);
            set_rmap(ppa, NO_SETTING); // 因為lba會指向新的ppa，所以舊的ppa指向NO_SETTING

            // 找出Block 
            struct Block *blk = get_blk(ppa);
            if (blk == NULL){
                printf("1442 err\n");
            }

            // 找出Sublock
            struct Sublock *sublk = get_sublk(ppa);
            if (sublk == NULL){
                printf("1448 err\n");
            }

            // 找出Page 
            struct Page *pg = get_pg(ppa);
            if (pg == NULL){ 
                printf("1454 err\n"); 
            }

            // 確認要不要做secure deletion
            if (pg->type == Sensitive){
                need_do_secure_deletion = TRUE;
                if (secure_deletion_map[blk->block_id] == 0){ // block尚未紀錄
                    
                    secure_deletion_map[blk->block_id] = 1;
                    
                    struct ppa blk_ppa;
                    blk_ppa.ch = pg->addr.ch;
                    blk_ppa.lun = pg->addr.lun;
                    blk_ppa.pl = pg->addr.pl;
                    blk_ppa.blk = pg->addr.blk;
                    secure_deletion_table[index] = blk_ppa;
                    index++;
                }
            }

            // Mark Page Invalid 
            Mark_Page_Invalid(ppa);
            
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
        if (boundary_1<=lba && lba<=boundary_2){
            ppa = Get_Empty_PPA_For_Senitive_LBA(lba_HotLevel);
        }else{
            ppa = Get_Empty_PPA(lba_HotLevel);
        }
        set_maplba(lba, ppa);
        set_rmap(&ppa, lba);

        struct Block *blk = get_blk(&ppa);
        if (blk == NULL){
            printf("871 err\n");
        }

        struct Page *pg = get_pg(&ppa);
        if (pg == NULL){
            printf("876 err\n");
        }

        Mark_Page_Valid(&ppa);
        // 設定page的屬性
        pg->LBA_HotLevel = lba_HotLevel;
        if (boundary_1<=lba && lba<=boundary_2){
            pg->type = Sensitive;
        }else{
            pg->type = General;
        }

        printf("lba %d -> ppa (ch %d, lun %d, pl %d, blk %d, sublk %d, pg %d)\n", lba, ppa.ch, ppa.lun, ppa.pl, ppa.blk, ppa.sublk, ppa.pg);
        printf("lba HotLevel %d, write page state %d, type %d\n",pg->LBA_HotLevel, pg->state, pg->type);
        printf("blk(%d) vpc %d, ipc %d, epc %d, position %d\n",blk->block_id, blk->vpc, blk->ipc, blk->epc, blk->position);

        if ( (blk->ipc+blk->vpc)>pgs_per_blk ){
            printf("887 err \n");
            abort();
        }
        printf("\n");

        write_pg_cnt++;
   }

   // 執行secure deletion
   if (need_do_secure_deletion == TRUE){
        do_secure_deletion(secure_deletion_table, index);
   }
   free(secure_deletion_table);
}

// 初始亂數種子
void initRandom() {
    // srand((unsigned int)time(NULL));
    unsigned int SEED  = 100;
    srand(SEED);
}

// 產生指定範圍內的亂數
int getRandomNumber(int min, int max) {
    return min + rand() % (max - min + 1);
}

void WA(void)
{
    double wa = (write_pg_cnt + move_pg_cnt) / write_pg_cnt;
    printf("write pg cnt %f\n", write_pg_cnt);
    printf("move pg cnt %f\n", move_pg_cnt);
    printf("WA = %f\n", wa);
}

int main(void)
{
    Init();
    initRandom();

    int count = 0;
    // write
    for (int i=0; i<2; i++){
        for(int lba=0; lba<130; lba=lba+8){
            ssd_write(lba, 8);

            int r = 0;
            if (should_do_gc() == TRUE){
                r = do_gc(NULL, FALSE);
            }
            count++;
        }
        /*for (int lba=0; lba<100; lba=lba+8){
            trim(lba, 8);
            
            int r = 0;
            if (should_do_gc() == TRUE){
                r = do_gc(NULL, FALSE);
            }
        }*/
    }
    
    printf("OVER -----------------------\n");
    printf("Task over , Execute times %d\n", count);
    printf("Total Empty Block %d\n", Total_Empty_Block);
    printf("Total vpc %d, Total ipc %d, Total epc %d\n", Total_vpc, Total_ipc, Total_epc);
    WA();
}