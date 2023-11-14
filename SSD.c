#include "SSD.h"
#include "Test.h"

/*
extern void Test1(void);
extern void Hello(void);
extern struct Node* init_node(struct Block *blk);
extern void add_Node(int HotLevel, struct Node *node);
extern void init_block(int id);
extern void init_finder2(void);
extern void print_list(int HotLevel);
extern struct Block *Get_Empty_Block(int HotLevel);
*/
struct Finder2 finder2;
struct ppa *maplba;

struct Node* init_node(struct Block *blk)
{
    struct Node *node = (struct Node*)malloc(sizeof(struct Node));
    node->blk = blk;
    node->next = NULL;
    return node;
}

void add_Node(int HotLevel, struct Node *node)
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

struct Page init_page(int id)
{   
    struct Page page;
    page.id = id;
    page.statue = EMPTY;
    page.LBA_HotLevel = NO_SETTING;
    return page;
}

void init_block(int id)
{
    struct Block *blk = (struct Block*)malloc(sizeof(struct Block));
    blk->page = (struct Page*)malloc(sizeof(struct Page) * pgs_per_blk);
    for (int  i=0; i<pgs_per_blk; i++){
        blk->page[i] = init_page(i);
        // printf("page id %d", blk->page[i].id);
    }
    blk->id = id;
    blk->vpc = 0;
    blk->ipc = 0;
    blk->epc = pgs_per_blk;
    blk->position = IN_Empty;

    int Blocks_per_linkedList = (Total_Block / nHotLevel); // 5
    int Current_HotLevel = Block_Count / Blocks_per_linkedList;

    struct Node *node = init_node(blk);
    add_Node(Current_HotLevel, node);
    Block_Count++;

    struct LinkedList *list = &finder2.Array[Current_HotLevel].Empty;
    list->epc += pgs_per_blk; 

    Total_Empty_Block++;
    Total_vpc = 0;
    Total_ipc = 0;
    Total_epc += blk->epc;
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
            printf("%d -> ", current->blk->id);
        }
        printf("%d -> NULL\n", current->blk->id);
    }
}

void print_finder2(void)
{
    printf("=============\n");
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

struct Block *Get_Empty_Block(int HotLevel)
{
    /* 統一回傳第一個Block */
    struct LinkedList *List = &finder2.Array[HotLevel].Empty;
    if (List == NULL){
        printf("137 err\n");
    }

    struct Node *node = NULL;
    if (List->head.next == NULL){
        printf("142 ArrayList (%d) Empty is NULL\n", HotLevel);
    }else{
        node = List->head.next;
        struct Block *blk = node->blk;
        if (blk == NULL){
            printf("175 blk NULL err\n");
        }
        if ((blk->vpc+blk->ipc) == pgs_per_blk){
            printf("178 blk is full\n");
        }
        return blk;
    }
    return NULL;
}

/*
struct Node *Find_Remove_Node(struct LinkedList *list, struct Block *blk)
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
}
*/

void Remove_Block(int ArrayList_Position, struct Block *blk)
{
    struct ArrayList *Array = &finder2.Array[ArrayList_Position];
    struct LinkedList *Empty = &Array->Empty;
    struct LinkedList *NonEmpty = &Array->NonEmpty;

    struct Node *First_Node = Empty->head.next; 
    struct Node *current = NULL;
    struct Node *Previous = NULL;

    /*把current node從Empty List移除 */
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
    /*更新Empty List的狀態*/ 
    Empty->vpc = Empty->vpc - current->blk->vpc;
    Empty->ipc = Empty->ipc - current->blk->ipc;
    Empty->epc = Empty->epc - current->blk->epc;
    printf("Empty List state : vpc %d, ipc %d, epc %d\n", Empty->vpc, Empty->ipc, Empty->epc);

    /*把current node加入NonEmpty List*/
    if (NonEmpty->head.next == NULL){
        NonEmpty->head.next = current;
        NonEmpty->size++;
    }else{
        struct Node *current2 = NULL;
        for (current2=NonEmpty->head.next; current2->next!=NULL; current2=current2->next){
            /*idle*/
        }
        current2->next = current;
        NonEmpty->size++;
    }
    blk->position = IN_NonEmpty;
    /*更新NonEmpty List的狀態*/
    NonEmpty->vpc = NonEmpty->vpc + current->blk->vpc;
    NonEmpty->ipc = NonEmpty->ipc + current->blk->ipc;
    NonEmpty->epc = NonEmpty->epc + current->blk->epc;
    printf("NonEmpty List state : vpc %d, ipc %d, epc %d\n", NonEmpty->vpc, NonEmpty->ipc, NonEmpty->epc);

    /*更新統計的資料*/
    Total_Empty_Block--;
}

void init_maplba(void)
{
    maplba = (struct ppa *)malloc(sizeof(struct ppa) * Total_Block*pgs_per_blk); // 120
    for (int i=0; i<Total_Block*pgs_per_blk; i++){
        maplba[i].block_id = NO_SETTING;
        //maplba[i].sublk_id = NO_SETTING;
        maplba[i].pg_id = NO_SETTING;
    }
}

int mapped(int lba)
{
    if (maplba[lba].block_id != -1){ 
        if (maplba[lba].pg_id != -1){
            return MAPPED;
        }
    }
    return UNMAPPED;
}

struct Block *Get_Block(int ArrayList_Position, int Block_id)
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
}

struct Page *Get_Page(struct Block *blk, int Page_id)
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
}

