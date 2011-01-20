#!/usr/bin/perl
# $Id: env.cgi 13 2007-11-26 00:19:53Z thomas $
print qq{Content-Type: text/plain\r\n\r\n};
print qq{HTTP_X_FORWARDED_FOR = } . (defined($ENV{HTTP_X_FORWARDED_FOR}) ? $ENV{HTTP_X_FORWARDED_FOR} : '');
print qq{\n};
print qq{REMOTE_ADDR = } . (defined($ENV{REMOTE_ADDR}) ? $ENV{REMOTE_ADDR} : '');
print qq{\n};
print qq{HTTP_HOST = } . (defined($ENV{HTTP_HOST}) ? $ENV{HTTP_HOST} : '');
print qq{\n};
print qq{HTTP_X_REAL_IP = } . (defined($ENV{HTTP_X_REAL_IP}) ? $ENV{HTTP_X_REAL_IP} : '');
print qq{\n};
