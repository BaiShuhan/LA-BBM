from mpmath import mp 
from functools import reduce
import datetime
import operator  

# dps (short for decimal places) is the decimal precision 
mp.dps = 200


def c(n, k):
    if k == 0:
        return 1
    else:
        return  mp.mpf(reduce(operator.mul, range(n - k + 1, n + 1))) / \
                mp.mpf(reduce(operator.mul, range(1, k + 1)))


def count_ecc_uber(n, k, rber):
    uber = mp.mpf('0')
    for i in range(k + 1, n + 1):
        uber += c(n, i) * (mp.mpf(rber) ** i) * ((1 - mp.mpf(rber)) ** (n - i))
    return uber


def count_ecc_uber_r(n, k, rber):
    ber = mp.mpf('0')
    uber = mp.mpf('0')
    for i in range(0, k + 1):
        ber += c(n, i) * (mp.mpf(rber) ** i) * ((1 - mp.mpf(rber)) ** (n - i))
    
    uber = 1 - ber
    return uber


def count_raid_uber(stripe, uber_ecc):
    p0 = mp.mpf(1 - uber_ecc) ** (stripe - 1)
    p1 = (stripe - 1) * mp.mpf(uber_ecc) * (mp.mpf(1 - uber_ecc) ** (stripe - 1))
    
    return mp.mpf(1 - p0 - p1)


def rber_count(n, k, rbers, stripes):
    uber_ecc_r = []
    for rber in rbers:
        print(rber)
    print()
    for rber in rbers:
        # uber_ecc.append(count_ecc_uber(n, k, rber))
        uber_ecc_r.append(count_ecc_uber_r(n, k, rber))
        print(uber_ecc_r[len(uber_ecc_r) - 1])
    print()
    
    for size in stripes:
        print(size)
        for uber in uber_ecc_r:
            print(count_raid_uber(size, uber))
        print()
    
    return


def main():
    rbers = [mp.mpf('0.001') * i for i in range(1, 11)]
    stripes = [2 ** i for i in range(1, 8)]
    rber_count(8192, 120, rbers, stripes)
    

def test():
    rbers = ['0.001', '0.005', '0.01']
    n = 8192
    for rber in rbers:
        print(rber)
        ber = mp.mpf(0)
        for i in range(0, n + 1):
            ber += c(n, i) * (mp.mpf(rber) ** i) * ((1 - mp.mpf(rber)) ** (n - i))
        print(ber)
        print()


def example():
    n, k = 1024*8, 120
    rber = '5e-3'
    stripe = 128

    uber_ecc_r = count_ecc_uber(n, k, rber)
     
    p0 = mp.mpf((1 - uber_ecc_r) ** (stripe - 1))
    p1 = (stripe - 1) * mp.mpf(uber_ecc_r) * (mp.mpf(1 - uber_ecc_r) ** (stripe - 1))
    uber_raid = mp.mpf(1 - p0 - p1)
    
    print("data length, max bit errors corrected by ECC, RBER, Stripe size")
    print("%d \t %d \t %s \t %d" % (n, k, rber, stripe))
    print("UBER with ECC:\n\t", uber_ecc_r)
    print("stripe size: %d" % stripe)
    print("\tZero error:\n\t\t", p0)
    print("\tOne error:\n\t\t", p1)
    print("\tUBER after RAID:\n\t\t", uber_raid)
    
    return


print("begin_r")
    
starttime = datetime.datetime.now()
print(starttime)

main()
# example()
# test()

endtime = datetime.datetime.now()
print()
print("%.2f min" % ((endtime - starttime).seconds / 60))
print()
print("end_r")
