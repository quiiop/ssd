def calculate(path, filename):
    r = 0
    FilePath = path + '/' + filename
    f = open(FilePath, 'r')

    for line in f.readlines():
        cnt = int(line)
        r = r + cnt
    f.close()
    return r

Move_Page_Cnt = calculate('/home/kuo/femu/build-femu', 'WA_Cnt_Record.txt')
Write_Page_Cnt = calculate('/home/kuo/femu/build-femu', 'Write_Cnt_Record.txt')

WA = (Write_Page_Cnt+Move_Page_Cnt) / Write_Page_Cnt
print(WA)






