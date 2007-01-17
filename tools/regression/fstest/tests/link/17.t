#!/bin/sh
# $FreeBSD$

desc="link returns EFAULT if one of the pathnames specified is outside the process's allocated address space"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..8"

n0=`namegen`

expect 0 create ${n0} 0644
expect EFAULT link ${n0} NULL
expect EFAULT link ${n0} DEADCODE
expect 0 unlink ${n0}
expect EFAULT link NULL ${n0}
expect EFAULT link DEADCODE ${n0}
expect EFAULT link NULL DEADCODE
expect EFAULT link DEADCODE NULL
