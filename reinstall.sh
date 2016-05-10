#!/bin/bash

make && sudo make install && sudo service apache2 restart && banner "G O  !"
tail -f /var/log/apache2/error.log

