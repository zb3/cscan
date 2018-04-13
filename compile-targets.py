import struct
import sys
import argparse

sys.path.append('target-utils')
from util import *

#include file read from stdin

parser = argparse.ArgumentParser(description='Compile target ranges from stdin and ports from portspec to binary .csw and .csp files for cscan')
parser.add_argument('output', nargs='?', default='builtin', help='destination file name without extensions')
parser.add_argument('-b', action='store_true',  help='treat input as a blacklist - adds inverted ranges')
parser.add_argument('-c', action='store_true', help='also generate the C file for builtin support')
parser.add_argument('-p', default=None,  help='portspec string')

args = parser.parse_args()

output_name = args.output
c_output = args.c
portspec = args.p
invert = args.b

ports_specified = bool(portspec)
portspec = portspec.split(',') if portspec else ['80']

port_weights = []

for entry in portspec:
  if ':' in entry:
    port_weights.append(map(int, entry.split(':')))
  else:
    port_weights.append((int(entry), 1))


ranges = ranges_from_file(sys.stdin)
if invert:
  ranges = invert_ranges(ranges)

def get_binmask(num):
  mask = 1
  
  while mask < num:
    mask <<= 1
 
  return mask-1


##
## ip config
##

ranges = merge_ranges(ranges)
num_ranges = len(ranges)
total = 0
range_data = []

for s, e in ranges:
  total += e - s + 1
  range_data.append(e)
  range_data.append(total-1)
  
ranges_mask = get_binmask(total)


##
## port config
##

num_ports = len(port_weights)
pt_sum = pt_mask = 0
port_data = []

for port, pt in port_weights:
  pt_sum += pt
  port_data.append(port)
  port_data.append(pt_sum-1)
 
pt_mask = get_binmask(pt_sum)


if c_output:
  with open('hardcoded.c', 'w') as f:
    f.write('#include "target.h"\n\n')

    f.write('struct range_cfg builtin_ranges = {\n')
    f.write('  %d, %dU, %dU,\n' % (num_ranges, total-1, ranges_mask))
    f.write('  {'+','.join(map(lambda x: str(x)+'U', range_data))+'}\n')
    f.write('};\n')
    f.write('\n')

    f.write('struct range_cfg builtin_ports = {\n')
    f.write('  %d, %dU, %dU,\n' % (num_ports, pt_sum-1, pt_mask))
    f.write('  {'+','.join(map(str, port_data))+'}\n')
    f.write('};')
 

with open(output_name+'.csw', 'wb') as f:
  f.write(struct.pack('<III', num_ranges, total-1, ranges_mask))
  f.write(struct.pack('<%dI' % len(range_data), *range_data))

if ports_specified:
  with open(output_name+'.csp', 'wb') as f:
    f.write(struct.pack('<III', num_ports, pt_sum-1, pt_mask))
    f.write(struct.pack('<%dI' % len(port_data), *port_data))
  
  