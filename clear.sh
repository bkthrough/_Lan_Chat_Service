#!/bin/bash
rm -f /var/lib/mysql/mysql.sock
killpro ser 9
/etc/init.d/mysqld restart
rm -rf ~/.data/swap/*
