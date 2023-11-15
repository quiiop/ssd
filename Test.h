/*
void Test1(void)
{
    struct ppa ppa;
    ppa.ch =0;
    ppa.lun = 0;
    ppa.pl = 0;
    ppa.blk = 0;
    ppa.sublk = 1;
    ppa.pg = 1;

    struct Block *blk = get_blk(&ppa);
    printf("Block id %d\n", blk->block_id);
    printf("Blokc vpc %d, ipc %d, epc %d\n", blk->vpc, blk->ipc, blk->epc);
    printf("Block victim count %d, Nonvictim count %d, position %d\n", blk->victim_sublk_count, blk->Nonvictim_sublk_count, blk->position);

    struct Sublock *sublk = get_sublk(&ppa);
    printf("Sublock id %d\n", sublk->id);
    printf("Sublokc vpc %d, ipc %d, epc %d\n", sublk->vpc, sublk->ipc, sublk->epc);
    printf("Sublock state %d\n", sublk->state);

    struct Page *pg = get_pg(&ppa);
    printf("pg id %d\n", pg->id);
    printf("pg addr ch %d, lun %d, pl %d, blk %d, sublk %d, pg %d\n", pg->addr.ch, pg->addr.lun, pg->addr.pl, pg->addr.blk, pg->addr.sublk, pg->addr.pg);
    printf("pg state %d, LBA_Hotlevel %d, type %d\n", pg->state, pg->LBA_HotLevel, pg->type);
}


void Test2(void) // 測試Finder1是否可以正常加入、移除、移動Block
{
    struct ppa ppa;
    ppa.ch =0;
    ppa.lun = 0;
    ppa.pl = 0;
    ppa.blk = 0;
    ppa.sublk = 0;
    ppa.pg = 0;
    
    struct Block *blk = get_blk(&ppa);
    printf("Block id %d\n", blk->block_id);
    printf("Blokc vpc %d, ipc %d, epc %d\n", blk->vpc, blk->ipc, blk->epc);
    printf("Block victim count %d, Nonvictim count %d, position %d\n", blk->victim_sublk_count, blk->Nonvictim_sublk_count, blk->position);
    printf("==========\n");

    for (int i=0; i<4; i++){
        int previous_victim_sublk_cnt = blk->victim_sublk_count;
        blk->victim_sublk_count++;
        blk->Nonvictim_sublk_count--;
        if (previous_victim_sublk_cnt == 0 && previous_victim_sublk_cnt != blk->victim_sublk_count){
            Add_Block_Finder1(blk->victim_sublk_count, blk);
        }else{
            if (blk->victim_sublk_count > nLayers_Finder1){
                printf("847 err blk victim sublk count over Finder1 layer \n");
                abort();
            }
            Change_Block_Position_Finder1(previous_victim_sublk_cnt, blk->victim_sublk_count, blk);
        }
    }
    Print_Finder1();
}

void Test3(void) //測試Mark_Page_Valid( )
{
    struct ppa ppa;
    ppa.ch =0;
    ppa.lun = 0;
    ppa.pl = 0;
    ppa.blk = 0;
    ppa.sublk = 0;
    ppa.pg = 0;

    for (int b=0; b<3; b++){
        ppa.blk = b;
        struct Block *blk = get_blk(&ppa);

        for (int s=0; s<sublks_per_blk; s++){
            ppa.sublk = s;
            struct Sublock *sublk = get_sublk(&ppa);

            for (int p=0; p<pgs_per_sublk; p++){
                ppa.pg = p;
                struct Page *pg = get_pg(&ppa);
                printf("Write Block %d, sublk %d, page %d\n", blk->block_id, sublk->id, pg->id);
                Mark_Page_Valid(&ppa);
            }
        }
    }
    printf("==========\n");
    Print_Finder1();
    printf("==========\n");
    Print_Finder2();
}

void Test4(void)
{
    struct ppa ppa;
    ppa.ch =0;
    ppa.lun = 0;
    ppa.pl = 0;
    ppa.blk = 0;
    ppa.sublk = 0;
    ppa.pg = 0;

    for (int b=0; b<3; b++){
        ppa.blk = b;
        struct Block *blk = get_blk(&ppa);

        for (int s=0; s<sublks_per_blk; s++){
            ppa.sublk = s;
            struct Sublock *sublk = get_sublk(&ppa);

            for (int p=0; p<pgs_per_sublk; p++){
                ppa.pg = p;
                struct Page *pg = get_pg(&ppa);
                printf("Write Block %d, sublk %d, page %d\n", blk->block_id, sublk->id, pg->id);
                Mark_Page_Valid(&ppa);
            }
        }
    }

    for (int b=0; b<3; b++){
        ppa.blk = b;
        struct Block *blk = get_blk(&ppa);

        for (int s=0; s<2; s++){
            ppa.sublk = s;
            struct Sublock *sublk = get_sublk(&ppa);

            for (int p=0; p<pgs_per_sublk; p++){
                ppa.pg = p;
                struct Page *pg = get_pg(&ppa);
                printf("Invalid Block %d, sublk %d, page %d, victim sublk %d, Nonvictim sublk %d\n", blk->block_id, sublk->id, pg->id, blk->victim_sublk_count, blk->Nonvictim_sublk_count);
                Mark_Page_Invalid(&ppa);
            }
        }        
    }

    printf("==========\n");
    Print_Finder1();
    printf("==========\n");
    Print_Finder2();
}

void Test5(void)
{
    for (int i=0; i<10; i++){
        struct ppa ppa = Get_Empty_PPA(0);
        struct Page *pg = get_pg(&ppa);
        printf("Page addr ch %d, lun %d, pl %d, blk %d, sublk %d, pg %d\n", pg->addr.ch, pg->addr.lun, pg->addr.pl, pg->addr.blk, pg->addr.sublk, pg->addr.pg);
        Mark_Page_Valid(&ppa);
    }
    Print_Finder1();
    Print_Finder2()
}
*/