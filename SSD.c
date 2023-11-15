#include "SSD.h"

struct Finder1 finder1;
struct Finder2 finder2;
struct ppa *maplba;
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
    blk->epc = sublks_per_blk*pgs_per_sublk;
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
        printf("118 err\n");
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
            printf("%d -> ", current->blk->block_id); // 這裡不能用blk->id
        }
        printf("%d -> NULL\n", current->blk->block_id);
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

void Init(void)
{
    /*初始化*/
    init_finder1();
    init_finder2();
    ssd_init();
    // print_finder2();
    init_maplba();
    printf("Init Finish \n");

}



/*struct Node *Find_Remove_Node(struct LinkedList *list, struct Block *blk)
{
    struct Node *current = NULL;
    for (current=list->head.next; current != NULL; current=current->next){
        if (current->blk == blk){
            printf("Find Remove Node %d\n", current->blk->id);
            return current;
        }
    }
    printf("Not Find Remove Node\n");
    return NULL;
}*/

void set_maplba(int lba, struct ppa ppa)
{
    maplba[lba] = ppa;
    maplba[lba].state = MAPPED;
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

/*struct Block *Get_Block(int ArrayList_Position, int Block_id)
{
    struct Block *blk = NULL;
    int position_array[] = {IN_Empty, IN_NonEmpty};
    for (int i=0 ;i<2; i++){
        int position = position_array[i];
        if (position == IN_Empty){
            struct LinkedList *list = &finder2.Array[ArrayList_Position].Empty;
            if (list->head.next == NULL){
                printf("219 Empty List NULL \n");
            }
            struct Node *current = NULL;
            for (current=list->head.next; current!=NULL; current=current->next){
                if (current->blk->id == Block_id){
                    blk = current->blk;
                    return blk;       
                }
            }
        }else if(position == IN_NonEmpty){
            struct LinkedList *list = &finder2.Array[ArrayList_Position].NonEmpty;
            if (list->head.next == NULL){
                printf("232 NonEmpty NULL \n");
            }
            struct Node *current = NULL;
            for (current=list->head.next; current!=NULL; current=current->next){
                if (current->blk->id == Block_id){
                    blk = current->blk;
                    return blk;
                }
            }
        }else{
            printf("220 err\n");
        }
    }
    printf("223 err\n");
    return NULL;
}*/

/*struct Page *Get_Page(struct Block *blk, int Page_id)
{
    struct Page *pg = NULL;
    for (int i=0; i<pgs_per_blk; i++){
        if (Page_id == blk->page[i].id){
            pg = &blk->page[i];
            break;
        }
    }
    if (pg == NULL){ 
        printf("258 err\n"); 
    }
    return pg;
}*/

// Mark Page Valid 的操作
void Remove_Block_From_EmptyList(int ArrayList_Position, struct Block *blk)
{
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
            printf("Remove Node %d\n", current->blk->id);
            break;
        }
        Previous = current;
    }
    //更新Empty List的狀態
    Empty->vpc = Empty->vpc - current->blk->vpc;
    Empty->ipc = Empty->ipc - current->blk->ipc;
    Empty->epc = Empty->epc - current->blk->epc;
    printf("Empty List state : vpc %d, ipc %d, epc %d\n", Empty->vpc, Empty->ipc, Empty->epc);

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
    NonEmpty->vpc = NonEmpty->vpc + current->blk->vpc;
    NonEmpty->ipc = NonEmpty->ipc + current->blk->ipc;
    NonEmpty->epc = NonEmpty->epc + current->blk->epc;
    printf("NonEmpty List state : vpc %d, ipc %d, epc %d\n", NonEmpty->vpc, NonEmpty->ipc, NonEmpty->epc);

    //更新統計的資料
    Total_Empty_Block--;
}

void Rmove_Block_To_NonEmpty(struct Block *blk)
{
    int ArrayList_Position = blk->block_id / Blocks_per_linkedList;
    
    Remove_Block_From_EmptyList(ArrayList_Position, blk);
    printf("Remove Block %d, position %d\n", blk->id, blk->position);
}

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

    // 更新Block狀態
    blk->vpc++;
    blk->epc--;

    // 更新List狀態
    list->vpc++;
    list->epc--;

    // 更新統計資料
    Total_vpc++;
    Total_epc--;

    // 確認Block是否要從Empty List移到NonEmpty List
    if (blk->epc == 0){
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

    struct Node *node = NULL;
    if (List->head.next == NULL){
        printf("142 ArrayList (%d) EmptyList is NULL\n", HotLevel);
    }else{
        node = List->head.next;
        struct Block *blk = node->blk;
        if (blk == NULL){
            printf("175 blk NULL err\n");
            abort();
        }
        if ((blk->vpc+blk->ipc) == pgs_per_blk){
            printf("178 blk is full\n");
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

struct ppa Get_Empty_PPA(int HotLevel)
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

int Is_Block_Full(struct Block *blk)
{
    if ((blk->ipc+blk->vpc) == pgs_per_blk){
        return TRUE;
    }else{
        return FALSE;
    }
}

void ssd_write(int lba)
{
   int is_new_lba = 1;
   int lba_HotLevel = 0;
   int Blocks_per_linkedList = (Total_Block / nHotLevel); // 5

   printf("write lba %d\n", lba);
   if (lba >= (Total_Block * pgs_per_blk)){
        printf("837 err\n");
        abort();
   }

   if (mapped(lba) == MAPPED){ // lba不是第一次寫入
        printf("mapped \n");
        struct ppa *ppa = &maplba[lba];
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
   struct ppa ppa = Get_Empty_PPA(lba_HotLevel);
   maplba[lba] = ppa;
   
   struct Block *blk = get_blk(&ppa);
   if (blk == NULL){
        printf("871 err\n");
   }
   
   struct Page *pg = get_pg(&ppa);
   if (pg == NULL){
        printf("876 err\n");
   }
   
   Mark_Page_Valid(&ppa);
   pg->LBA_HotLevel = lba_HotLevel;

   printf("lba %d -> ppa (ch %d, lun %d, pl %d, blk %d, sublk %d, pg %d)\n", lba, ppa.ch, ppa.lun, ppa.pl, ppa.blk, ppa.sublk, ppa.pg);
   printf("lba HotLevel %d, write page state %d\n",pg->LBA_HotLevel, pg->state);
   printf("blk(%d) vpc %d, ipc %d, epc %d\n",blk->block_id, blk->vpc, blk->ipc, blk->epc);
   
    if ( (blk->ipc+blk->vpc)>pgs_per_blk ){
        printf("887 err \n");
        abort();
    }   

   if (Is_Block_Full(blk) == TRUE){
        if (blk->position == IN_Empty){
            Rmove_Block_To_NonEmpty(blk);
        }
   }
   printf("\n");
}

void Print_SSD_State(void)
{
    printf("Total Empty Block %d\n", Total_Empty_Block);
    printf("Total vpc %d, Total ipc %d, Total epc %d\n", Total_vpc, Total_ipc, Total_epc);
}

int main(void)
{
    Init();
    
    for (int lba=0; lba<(nblks*pgs_per_blk); lba++){
        ssd_write(lba);
    }
    printf("==============\n");
    Print_Finder1();
    printf("==============\n");
    Print_Finder2();
}


