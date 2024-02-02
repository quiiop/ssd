lba_range = 2000000
std_percentage = 50 / 100

avg = lba_range / 2
std = avg * std_percentage
size = 1000 #1000MB
'''
section1 : lba range = [0 ~ (avg-2*std)] 2%
section2 : lba range = [(avg-2*std) ~ (avg-1*std)] 14%
section3 : lba range = [(avg-1*std) ~ (avg)] 34%
section4 : lba range = [(avg) ~ (avg+1*std)] 34%
section5 : lba range = [(avg+1*std) ~ (avg+2*std)] 14%
section6 : lba range = [(avg+2*std) ~ (lba_range)] 2%
'''


section1_mb = ((avg - (2*std)) / lba_range ) * size 
section2_mb = ((avg - (1*std)) / lba_range ) * size 
section3_mb = ((avg - (0*std)) / lba_range ) * size 
section4_mb = ((avg + (1*std)) / lba_range ) * size 
section5_mb = ((avg + (2*std)) / lba_range ) * size 

print("avg ",avg)
print("std ",std)
print("2%: 0MB ~ ", section1_mb, " MB")
print("14% ", section1_mb, " MB ~ ", section2_mb, " MB")
print("34% ", section2_mb, " MB ~ ", section3_mb, " MB")
print("34% ", section3_mb, " MB ~ ", section4_mb, " MB")
print("14% ", section4_mb, " MB ~ ", section5_mb, " MB")
print("2% ", section5_mb, " MB ~ ", size, " MB")
