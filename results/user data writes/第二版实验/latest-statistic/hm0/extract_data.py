import os
import sys

tracename = sys.argv[1]

print("tracename: %s"%tracename)

a = {}
a[0] = "tradition"
a[1] = "0.1"
a[2] = "0.2"
a[3] = "0.3"
a[4] = "0.4"
a[5] = "0.5"
a[6] = "0.6"
a[7] = "0.7"
a[8] = "0.8"
a[9] = "0.9"

def get_data(s):
    return s.split(":")[1]

with open("test_data.csv", "w") as f_out:
    

    filenameset = []
    
    filename_tra=tracename + "_statistic_tradition_.dat"
    
    filenameset.append((0, filename_tra))

    for i in range(1, 10, 2):
        filename_new = tracename + "_statistic_new_" + str(i) + ".dat"
        filenameset.append((i, filename_new))

        file_debug = tracename + "_debug_new_" + str(i) + ".csv"
        print(file_debug)
        os.system("python check_inc.py " + file_debug)


    f_out.write(tracename + "\n")

    for i in range(len(filenameset)):
        print("extracting %s"%filenameset[i][1])
        
        with open(filenameset[i][1], "r") as f:
            lines = f.readlines()
            if (len(lines) < 135):
                print("Trace not complete")
                continue

            # r_req_count = float(get_data(lines[131-1]))
            w_req_count = get_data(lines[132-1]).strip()
            # r_req_avr   = float(get_data(lines[134-1]))
            w_req_avr   = get_data(lines[135-1]).strip()

            r_response_time = get_data(lines[137-1]).strip()
            w_response_time = get_data(lines[138-1]).strip()

            f_out.write(a[filenameset[i][0]] + "," + w_req_count + "," + w_req_avr + "," + r_response_time + "," + w_response_time + "\n")
