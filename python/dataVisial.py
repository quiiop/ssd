# dataVisial.py
import matplotlib as mpl
import matplotlib.pyplot as plt
import numpy as np

def compare_wa(x, sublk_wa_y, femu_wa_y):
    plt.title("compare wa")

    plt.scatter(x, sublk_wa_y, color="blue", label="Experimental Data", marker='o')
    plt.plot(x, sublk_wa_y, color="blue")
    
    plt.scatter(x, femu_wa_y, color="red", label="baseline", marker='s')
    plt.plot(x, femu_wa_y, color="red")
    
    plt.legend()
    plt.savefig("/home/kuo/Desktop/python/ssd/imgs/compare_wa.png")
    plt.show()

def compare_iops(x, sublk_iops_y, femu_iops_y):
    plt.title("compare iops")

    plt.scatter(x, sublk_iops_y, color="blue", label="Experimental Data", marker='o')
    plt.plot(x, sublk_iops_y, color="blue")
    
    plt.scatter(x, femu_iops_y, color="red", label="baseline", marker='s')
    plt.plot(x, femu_iops_y, color="red")
    
    plt.legend()
    plt.savefig("/home/kuo/Desktop/python/ssd/imgs/compare_iops.png")
    plt.show()

def compare_latency(x, sublk_latency_y, femu_latency_y):
    plt.title("compare latency")

    plt.scatter(x, sublk_latency_y, color="blue", label="Experimental Data", marker='o')
    plt.plot(x, sublk_latency_y, color="blue")
    
    plt.scatter(x, femu_latency_y, color="red", label="baseline", marker='s')
    plt.plot(x, femu_latency_y, color="red")
    
    plt.legend()
    plt.savefig("/home/kuo/Desktop/python/ssd/imgs/compare_latency.png")
    plt.show()

def decrease_percentage_picture(x, decrease_wa_y, decrease_iops_y, decrease_latency_y):
    plt.title("wa, iops, latency decrease percentage (%)")

    plt.scatter(x, decrease_wa_y, color="blue", label="WA", marker='o')
    plt.plot(x, decrease_wa_y, color="blue")

    plt.scatter(x, decrease_iops_y, color="green", label="IOPS", marker='s')
    plt.plot(x, decrease_iops_y, color="green")

    plt.scatter(x, decrease_latency_y, color="red", label="Latency", marker='^')
    plt.plot(x, decrease_latency_y, color="red")

    plt.legend()
    plt.savefig("/home/kuo/Desktop/python/ssd/imgs/decrease_percentage.png")
    plt.show()

def blk_sublk_cnt(x, blk_cnt, sublk_cnt):
    plt.title("blk sublk cnt")

    plt.scatter(x, sublk_cnt, color="blue", label="sublk cnt", marker='o')
    plt.plot(x, sublk_cnt, color="blue")
    
    plt.scatter(x, blk_cnt, color="red", label="blk cnt", marker='s')
    plt.plot(x, blk_cnt, color="red")
    
    plt.legend()
    plt.savefig("/home/kuo/Desktop/python/ssd/imgs/blk_sublk_cnt.png")
    plt.show()

def different_wa(x, decrease_wa_y):
    plt.title("WA decrease percentage (%)")

    plt.scatter(x, decrease_wa_y, color="blue", label="WA", marker='o')
    plt.plot(x, decrease_wa_y, color="blue")

    plt.legend()
    plt.savefig("/home/kuo/Desktop/python/ssd/imgs/different_wa.png")
    plt.show()

def different_latency(x, decrease_latency_y):
    plt.title("Latency decrease percentage (%)")

    plt.scatter(x, decrease_latency_y, color="blue", label="Latency", marker='o')
    plt.plot(x, decrease_latency_y, color="blue")

    plt.legend()
    plt.savefig("/home/kuo/Desktop/python/ssd/imgs/different_latency.png")
    plt.show()

def different_iops(x, increase_iops_y):
    plt.title("IOPS increase percentage (%)")

    plt.scatter(x, increase_iops_y, color="blue", label="IOPS", marker='o')
    plt.plot(x, increase_iops_y, color="blue")

    plt.legend()
    plt.savefig("/home/kuo/Desktop/python/ssd/imgs/different_iops.png")
    plt.show()    

