failures=0

init () {
	tmpdir="tmp-${0##*/}"
	mkdir -p "$tmpdir" || exit $?
	trap 'rm -rf "$tmpdir"' HUP INT QUIT TERM
}

fake_config () {
	for dir; do
		echo "MANDATORY_MANPATH	$tmpdir$dir"
	done >"$tmpdir/manpath.config"
}

db_ext () {
	case $DBTYPE in
		gdbm)	echo .db ;;
		btree)	echo .bt ;;
	esac
}

# Arguments: name section path encoding compression_extension preprocessor_line name_line
write_page () {
	mkdir -p "${3%/*}"
	>"$3.tmp1"
	if [ "$6" ]; then
		echo "'\\\" $6" >>"$3.tmp1"
	fi
	cat >>"$3.tmp1" <<EOF
.TH $1 $2
.SH NAME
$7
.SH DESCRIPTION
test
EOF
	iconv -f UTF-8 -t "$4" <"$3.tmp1" >"$3.tmp2"
	case $5 in
		'')	cat ;;
		gz|z)	gzip -9c ;;
		Z)	compress -c ;;
		bz2)	bzip2 -9c ;;
		lzma)	lzma -9c ;;
	esac <"$3.tmp2" >"$3"
	rm -f "$3.tmp1" "$3.tmp2"
}

run_clean_path () {
	if [ "$CLEANPATH" ]; then
		PATH="$CLEANPATH" "$@"
	else
		"$@"
	fi
}

accessdb_filter () {
	# e.g. 'test -> "- 1 1 1250702063 A - - gz simple mandb test"'
	$ACCESSDB "$1" | grep -v '^\$' | \
		sed 's/\(-> "[^ ][^ ]* [^ ][^ ]* [^ ][^ ]* \)[^ ][^ ]* /\1MTIME /'
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
