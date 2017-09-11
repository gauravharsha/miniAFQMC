////////////////////////////////////////////////////////////////////////
// This file is distributed under the University of Illinois/NCSA Open Source
// License.  See LICENSE file in top directory for details.
//
// Copyright (c) 2016 Jeongnim Kim and QMCPACK developers.
//
// File developed by:
// Miguel A. Morales, moralessilva2@llnl.gov 
//    Lawrence Livermore National Laboratory 
// Alfredo Correa, correaa@llnl.gov 
//    Lawrence Livermore National Laboratory 
//
// File created by:
// Miguel A. Morales, moralessilva2@llnl.gov 
//    Lawrence Livermore National Laboratory 
////////////////////////////////////////////////////////////////////////////////


#ifndef  AFQMC_VHS_HPP 
#define  AFQMC_VHS_HPP 

#include "Numerics/ma_operations.hpp"

namespace qmcplusplus
{

namespace base
{

/*
 * Calculates the H-S potential: 
 *  vHS = Spvn * X 
 *     vHS(ik,w) = sum_n Spvn(ik,n) * X(n,w) 
 */
// Serial Implementation
template< class SpMat,
	  class Mat	
        >
inline void get_vHS(const SpMat& Spvn, const Mat& X, Mat& v)
{
  // check dimensions are consistent
  assert( Spvn.cols() == X.shape()[0] );
  assert( Spvn.rows() == v.shape()[0] );
  assert( X.shape()[1] == v.shape()[1] );

  typedef typename std::decay<Mat>::type::element ComplexType;

  // Spvn*X 
  ma::product(Spvn,X,v);  
}

/*
 * Calculate S = exp(V)*S using a Taylor expansion of exp(V)
 */ 
template< class MatA,
          class MatB,
          class MatC
        >
inline void apply_expM( const MatA& V, MatB& S, MatC& T1, MatC& T2, int order=6)
{ 
  assert( V.shape()[0] == V.shape()[1] );
  assert( V.shape()[1] == S.shape()[0] );
  assert( S.shape()[0] == T1.shape()[0] );
  assert( S.shape()[1] == T1.shape()[1] );
  assert( S.shape()[0] == T2.shape()[0] );
  assert( S.shape()[1] == T2.shape()[1] );

  typedef typename std::decay<MatB>::type::element ComplexType;
  ComplexType zero(0.);

  T1 = S;
  for(int n=1; n<=order; n++) {
    ComplexType fact = ComplexType(0.0,1.0)*static_cast<ComplexType>(1.0/static_cast<double>(n));
    ma::product(fact,V,T1,zero,T2);
    T1  = T2;
    // overload += ???
    for(int i=0, ie=S.shape()[0]; i<ie; i++)
     for(int j=0, je=S.shape()[1]; j<je; j++)
      S[i][j] += T1[i][j];
  }

}

}

namespace shm 
{

/*
 * Calculates the H-S potential: 
 *  vHS = Spvn * X 
 *     vHS(ik,w) = sum_n Spvn(ik,n) * X(n,w) 
 */
template< class SpMat,
	  class MatA,
          class MatB	
        >
inline void get_vHS(const SpMat& Spvn, const MatA& X, MatB& v)
{
  // check dimensions are consistent
  assert( Spvn.cols() == X.shape()[0] );
  assert( Spvn.global_row() == v.shape()[0] );
  assert( X.shape()[1] == v.shape()[1] );
  assert( v.shape()[1] == v.strides()[0] );

  typedef typename std::decay<MatA>::type::element ComplexType;

  // Spvn*X 
  boost::multi_array_ref<ComplexType,2> v_(v.data()+Spvn.global_r0()*v.strides()[0], extents[Spvn.rows()][v.shape()()[1]]);
  ma::product(Spvn,X,v_);  
}

}

}

#endif
