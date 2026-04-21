#
# Regular cron jobs for the ftl-maker package.
#
0 4	* * *	root	[ -x /usr/bin/ftl-maker_maintenance ] && /usr/bin/ftl-maker_maintenance
