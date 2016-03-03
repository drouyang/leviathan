#!/bin/bash

# boilerplate junk
IFS=

#if [ -f leviathan.pid ]; then
#    echo "PID file exists. Is Leviathan already running?"
#    exit
#fi

if [ ! -f ./build.cfg ]; then
    echo "Leviathan configuration file (build.cfg) not present."
    exit
fi

IFS="
" # no, you can't actually use "\n" to specify a newline....

for cfg in `grep -v '^#' < ./build.cfg | grep -e '^$' -v`; do
    if  [ ! -z ${cfg} ]; then
        eval "export ${cfg}"
    fi
done
IFS=" "

echo "Inserting Petos Module."
insmod $PETLIB_PATH/petos/petos.ko

echo "Inserting XPMEM Module."
insmod $XPMEM_PATH/mod/xpmem.ko ns=1

#echo $PALACIOS_PATH

if [ -f $PALACIOS_PATH/v3vee.ko ]; then
echo "Inserting Palacios Module."
insmod $PALACIOS_PATH/v3vee.ko
else
echo "Could not find v3vee module.  Palacios/Linux will not be enabled."
fi


echo "Inserting Pisces Module."
insmod $PISCES_PATH/pisces.ko

export HOBBES_ENCLAVE_ID=0
export HOBBES_APP_ID=0

echo "Launching Leviathan Node Manager."
#$LEVIATHAN_PATH/lnx_inittask/lnx_init ${@:1} &
$LEVIATHAN_PATH/lnx_inittask/lnx_init --mem=128 --numa=0 &
echo $! > leviathan.pid
