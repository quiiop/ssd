#ifndef Test_H
#define Test_H

void Test1(void);
void Hello(void);

void Test1(void)
{
    struct Block *blk = NULL;
    blk = Get_Empty_Block(0);
    printf("Blok id %d\n", blk->id);
    blk = Get_Empty_Block(1);
    printf("Blok id %d\n", blk->id);
}

void Hello(void)
{
    printf("Hello Test1\n");
}

#endif