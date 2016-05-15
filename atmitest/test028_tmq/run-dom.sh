#!/bin/bash
## 
## @(#) Test028 - clusterised version
##
## @file run-dom.sh
## 
## -----------------------------------------------------------------------------
## Enduro/X Middleware Platform for Distributed Transaction Processing
## Copyright (C) 2015, ATR Baltic, SIA. All Rights Reserved.
## This software is released under one of the following licenses:
## GPL or ATR Baltic's license for commercial use.
## -----------------------------------------------------------------------------
## GPL license:
## 
## This program is free software; you can redistribute it and/or modify it under
## the terms of the GNU General Public License as published by the Free Software
## Foundation; either version 2 of the License, or (at your option) any later
## version.
##
## This program is distributed in the hope that it will be useful, but WITHOUT ANY
## WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
## PARTICULAR PURPOSE. See the GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License along with
## this program; if not, write to the Free Software Foundation, Inc., 59 Temple
## Place, Suite 330, Boston, MA 02111-1307 USA
##
## -----------------------------------------------------------------------------
## A commercial use license is available from ATR Baltic, SIA
## contact@atrbaltic.com
## -----------------------------------------------------------------------------
##

export TESTNO="028"
export TESTNAME_SHORT="tmq"
export TESTNAME="test${TESTNO}_${TESTNAME_SHORT}"

PWD=`pwd`
if [ `echo $PWD | grep $TESTNAME ` ]; then
	# Do nothing 
	echo > /dev/null
else
	# started from parent folder
	pushd .
	echo "Doing cd"
	cd $TESTNAME
fi;

. ../testenv.sh

export TESTDIR="$NDRX_APPHOME/atmitest/$TESTNAME"
export PATH=$PATH:$TESTDIR
# Override timeout!
export NDRX_TOUT=90

#
# Domain 1 - here client will live
#
function set_dom1 {
    echo "Setting domain 1"
    . ../dom1.sh
    export NDRX_CONFIG=$TESTDIR/ndrxconfig-dom1.xml
    export NDRX_DMNLOG=$TESTDIR/ndrxd-dom1.log
    export NDRX_LOG=$TESTDIR/ndrx-dom1.log
    export NDRX_DEBUG_CONF=$TESTDIR/debug-dom1.conf

# XA config, mandatory for TMQ:
    export NDRX_XA_RES_ID=1
    export NDRX_XA_OPEN_STR="./QSPACE1"
    export NDRX_XA_CLOSE_STR=$NDRX_XA_OPEN_STR
# Used from parent
    export NDRX_XA_DRIVERLIB=$NDRX_XA_DRIVERLIB_FILENAME
    export NDRX_XA_RMLIB=libndrxxaqdisk.so
    export NDRX_XA_LAZY_INIT=0
}

#
# Generic exit function
#
function go_out {
    echo "Test exiting with: $1"
    
    set_dom1;
    xadmin stop -y
    xadmin down -y

    # If some alive stuff left...
    killall -9 atmiclt28

    popd 2>/dev/null
    exit $1
}


function clean_logs {

    # clean-up the logs for debbuging at the error.
    for f in `ls *.log`; do
         echo > $f
    done

}
#
# Test Q space for empty condition
#
function test_empty_qspace {
	echo "Testing Qspace empty"
    
	COUNT=`find ./QSPACE1 -type f | wc | awk '{print $1}'`

	if [[ "X$COUNT" != "X0" ]]; then
		echo "QSPACE1 MUST BE EMPTY AFTER TEST!!!!"
		go_out 2
	fi

    clean_logs;
}

rm *dom*.log

# Where to store TM logs
rm -rf ./RM1
mkdir RM1

# Where to store Q messages (QSPACE1)
rm -rf ./QSPACE1
mkdir QSPACE1

set_dom1;
xadmin down -y
xadmin start -y || go_out 1

# Go to domain 1
set_dom1;

# Run the client test...
xadmin psc
xadmin psvc
xadmin ppm

echo "Running: basic test (enq + deq)"
(./atmiclt28 basic 2>&1) > ./atmiclt-dom1.log
RET=$?

if [[ "X$RET" != "X0" ]]; then
    go_out $RET
fi

test_empty_qspace;

