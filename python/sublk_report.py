#report_sublk.py
import matplotlib.pyplot as plt
import numpy as np

def calculate(path, filename):
    FilePath = path + '/' + filename
    f = open(FilePath, 'r')
    count = 0
    total = np.uint64(0)

    for line in f.readlines():
        temp = np.uint64(line)
        total = total + temp
        count = count + 1
    f.close()
    return total, count

def calculate_WA(path, filename):
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

dir = "/home/kuo/Desktop/femu/build-femu"

#total lat
read_total_lat, read_count = calculate(dir, 'READ_lat.txt')
print("Read total lat = ", read_total_lat, " Read count = ", read_count)

write_total_lat, write_count = calculate(dir, 'WRITE_lat.txt')
print("Write total lat = ", write_total_lat, " Write count = ", write_count)

erase_total_lat, erase_count = calculate(dir, 'ERASE_lat.txt')
print("Erase total lat = ", erase_total_lat, " Erase count = ", erase_count)

#function lat
ssd_write_lat, ssd_write_cnt = calculate(dir, 'ssd_write_lat.txt')
print("SSD write lat = ", ssd_write_lat, " SSD write count = ", ssd_write_cnt)

ssd_read_lat, ssd_read_cnt = calculate(dir, 'ssd_read_lat.txt')
print("SSD read lat = ", ssd_read_lat, " SSD read count = ", ssd_read_cnt)

#printf waste time
ssd_write_waste_time, cnt = calculate(dir, 'ssd_write_waste_time.txt')
print("SSD write waste time = ", ssd_write_waste_time)

ssd_trim_waste_time, cnt = calculate(dir, 'ssd_trim_waste_time.txt')
print("SSD trim waste time = ", ssd_trim_waste_time)

# GC sublk & blk cnt
total_sublk, cnt = calculate(dir, 'gc_sublk_cnt.txt')
print("SSD gc total sublk cnt = ", total_sublk)
total_blk, cnt = calculate(dir, 'gc_blk_cnt.txt')
print("SSD gc total blk cnt = ", total_blk)

# iops count 
print("write cnt ", ssd_write_cnt)
print("read cnt ", ssd_read_cnt)
print("gc write cnt ", (write_count - ssd_write_cnt));
print("gc read cnt ", (read_count - ssd_read_cnt));
print("erase cnt", erase_count)

# total latency
div = 1
for i in range(9):
    div = div * 10
Latency = ((read_total_lat + write_total_lat + erase_total_lat) - ssd_write_waste_time - ssd_trim_waste_time)/div
print("Latency = ",Latency)

# iops
IOPS = ( ssd_write_cnt + ssd_read_cnt +  erase_count) / Latency
print("IOPS = ",IOPS)

# WA
Move_Page_Cnt = calculate_WA(dir, 'WA_Cnt_Record.txt')
Write_Page_Cnt = calculate_WA(dir, 'Write_Cnt_Record.txt')
WA = (Write_Page_Cnt[0]+Move_Page_Cnt[0]) / Write_Page_Cnt[0]
print("WA = ", WA)