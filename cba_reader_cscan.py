import sys
import datetime

from cba_reader import *

STATS_EVERY = 10
STAT_TIME = 11
EVENT = 20
CANARY_FAIL = 21
CANARY_FAIL_RESPONSE = 22

if len(sys.argv) < 2:
  exit('python %s [file] [-s] [-r]')
  
file_name = sys.argv[1]
print_stats = '-s' in sys.argv or len(sys.argv) == 2
print_results = '-r' in sys.argv or len(sys.argv) == 2

def stat_to_string(stat):
  return '%s: %d new events, %d new results' % stat

def handle_cscan_entry(entry):
  if CANARY_FAIL in entry.dict:
    print('canary fail - timeout')
  elif CANARY_FAIL_RESPONSE in entry.dict:
    print('canary fail - invalid response')
   
  stats_every = 0
  idx, nevents = 0, len(entry.events)
  oldest, newest = None, None
  min_events, min_stat = 0, None
  max_events, max_stat = 0, None
    
  while idx < nevents:
    _, code, val = entry.events[idx]
       
    if code == EVENT:
      print('Event:', val)
      
    elif code == STATS_EVERY:
      stats_every = val
 
    elif code == STAT_TIME:
      ts = datetime.datetime.fromtimestamp(entry.events[idx][2]).strftime('%Y-%m-%d %H:%M:%S')
      event_count = entry.events[idx+1][2]
      result_count = entry.events[idx+2][2]
      
      stat = (ts, event_count, result_count)
      
      print(stat_to_string(stat))
        
      if not oldest:
        oldest = stat
        
      newest = stat
          
      if not min_stat or event_count < min_events:
        min_stat = stat
        min_events = event_count

      if event_count > max_events:
        max_stat = stat
        max_events = event_count
        
      idx += 2
        
    idx += 1
    
    
  #print('Oldest:', stat_to_string(oldest))
  #print('Newest:', stat_to_string(newest))
  print('Min events:', stat_to_string(min_stat))
  print('Max events:', stat_to_string(max_stat))
  
      
with open(sys.argv[1], 'rb') as f:
  cf = CBA(f)
  
  if print_stats:
    print('Stats:')
    for entry in cf.entries('cscan'):
        handle_cscan_entry(entry)
  
  if print_stats and print_results:
     print('')

  if print_results:
    print('Results:', file=sys.stderr)
    for entry in cf.entries(''):
    	print(entry.dict[0])