echo "Running: enqueue"
(./atmiclt28 enq 2>&1) > ./atmiclt-dom1.log
RET=$?

if [[ "X$RET" != "X0" ]]; then
    go_out $RET
fi

#find ./QSPACE1 -type f

xadmin down -y
xadmin start -y || go_out 1
clean_logs;

################################################################################
#
# CLI TESTS
#
################################################################################
#reload config
xadmin mqrc
RET=$?

if [[ "X$RET" != "X0" ]]; then
    go_out $RET
fi

# List the queues (should be something..)
xadmin mqlq
# List q config
xadmin mqlc
# List messages in q
xadmin mqlm -s MYSPACE -q TESTC 

#Dump the first message to stdout:

MSGID=`xadmin mqlm -s MYSPACE -q TESTC  | tail -n1  | awk '{print($3)}'`

echo "Dumping message [$MSGID]"
xadmin mqdm -n 1 -i 100 -m $MSGID


xadmin mqch -n 1 -i 100 -q MEMQ,svcnm=FAILRND,autoq=y,tries=10,waitinit=1,waitretry=1,waitretryinc=0,waitretrymax=1,memonly=y
# List q config
xadmin mqlc

################################################################################

echo "Running: dequeue (abort)"
(./atmiclt28 deqa 2>&1) >> ./atmiclt-dom1.log
RET=$?

if [[ "X$RET" != "X0" ]]; then
    go_out $RET
fi

sleep 10

echo "Running: dequeue (commit)"
(./atmiclt28 deqc 2>&1) >> ./atmiclt-dom1.log
RET=$?

if [[ "X$RET" != "X0" ]]; then
    go_out $RET
fi

test_empty_qspace;

echo "Running: dequeue - empty"
(./atmiclt28 deqe 2>&1) >> ./atmiclt-dom1.log
RET=$?

if [[ "X$RET" != "X0" ]]; then
    go_out $RET
fi

test_empty_qspace;

echo "Running: msgid tests"
(./atmiclt28 msgid 2>&1) >> ./atmiclt-dom1.log
RET=$?

if [[ "X$RET" != "X0" ]]; then
    go_out $RET
fi

test_empty_qspace;

echo "Running: corid tests"
(./atmiclt28 corid 2>&1) >> ./atmiclt-dom1.log
RET=$?

if [[ "X$RET" != "X0" ]]; then
    go_out $RET
fi

test_empty_qspace;

echo "Running: Auto queue ok + reply q"
(./atmiclt28 autoqok 2>&1) >> ./atmiclt-dom1.log
RET=$?

if [[ "X$RET" != "X0" ]]; then
    go_out $RET
fi

test_empty_qspace;

echo "Running: Auto queue dead"
(./atmiclt28 autodeadq 2>&1) >> ./atmiclt-dom1.log
RET=$?

if [[ "X$RET" != "X0" ]]; then
    go_out $RET
fi

test_empty_qspace;

echo "Running: raindom fail for auto"
(./atmiclt28 rndfail 2>&1) >> ./atmiclt-dom1.log
RET=$?

if [[ "X$RET" != "X0" ]]; then
    go_out $RET
fi

test_empty_qspace;

echo "LIFO tests..."

echo "Running: enqueue (LIFO)"
(./atmiclt28 lenq 2>&1) > ./atmiclt-dom1.log
RET=$?

if [[ "X$RET" != "X0" ]]; then
    go_out $RET
fi

xadmin down -y
xadmin start -y || go_out 1
clean_logs;

#find ./QSPACE1 -type f
echo "Running: dequeue (abort) (LIFO)"
(./atmiclt28 ldeqa 2>&1) >> ./atmiclt-dom1.log
RET=$?

if [[ "X$RET" != "X0" ]]; then
    go_out $RET
fi

sleep 10

echo "Running: dequeue (commit) (LIFO)"
(./atmiclt28 ldeqc 2>&1) >> ./atmiclt-dom1.log
RET=$?

if [[ "X$RET" != "X0" ]]; then
    go_out $RET
fi

test_empty_qspace;

# Catch is there is test error!!!
if [ "X`grep TESTERROR *.log`" != "X" ]; then
	echo "Test error detected!"
	RET=-2
fi



go_out $RET