void Mark_Page_Invalid(int ArrayList_Position, struct Block *blk, struct Page *pg)
{
    struct LinkedList *list = NULL;
    if (blk->position == IN_Empty){
        list = &finder2.Array[ArrayList_Position].Empty;
    }else if (blk->position == IN_NonEmpty){
        list = &finder2.Array[ArrayList_Position].NonEmpty;
    }else{
        printf("330 err\n");
    }

    pg->statue = INVALID;
    
    blk->ipc++;
    blk->vpc--;

    list->ipc++;
    list->vpc--;

    Total_ipc++;
    Total_vpc--;
}

struct Page *Get_Empty_Page(struct Block *blk)
{
    struct Page *pg = NULL;
    for (int i=0; i<pgs_per_blk; i++){
        pg = &blk->page[i];
        if (pg->statue == EMPTY){
            break;
        }
    }
    if (pg == NULL){
        printf("281 err\n");
    }
    return pg;
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

struct ppa Get_Empty_Ppa(int HotLevel)
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

    ppa.block_id = blk->id;
    ppa.pg_id = pg->id;
    return ppa;
}

void Mark_Page_Valid(int ArrayList_Position, struct Block *blk ,struct Page *pg)
{
    struct LinkedList *list = NULL;
    if (blk->position == IN_Empty){
        list = &finder2.Array[ArrayList_Position].Empty;
    }else if (blk->position == IN_NonEmpty){
        list = &finder2.Array[ArrayList_Position].NonEmpty;
    }else{
        printf("389 err\n");
    }
    
    pg->statue = VALID;
    
    blk->vpc++;
    blk->epc--;

    list->vpc++;
    list->epc--;

    Total_vpc++;
    Total_epc--;
}

int Is_Block_Full(struct Block *blk)
{
    if ((blk->ipc+blk->vpc) == pgs_per_blk){
        return TRUE;
    }else{
        return FALSE;
    }
}

void Rmove_Block_To_NonEmpty(struct Block *blk)
{
    int Blocks_per_linkedList = (Total_Block / nHotLevel);
    int ArrayList_Position = blk->id / Blocks_per_linkedList;
    
    Remove_Block(ArrayList_Position, blk);
    printf("Remove Block %d, position %d\n", blk->id, blk->position);
}

void ssd_write(int lba)
{
   int is_new_lba = 1;
   int lba_HotLevel = 0;
   int Blocks_per_linkedList = (Total_Block / nHotLevel); // 5

   printf("write lba %d\n", lba);
   if (lba >= (Total_Block * pgs_per_blk)){
        printf("471 err\n");
        abort();
   }

   if (mapped(lba) == MAPPED){ // lba不是第一次寫入
        printf("mapped \n");
        struct ppa *ppa = &maplba[lba];
        /* 找出Block */
        printf("Mapped lba %d -> Old ppa (Block %d, Page %d)\n", lba, ppa->block_id, ppa->pg_id);
        int ArrayList_Position = ppa->block_id / Blocks_per_linkedList;
        if (ArrayList_Position >= nHotLevel){
            printf("477 err \n");
            abort();
        }
        struct Block *blk = Get_Block(ArrayList_Position, ppa->block_id);
        if (blk == NULL){
            printf("259 err\n");
        }
        /* 找出Page */
        struct Page *pg = Get_Page(blk, ppa->pg_id);
        if (pg == NULL){ 
            printf("279 err\n"); 
        }
        /* Mark Page Invalid */
        Mark_Page_Invalid(ArrayList_Position, blk, pg);
        /* 更新Finder1的資訊 */
        // 某個更新Finder1的function

        is_new_lba = 0;
        lba_HotLevel = pg->LBA_HotLevel;
   }

   if (is_new_lba == 0){
        if (++lba_HotLevel == nHotLevel){ // 限制lba Hotlevel上限
            lba_HotLevel = nHotLevel-1;
        }
   }
   struct ppa ppa = Get_Empty_Ppa(lba_HotLevel);
   maplba[lba] = ppa;
   
   int ArrayList_Position = ppa.block_id / Blocks_per_linkedList;
   struct Block *blk = Get_Block(ArrayList_Position, ppa.block_id);
   if (blk == NULL){
        printf("365 err\n");
   }
   
   struct Page *pg = Get_Page(blk, ppa.pg_id);
   if (pg == NULL){
        printf("369 err\n");
   }
   
   Mark_Page_Valid(ArrayList_Position, blk, pg);
   pg->LBA_HotLevel = lba_HotLevel;

   printf("lba %d -> ppa (block %d, page %d)\n", lba, ppa.block_id, ppa.pg_id);
   printf("lba HotLevel %d, write page state %d\n",pg->LBA_HotLevel, pg->statue);
   printf("blk vpc %d, ipc %d\n", blk->vpc, blk->ipc);
   
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
    /*初始化*/
    init_maplba();
    init_finder2();

    for (int i=0; i<Total_Block; i++){
        init_block(i);
    }
    print_finder2();
    Print_SSD_State();
    printf("=============\n");

    for (int n=0; n<1; n++){
        for (int lba=0; lba<Total_Block*pgs_per_blk; lba++){
            ssd_write(lba);
        }
        print_finder2();
        Print_SSD_State();
        printf("=============\n");
    }
}

/*
struct Block *blk = Get_Empty_Block(0);
    Remove_Block(0, blk);

    print_list(0);
*/
    /*for (int i=0; i<nHotLevel; i++){
        print_list(i);
    }*/

