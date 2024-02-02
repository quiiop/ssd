import matplotlib.pyplot as plt
import numpy as np
from collections import Counter
import statistics

def calculate(path, filename):
    FilePath = path + '/' + filename
    f = open(FilePath, 'r')
    count = 0
    array = []

    for line in f.readlines():
        temp = np.uint64(line)
        
        array.append(temp)

    f.close()
    return array, count

def scatter_1(dictory):  
    for element, count in dictory.items():
        plt.scatter(element, count, color="blue", s=10)

    plt.savefig("/home/kuo/Desktop/python/ssd/imgs/pic1.png")
    plt.show()

def plot_histogram(data):
    plt.hist(data, bins=50, color='blue', alpha=0.7)
    plt.xlabel('Value')
    plt.ylabel('Frequency')
    plt.title('Histogram of Write Values')
    plt.grid(True)
    plt.savefig("/home/kuo/Desktop/python/ssd/imgs/histogram_2.png")
    plt.show()

dir = "/home/kuo/Desktop/femu/build-femu"
array, cnt = calculate(dir, "write_node.txt")
print("avg ", np.mean(array))
print("max ", max(array))
print("min ", min(array))


plot_histogram(array)


'''
tmp_range_1 = max(array) * 0.2
tmp_range_2 = max(array) * 0.8
print("zone_1 range = [ ", 0, " ~ ", tmp_range_1, " ]")
print("zone_2 range = [ ", tmp_range_1, " ~ ", tmp_range_2, " ]")
print("zone_3 range = [ ", tmp_range_2, " ~ ", max(array), " ]")
'''

'''
counter_dict = dict(Counter(array))
elements, counts = zip(*counter_dict.items())
standard_deviation = np.std(elements)
print(standard_deviation)

# 设置随机种子以确保可复现性kuo
ㄧㄟ
np.random.seed(42)

# 创建两个列表，一个标准差为0，一个标准差为99
list_std_0 = np.random.normal(loc=0, scale=0, size=1000)
list_std_99 = np.random.normal(loc=0, scale=99, size=1000)

# 打印前几个元素以查看数据
print("列表1（标准差为0）：", list_std_0[:5])
print("列表2（标准差为99）：", list_std_99[:5])

# 画出散点图
plt.scatter(range(len(list_std_0)), list_std_0, label='标准差为0')
plt.scatter(range(len(list_std_99)), list_std_99, label='标准差为99')

# 添加标题和标签
plt.title('散点图 - 不同标准差')
plt.xlabel('数据点')
plt.ylabel('数值')

# 显示图例
plt.legend()

# 显示图表
plt.show()
'''