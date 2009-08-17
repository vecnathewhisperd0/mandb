failures=0

init () {
	tmpdir="tmp-${0##*/}"
	mkdir -p "$tmpdir" || exit $?
	trap 'rm -rf "$tmpdir"' HUP INT QUIT TERM
}

expect_pass () {
	ret=0
	eval "$2" || ret=$?
	if [ "$ret" = 0 ]; then
		echo "  PASS: $1"
	else
		failures="$(($failures + 1))"
		echo "  FAIL: $1"
	fi
}

finish () {
	case $failures in
		0)
			rm -rf "$tmpdir"
			exit 0
			;;
		*)
			if [ -z "$TEST_FAILURE_KEEP" ]; then
				rm -rf "$tmpdir"
			fi
			exit 1
			;;
	esac
}
