import random
import sys
import getopt


rules = 0
chunk = 0
seed = 0

opts, args = getopt.getopt(sys.argv[1:], 'R:C:S:', ["as", "ff", "gg"])

for o, a in opts:
        if o == "-R":
            rules = a 
        if o == "-C":
            chunk = a 
        if o == "-S":
            seed = a 

if chunk < 1 or rules < 1:
    print "need rules and chunk"
    print sys.argv[0], "\n -R num_rules in HW\n -C chunk size rules to play with\n -S seed"
    sys.exit()

if int(seed):
    random.seed(int(seed))

idx_size = int(rules)
first_dn=int(chunk)/10
ops=int(chunk)

inserted_arr = range(0, idx_size)
deleted_arr = []
op_arr = [[0] * 2 for i in range(0, ops)]
j = 0

#start with first_dn deletes
j=0
for i in range(0,first_dn):
    op_arr[j][0] = 1
    op_arr[j][1] = inserted_arr.pop(random.randint(0, len(inserted_arr) -1))
    deleted_arr.append(op_arr[j][1])
    j = j + 1

for i in range(0, ops - first_dn):
    if (len(inserted_arr) == 0):
        rand_op = 0
    elif (len(deleted_arr) == 0):
        rand_op = 1
    else:
        rand_op = random.randint(0,1)

    op_arr[j][0] = rand_op
    if (rand_op == 1): #delete
            op_arr[j][1] = inserted_arr.pop(random.randint(0, len(inserted_arr) -1))
            deleted_arr.append(op_arr[j][1])
    else:
            op_arr[j][1] = deleted_arr.pop(random.randint(0, len(deleted_arr) -1))
            inserted_arr.append(op_arr[j][1])
    j = j + 1

            
sz = len(deleted_arr)
for i in range(0, sz):
    op_arr.append([ 0, deleted_arr.pop(random.randint(0, len(deleted_arr) -1))])
    j = j + 1




for i in range(0, j):
    print "%s, %s" % (op_arr[i][0], op_arr[i][1])

