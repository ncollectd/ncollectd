#! /bin/bash
#
# ncollectd - start and stop the statistics collection daemon
# https://ncollectd.org/
#
# Copyright (C) 2005-2006 Florian Forster <octo@verplant.org>
# Copyright (C) 2006-2009 Sebastian Harl <tokkee@debian.org>
#

### BEGIN INIT INFO
# Provides:          ncollectd
# Required-Start:    $local_fs $remote_fs
# Required-Stop:     $local_fs $remote_fs
# Should-Start:      $network $named $syslog $time cpufrequtils
# Should-Stop:       $network $named $syslog
# Default-Start:     2 3 4 5
# Default-Stop:      0 1 6
# Short-Description: manage the statistics collection daemon
# Description:       ncollectd is the statistics collection daemon.
#                    It is a small daemon which collects system information
#                    periodically and provides mechanisms to monitor and store
#                    the values in a variety of ways.
### END INIT INFO

. /lib/lsb/init-functions

export PATH=/sbin:/bin:/usr/sbin:/usr/bin

DISABLE=0

DESC="statistics collection and monitoring daemon"
NAME=ncollectd
DAEMON=/usr/sbin/ncollectd

CONFIGFILE=/etc/ncollectd/ncollectd.conf
PIDFILE=/var/run/ncollectd.pid

USE_COLLECTDMON=1
COLLECTDMON_DAEMON=/usr/sbin/ncollectdmon

MAXWAIT=30

# Gracefully exit if the package has been removed.
test -x $DAEMON || exit 0

if [ -r /etc/default/$NAME ]; then
	. /etc/default/$NAME
fi

if test "$ENABLE_COREFILES" == 1; then
	ulimit -c unlimited
fi

# return:
#   0 if config is fine
#   1 if there is a syntax error
#   2 if there is no configuration
check_config() {
	if test ! -e "$CONFIGFILE"; then
		return 2
	fi
	if ! $DAEMON -t -C "$CONFIGFILE"; then
		return 1
	fi
	return 0
}

# return:
#   0 if the daemon has been started
#   1 if the daemon was already running
#   2 if the daemon could not be started
#   3 if the daemon was not supposed to be started
d_start() {
	if test "$DISABLE" != 0; then
		# we get here during restart
		log_progress_msg "disabled by /etc/default/$NAME"
		return 3
	fi

	if test ! -e "$CONFIGFILE"; then
		# we get here during restart
		log_progress_msg "disabled, no configuration ($CONFIGFILE) found"
		return 3
	fi

	check_config
	rc="$?"
	if test "$rc" -ne 0; then
		log_progress_msg "not starting, configuration error"
		return 2
	fi

	if test "$USE_COLLECTDMON" == 1; then
		start-stop-daemon --start --quiet --oknodo --pidfile "$PIDFILE" \
			--exec $COLLECTDMON_DAEMON -- -P "$PIDFILE" -- -C "$CONFIGFILE" \
			|| return 2
	else
		start-stop-daemon --start --quiet --oknodo --pidfile "$PIDFILE" \
			--exec $DAEMON -- -C "$CONFIGFILE" -P "$PIDFILE" \
			|| return 2
	fi
	return 0
}

still_running_warning="
WARNING: $NAME might still be running.
In large setups it might take some time to write all pending data to
the disk. You can adjust the waiting time in /etc/default/ncollectd."

# return:
#   0 if the daemon has been stopped
#   1 if the daemon was already stopped
#   2 if daemon could not be stopped
d_stop() {
	PID=$( cat "$PIDFILE" 2> /dev/null ) || true

	start-stop-daemon --stop --quiet --oknodo --pidfile "$PIDFILE"
	rc="$?"

	if test "$rc" -eq 2; then
		return 2
	fi

	sleep 1
	if test -n "$PID" && kill -0 $PID 2> /dev/null; then
		i=0
		while kill -0 $PID 2> /dev/null; do
			i=$(( $i + 2 ))
			echo -n " ."

			if test $i -gt $MAXWAIT; then
				log_progress_msg "$still_running_warning"
				return 2
			fi

			sleep 2
		done
		return "$rc"
	fi
	return "$rc"
}

case "$1" in
	start)
		log_daemon_msg "Starting $DESC" "$NAME"
		d_start
		case "$?" in
			0|1) log_end_msg 0 ;;
			2) log_end_msg 1 ;;
			3) log_end_msg 255; true ;;
			*) log_end_msg 1 ;;
		esac
		;;
	stop)
		log_daemon_msg "Stopping $DESC" "$NAME"
		d_stop
		case "$?" in
			0|1) log_end_msg 0 ;;
			2) log_end_msg 1 ;;
		esac
		;;
	status)
		status_of_proc -p "$PIDFILE" "$DAEMON" "$NAME" && exit 0 || exit $?
		;;
	restart|force-reload)
		log_daemon_msg "Restarting $DESC" "$NAME"
		check_config
		rc="$?"
		if test "$rc" -eq 1; then
			log_progress_msg "not restarting, configuration error"
			log_end_msg 1
			exit 1
		fi
		d_stop
		rc="$?"
		case "$rc" in
			0|1)
				sleep 1
				d_start
				rc2="$?"
				case "$rc2" in
					0|1) log_end_msg 0 ;;
					2) log_end_msg 1 ;;
					3) log_end_msg 255; true ;;
					*) log_end_msg 1 ;;
				esac
				;;
			*)
				log_end_msg 1
				;;
		esac
		;;
	*)
		echo "Usage: $0 {start|stop|restart|force-reload|status}" >&2
		exit 3
		;;
esac

# vim: syntax=sh noexpandtab sw=4 ts=4 :

