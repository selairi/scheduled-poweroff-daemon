scheduled-poweroff-daemon
=========================

This tool lets to schedule the shutdown time of your computer. Users will see a dialog ten minutes before the power off. If they close the dialog, the power off will be disabled until the next scheduled time.

## Install

The following dependencies are required:

- systemd
- libsystemd
- g++
- make
- cmake
- kdialog

Then run in terminal:
```
	mkdir build
	cd build
	cmake ..
	make
	sudo make install
```

The service can be enabled running:
```
  sudo systemctl enable scheduled-poweroff
  sudo systemctl start scheduled-poweroff
```

## Daily power-off

Edit the /etc/scheduled-poweroff.conf file and set the hours to shutdown:
```
# Power off in 24 hours format. Ex:
14:15
15:10
21:20
23:30
```

You have to restart the service:
```
  sudo systemctl stop scheduled-poweroff
  sudo systemctl start scheduled-poweroff
```

## Scheduling shutdown

If you don't want to use the daily shutdown, you will left the /etc/scheduled-poweroff.conf file empty. You can use crontab to set the shutdown.

To show a shutdown warning to users, run the command:
```
scheduled-poweroff-send -m hour
```
If the user close the warning dialog, the shutdown will be stopped.

To shutdown the computer run:
```
scheduled-poweroff-send -s
```

An example of crontab file to shutdown at "14:30" the first day of each month:
```
20 14 1 * * scheduled-poweroff-send -m 14:30
30 14 1 * * scheduled-poweroff-send -s
```
