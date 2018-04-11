* shouldn't autointerval be 100% ?
* make module code less horribly repetitive => easier to add remove/modules
* ability to use rotated .cba files?
  1 file per X events, like file%d.cba => file1.cba file2cba etc
  file is locked @ event level, so we could only quarantee that the concat of those files work
  swapped file may contain events without module def - we can ignore that @ the reader level
* many more...