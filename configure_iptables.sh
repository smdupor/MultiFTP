#!/bin/sh
sudo iptables -I INPUT -p udp --dport 7735 -m conntrack --ctstate NEW,ESTABLISHED -j ACCEPT
sudo iptables -I OUTPUT -p udp --dport 7735 -m conntrack --ctstate NEW,ESTABLISHED -j ACCEPT
