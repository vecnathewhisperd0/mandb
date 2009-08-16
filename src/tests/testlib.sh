failures=0

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
			exit 0
			;;
		*)
			exit 1
			;;
	esac
}
