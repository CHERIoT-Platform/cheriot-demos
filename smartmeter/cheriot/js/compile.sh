#!/bin/bash
set -e

unset HEADER_OUT

while getopts "H:" o; do
	case "$o" in
		H) HEADER_OUT="${OPTARG}";;
		*) exit 1;
	esac
done
shift $((OPTIND - 1))

if [ -z "$1" ] ; then
	echo Usage: $0 {javascript file}
	exit 1
fi

if [ ! -f ./node_modules/.bin/microvium ] ; then
	if ! type npm >/dev/null 2>&1 ; then
		if type apt >/dev/null 2>&1 ; then
			echo npm is not installed.  Trying to install it from apt.
			echo If you are not using the devcontainer, this may fail.
			sudo apt update
			sudo apt install nodejs make gcc g++
		else
			echo npm is not installed.  Please install it.
			exit 1
		fi
	fi
	npm install microvium
fi

declare -a MVM_CMD
MVM_CMD=(./node_modules/.bin/microvium "$1")

[ -n "${HEADER_OUT-}" ] && {
	MVM_CMD+=("--output-bytes")
	MVM_CMD+=(">>")
	MVM_CMD+=("${HEADER_OUT}")

    cat > "${HEADER_OUT}" <<HERE
/*
 * Auto-generated file from ${1}; do not edit.
 * See js/compile.sh.
 */
unsigned char hello_mvm_bc[] =
HERE
}

eval "${MVM_CMD[@]}"
echo Output in $1.mvm-bc

[ -n "${HEADER_OUT-}" ] && {
    cat >> "${HEADER_OUT}" <<HERE
;
unsigned int hello_mvm_bc_len = sizeof(hello_mvm_bc);
HERE
	clang-format -i "${HEADER_OUT}"
}
