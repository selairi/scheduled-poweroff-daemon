all: scheduled-poweroff-daemon scheduled-poweroff-client


scheduled-poweroff-daemon: scheduled-poweroff-daemon.cpp
	c++ -Os scheduled-poweroff-daemon.cpp -o scheduled-poweroff-daemon `pkg-config --cflags --libs libsystemd`	
	strip scheduled-poweroff-daemon

scheduled-poweroff-client: scheduled-poweroff-client.cpp
	c++ -Os scheduled-poweroff-client.cpp -o scheduled-poweroff-client `pkg-config --cflags --libs libsystemd`	
	strip scheduled-poweroff-client

clean:
	rm scheduled-poweroff-client scheduled-poweroff-daemon

install: scheduled-poweroff-daemon scheduled-poweroff-client
	cp scheduled-poweroff-client scheduled-poweroff-daemon /usr/local/bin/
	cp scheduled-poweroff-client.desktop /etc/xdg/autostart/
	cp scheduled-poweroff.conf /etc/ 
	cp local.sheduledpoweroffdaemon.conf /etc/dbus-1/system.d/
	cp scheduled-poweroff.service /etc/systemd/system/
	chmod 777 /etc/systemd/system/scheduled-poweroff.service

uninstall:
	rm /usr/local/bin/scheduled-poweroff-client /usr/local/bin/scheduled-poweroff-daemon
	rm /etc/xdg/autostart/scheduled-poweroff-client.desktop
	rm /etc/scheduled-poweroff.conf
	rm /etc/dbus-1/system.d/local.sheduledpoweroffdaemon.conf
	rm /etc/systemd/system/scheduled-poweroff.service
 
