#include "ftl.h"
# include <stdlib.h>
# include <stdio.h>

static void Test_Case(struct ssd *ssd)
{
/*Test 從Free_Block_Management取得empty page*/
    /*
    empty_ppa = get_empty_page(ssd, -1, DO_Write);
    empty_pg = get_pg(ssd, empty_ppa);
    empty_pg->Hot_level = Hot_level_0;
    mark_page_valid(ssd, empty_ppa);
    fprintf(outfile26, "ppa : ch %d, lun %d, pl %d, blk %d, sublk %d, pg %d\n", empty_ppa->g.ch, empty_ppa->g.lun, empty_ppa->g.pl ,empty_ppa->g.blk, empty_ppa->g.subblk, empty_ppa->g.pg);
    */
     
/*Test 從Temp_Block_Management取得empty page*/
    /*
    empty_ppa = get_empty_page(ssd, -1, DO_Write);
    empty_pg = get_pg(ssd, empty_ppa);
    empty_pg->Hot_level = Hot_level_0;
    mark_page_valid(ssd, empty_ppa); 
    fprintf(outfile26, "ppa : ch %d, lun %d, pl %d, blk %d, sublk %d, pg %d\n", empty_ppa->g.ch, empty_ppa->g.lun, empty_ppa->g.pl ,empty_ppa->g.blk, empty_ppa->g.subblk, empty_ppa->g.pg);
    */

    /*
    for (int i=0; i<500; i++){
        if (i<250){
            empty_ppa = get_empty_page(ssd, -1, DO_Write);
            empty_pg = get_pg(ssd, empty_ppa);
            empty_pg->Hot_level = Hot_level_0;
            mark_page_valid(ssd, empty_ppa); 
            fprintf(outfile26, "ppa : ch %d, lun %d, pl %d, blk %d, sublk %d, pg %d\n", empty_ppa->g.ch, empty_ppa->g.lun, empty_ppa->g.pl ,empty_ppa->g.blk, empty_ppa->g.subblk, empty_ppa->g.pg);
        }else{
            empty_ppa = get_empty_page(ssd, -1, DO_Write);
            empty_pg = get_pg(ssd, empty_ppa);
            empty_pg->Hot_level = Hot_level_1;
            mark_page_valid(ssd, empty_ppa); 
            fprintf(outfile26, "ppa : ch %d, lun %d, pl %d, blk %d, sublk %d, pg %d\n", empty_ppa->g.ch, empty_ppa->g.lun, empty_ppa->g.pl ,empty_ppa->g.blk, empty_ppa->g.subblk, empty_ppa->g.pg);
        }
    }
    */

/*Test Finder1*/
   /*
   for (int i=0; i<300; i++){ // Test victim sublk 10
        empty_ppa = get_empty_page(ssd, -1, DO_Write);
        fprintf(outfile26, "valid ppa : ch %d, lun %d, pl %d, blk %d, sublk %d, pg %d\n", empty_ppa->g.ch, empty_ppa->g.lun, empty_ppa->g.pl ,empty_ppa->g.blk, empty_ppa->g.subblk, empty_ppa->g.pg);

        empty_pg = get_pg(ssd, empty_ppa);
        empty_pg->Hot_level = Hot_level_0;
        mark_page_valid(ssd, empty_ppa); 
        if (i<280){
            mark_page_invalid(ssd, empty_ppa, NULL);
            fprintf(outfile26, "invalid ppa : ch %d, lun %d, pl %d, blk %d, sublk %d, pg %d\n", empty_ppa->g.ch, empty_ppa->g.lun, empty_ppa->g.pl ,empty_ppa->g.blk, empty_ppa->g.subblk, empty_ppa->g.pg);
        }
   }
   */
   
   /*
   for (int i=0; i<72; i++){
        empty_ppa = get_empty_page(ssd, -1, DO_Write);
        empty_pg = get_pg(ssd, empty_ppa);
        empty_pg->Hot_level = Hot_level_0;
        mark_page_valid(ssd, empty_ppa); 
        fprintf(outfile26, "ppa : ch %d, lun %d, pl %d, blk %d, sublk %d, pg %d\n", empty_ppa->g.ch, empty_ppa->g.lun, empty_ppa->g.pl ,empty_ppa->g.blk, empty_ppa->g.subblk, empty_ppa->g.pg);
        if (i>=60){
            mark_page_invalid(ssd, empty_ppa, NULL);
            empty_pg->Hot_level++;
            if (empty_pg->Hot_level>3){
                empty_pg->Hot_level = Hot_level_3;
            }
            empty_ppa = get_empty_page(ssd, empty_ppa->Hot_Level, DO_Write);
            mark_page_valid(ssd, empty_ppa);
        }
    }
   */

/*Test 從Finder2取得empty page*/
    /*
    Free_Block_Management->Queue_Size = 0;
    Temp_Block_Management->Queue_Size = 0;
    empty_ppa = get_empty_page(ssd, -1, DO_Write);
    fprintf(outfile26, "From Finder2 : ppa : ch %d, lun %d, pl %d, blk %d, sublk %d, pg %d\n", empty_ppa->g.ch, empty_ppa->g.lun, empty_ppa->g.pl ,empty_ppa->g.blk, empty_ppa->g.subblk, empty_ppa->g.pg);
    */

/* Test Block */ 
    /*
    struct nand_block *blk = NULL;
    struct nand_subblock *sublk = NULL;
    struct ppa *arr = malloc(sizeof(struct ppa)*10);
    int index = 0;

    printf("1774\n");
    for (int j=0; j<16; j++){
        printf("1776\n");
        empty_ppa = get_empty_page(ssd, -1, DO_Write);
        printf("1777\n");
        empty_pg = get_pg(ssd, empty_ppa);
        printf("1779\n");
        if (j<7){
            printf("1781\n");
            empty_pg->Hot_level = Hot_level_1;
            printf("1783\n");
        }else{
            printf("1784\n");
            empty_pg->Hot_level = Hot_level_2;
            printf("1782\n");
            arr[index] = *empty_ppa;
            index++;
        }
        printf("1790\n");
        mark_page_valid(ssd, empty_ppa);
        printf("1791\n");
        blk = get_blk(ssd, empty_ppa);
        sublk = get_subblk(ssd, empty_ppa); 
        fprintf(outfile26, "R1, Block : current sublk id %lu, F1 pos %d, F2 pos %d\n",blk->current_sublk_id ,blk->In_Finder1_Position, blk->In_Finder2_Position);
        fprintf(outfile26, "R1, sublk id %lu, vpc %d, ipc %d, epc %d, Hot_Level %d\n",sublk->sublk, sublk->vpc, sublk->ipc, sublk->epc, sublk->Current_Hot_Level);
    }
    printf("1789\n");

    for (int j=0; j<10; j++){
        empty_ppa = get_empty_page(ssd, -1, DO_Write);
        empty_pg = get_pg(ssd, empty_ppa);
        empty_pg->Hot_level = Hot_level_3;
        mark_page_valid(ssd, empty_ppa);
        blk = get_blk(ssd, empty_ppa);
        sublk = get_subblk(ssd, empty_ppa); 
        fprintf(outfile26, "R2, Block : current sublk id %lu, F1 pos %d, F2 pos %d\n",blk->current_sublk_id ,blk->In_Finder1_Position, blk->In_Finder2_Position);
        fprintf(outfile26, "R2, sublk id %lu, vpc %d, ipc %d, epc %d, Hot_Level %d\n",sublk->sublk, sublk->vpc, sublk->ipc, sublk->epc, sublk->Current_Hot_Level);
    }
    printf("1799\n");

    for (int i=0; i<=index ;i++){
        mark_page_invalid(ssd, &arr[i], NULL);
    }
    printf("1804\n");

    fprintf(outfile26, "Finall, Block : current sublk id %lu, F1 pos %d, F2 pos %d\n",blk->current_sublk_id ,blk->In_Finder1_Position, blk->In_Finder2_Position);
    fprintf(outfile26, "Finall, sublk id %lu, vpc %d, ipc %d, epc %d, Hot_Level %d\n",sublk->sublk, sublk->vpc, sublk->ipc, sublk->epc, sublk->Current_Hot_Level);
    fprintf(outfile26, "------------------------\n");

    Print_Finder(outfile26, 1);
    fprintf(outfile26, "  \n");
    Print_Finder(outfile26, 2); 
    */

/* Test Secure deletion */
    /*
    struct ppa *secure_deletion_table = malloc(sizeof(struct ppa) * (300));
    int sensitive_lpn_count = 0;
    for (int i=0; i<300; i++){ // Test victim sublk 10
            empty_ppa = get_empty_page(ssd, -1, DO_Write);
            empty_pg = get_pg(ssd, empty_ppa);
            empty_pg->Hot_level = Hot_level_0;
            mark_page_valid(ssd, empty_ppa); 
            fprintf(outfile26, "vpa : ch %d, lun %d, pl %d, blk %d, sublk %d, pg %d\n", empty_ppa->g.ch, empty_ppa->g.lun, empty_ppa->g.pl ,empty_ppa->g.blk, empty_ppa->g.subblk, empty_ppa->g.pg);
            if(i%10 == 8){
                mark_page_invalid(ssd, empty_ppa, NULL);
                secure_deletion_table[sensitive_lpn_count] = *empty_ppa;
                sensitive_lpn_count++;
                fprintf(outfile26, "DSS ipa : ch %d, lun %d, pl %d, blk %d, sublk %d, pg %d\n", empty_ppa->g.ch, empty_ppa->g.lun, empty_ppa->g.pl ,empty_ppa->g.blk, empty_ppa->g.subblk, empty_ppa->g.pg);
            }
    }
    fprintf(outfile26, "Before\n");
    Print_Finder(outfile26, 1);
    fprintf(outfile26, "------------------------\n");
    Print_Finder(outfile26, 2);
    fprintf(outfile26, "------------------------\n");

    while(1){
        if (Temp_Block_Management->Queue_Size == 0){
            break;
        }
        Pop(Temp_Block_Management);
    }
    
    do_secure_deletion(ssd, secure_deletion_table, sensitive_lpn_count, 300);
    
    fprintf(outfile26, "After\n");
    Print_Finder(outfile26, 1);
    fprintf(outfile26, "------------------------\n");
    Print_Finder(outfile26, 2);
    fprintf(outfile26, "------------------------\n");
    */
    
    
/* Test Secure deletion 2*/
    /*
    struct ppa *empty_ppa = NULL;
    struct nand_page *empty_pg = NULL;
    outfile26 = fopen(fileName26, "wb");

    struct nand_block *blk = NULL;
    struct nand_subblock *sublk = NULL;
   
   struct ppa *secure_deletion_table = malloc(sizeof(struct ppa) * (100));
   int sensitive_lpn_count = 0;
   struct ppa first_ppa;

   for (int i=0; i<90; i++){ // Test victim sublk 10
            empty_ppa = get_empty_page(ssd, -1, DO_Write);
            if (i==0){
                first_ppa = *empty_ppa;
            }
            printf("1979\n");
            empty_pg = get_pg(ssd, empty_ppa);
            printf("1981\n");
            empty_pg->Hot_level = Hot_level_0;
            printf("1983\n");
            mark_page_valid(ssd, empty_ppa); 
            printf("1984\n");
            blk = get_blk(ssd, empty_ppa);
            fprintf(outfile26, "Block : current sublk id %lu, F1 pos %d, F2 pos %d\n",blk->current_sublk_id ,blk->In_Finder1_Position, blk->In_Finder2_Position);
            printf("1987\n");
            sublk = get_subblk(ssd, empty_ppa);
            fprintf(outfile26, "Sublk id %lu, vpc %d, ipc %d, epc %d, Hot_Level %d\n",sublk->sublk, sublk->vpc, sublk->ipc, sublk->epc, sublk->Current_Hot_Level);
            printf("1989\n");
            if(i%10 == 8){
                mark_page_invalid(ssd, empty_ppa, NULL);
                fprintf(outfile26, "1998 Mark Free sublk %p\n", sublk);
                fprintf(outfile26, "1998 blk=%d, sublk=%d, pg=%d\n", empty_ppa->g.blk, empty_ppa->g.subblk, empty_ppa->g.pg);
                secure_deletion_table[sensitive_lpn_count] = *empty_ppa;
                sensitive_lpn_count++;
                fprintf(outfile26, "DSS ipa : ch %d, lun %d, pl %d, blk %d, sublk %d, pg %d\n", empty_ppa->g.ch, empty_ppa->g.lun, empty_ppa->g.pl ,empty_ppa->g.blk, empty_ppa->g.subblk, empty_ppa->g.pg);
            }
    }
    printf("1987\n");

    for (int j=0; j<90; j++){
        empty_ppa = get_empty_page(ssd, -1, DO_Write);
        empty_pg = get_pg(ssd, empty_ppa);
        empty_pg->Hot_level = Hot_level_0;
        mark_page_valid(ssd, empty_ppa);
        
        blk = get_blk(ssd, empty_ppa);
        sublk = get_subblk(ssd, empty_ppa); 
        fprintf(outfile26, "Block : current sublk id %lu, F1 pos %d, F2 pos %d\n",blk->current_sublk_id ,blk->In_Finder1_Position, blk->In_Finder2_Position);
        fprintf(outfile26, "Sublk id %lu, vpc %d, ipc %d, epc %d, Hot_Level %d\n",sublk->sublk, sublk->vpc, sublk->ipc, sublk->epc, sublk->Current_Hot_Level);
    }
    printf("2000\n");

    while(1){
        if (Temp_Block_Management->Queue_Size == 0){
            break;
        }
        Pop(Temp_Block_Management);
    }
    printf("2008\n");
    
    do_secure_deletion(ssd, secure_deletion_table, sensitive_lpn_count, 100);

    blk = get_blk(ssd, &first_ppa);
    struct nand_subblock *first_sublk = get_subblk(ssd, &first_ppa);
    fprintf(outfile26, "First Blk current sublk id %lu, Finder2 pos %d\n", blk->current_sublk_id, blk->In_Finder2_Position);
    fprintf(outfile26, "Fist sublk vpc %d, ipc %d, epc %d, Hot_level %d\n", first_sublk->vpc, first_sublk->ipc, first_sublk->epc, first_sublk->Current_Hot_Level);

    */
    /*
    struct ppa *empty_ppa = NULL;
    struct nand_page *empty_pg = NULL;
    struct ppa *secure_deletion_table = malloc(sizeof(struct ppa) * (100));
    int sensitive_lpn_count = 0;
    outfile26 = fopen(fileName26, "wb");
    fprintf(outfile26, "7/22 Test !! \n");
    */

    /*blk0*/
    /*
    for (int i=0; i<256; i++){
        empty_ppa = get_empty_page(ssd, -1, DO_Write);
        empty_pg = get_pg(ssd, empty_ppa);
        empty_pg->Hot_level = Hot_level_0;
        mark_page_valid(ssd, empty_ppa);
    }
    */

    /*blk1*/
    /*
    for (int i=0; i<256; i++){
        empty_ppa = get_empty_page(ssd, -1, DO_Write);
        empty_pg = get_pg(ssd, empty_ppa);
        empty_pg->Hot_level = Hot_level_0;
        mark_page_valid(ssd, empty_ppa);
    }
    */

    /*blk2*/
    /*
    struct nand_subblock *sublk2 = NULL;
    for (int i=0; i<16; i++){
        for (int j=0; j<16; j++){
            empty_ppa = get_empty_page(ssd, -1, DO_Write);
            empty_pg = get_pg(ssd, empty_ppa);
            if (i==2){
                empty_pg->Hot_level = Hot_level_2;
            }else{
                empty_pg->Hot_level = Hot_level_0;
            }
            mark_page_valid(ssd, empty_ppa);
            if (i== 2){
                mark_page_invalid(ssd, empty_ppa, NULL);
                sublk2 = get_subblk(ssd, empty_ppa);
                secure_deletion_table[sensitive_lpn_count] = *empty_ppa;
                sensitive_lpn_count++;
            }
        }
    }
    fprintf(outfile26, "2094 invalid sublk : ch %lu, lun %lu, pl %lu, blk %lu, sublk %lu, ipc %d, vpc %d, epc %d\n", sublk2->ch, sublk2->lun, sublk2->pl ,sublk2->blk, sublk2->sublk, sublk2->ipc, sublk2->vpc, sublk2->epc);
    Print_Finder(outfile26, 2);
    Print_Finder(outfile26, 1);

    fprintf(outfile26, "After --------------------------------------------\n");
    do_secure_deletion(ssd, secure_deletion_table, sensitive_lpn_count, 100);
    Free_Block_Management->Queue_Size = 0;
    Temp_Block_Management->Queue_Size = 0;
    for(int i=0; i<10; i++){
        empty_ppa = get_empty_page(ssd, -1, DO_Write);
        empty_pg = get_pg(ssd, empty_ppa);
        empty_pg->Hot_level = Hot_level_0;
        mark_page_valid(ssd, empty_ppa);
        fprintf(outfile26, "2017: ch %d, lun %d, pl %d, blk %d, sublk %d, pg %d\n", empty_ppa->g.ch, empty_ppa->g.lun, empty_ppa->g.pl ,empty_ppa->g.blk, empty_ppa->g.subblk, empty_ppa->g.pg);
    }
    Print_Finder(outfile26, 2);
    Print_Finder(outfile26, 1);
    */

   /*blk2*/
   /*
   for (int i=0 ;i<16; i++){
        for (int j=0; j<16; j++){
            empty_ppa = get_empty_page(ssd, -1, DO_Write);
            empty_pg = get_pg(ssd, empty_ppa);
            empty_pg->Hot_level = Hot_level_0;
            mark_page_valid(ssd, empty_ppa);
            if (i==2 || i==3 || i==4){
                mark_page_invalid(ssd, empty_ppa, NULL);
                if(i==2){
                    secure_deletion_table[sensitive_lpn_count] = *empty_ppa;
                    sensitive_lpn_count++;
                }
            }
        }
   }
    Print_Finder(outfile26, 2);
    Print_Finder(outfile26, 1);
    fprintf(outfile26, "After --------------------------------------------\n");
   */
    //do_secure_deletion(ssd, secure_deletion_table, sensitive_lpn_count, 100);
    /*
    do_gc(ssd, true, NULL);
    Print_Finder(outfile26, 2);
    Print_Finder(outfile26, 1);

    */

    /*
    for (int i=0; i<16; i++){
        for(int j=0; j<16; j++){
            empty_ppa = get_empty_page(ssd, -1, DO_Write);
            empty_pg = get_pg(ssd, empty_ppa);
            if(i==15){
                if(j<5){
                     empty_pg->Hot_level = Hot_level_0;
                }else{
                    empty_pg->Hot_level = Hot_level_2;
                }
            }else{
                empty_pg->Hot_level = Hot_level_0;
            }
            mark_page_valid(ssd, empty_ppa);
        }
    }

    for (int i=0; i<5; i++){
        if(i==4){
           for(int j=0; j<5; j++){
                empty_ppa = get_empty_page(ssd, -1, DO_Write);
                empty_pg = get_pg(ssd, empty_ppa);
                empty_pg->Hot_level = Hot_level_0;
                mark_page_valid(ssd, empty_ppa);
           }
        }else{
            for(int j=0; j<16; j++){
                empty_ppa = get_empty_page(ssd, -1, DO_Write);
                empty_pg = get_pg(ssd, empty_ppa);
                empty_pg->Hot_level = Hot_level_0;
                mark_page_valid(ssd, empty_ppa);
           }
        }
    }
    while(1){
        if (Temp_Block_Management->Queue_Size == 0){
            break;
        }
        Pop(Temp_Block_Management);
    }

    struct ppa *secure_deletion_table = malloc(sizeof(struct ppa) * (100));
    int sensitive_lpn_count = 0;
    struct nand_block *blk4 = NULL;

    for (int i=0 ;i<10; i++){
        for (int j=0; j<16; j++){
            empty_ppa = get_empty_page(ssd, -1, DO_Write);
            empty_pg = get_pg(ssd, empty_ppa);
            empty_pg->Hot_level = Hot_level_0;
            mark_page_valid(ssd, empty_ppa);
            fprintf(outfile26, "blk4 : ch %d, lun %d, pl %d, blk %d, sublk %d, pg %d\n", empty_ppa->g.ch, empty_ppa->g.lun, empty_ppa->g.pl ,empty_ppa->g.blk, empty_ppa->g.subblk, empty_ppa->g.pg);
            if (j==10){
                mark_page_invalid(ssd, empty_ppa, NULL);
                secure_deletion_table[sensitive_lpn_count] = *empty_ppa;
                sensitive_lpn_count++;
                fprintf(outfile26, "invalid blk4 : ch %d, lun %d, pl %d, blk %d, sublk %d, pg %d\n", empty_ppa->g.ch, empty_ppa->g.lun, empty_ppa->g.pl ,empty_ppa->g.blk, empty_ppa->g.subblk, empty_ppa->g.pg);
            } 
        }
        blk4 = get_blk(ssd, empty_ppa);
    }

    for(int i=0; i<16; i++){
        struct nand_subblock *sublk4 = &blk4->subblk[i];
        fprintf(outfile26, "2139 : sublk id %lu, vpc %d, ipc %d, epc %d\n", sublk4->sublk, sublk4->vpc, sublk4->ipc, sublk4->epc);
    }

    fprintf(outfile26, " \n");
    fprintf(outfile26, "Before -------------------- \n");
    Print_Finder(outfile26, 2);
    fprintf(outfile26, "-------------------- \n");
    fprintf(outfile26, " \n");
    
    do_secure_deletion(ssd, secure_deletion_table, sensitive_lpn_count, 100);
    
    fprintf(outfile26, " \n");
    fprintf(outfile26, "After -------------------- \n");
    Print_Finder(outfile26, 2);
    fprintf(outfile26, "-------------------- \n");
    fprintf(outfile26, " \n");
    */

    /*
    Print_Queue(Temp_Block_Management, outfile26);
    fclose(outfile26);
    */
} 