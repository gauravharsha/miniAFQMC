#!/bin/tcsh
echo Configuring and building froom inside the build directory.
echo Check the results of the CMake configuration to ensure that the preferred
echo compilers and libraries have been selected. See README and documentation 
echo for guidance.

setenv CXX mpicxx
setenv CC mpicc

source /usr/tce/packages/mkl/mkl-2017.1/mkl/bin/mklvars.csh intel64

cmake -DBLA_VENDOR=Intel10_64lp_seq -DCMAKE_PREFIX_PATH=$MKLROOT/lib -DBUILD_AFQMC=1 -DQMC_COMPLEX=1 ..; 
make

