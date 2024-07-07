import numpy as np

def count_lines(file_path):
    line_counts = {}

    file = open(file_path, 'r')
    for line in file:
        line = line.strip()
        if line in line_counts:
            line_counts[line] += 1
        else:
            line_counts[line] = 1
    
    return line_counts

def main():
    file_path = "/home/user/Desktop/femu/build-femu/write_leveling_record.txt"
    line_counts = count_lines(file_path)
    counts = list(line_counts.values())

    std_dev = np.std(counts, ddof=1)

    print("std = ", std_dev)

if __name__ == "__main__":
    main()