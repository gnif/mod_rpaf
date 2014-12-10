#
# Regular cron jobs for the libapache2-mod-rpaf-gnif package
#
0 4	* * *	root	[ -x /usr/bin/libapache2-mod-rpaf-gnif_maintenance ] && /usr/bin/libapache2-mod-rpaf-gnif_maintenance
