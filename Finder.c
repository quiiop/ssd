#include <stdio.h>
#include <stdlib.h>

struct Block{
    int id;
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

struct node* get_victim_block(struct link *list){
    struct node *victim_block;
    if (list->head == NULL){
        printf("error\n");
        return NULL;
    }else {
        victim_block = list->head;
        list->head = list->head->next;
    }
    return victim_block;
}

void print_link(struct link *list){ 
    struct node *current = list->head;
    current = list->head;
    printf("Link id %d\n", list->id);
    if (list->head == NULL){
        printf("list empty\n");
    }else{
        while (1){
            if (current->next == NULL){
            break;
            }
            printf("Block id %d \n", current->blk->id);
            current = current->next;
        }
        printf("Block id %d \n", current->blk->id);
    }
}

void print_Finder_all_list_id(struct Finder *finder) {
    int size = finder->size;
    for (int i=0; i<size; i++){
        printf("link id %d\n", finder->list[i]->id);
    }
}

void print_Finder_one_list_node(struct Finder *finder, int pos) {
    print_link(finder->list[pos]);
}

void add_Finder(struct Finder *finder, struct Block *blk, int pos){
    struct node *n = init_node(blk);
    add_link(finder->list[pos], n);
}

struct node* get_victim_block_from_Finder(struct Finder *finder, int pos){
    struct node *victim_block;
    victim_block = get_victim_block(finder->list[pos]);
    return victim_block;
}

int main(void)
{
    struct Finder *finder = init_Finder(5); // 0~4

    struct Block *b1 = malloc(sizeof(struct Block)); 
    b1->id=1;
    struct Block *b2 = malloc(sizeof(struct Block)); 
    b2->id=2;
    struct Block *b3 = malloc(sizeof(struct Block)); 
    b3->id=3;
    struct Block *b4 = malloc(sizeof(struct Block)); 
    b4->id=4;
    struct Block *b5 = malloc(sizeof(struct Block)); 
    b5->id=5;
    
    add_Finder(finder, b1, 0);
    add_Finder(finder, b2, 0);
    add_Finder(finder, b3, 0);
    add_Finder(finder, b4, 3);
    add_Finder(finder, b5, 3);

    print_Finder_one_list_node(finder, 0);
    // print_Finder_one_list_node(finder, 3);
    // print_Finder_one_list_node(finder, 2);
    printf("\n");

    struct node* victim_block = get_victim_block_from_Finder(finder, 0);
    printf("victim block id %d\n", victim_block->blk->id);
    print_Finder_one_list_node(finder, 0);

    printf("over\n");
}

/*
add_link(list1, n1);
    print_link(list1);
    
    struct node *vblk = get_victim_block(list1);
    printf("vblk id %d\n", vblk->blk->id);
    printf("----------\n");
    
    print_link(list1);
*/

/*
void add_Finder(struct Finder* finder, struct Block *blk, int pos){
    if (pos==1){

    }else if (pos==2)
    {

    }else if (pos==3)
    {

    }else if (pos==4)
    {
        
    }else {
        printf("error\n");
    } 
}
*/

/*
struct node *n1 = init_node(b1);
    struct node *n2 = init_node(b2);
    struct node *n3 = init_node(b3);
    struct node *n4 = init_node(b4);
    struct node *n5 = init_node(b5);
*/