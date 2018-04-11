from cba_reader import *

with open('test.cba', 'rb') as f:
  c = CBA(f)
  x = list(c.entries())
  print(x)
  print(vars(x[0]))
  print(vars(x[1]))