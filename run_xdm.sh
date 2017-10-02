#! /bin/bash

exec xdm -nodaemon -server ":1 local /usr/bin/Xephyr :1 -screen ${1:-1024x768}" -xrm 'DisplayManager.pidFile: /var/lock/xdm-windowed.pid'
