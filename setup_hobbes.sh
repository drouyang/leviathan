#!/bin/sh

source ./build.cfg

if [ ! -d $XPMEM_PATH ]; then
	echo "Error: XPMEM could not be found at $XPMEM_PATH"
	exit
fi

if [ ! -d $PALACIOS_PATH ]; then
	echo "Error: Palacios could not be found at $PALACIOS_PATH"
	exit
fi

if [ ! -d $PISCES_PATH ]; then
	echo "Error: PISCES could not be found at $PISCES_PATH"
	exit
fi

if [ ! -d $PETLIB_PATH ]; then
	echo "Error: PETLIB could not be found at $PETLIB_PATH"
	exit
fi

if [ ! -d $KITTEN_PATH ]; then
	echo "Error: KITTEN could not be found at $KITTEN_PATH"
	exit
fi

if [ ! -d xpmem ]; then
	ln -s $XPMEM_PATH xpmem
fi

if [ ! -d pisces ]; then
	ln -s $PISCES_PATH pisces
fi

if [ ! -d palacios ]; then
	ln -s $PALACIOS_PATH palacios
fi

if [ ! -d petlib ]; then
	ln -s $PETLIB_PATH petlib
fi

if [ ! -d kitten ]; then
	ln -s $KITTEN_PATH kitten
fi
