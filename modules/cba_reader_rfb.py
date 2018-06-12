import sys
import os
import argparse
import datetime

sys.path.append(os.path.dirname(__file__)+'/../')

from cba_reader import *

codes = [
         'ZERO',
         'CONN_FAIL',
         'BANNER_FAIL',
         'READ_FAIL', #but not honeypots
         'WRITE_FAIL',
         'REASON', 
         'WIN',
         'ONLY_TLS',
         'TLS_FAIL',
         'HAS_VENCRYPT',
         'HAS_TIGHT'
]

for idx, k in enumerate(codes):
  globals()[k] = idx
  
  
parser = argparse.ArgumentParser()
parser.add_argument('file')
parser.add_argument('what', nargs='?', default='WIN')
parser.add_argument('-g', action='store_true',  help='print countries')
parser.add_argument('-i', action='store_true', help='ip_mode')
args = parser.parse_args()


geo = False
if args.g:
  import GeoIP
  geo = GeoIP.new(GeoIP.GEOIP_MEMORY_CACHE)
  
what = args.what
if not args.i:
  what = codes.index(what)
    
with open(args.file, 'rb') as f:
  for entry in CBA(f).entries('rfb'):
    ip = entry.dict[0][6:]
    
    if args.i and ip == what:
      print(entry.events)
  
    if what in entry.dict:
      #print(entry.events)
      if not geo:
        print(ip)
      else:
        print(geo.country_code_by_addr(ip[:ip.index(':')]), ip)
        
    
        
   

