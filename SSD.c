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
    blk->position = IN_Empty;

    int Blocks_per_linkedList = (Total_Block / nHotLevel); // 5
    int Current_HotLevel = Block_Count / Blocks_per_linkedList;

    struct Node *node = init_node(blk);
    // printf("block count %d, current Hot Level %d\n", Block_Count, Current_HotLevel);
    add_Node(Current_HotLevel, node);
    Block_Count++;
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
        finder2.Array[i].NonEmpty.list_id = NonEmpty_id;
        finder2.Array[i].NonEmpty.head.blk = NULL;
        finder2.Array[i].NonEmpty.head.next = NULL;
        finder2.Array[i].NonEmpty.size = 0;
    }
}

void print_list(int ArrayList_Position, int position)
{
    struct ArrayList *Temp_Array = &finder2.Array[ArrayList_Position];
    struct LinkedList *Temp_List = NULL;
    
    if (position == Empty_id){
        Temp_List = &Temp_Array->Empty;
    }
    if (position == NonEmpty_id){
        Temp_List = &Temp_Array->NonEmpty;
    }
    
    if (Temp_List->head.next == NULL){
        if (Temp_List->list_id == NonEmpty_id){
            printf("NonEmpty List : NULL\n");
        }else if (Temp_List->list_id == Empty_id){
            printf("Empty List : NULL\n");
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
        printf("142 List is NULL err \n");
    }
    node = List->head.next;
    struct Block *blk = node->blk;
    if (blk == NULL){
        printf("147 blk NULL err\n");
    }
    if ((blk->vpc+blk->ipc) == pgs_per_blk){
        printf("150 blk is full\n");
    }
    return blk;
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

    /*Print NonEmpty*/
    /* printf("215 NonEmpty : ");
    current = NULL;
    for (current=NonEmpty->head.next; current!=NULL; current=current->next){
        printf("%d -> ", current->blk->id);
    }
    printf("NULL\n"); */
}

void init_maplba(void)
{
    maplba = (struct ppa *)malloc(sizeof(struct ppa) * Total_Block*pgs_per_blk); // 120
    for (int i=0; i<Total_Block*pgs_per_blk; i++){
        maplba[i].block_id = -1;
        maplba[i].pg_id = -1;
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
                printf("219 err \n");
            }
            struct Node *current = NULL;
            for (current=list->head.next; current!=NULL; current=current->next){
                if (current->blk->id == Block_id){
                    blk = current->blk;
                    break;
                }
            }
            return blk;
        }else if(position == IN_NonEmpty){
            struct LinkedList *list = &finder2.Array[ArrayList_Position].NonEmpty;
            if (list->head.next == NULL){
                printf("232 err \n");
            }
            struct Node *current = NULL;
            for (current=list->head.next; current!=NULL; current=current->next){
                if (current->blk->id == Block_id){
                    blk = current->blk;
                    break;
                }
            }
            return blk;
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

void Mark_Page_Invalid(struct Block *blk, struct Page *pg)
{
    pg->statue = INVALID;
    blk->ipc++;
    blk->vpc--;
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
    
    struct Page *pg = Get_Empty_Page(blk);

    ppa.block_id = blk->id;
    ppa.pg_id = pg->id;
    return ppa;
}

void Mark_Page_Valid(struct Block *blk ,struct Page *pg)
{
    pg->statue = VALID;
    blk->vpc++;
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
   if (mapped(lba) == MAPPED){ // lba不是第一次寫入
        printf("mapped \n");
        struct ppa *ppa = &maplba[lba];
        /* 找出Block */
        int ArrayList_Position = ppa->block_id / Blocks_per_linkedList;
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
        Mark_Page_Invalid(blk, pg);
        /* 更新Finder1的資訊 */
        // 某個更新Finder1的function

        is_new_lba = 0;
        lba_HotLevel = pg->LBA_HotLevel;
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
   
   Mark_Page_Valid(blk, pg);
   if (is_new_lba == 0){
        lba_HotLevel++;
   }
   pg->LBA_HotLevel = lba_HotLevel;

   printf("lba %d -> ppa (block %d, page %d)\n", lba, ppa.block_id, ppa.pg_id);
   printf("lba HotLevel %d, write page state %d\n",pg->LBA_HotLevel, pg->statue);
   printf("blk vpc %d, ipc %d\n", blk->vpc, blk->ipc);
   
   if (Is_Block_Full(blk) == TRUE){
        Rmove_Block_To_NonEmpty(blk);
   }
   printf("\n");
}

int main(void)
{
    init_maplba();
    init_finder2();

    for (int i=0; i<Total_Block; i++){
        init_block(i);
    }
    for (int lba=0; lba<10; lba++){
        ssd_write(lba);
    }


    print_finder2();
}

/*
struct Block *blk = Get_Empty_Block(0);
    Remove_Block(0, blk);

    print_list(0);
*/
    /*for (int i=0; i<nHotLevel; i++){
        print_list(i);
    }*/

