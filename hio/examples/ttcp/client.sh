# -l is the length of buffer
# -n is the number of network transfers
# data volume = length * num

./ttcp -t -l65536 -n16384 localhost # 64K * 16K = 1G
#./ttcp -t localhost # 64K * 16K = 1G
