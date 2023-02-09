#include <stdio.h>
#include <stdlib.h>

struct Block *b1;
struct Block *b2;
struct Block *b3;
struct Block *b4;
struct Block *b5;
struct Block *b6;


struct node *n1;
struct node *n2;
struct node *n3;
struct node *n4;
struct node *n5;
struct node *n6;

struct subBlock{
    int Block_id;
    int invalid_page;
    int valid_page;
};

struct Block{
    int id;
    int invalid_sublk;
    struct subBlock **sblk;
};

struct node{
    struct Block *blk;
    struct node *next;
};

struct link
{
    int id;
    struct node *head;
    struct node *tail;
};

struct Finder{
    int size; // How many subblock
    // struct link *list[nsublk];
    struct link **list;
};

struct subBlock* init_subBlock(int id){
    struct subBlock *sblk = malloc(sizeof(struct subBlock));
    sblk->Block_id = id;
    sblk->invalid_page = 0;
    sblk->valid_page = 0;
    return sblk;
}

struct Block* init_Block(int id, int n){
    struct Block *block = malloc(sizeof(struct Block));
    block->id = id;
    block->invalid_sublk = 0;
    block->sblk = (struct subBlock **)malloc(sizeof(struct subBlock)*n);
    for (int i=0; i<n ; i++){
        block->sblk[i] = init_subBlock(block->id);
    }
    return block;
}

struct link* init_link(int id){
    struct link* list = malloc(sizeof(struct link));
    list->id = id;
    list->head = NULL;
    list->tail = NULL;
    return list;
}

struct node* init_node(struct Block *blk)
{
    struct node *n = malloc(sizeof(struct node));
    n->blk = blk;
    n->next = NULL;
    return n;
}

struct Finder* init_Finder(int size) {
    struct Finder *finder = malloc(sizeof(struct Finder));
    finder->size = size;
    finder->list = (struct link **)malloc(sizeof(struct link)*size);
    for (int i=0; i<size; i++){
        finder->list[i] = init_link(i);
    }
    return finder;
}

void add_link(struct link *list, struct node *n){
    struct node *current;
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
}

int remove_link(struct link *list, struct node *n){
    int result = -1;
    struct node *current;
    struct node *remove_node;
    
    if (list->head == NULL){
        printf("error list empty\n");
        return result;
    }
    if (list->head == n){
        remove_node = list->head;
        // printf("list %d , remove node %p , blk %p , blk id %d\n", list->id, remove_node, remove_node->blk ,remove_node->blk->id);
        list->head = list->head->next;
        result = 0;
        return result;
    }
    for (current=list->head; current!=NULL; current=current->next){
            if (current->next == n){
                remove_node = current->next;
                current->next = remove_node->next;
                // printf("list %d , remove node %p , blk %p , blk id %d\n", list->id, remove_node, remove_node->blk ,remove_node->blk->id);
                result = 0;
                break;
            }
    }
    return result;
}

struct node* get_victim_node(struct link *list){
    struct node *victim_node;

    if (list->head == NULL){
        printf("142 err list is empty\n");
        return NULL;
    }else {
        printf("146 target : node %p , blk %p\n", list->head, list->head->blk);
        if (list->head->next == NULL){
            victim_node = list->head;
            list->head = NULL;
        } else{
            victim_node = list->head;
            list->head = list->head->next;
        }
    }
    printf("155 victim_node : node %p , blk %p\n", victim_node, victim_node->blk);
    return victim_node;
}

