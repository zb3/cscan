DIR=presets
PYTHON=${PYTHON-python}

if [ ! -f cscan.c ]; then
  cd ..
fi

cat $DIR/private.txt | $PYTHON compile-targets.py $DIR/private
cat $DIR/reserved.txt $DIR/private.txt | $PYTHON compile-targets.py -b $DIR/public-all
cat $DIR/reserved.txt $DIR/private.txt $DIR/assigned8.txt | $PYTHON compile-targets.py -b $DIR/public

# commands below assume GeoLite2 ASN database in csv format is in db.csv

if [ -f $DIR/db.csv ]; then
  echo running
  cd target-utils
  
  $PYTHON ignore-networks.py ../$DIR/db.csv ../$DIR/default.txt exclude.conf
  $PYTHON ignore-networks.py -c config_hostcloud.py ../$DIR/db.csv ../$DIR/default-with-hostcloud.txt exclude.conf
  
  $PYTHON include-networks.py ../$DIR/db.csv ../$DIR/cloud.txt
  $PYTHON include-networks.py -c config_hostcloud.py ../$DIR/db.csv ../$DIR/hostcloud.txt
  
  cd ..
  
  cat $DIR/default.txt | $PYTHON compile-targets.py -b -c $DIR/default
  
  for p in default-with-hostcloud; do
   cat $DIR/$p.txt | $PYTHON compile-targets.py -b $DIR/$p
  done
  
  for p in cloud hostcloud; do
   cat $DIR/$p.txt | $PYTHON compile-targets.py $DIR/$p
  done
fi
