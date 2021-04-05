#/bin/bash

# PERFPATH=./../../../WSL2-Linux-Kernel/tools/perf/perf
PERFPATH=/usr/lib/linux-tools/5.4.0-70-generic/perf

# SATCOMMAND=$1
# SATINPUT=$2

# $PERFPATH record $1 < $2

# Get the topmost perf function name
# grep finds the first line reporting usage (litearlly we're just looking for "%")
# awk, I don't know, see: https://stackoverflow.com/questions/16616975/how-do-i-get-the-last-word-in-each-line-with-bash
TOPFUNC=$($PERFPATH report | grep '\s*%.*$' -m 1 | awk '{ print $NF }')
$PERFPATH annotate -s $TOPFUNC > top_func.txt