#!/bin/sh -e
pgm=`basename $0`
edir=/usr/lib/man-db
cmd="${edir}/${pgm} ${1+$@}"
[ `id -u` = 0 ] || exec ${cmd}
su nobody -c "/bin/true" && exec su nobody -c "$cmd"
su -s /bin/true 2>/dev/null && exec su -s /bin/sh nobody -c "$cmd"
exec su man -c "$cmd"
