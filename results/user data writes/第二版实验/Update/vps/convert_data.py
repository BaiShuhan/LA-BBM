#!/usr/bin/env python

import os
import sys

tracename = sys.argv[1]

print("tracename: %s"%tracename)


def convert_data(inputname, outputname):
    print("inputfile: %s, outputfile: %s"%(inputname, outputname))
    prev_val = 0
    is_first_line = True
    with open(outputname, "w") as out_:
        with open(inputname, "r") as f_:
            lines = f_.readlines()
            for t in lines:
                e = t.strip().split(",")
                tmpval = float(e[-1]) + float(e[-2])
                if is_first_line is True:
                    is_first_line = False
                    prev_val = tmpval
                    out_.write(e[0] + "," + str(prev_val) + "\n")
                else:
                    cur_val = tmpval
                    out_.write(e[0] + "," + str(cur_val - prev_val) + "\n")
                    prev_val = cur_val
            pass


ext_tra = "_debug_tradition_.csv"
ext_9 = "_debug_new_9.csv"

ext_convert_tra = "_convert_response_tradidition_.csv"
ext_convert_9 = "_convert_response_new_9_.csv"

file_ext_tra = sys.argv[1] + ext_tra
file_ext_9 = sys.argv[1] + ext_9

file_convert_ext_convert_tra = sys.argv[1] + ext_convert_tra
file_convert_ext_convert_9 = sys.argv[1] + ext_convert_9


convert_data(file_ext_tra, file_convert_ext_convert_tra)
convert_data(file_ext_9, file_convert_ext_convert_9)

print("done")


