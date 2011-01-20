#!/bin/sh

~/build/apache-dev/bin/httpd -d . -f `pwd`/httpd-rpaf.conf -X 
