#!/bin/sh
PATH=$PATH:/home/thomas/build/apache-dev/bin
DIRECTORY=`pwd`/t
HTTPD=`which httpd`
HTTPD2=`which apache2`

cat test-Makefile-template | sed -s "s|\@\@HTTPD\@\@|$HTTPD|" | sed -s "s|\@\@HTTPD2\@\@|$HTTPD2|" > t/Makefile


if [ "$HTTPD" != "" ]; then
  echo "Found httpd as $HTTPD"
  echo "Creating test configuration for apache 1.3.x"
  echo "in directory $DIRECTORY"
  cat httpd-rpaf.conf-template | sed -s "s|\@\@DIR\@\@|$DIRECTORY|" > t/httpd-rpaf.conf
fi

APACHE2=`which apache2`

if [ "$APACHE2" != "" ]; then
  echo "Found apache2 as $HTTPD"
  echo "Creating test configuration for apache 2.x.x"
  echo "in directory $DIRECTORY"
  cat httpd-rpaf.conf-template-2.0 | sed -s "s|\@\@DIR\@\@|$DIRECTORY|" > t/httpd-rpaf.conf-2.0
fi


