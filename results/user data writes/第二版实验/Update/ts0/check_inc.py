import sys

test_file = sys.argv[1]

print("test_file: %s"%test_file)

last_w_count = 0
last_r_count = 0
last_req_count = 0

cur_w_count = 0
cur_r_count = 0
last_req_count = 0

with open(test_file, "r") as f:
    multiline = f.readlines()
    for i in range(len(multiline)):
        lines = multiline[i].split(',')
        
        cur_req_count = float(lines[0])
        cur_w_count = float(lines[1])
        cur_r_count = float(lines[2])

        if last_r_count > cur_r_count:
            print("lineno: %d"%i)
            print("err-r")
        if last_w_count > cur_w_count:
            print("lineno: %d"%i)
            print(last_w_count, cur_w_count)
            print("err-w")
        if last_req_count > cur_req_count:
            print("lineno: %d"%i)
            print(last_req_count, cur_req_count)
            print("err-req")
        
        last_r_count = cur_r_count
        last_w_count = cur_w_count
        last_req_count = cur_req_count

print("done")