void from_link_get_node(struct link *list, struct Block *blk, struct node **target) {
    struct node *current;
    struct node *temp;

    if (list->head == NULL){
        printf("156 err link is empty\n");
    }else{
        if (list->head->blk == blk){
            temp = list->head;
            list->head = list->head->next;
            // printf("target is list->head \n");
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

void print_link(struct link *list){ 
    struct node *current = list->head;
    current = list->head;
    printf("Link id %d : ", list->id);
    if (list->head == NULL){
        printf("list empty\n");
    }else{
        while (1){
            if (current->next == NULL){
            break;
            }
            printf("[ Block id %d , addr %p ] -> ", current->blk->id, current->blk);
            current = current->next;
        }
        printf("[ Block id %d , addr %p ]\n", current->blk->id, current->blk);
    }
}

void print_Finder_one_list_node(struct Finder *finder, int pos) {
    print_link(finder->list[pos]);
}

void print_Finder_all_list(struct Finder *finder) {
    int size = finder->size;
    printf("Finder :\n");
    for (int i=0; i<size; i++){
        // printf("link id %d\n", finder->list[i]->id);
        print_link(finder->list[i]);
        printf("\n");
    }
    printf("---------------\n");
}

void add_Finder(struct Finder *finder, struct Block *blk, int pos){
    int last_pos = pos-1;
    int result;

    if (pos == 0){ // block first time add Finder
        struct node *n = init_node(blk);
        add_link(finder->list[pos], n);
    }else{
        struct node **target;
        struct node *n;
        int last_pos = pos-1;

        from_link_get_node(finder->list[last_pos], blk, target);
        n = *target;
        add_link(finder->list[pos], n);
    }
    
}

struct node* get_victim_node_from_Finder(struct Finder *finder){
    struct node *victim_node;
    struct node **target;
    struct node *n;
    int pos = -1;

    for (int i=finder->size-1; i>=0; i--){
        if (finder->list[i]->head != NULL){
            pos = i;
            break;
        }
    }
    printf("pos %d\n", pos);
    
    if (pos == -1){
        printf("err Finder is empty\n");
        return NULL;
    }
    
    victim_node = get_victim_node(finder->list[pos]);
    printf("256 victim_node : node %p , blk %p , blk id %d\n", victim_node, victim_node->blk, victim_node->blk->id);
    return victim_node;
}

void ssd_write(struct Finder *finder, struct Block *blk, int nsublk) {
    int total_page = 10;
    int pos;

    for (int i=0; i<nsublk; i++){
        blk->sblk[i]->invalid_page = total_page;
        if (blk->sblk[i]->invalid_page == total_page){
            blk->invalid_sublk = blk->invalid_sublk +1;
            pos = blk->invalid_sublk -1;
            add_Finder(finder, blk, pos);
        }
    }
}

int main(void)
{
    // one block have 5 sub-block
    // one Finder have 5 links
    struct Finder *finder = init_Finder(5); // 0~4
    
    b1 = init_Block(1, 5);
    b2 = init_Block(2, 5);
    b3 = init_Block(3, 5);
    b4 = init_Block(4, 5);
    b5 = init_Block(5, 5);
    
    ssd_write(finder, b1, 3);
    ssd_write(finder, b2, 3);
    ssd_write(finder, b3, 1);
    ssd_write(finder, b4, 5);
    ssd_write(finder, b5, 5);
    ssd_write(finder, b3, 1);

    print_Finder_all_list(finder);

    struct node *victim_node;
    // victim_node = get_victim_node(finder->list[4]);
    // printf("297 victim_node : node %p , blk %p , blk id %d\n", victim_node, victim_node->blk, victim_node->blk->id);

    victim_node = get_victim_node_from_Finder(finder);
    printf("300 victim_node : node %p , blk %p , blk id %d\n", victim_node, victim_node->blk, victim_node->blk->id);
    
    victim_node = get_victim_node_from_Finder(finder);
    printf("303 victim_node : node %p , blk %p , blk id %d\n", victim_node, victim_node->blk, victim_node->blk->id);

    victim_node = get_victim_node_from_Finder(finder);
    printf("306 victim_node : node %p , blk %p , blk id %d\n", victim_node, victim_node->blk, victim_node->blk->id);

    printf("\n");
    print_Finder_all_list(finder);

    printf("over\n");   
    
}