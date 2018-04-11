DIR=$(basename `pwd`)
cd ..

cat $DIR/private.txt | python compile-targets.py $DIR/private
cat $DIR/reserved.txt $DIR/private.txt | python compile-targets.py -b $DIR/public-all
cat $DIR/reserved.txt $DIR/private.txt $DIR/assigned8.txt | python compile-targets.py -b $DIR/public