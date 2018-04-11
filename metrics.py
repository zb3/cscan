import struct
import sys
import io

"""
todo:
-don't reimplement numpy
-log y scale for evts? density is more important than count
  
"""

marker_every = 1000
ct = 400


tsp_timesize2 = 50
tsp_timesize3 = 500

tsp_samples = 200
evt_samples = 120
hist_bins = 80

mtlog = []
packetfd = []

class TVec():
  def __init__(self, dimensions):
    self.dimensions = dimensions
    self.list = [[] for x in range(dimensions)]
  
  def add(self, *args):
    for idx, arg in enumerate(args):
      self.list[idx].append(arg)
      
  def sort(self, dim):
    #this is idiotic in python, coz "key" for .sort() doesn't give us indices.
    
    indices = list(range(len(self.list[0])))
    indices.sort(key = self.list[dim].__getitem__)

    for x in range(self.dimensions):
      self.list[x] = [self.list[x][y] for y in indices]
     
    
  

epoll = TVec(3)
deltainfo = TVec(2)
sendinfo = TVec(3)
delinfo = TVec(3) 
scoreinfo = TVec(3) 

tpackets = {}

first_packet = last_packet = 0


class TimeBucket():
  def __init__(self, dimensions):
    self.dimensions = dimensions
    self.sorted = True
    self.biggest_idx = -1
    self.indices = []
    self.values = {}
    
  def ensure_key(self, key):
    if key not in self.values:
      self.indices.append(key)
      self.values[key] = [0]*self.dimensions
      
      if key < self.biggest_idx:
        self.sorted = False
      else:
        self.biggest_idx = key
        
        
  def sort(self):
    if not self.sorted:
      self.indices.sort()
      
      if len(self.indices):
        self.biggest_idx = self.indices[-1]
        
      self.sorted = True

              
  def add_event(self, key, evt):
    self.ensure_key(key)
    self.values[key][evt] += 1
    
  def get_events(self, axis):
    ret = []
    
    for idx in self.indices:
      ret.append(self.values[idx][axis])
      
    return self.indices, ret
    
  def sample(self, axis, size):
    indices, values = [], []
    last_key = -1
    
    for idx in self.indices:
      nkey = (idx // size)*size
      if nkey > last_key:
        indices.append(nkey)
        values.append(0)
        
        last_key = nkey
     
      values[-1] += self.values[idx][axis]
        
    return indices, values
    
  
    


      
tbuckets = TimeBucket(5)
ebuckets = TimeBucket(5)
esbuckets = TimeBucket(5)


num_packets = 0

marker_times = []

other_sofar, score_sofar = 0, 0

with open(sys.argv[1], 'rb') as fo:
  f = io.BytesIO(fo.read())
  
  ct = struct.unpack('<i', f.read(4))[0]
  print('ct is', ct)
  
  while True:
    mt = f.read(1)
    
    if not mt:
      break
  
    try:
      mt = mt[0]
        
      if mt < 0xe0 or mt > 0xe5:
        print('w00t', hex(mt), f.tell())
        exit(1)
      
   
      time = struct.unpack('<Q', f.read(8))[0]
      if not first_packet:
        first_packet = time
      
      time -= first_packet
    
      if time > last_packet:
        last_packet = time
      
      mtlog.append((mt, time))    
    
      if mt == 0xe0:
        fd = struct.unpack('<i', f.read(4))[0]
        tbuckets.add_event(time, 0)
        packetfd.append(fd)
        
        if not len(packetfd) % marker_every:
          marker_times.append(time)
    
      elif mt == 0xe1: 
        returned, timeout = struct.unpack('<ii', f.read(8))
        epoll.add(time, returned, timeout)

      elif mt == 0xe2:
        delta, kind = struct.unpack('<ib', f.read(5))
        ebuckets.add_event(time, kind)
        ebuckets.add_event(time, 4)
        
        esbuckets.add_event(time-delta, kind)
        esbuckets.add_event(time-delta, 4)
        
        #since separate 0xe3 records are not written
        deltainfo.add(time-delta, delta)
        
        if kind >= 2:
          score_sofar += 1
        else:
          other_sofar += 1
        
        scoreinfo.add(time, score_sofar, other_sofar)
        
   
      elif mt == 0xe3:
        delta = struct.unpack('<i', f.read(4))[0]
        deltainfo.add(time-delta, delta)
        if delta < ct:
          print('err wat?')
    
      elif mt == 0xe4:
        delta, sent = struct.unpack('<ii', f.read(8))
        sendinfo.add(time-delta, delta, sent)
        
      elif mt == 0xe5:
        delta, deleted = struct.unpack('<ii', f.read(8))
        delinfo.add(time-delta, delta,  deleted)
    
    except struct.error:
      break
    
typecnt = {}

for mt, time in mtlog:
  typecnt[mt] = typecnt.get(mt, 0)+1
  
print('all', typecnt)    

typecnt = {}

for mt, time in mtlog:
  if time < last_packet-ct:
    typecnt[mt] = typecnt.get(mt, 0)+1
  
print('cut', typecnt)    

evt_timesize = last_packet//evt_samples
tsp_timesize = last_packet//tsp_samples

for tv in (deltainfo, epoll, sendinfo, delinfo, scoreinfo):
  tv.sort(0)
  
  
for tb in (tbuckets, ebuckets, esbuckets):
  tb.sort()



import matplotlib.pyplot as plt



rplot = plt.figure()
p = rplot.add_subplot(211)

p.set_title('sum of events received before')

p.stackplot(*scoreinfo.list)
p.legend(['open', 'other'], loc=2)
p.set_xlabel('time')
p.set_ylabel('event sum')

##send-time score info
#starthere. does this make sense?
#allso all_sofar may void need to count ebuckets

st_scoreinfo = TVec(3) #no need to sort it as we use sorted TB. TB must be sorted for this to make sense
other_st_sofar, score_st_sofar = 0, 0

for idx in esbuckets.indices:
  other_st_sofar += sum(esbuckets.values[idx][0:1])
  score_st_sofar += sum(esbuckets.values[idx][2:3])
  
  st_scoreinfo.add(idx, score_st_sofar, other_st_sofar)
  
p = rplot.add_subplot(212)
p.set_title('sum of events by sockets sent before')
p.stackplot(*st_scoreinfo.list)
p.legend(['open', 'other'], loc=2)
p.set_xlabel('socket send time')

rplot.tight_layout()
rplot.show()



fdplot = plt.figure()
p = fdplot.add_subplot(111)
p.scatter(range(len(packetfd)), packetfd)
p.set_xlabel('packet index')
p.set_ylabel('assigned fd')
fdplot.show()

def add_pps_ax(ax, size):
  prx = ax.twinx()
  prx.set_ylabel('PPS')
  prx.tick_params('y')
  p1lim = ax.get_ylim()
  prx.set_ylim(p1lim[0]*1000/size, p1lim[1]*1000/size)

tsp = plt.figure()

p1 = tsp.add_subplot(211)
p1.set_title('Packets sent per %d ms'%tsp_timesize)
p1.bar(*tbuckets.sample(0, tsp_timesize), width=tsp_timesize, align='edge') #, edgecolor='C0')
p1.set_xlabel('time')
p1.set_ylabel('packets')

add_pps_ax(p1, tsp_timesize)

#p2 is misleading, when we see the gap, we don't know how much packets have been sent that's why we place it next to p1

p2 = tsp.add_subplot(212, sharex=p1)
p2.set_xlabel('send time')
p2.set_ylabel('TTR')

for mt in marker_times:
  p2.axvline(x=mt, linewidth=1, color='#000000', alpha=0.4)
  

p2.plot(*deltainfo.list)

tsp.tight_layout()
tsp.show()


tsp2 = plt.figure()

p1 = tsp2.add_subplot(211)
p1.set_title('Packets sent per %d ms'%tsp_timesize2)
p1.set_xlabel('time')
p1.set_ylabel('packets')
p1.bar(*tbuckets.sample(0, tsp_timesize2), width=tsp_timesize2, align='edge') #, edgecolor='C0')
add_pps_ax(p1, tsp_timesize2)

p2 = tsp2.add_subplot(212, sharex=p1)
p2.set_title('Packets sent per %d ms'%tsp_timesize3)
p2.set_xlabel('time')
p2.set_ylabel('packets')
p2.bar(*tbuckets.sample(0, tsp_timesize3), width=tsp_timesize3, align='edge')
add_pps_ax(p2, tsp_timesize3)

tsp2.tight_layout()
tsp2.show()

#
#receive time
#
evt = plt.figure()

evt.suptitle('Events received per %d ms'%evt_timesize)

p1 = evt.add_subplot(211)
p1.set_xlabel('time')

for t in range(4):
  p1.bar(*ebuckets.sample(t, evt_timesize), width=evt_timesize, alpha=0.8, align='edge')
p1.legend(['fail', 'fail_imm', 'win', 'win_imm'])

p2 = evt.add_subplot(212, sharex=p1)
p2.set_xlabel('time')
p2.bar(*ebuckets.sample(4, evt_timesize), width=evt_timesize, align='edge')
p2.legend(['all'])

for mt in marker_times:
  p1.axvline(x=mt, linewidth=1, color='#000000', alpha=0.4)
  p2.axvline(x=mt, linewidth=1, color='#000000', alpha=0.4)
  
for idx in range(len(sendinfo.list[0])):
  if sendinfo.list[1][idx] > 5:
    p2.axvspan(sendinfo.list[0][idx], sendinfo.list[0][idx]+sendinfo.list[1][idx], color='#FFA500', alpha=0.4)
  
evt.tight_layout()
evt.show()


#
#send time
#

stevt = plt.figure()

stevt.suptitle('Events by sockets per %d ms'%evt_timesize)

p1 = stevt.add_subplot(211)
p2.set_xlabel('send time')
for t in range(4):
  p1.bar(*esbuckets.sample(t, evt_timesize), width=evt_timesize, alpha=0.8, align='edge')
p1.legend(['fail', 'fail_imm', 'win', 'win_imm'])

p2 = stevt.add_subplot(212, sharex=p1)
p2.set_xlabel('send time')
p2.bar(*esbuckets.sample(4, evt_timesize), width=evt_timesize, align='edge')
p2.legend(['events'])

for mt in marker_times:
  p1.axvline(x=mt, linewidth=1, color='#000000', alpha=0.4)
  p2.axvline(x=mt, linewidth=1, color='#000000', alpha=0.4)
  
stevt.tight_layout()
stevt.show()

#

rtimes = []
for rt in deltainfo.list[1]:
  if rt < ct:
    rtimes.append(rt)
    
r_ratio = len(rtimes)/len(deltainfo.list[1])


dplot = plt.figure()

p1= dplot.add_subplot(211)
p1.set_title('reply time histogram')
p1.hist(rtimes, hist_bins)

p2 = dplot.add_subplot(212)
p2.set_title('reply time cumulative histogram')
p2.hist(rtimes, hist_bins, cumulative=True, normed=True)

dplot.tight_layout()

dplot.show()


pplot = plt.figure()
p1 = pplot.add_subplot(121)
p1.axis("equal")
p1.pie([r_ratio, 1-r_ratio], labels=['replied', 'timed out'], autopct='%.2f%%', labeldistance=1.1)
  
w_ratio = score_sofar/(score_sofar+other_sofar)
  

p2 = pplot.add_subplot(122)
p2.axis("equal")
p2.pie([w_ratio, 1-w_ratio], labels=['open', 'closed/fail'], autopct='%.2f%%', labeldistance=1.1)



pplot.show()

splot = plt.figure()
p1 = splot.add_subplot(211)
p1.plot(sendinfo.list[0], sendinfo.list[1])
p1.plot(sendinfo.list[0], sendinfo.list[2])
p1.legend(['time spent', 'sent packets'])

p2 = splot.add_subplot(212, sharex=p1)
p2.plot(delinfo.list[0], delinfo.list[1], alpha=0.6)
p2.plot(delinfo.list[0], delinfo.list[2], alpha=0.6)
p2.legend(['time spent', 'deleted'])


splot.show()

"""
not effective, speedcoded graph showing % of events...

xplot = plt.figure()
p1 = xplot.add_subplot(111)

allsent = tbuckets.sample(0, evt_timesize)
evtgot = esbuckets.sample(4, evt_timesize)
evtmap = {}
for x in range(len(evtgot[0])):
  idx, val = evtgot[0][x], evtgot[1][x]
  evtmap[idx] = val

nidx, nval = [], []

for x in range(len(allsent[0])):
  idx, val = allsent[0][x], allsent[1][x]
  
  if idx in evtmap:
    nidx.append(idx)
    nval.append(evtmap[idx]/val)

p1.plot(nidx, nval)
xplot.show()
"""

plt.show()
#import code
#code.interact(local=locals())
