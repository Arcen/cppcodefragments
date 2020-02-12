#!/bin/sh

cat gzip | gzip | ./gzip -d > .tmp
diff .tmp gzip &> /dev/null
if [ $? != 0 ] ; then
 rm .tmp
 echo uncompress error
 return 1
fi

cat gzip | ./gzip | gunzip > .tmp
diff .tmp gzip &> /dev/null
if [ $? != 0 ] ; then
 rm .tmp
 echo compress error
 return 1
fi

rm .tmp
