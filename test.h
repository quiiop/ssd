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

*/

