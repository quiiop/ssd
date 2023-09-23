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

    FilePath = path + '/' + filename
    f = open(FilePath, 'r')

    for line in f.readlines():
        valid_pg_cnt = int(line.split(" ")[0])
        invalid_pg_cnt = int(line.split(" ")[1])

        valid_cnt.append(valid_pg_cnt)
        invalid_cnt.append(invalid_pg_cnt)
    array.append(valid_cnt)
    array.append(invalid_cnt)
    
    return array 

def sum(temp):
    total = 0
    for i in range(0, len(temp)):
        total = total + int(temp[i])
    return total

Move_Page_Cnt = calculate('/home/kuo/femu/build-femu', 'WA_Cnt_Record.txt')
Write_Page_Cnt = calculate('/home/kuo/femu/build-femu', 'Write_Cnt_Record.txt')

WA = (Write_Page_Cnt[0]+Move_Page_Cnt[0]) / Write_Page_Cnt[0]
print("WA = ", WA)

Space_Cnt = calculate('/home/kuo/femu/build-femu', 'space.txt')
print("Space total = ", Space_Cnt[0])
print("GC blk Cnt = ", Space_Cnt[1])

GC_Blk_Cnt = calculate2('/home/kuo/femu/build-femu', 'GC_Blk_Record.txt')
valid_cnt = GC_Blk_Cnt[0]
invalid_cnt = GC_Blk_Cnt[1]
x_data = np.arange(0, len(valid_cnt))
print('valid cnt total : ',sum(valid_cnt))
print('invalid cnt total : ',sum(invalid_cnt))

space_avg = (256 * Space_Cnt[1])/sum(valid_cnt)
print("Space Avg = ", space_avg)

fig = plt.figure()
#plt.subplot(1, 2, 1)
plt.title("GC Blk Record, red= valid cnt, blue= invalid cnt")
plt.plot(x_data, valid_cnt, 'red')
plt.plot(x_data, invalid_cnt, "blue")
plt.show()







