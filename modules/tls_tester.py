import socket
import struct
import time
import ssl

def recv(s, l):
  buf = b''
  torecv = 0
  
  while True:
    torecv = l-len(buf)
    
    if not torecv:
      return buf
      
    buf += s.recv(torecv)


srv = socket.socket()
srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

srv.bind(('127.0.0.1', 7546))
srv.listen(1)

while True:
  cs, _ = srv.accept()
  print('got client')

  cs.sendall(b'12345')
  time.sleep(0.4)
  cs.sendall(b'6789\x00')

  l = struct.unpack('I', recv(cs, 4))[0]
  msg = recv(cs, l)

  print('got msg', msg)

  cs.sendall(b'ok')

  # client starts handshake
  cs = ssl.wrap_socket(cs, server_side=True, certfile='./server.pem')

  l = struct.unpack('I', recv(cs, 4))[0]
  msg = recv(cs, l)

  print('got tls msg:', msg)

  cs.sendall(b'\x04\x00\x00\x00')
  cs.sendall(b'ye')
  time.sleep(0.5)
  cs.sendall(b'ah')

  cs.close()
