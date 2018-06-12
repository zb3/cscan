import struct
import os
import io

CBA_HEAD = 0
CBA_STR = 1
CBA_FILE = 2
CBA_DATA = 3
CBA_INT32 = 4
CBA_INT64 = 5
CBA_BIT = 6

class ThreadState():
  def __init__(self):
    self.reset()
    
  def reset(self):
    self.module = ''
    self.dict = {}
    self.events = []
    self.cur_code = 0
    self.cur_type = 0
    self.cur_stream = None
    
  def add_event(self, kind, code, val):
    self.events.append((kind, code, val))
    self.dict[code] = val
    
    
class Entry():
  def __init__(self, ts):
    self.module = ts.module
    self.dict = ts.dict
    self.events = ts.events

        

class CBA():
  def __init__(self, f, extract_dir='cba_files', extract_for=[], overwrite=False):
    self.state = {}
    self.file = f
    self.order = '<' if self.file.read(1) == b'L' else '>'
    self.extract_dir = extract_dir
    self.extract_for = extract_for
    self.overwrite = overwrite
    self.eof = False
    self.filter = None
    
  def reset(self):
    self.eof = False
    self.file.seek(0)
    
  def unpack(self, fmt, *args):
    return struct.unpack(self.order+fmt, *args)
        
  def entries(self, module=None):
    self.filter = module
    if self.eof:
      self.reset()
    
    while not self.eof:
      yield from self.read_packet()
      
    yield from self.read_final()
    
    
  def result(self, state):
    if len(state.events) and (self.filter is None or state.module == self.filter):
      yield Entry(state)
      
  def read_final(self):
    for tid in self.state:
      yield from self.result(self.state[tid])
    
  def read_packet(self):
    tbyte = self.file.read(1)
    if tbyte == b'':
      self.eof = True
      return
    
    tid = self.unpack('B', tbyte)[0]
    if tid not in self.state:
      self.state[tid] = ThreadState()
      
    state = self.state[tid]
      
    if state.cur_stream is not None:
      l = self.unpack('H', self.file.read(2))[0]
      
      if l:
        buff = self.file.read(l)
        
        if state.cur_stream != 'ignore':
          state.cur_stream.write(buff)
        
      else:
        if state.cur_type == CBA_DATA:
          val = state.cur_stream.getvalue()
          state.add_event(state.cur_type, state.cur_code, val)
        
        if state.cur_stream != 'ignore': 
          state.cur_stream.close()
          
        state.cur_stream = None
        
    else:
      mtype = self.unpack('B', self.file.read(1))[0]
      code = 0
    
      if mtype & 0x80:
        mtype ^= 0x80
        code = self.unpack('I', self.file.read(4))[0]
       
      if mtype != CBA_HEAD:
        state.cur_code = code
        state.cur_type = mtype
  
      if mtype == CBA_HEAD:
        yield from self.result(state)
        state.reset()
         
        l = self.unpack('B', self.file.read(1))[0]
        if l:
          state.module = self.file.read(l).decode('ascii')
          
     
      elif mtype == CBA_STR:
        l = self.unpack('H', self.file.read(2))[0]
        val = self.file.read(l).decode('iso-8859-1')
        state.add_event(mtype, code, val)
        
      elif mtype == CBA_DATA:
        state.cur_stream = io.BytesIO()
        
      elif mtype == CBA_BIT:
        state.add_event(mtype, code, True)
        
      elif mtype == CBA_INT32:
        val = self.unpack('i', self.file.read(4))[0]
        state.add_event(mtype, code, val)
        
      elif mtype == CBA_INT64:
        val = self.unpack('q', self.file.read(8))[0]
        state.add_event(mtype, code, val)
     
      elif mtype == CBA_FILE:       
        l = self.unpack('H', self.file.read(2))[0]
        filename = ''
       
        state.cur_stream = 'ignore'
        should_extract = '*' in self.extract_for or state.module in self.extract_for
       
        if should_extract and l:
          filename = self.file.read(l).decode('ascii')
         
          #safety...
          filename = filename.replace('..', '')
          if filename.startswith('/'):
            filename = filename[1:]
       
        if filename:       
          try:
            #should we mkdir?
            mode = 'wb' if self.overwrite else 'xb'
            state.cur_stream = open(os.path.join(self.extract_dir, filename), mode)
         
          except Exception as e:
            print('ignoring file', filename, e)
            pass

          state.add_event(CBA_FILE, code, filename)
           

        
