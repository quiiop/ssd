import matplotlib.pyplot as plt
import numpy as np

def calculate(path, filename):
    array = []
    r = 0
    FilePath = path + '/' + filename
    f = open(FilePath, 'r')
    count = 0

    for line in f.readlines():
        cnt = float(line)
        r = r + cnt
        count = count +1
    f.close()
    array.append(r)
    array.append(count)
    return array

def calculate2(path, filename):
    array = []
    valid_cnt = []
    invalid_cnt = []
    space_array = []
    All_Invalid_Page_Cnt = 0

    FilePath = path + '/' + filename
    f = open(FilePath, 'r')

    for line in f.readlines():
        valid_pg_cnt = int(line.split(" ")[0])
        invalid_pg_cnt = int(line.split(" ")[1])
        space = 0

        valid_cnt.append(valid_pg_cnt)
        invalid_cnt.append(invalid_pg_cnt)
        
        if (valid_pg_cnt == 0):
            All_Invalid_Page_Cnt = All_Invalid_Page_Cnt +1
        if (valid_pg_cnt == 0):
            space = (invalid_pg_cnt + valid_pg_cnt) / 1
        else:
            space = (invalid_pg_cnt + valid_pg_cnt) / valid_pg_cnt
        space_array.append(space)

    array.append(valid_cnt)
    array.append(invalid_cnt)
    array.append(space_array)
    array.append(All_Invalid_Page_Cnt)
    return array 

def sum(temp):
    total = 0
    for i in range(0, len(temp)):
        total = total + int(temp[i])
    return total

# WA
Move_Page_Cnt = calculate('/home/kuo/femu/build-femu', 'WA_Cnt_Record.txt')
Write_Page_Cnt = calculate('/home/kuo/femu/build-femu', 'Write_Cnt_Record.txt')
WA = (Write_Page_Cnt[0]+Move_Page_Cnt[0]) / Write_Page_Cnt[0]
print("WA = ", WA)

# Extra Empty page
Space_Cnt = calculate2('/home/kuo/femu/build-femu', 'GC_Sublk_Record.txt')
GC_Valid_Pg_List = Space_Cnt[0]
GC_Invalid_Pg_List = Space_Cnt[1]
Total_Valid_Pg = sum(GC_Valid_Pg_List)
Total_Invalid_Pg = sum(GC_Invalid_Pg_List)
Total_Space = sum(Space_Cnt[2])
Avg_Extra_Empty_Pg = (Total_Space) / (len(Space_Cnt[2]) - Space_Cnt[3])
print('Total Space = ', Total_Space)
print("Avg Extra Empty Pg = ", Avg_Extra_Empty_Pg)


fig = plt.figure()
x_data = np.arange(0, len(GC_Valid_Pg_List))
plt.title("Our Research")
plt.plot(x_data, GC_Valid_Pg_List  , 'red')
plt.plot(x_data, GC_Invalid_Pg_List, 'blue')
plt.savefig('/home/kuo/PyProject/OurResearch_test.png') #here
plt.show()
