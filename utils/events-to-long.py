"""
Lick event data collected during lick sensing experiments are saved in
text (csv) files using a "short" format where one single number is used
to indicate which electrodes were active at any one time.

For example, the decimal number 33 indicates that a lick event was
detected by electrodes 0 and 5. This is because 33 in binary form is
0b0000_0010_0001, i.e. a 1 each in positions 5 and 0, indicating these
electrodes as active.

This script converts one such file into long format suitable for
analysis. For example, if the original csv shows the line

    timestamp,sensorA,sensorB
    0.2,3,1

which indicates that at timestamp 0.2 sensor A detected a lick in
electrodes 0 and 1, and sensor B detected a lick in electrode 1, then
this script will convert that line into

    timestamp,electrodeID
    0.2,A0
    0.2,A1
    0.2,B1

which can then be used for further analysis.
    
author: Antonio Gonzalez
last updated: 2026-01-06
"""
import os
import sys

NELE = 12  # Number of electrodes in each touch sensor

fname_in = sys.argv[1]

fname, ext = os.path.splitext(fname_in)
fname_out = f'{fname}-long{ext}'

if os.path.exists(fname_out):
    answ = input("Output file exists. Overwrite? [N/y] ")
    if answ != 'y':
        print('File will not be overwritten. Exiting.')
        sys.exit()

header = ''
counter = 0
with open(fname_in, 'r') as fin, open(fname_out, 'w') as fout:
    for line in fin:
        if line.startswith('#'):
            fout.write(line)
        elif header == '':
            header = line
            header = header.replace('sensor', '')
            header = header.strip().split(',')
            # fout.write('timestamp,sensorID,electrodeID\n')
            fout.write('timestamp,eleID\n')
            # First column is timestamp, rest are values from sensors
            ncols = len(header)
        else:
            vals = line.strip().split(',')
            timestamp = int(vals[0])
            # Iterate over each sensor's data: start from column 1 (column 0 is
            # timestamp)
            for col in range(1, ncols):
                val = int(vals[col])
                if val > 0:
                    for ele in range(NELE):
                        if (val >> ele) & 0x1:
                            fout.write(f"{timestamp},{header[col]}{ele}\n")
                    # print(line[0], line[1], ele)
        # if counter == 9:
            # break
        # counter += 1
print(f'Done: {fname_out}')
