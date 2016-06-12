BASE=`pwd`

cd $BASE/petlib
make clean; make

cd $BASE/petlib/petos
make clean; make

cd $BASE/../palacios-kitten/
make clean; make -j4

cd $BASE/palacios/
make clean; make -j4

cd $BASE/kitten/
make clean; make -j4

cd $BASE/xpmem/mod
make clean; make

cd $BASE/xpmem/lib
make clean; make

cd $BASE/pisces/
make clean; make

cd $BASE/libhobbes
make clean; make

cd $BASE/shell
make clean; make

cd $BASE/lnx_inittask
make clean; make

cd $BASE/lwk_inittask
make clean; make

cd $BASE/hio/mod
make clean; make

cd $BASE/hio/mod/stub
make clean; make
