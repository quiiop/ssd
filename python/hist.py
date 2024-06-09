import numpy as np
import matplotlib.pyplot as plt

def calculate(path, filename):
    FilePath = path + '/' + filename
    f = open(FilePath, 'r')
    arr = []

    for line in f.readlines():
        temp = np.uint64(line)
        arr.append(temp)
    f.close()
    return arr

dir = '/home/kuo/Desktop/femu/build-femu'
arr = calculate(dir, 'write_node.txt')
print("lba cnt = ", len(arr))

# 繪製直方圖
plt.hist(arr, bins=30, edgecolor='black')

# 添加標題和標籤
plt.title('Histogram of Normally Distributed Data')
plt.xlabel('Value')
plt.ylabel('Frequency')

# 顯示圖表
plt.show()
