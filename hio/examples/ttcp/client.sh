# -l is the length of buffer
# -n is the number of network transfers
# data volume = length * num

#./ttcp-linux -t -l65536 -n16384 127.0.0.1 # 64K * 16K = 1G
#./ttcp-linux -t -l65536 -n128 127.0.0.1 # 64K * 16K = 1G
#./ttcp -t localhost # 64K * 16K = 1G
./ttcp-linux -t -n128 127.0.0.1 # 64K * 16K = 1G
