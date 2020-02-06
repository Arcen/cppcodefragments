#!/bin/sh

cat compress | compress | ./compress -d > .tmp
diff .tmp compress &> /dev/null
if [ $? != 0 ] ; then
 rm .tmp
 echo uncompress error
 return 1
fi


cat compress | ./compress | uncompress > .tmp
diff .tmp compress &> /dev/null
if [ $? != 0 ] ; then
 rm .tmp
 echo compress error
 return 1
fi

rm .tmp