def sensitive_different_wa(x, decrease_wa_y):
    plt.title("WA decrease percentage (%)")

    plt.scatter(x, decrease_wa_y, color="blue", label="WA", marker='o')
    plt.plot(x, decrease_wa_y, color="blue")

    plt.legend()
    plt.savefig("/home/kuo/Desktop/python/ssd/imgs/sensitive_different_wa.png")
    plt.show()

def sensitive_different_latency(x, decrease_latency_y):
    plt.title("Latency decrease percentage (%)")

    plt.scatter(x, decrease_latency_y, color="blue", label="Latency", marker='o')
    plt.plot(x, decrease_latency_y, color="blue")

    plt.legend()
    plt.savefig("/home/kuo/Desktop/python/ssd/imgs/sensitive_different_latency.png")
    plt.show()

def sensitive_different_iops(x, increase_iops_y):
    plt.title("IOPS increase percentage (%)")

    plt.scatter(x, increase_iops_y, color="blue", label="IOPS", marker='o')
    plt.plot(x, increase_iops_y, color="blue")

    plt.legend()
    plt.savefig("/home/kuo/Desktop/python/ssd/imgs/sensitive_different_iops.png")
    plt.show() 

x = [0, 20, 40, 60, 80, 99]
#x = [0, 25, 50, 75, 100]
index = 6

sublk_wa_y = [1.577562967936198, 1.5752042134602864, 1.5741246541341145, 1.5589930216471355, 1.5660781860351562, 1.5620689392089844] 
femu_wa_y = [16.02752176920573, 16.3805414835612, 14.64471689860026, 14.526606241861979, 14.838912963867188, 15.071908315022787]
#sublk_wa_y = [1.0852864583333333, 1.592516581217448, 1.9534085591634114, 2.3420079549153647, 2.7725613911946616]
#femu_wa_y = [1.1648483276367188, 16.02752176920573, 36.139878590901695, 58.21242904663086, 74.16619364420573]

decrease_wa_y = []
for i in range(0, index):
    n = (femu_wa_y[i] - sublk_wa_y[i]) / femu_wa_y[i] *100
    decrease_wa_y.append(n)


sublk_iops_y = [137.387270198, 136.134744406, 143.965237421, 128.548090522, 108.015367007, 114.750461208] 
femu_iops_y = [12.937690765554251, 12.491570024550027, 13.906878413295066, 13.953947107694383, 13.643489949375823, 13.465375778873579]
#sublk_iops_y = [1372.58162927, 113.621056988, 64.321110685, 45.4338463279, 30.0487270358]
#femu_iops_y = [204.54179878928187, 12.937690765554251, 5.901932547417065, 3.905855589055403, 3.3015357455468504]

increase_iops_y = []
for i in range(0, index):
    n = -1*(femu_iops_y[i] - sublk_iops_y[i]) / femu_iops_y[i] *100
    increase_iops_y.append(n)


sublk_latency_y = [5882.488230803, 5941.165156093, 5628.275370625, 6299.914660027, 7491.813641148, 7066.272252524] 
femu_latency_y = [69905.133488577, 72493.769655878, 63979.562742807, 63611.965356422, 65228.032072594, 66264.173733638]
#sublk_latency_y = [590.979059241, 7145.00482149, 12817.875674411, 18410.327709508, 28447.328200686]
#femu_latency_y = [3851.770174426, 69905.133488577, 180403.620584578, 317621.830022657, 418941.397761817]

decrease_latency_y = []
for i in range(0, index):
    n = (femu_latency_y[i] - sublk_latency_y[i]) / femu_latency_y[i] *100
    decrease_latency_y.append(n)

blk_cnt = [189, 196, 202, 204, 208, 218]
sublk_cnt = [27659, 26815, 28015, 28304, 27849, 28527]

#compare_wa(x, sublk_wa_y, femu_wa_y)
#compare_iops(x, sublk_iops_y, femu_iops_y)
#compare_latency(x, sublk_latency_y, femu_latency_y)
#decrease_percentage_picture(x, decrease_wa_y, decrease_iops_y, decrease_latency_y)

different_wa(x, decrease_wa_y)
different_iops(x, increase_iops_y)
different_latency(x, decrease_latency_y)

#sensitive_different_wa(x, decrease_wa_y)
#sensitive_different_iops(x, increase_iops_y)
#sensitive_different_latency(x, decrease_latency_y)
#blk_sublk_cnt(x, blk_cnt, sublk_cnt)