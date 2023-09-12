/*
static void Test1(struct ssd *ssd, FILE *outfile)
{   
    // Request 1
    for (int i=0; i<2000; i++){
        int original_hot_level = -1;
        struct ppa *empty_ppa = get_empty_page(ssd, original_hot_level, General_Write, No_CopyWrite);
        struct nand_page *empty_pg = get_pg(ssd, empty_ppa);
        empty_pg->Hot_level = Hot_level_0;
        mark_page_valid(ssd, empty_ppa);
        if (i<1500){
            mark_page_invalid(ssd, empty_ppa, NULL);
        }
        fprintf(outfile, "ppa : ch %d, lun %d, pl %d, blk %d, sublk %d, pg %d\n", empty_ppa->g.ch, empty_ppa->g.lun, empty_ppa->g.pl ,empty_ppa->g.blk, empty_ppa->g.subblk, empty_ppa->g.pg);
    }
    clean_Temp_Block_Management();

    // Request 2
    for (int i=0; i<800; i++){
        int Hot_level = 1;
        struct ppa *empty_ppa = get_empty_page(ssd, Hot_level, General_Write, No_CopyWrite);
        struct nand_page *empty_pg = get_pg(ssd, empty_ppa);
        empty_pg->Hot_level = Hot_level_1;
        mark_page_valid(ssd, empty_ppa);
        fprintf(outfile, "ppa : ch %d, lun %d, pl %d, blk %d, sublk %d, pg %d\n", empty_ppa->g.ch, empty_ppa->g.lun, empty_ppa->g.pl ,empty_ppa->g.blk, empty_ppa->g.subblk, empty_ppa->g.pg);
    }
    clean_Temp_Block_Management();

    // GC Request
    for (int i=0; i<3; i++){
        fprintf(outfile,"Before Free_Block_Management Size %d\n", Free_Block_Management->Queue_Size);
        do_gc(ssd, true, NULL);
        fprintf(outfile,"After Free_Block_Management Size %d\n", Free_Block_Management->Queue_Size);
    }

    fprintf(outfile, "--------------------------------------\n");
    Print_Finder(outfile, 1);
    fprintf(outfile, "--------------------------------------\n");
    Print_Finder(outfile, 2);
}

static void Test2(struct ssd *ssd, FILE *outfile)
{   
    struct ppa *secure_deletion_table = malloc(sizeof(struct ppa) * (400));
    int sensitive_lpn_count = 0;
    int write_count = 0;

    // Request 1
    for (int i=0; i<400; i++){
        int original_hot_level = -1;
        struct ppa *empty_ppa = get_empty_page(ssd, original_hot_level, General_Write, No_CopyWrite);
        struct nand_page *empty_pg = get_pg(ssd, empty_ppa);
        empty_pg->Hot_level = Hot_level_0;
        empty_pg->pg_type = PG_General;
        mark_page_valid(ssd, empty_ppa);
        if (write_count == 30){
            empty_pg->pg_type = PG_Sensitive;
            mark_page_invalid(ssd, empty_ppa, NULL);
            secure_deletion_table[sensitive_lpn_count] = *empty_ppa;
            sensitive_lpn_count++;
            write_count = 0;
        }
        write_count++;
        fprintf(outfile, "ppa : ch %d, lun %d, pl %d, blk %d, sublk %d, pg %d\n", empty_ppa->g.ch, empty_ppa->g.lun, empty_ppa->g.pl ,empty_ppa->g.blk, empty_ppa->g.subblk, empty_ppa->g.pg);
    }
    clean_Temp_Block_Management();
    
    // Secure Deletion
    fprintf(outfile,"Before Free_Block_Management Size %d\n", Free_Block_Management->Queue_Size);
    do_secure_deletion(ssd, secure_deletion_table, sensitive_lpn_count, 400);
    fprintf(outfile,"After Free_Block_Management Size %d\n", Free_Block_Management->Queue_Size);

    fprintf(outfile, "--------------------------------------\n");
    Print_Finder(outfile, 1);
    fprintf(outfile, "--------------------------------------\n");
    Print_Finder(outfile, 2);
}

static void Test3(struct ssd *ssd, FILE *outfile)
{   
    NvmeRequest req;

    printf("1931\n");
    fprintf(outfile, "Free Block Management Size %d\n", Free_Block_Management->Queue_Size);
    uint64_t slba = 0;
    uint64_t nlb = 64;
    for (int i=0; i<256; i++){
        slba = slba + nlb;
        req.slba = slba;
        req.nlb = nlb;
        ssd_write(ssd, &req);
    }
    fprintf(outfile, "Free Block Management Size %d\n", Free_Block_Management->Queue_Size);

    
    req.slba = 1000;
    req.nlb = 4096;
    ssd_write(ssd, &req);
    fprintf(outfile, "Free Block Management Size %d\n", Free_Block_Management->Queue_Size);

   
    req.slba = 0;
    req.nlb = 1600;
    ssd_write(ssd, &req);
    fprintf(outfile, "Free Block Management Size %d\n", Free_Block_Management->Queue_Size);


    fprintf(outfile, "--------------------------------------\n");
    Print_Finder(outfile, 1);
    Print_Finder(outfile, 2);
    fprintf(outfile, "--------------------------------------\n");
}

static void Test4(struct ssd *ssd, FILE *outfile)
{   
    NvmeRequest req;

    fprintf(outfile, "Free Block Management Size %d\n", Free_Block_Management->Queue_Size);
    // 2048000
    for (int i=0 ;i<1000; i++){
        req.slba = i*2048;
        req.nlb = 2048;
        ssd_write(ssd, &req);           
    }
    fprintf(outfile, "Free Block Management Size %d\n", Free_Block_Management->Queue_Size);

    for (int i=0; i<342; i++){
        req.slba = i*6000;
        req.nlb = 6000;
        ssd_write(ssd, &req);      
    }
    fprintf(outfile, "Free Block Management Size %d\n", Free_Block_Management->Queue_Size);

    fprintf(outfile, "--------------------------------------\n");
    Print_Finder(outfile, 1);
    Print_Finder(outfile, 2);
    fprintf(outfile, "--------------------------------------\n");
    fprintf(outfile, "Do GC\n");
    do_gc(ssd, true, &req);
}

static void Test3(struct ssd *ssd, FILE *outfile)
{   
    //printf("2041\n");
    NvmeRequest req;

    fprintf(outfile, "Free Block Management Size %d, Free page %lu\n", Free_Block_Management->Queue_Size, Free_Page);
   
    uint64_t slba = 0;
    uint64_t nlb = 64;
    for (int i=0; i<10; i++){
        slba = slba + nlb;
        req.slba = slba;
        req.nlb = nlb;
        //printf("2052\n");
        ssd_write(ssd, &req);
        //printf("2054\n");

        fprintf(outfile, " \n");
        Print_Finder(outfile, 2);
        fprintf(outfile, "--------------------------------------\n");
    }
    
    fprintf(outfile, "Free Block Management Size %d, Free page %lu\n", Free_Block_Management->Queue_Size, Free_Page);
    //printf("2060\n");
}

static struct ppa *Get_General_Empty_Page(struct ssd *ssd, int Hot_Level)
{
    //printf("1423\n");
    struct ssdparams *spp = &ssd->sp;
    struct ppa *temp_ppa = NULL;
    struct ppa *empty_pg = malloc(sizeof(struct ppa));

    //printf("1428\n");
    temp_ppa = get_Empty_pg_from_Finder2(ssd, Hot_Level);
    //printf("1430\n");

    if (temp_ppa == NULL){
        if (Temp_Block_Management->Queue_Size!=0){
            //printf("1434\n");
            struct nand_block *blk = Peek(Temp_Block_Management);
            //printf("1436\n");
            empty_pg->g.ch = blk->ch;
            empty_pg->g.lun = blk->lun;
            empty_pg->g.pl = blk->pl;
            empty_pg->g.blk = blk->blk;
            int is_find = False;
            for (int i=0; i<spp->subblks_per_blk; i++){
                //printf("1443\n");
                struct nand_subblock *sublk = &blk->subblk[i];
                //printf("1445\n");
                if (sublk->was_full != SUBLK_FULL){
                    for (int j=0; j<spp->pgs_per_subblk; j++){
                        //printf("1448\n");
                        struct nand_page *pg = &sublk->pg[j];
                        //printf("1450\n");
                        if (pg->status == PG_FREE){
                            empty_pg->g.subblk = i;
                            empty_pg->g.pg = j;
                            is_find = True;
                            break;
                        }
                    }
                }
                if (is_find==True){
                    break;
                }
            }
            //printf("1463\n");
            if(empty_pg->g.subblk == (spp->subblks_per_blk-1) && empty_pg->g.pg == (spp->pgs_per_subblk-1)){
                //printf("1465\n");
                Pop(Temp_Block_Management);
                //printf("1467\n");
            }
            
            blk->current_sublk_id = empty_pg->g.subblk;
            blk->subblk[blk->current_sublk_id].current_page_id = empty_pg->g.pg;
            if (is_find == True){
                //printf("1473\n");
                return empty_pg;
            }else{
                //printf("1464 err !!\n");
                free(empty_pg);
                return NULL;
            }
        }else{
            if (Free_Block_Management->Queue_Size!=0){
                //printf("1484\n");
                struct nand_block *blk = Peek(Free_Block_Management);
                //printf("1486\n");
                Pop(Free_Block_Management);
                //printf("1488\n");
                
                if (blk==NULL){
                    printf("1490 err\n");
                }
                empty_pg->g.ch = blk->ch;
                empty_pg->g.lun = blk->lun;
                empty_pg->g.pl = blk->pl;
                empty_pg->g.blk = blk->blk;
                empty_pg->g.subblk = 0;
                empty_pg->g.pg = 0;
                //printf("1496\n");
                Push(Temp_Block_Management, blk);
                //printf("1498\n");
                
                blk->current_sublk_id = empty_pg->g.subblk;
                blk->subblk[blk->current_sublk_id].current_page_id = empty_pg->g.pg;

                //printf("1501\n");
                return empty_pg;
            }else{
                //printf("SSD space not enough !!\n");
                free(empty_pg);
                return NULL;
            }
        }
    }else{
        //printf("1495\n");
        printf("finder2 find page\n");
        *empty_pg = *temp_ppa;
        return empty_pg;
    }
}

*/

