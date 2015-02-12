#!/bin/sh

# boilerplate junk
IFS=

if [ -f pids ]; then
    echo "PID file exists. Is Hobbes already running?"
    exit
fi

if [ ! -f hobbes.cfg ]; then
    echo "Hobbes configuration file not present. Please run setup.sh"
    exit
fi

IFS="
" # no, you can't actually use "\n" to specify a newline....

for cfg in `cat hobbes.cfg`; do
    if  [ ! -z ${cfg} ]; then
        eval export ${cfg}
    fi
done
IFS=" "


echo "Inserting XPMEM Module."
insmod $XPMEM_PATH/mod/xpmem.ko

echo "Inserting Palacios Module."
insmod $PALACIOS_PATH/v3vee.ko

echo "Inserting Pisces Module."
insmod $PISCES_PATH/pisces.ko




