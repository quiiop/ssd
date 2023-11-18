#include "SSD.h"

struct Finder1 finder1;
struct Finder2 finder2;
struct ppa *maplba; // maplba是lba -> ppa
int *rmap;  // rmap是ppa -> lba
struct SSD *ssd;

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
        printf("58 err\n");
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
        /*沒有找到Victim Block不一定是錯誤的，因為有時候連續大批資料寫入寫會導致觸發了should_do_gc，但是所有的資料都是valid page，這時候就會發現沒有Victim Block可以處理，但是如果是Enforce_do_gc觸觸發的話，就一定要找出Victim Block*/
        
        //Print_Finder1();
        //Print_SSD_State();
        //abort();
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
    printf("Total Empty Block %d\n", Total_Empty_Block);
    printf("Total vpc %d, Total ipc %d, Total epc %d\n", Total_vpc, Total_ipc, Total_epc);
    Print_Finder2();
    Print_All_Block();
    Print_Finder1();
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
        printf("756 err\n");
        Print_Finder2();
        abort();
    }

    // 更新統計資料
    Total_vpc++;
    Total_epc--;
    if (Total_epc < 0){
        printf("764 err\n");
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
    if (sublk->vpc == 0){
        return Victim;
    }

    double n = (sublk->vpc+sublk->ipc) / sublk->vpc;
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
        printf("394 No Empty Block err\n");
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
    
    printf("GC write\n");
    printf("New lba %d -> ppa (ch %d, lun %d, pl %d, blk %d, sublk %d, pg %d)\n", lba, new_ppa.ch, new_ppa.lun, new_ppa.pl, new_ppa.blk, new_ppa.sublk, new_ppa.pg);
    printf("New lba HotLevel %d, write page state %d\n",new_pg->LBA_HotLevel, new_pg->state);
    printf("blk(%d) vpc %d, ipc %d, epc %d, position %d\n",blk->block_id, blk->vpc, blk->ipc, blk->epc, blk->position);
   
    if ( (blk->ipc+blk->vpc)>pgs_per_blk ){
        printf("965 err \n");
        abort();
    }   
}

void Clean_One_Sublock(struct Sublock *sublk)
{
    if (sublk == NULL){
        printf("935 err\n");
        abort();
    }

    for (int i=0; i<pgs_per_sublk; i++){
        struct Page *pg = &sublk->pg[i];
        if (pg == NULL){
            printf("942 err\n");
            abort();
        }
        if (pg->state == VALID){
            GC_Write(pg);
        }
    }
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

int do_gc(void)
{
    printf("do gc !!\n");
    
    struct Block *victim_blk = Get_Victim_Block_From_Finder1();
    if (victim_blk == NULL){
        printf("12088 No Victim Block\n");
        return Stop_GC;
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

    for (int i=0; i<sublks_per_blk; i++){
        struct Sublock *victim_sublk = &victim_blk->sublk[i];
        // printf("victim sublk (%d), vpc %d, ipc %d, epc %d, state %d\n", victim_sublk->id, victim_sublk->vpc, victim_sublk->ipc, victim_sublk->epc, victim_sublk->state);
        if (victim_sublk->state == Victim){
            // 將victim sublk vpc、ipc資訊記下來
            total_victim_sublk_vpc += victim_sublk->vpc;
            total_victim_sublk_ipc += victim_sublk->ipc;

            have_victim_sublk = TRUE;
            Clean_One_Sublock(victim_sublk);
            printf("1237 After Clean Blk(%d), vpc %d, ipc %d, epc %d\n", victim_blk->block_id, victim_blk->vpc, victim_blk->ipc, victim_blk->epc);
            printf("1238 After Clean Sublk(%d), vpc %d, ipc %d, epc %d\n", victim_sublk->id, victim_sublk->vpc, victim_sublk->ipc, victim_sublk->epc);            

            Mark_Sublock_Free(victim_blk, victim_sublk);
            printf("1241 After MarkFree Blk(%d), vpc %d, ipc %d, epc %d\n", victim_blk->block_id, victim_blk->vpc, victim_blk->ipc, victim_blk->epc);
            printf("1242 After MarkFree Sublk(%d), vpc %d, ipc %d, epc %d\n", victim_sublk->id, victim_sublk->vpc, victim_sublk->ipc, victim_sublk->epc);
            
            // 更新block資訊
            victim_blk->victim_sublk_count--;
            if (victim_blk->victim_sublk_count < 0){
                printf("1060 err\n");
                abort();
            }
            victim_blk->Nonvictim_sublk_count++;
            if (victim_blk->Nonvictim_sublk_count > sublks_per_blk){
                printf("1065 err\n");
                abort();
            }
        }
    }

    /* 不需要擔心block在finder1的位置，finder1是根據block的victim sublk cnt來決定blk的存放位置，Get_Victim_Block_From_Finder1()回傳victim block後，victim block是已經從finder1移除了，而經過do_gc()處理後  
    victim block所有的victim sublk都會被清除，所以victim block經過do_gc()後也就不應該再finder1了。*/

    // 更新block position
    if (have_victim_sublk == TRUE){
        printf("move victim blk(%d), vpc %d, ipc %d, epc %d\n", victim_blk->block_id, victim_blk->vpc, victim_blk->ipc, victim_blk->epc);
        Move_Block_Position(victim_blk, victim_blk_vpc, victim_blk_ipc, victim_blk_epc, total_victim_sublk_vpc, total_victim_sublk_ipc);
    }else{
        printf("1082 err No victim sublk\n");
        abort();
    }

    printf("Clean blk(%d), vpc %d, ipc %d, epc %d, position %d\n", victim_blk->block_id, victim_blk->vpc, victim_blk->ipc, victim_blk->epc, victim_blk->position);
    printf("do gc over\n");

    return Finish_GC;
}

void ssd_write(int slba, int size)
{  
   int start_lba = slba;
   int end_lba = start_lba + size -1;

   for (int lba=start_lba; lba<=end_lba; lba++){
        int is_new_lba = 1;
        int lba_HotLevel = 0;

        while (Enforce_do_gc() == TRUE){
            int r = do_gc();
        
            if (r == Stop_GC){
                Print_SSD_State();
                printf("1282 err Enforce GC Not Find Victim Blk\n");
                printf("Total vpc %d, ipc %d, epc %d\n", Total_vpc, Total_ipc, Total_epc);
                if (Total_epc < 2*pgs_per_blk){
                    abort();   
                }
            }
        }

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
                printf("847 err\n");
            }
            // 找出Page 
            struct Page *pg = get_pg(ppa);
            if (pg == NULL){ 
                printf("852 err\n"); 
            }
            // Mark Page Invalid 
            Mark_Page_Invalid(ppa);

            is_new_lba = 0;
            lba_HotLevel = pg->LBA_HotLevel;
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
   }
}

/*
    要新增強制GC的功能。
    當Total_epc < 2個Block Page的大小時，需要進行強制GC的功能，
*/

int main(void)
{
    Init();

    int count = 0;
    for (int i=0; i<1; i++){
        for(int lba=0; lba<18; lba++){
            ssd_write(lba, 8);
            
            if (should_do_gc() == TRUE){
                do_gc();
            }
            count++;
        }
    }
    Print_SSD_State();
    printf("Task over Execute time %d!!\n", count);
}