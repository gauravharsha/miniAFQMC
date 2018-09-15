////////////////////////////////////////////////////////////////////////////////
// This file is distributed under the University of Illinois/NCSA Open Source
// License.  See LICENSE file in top directory for details.
//
// Copyright (c) 2016 Jeongnim Kim and QMCPACK developers.
//
// File developed by:
// Ken Esler, kpesler@gmail.com,
//    University of Illinois at Urbana-Champaign
// Miguel Morales, moralessilva2@llnl.gov,
//    Lawrence Livermore National Laboratory
// Jeongnim Kim, jeongnim.kim@gmail.com,
//    University of Illinois at Urbana-Champaign
// Jeremy McMinnis, jmcminis@gmail.com,
//    University of Illinois at Urbana-Champaign
// Mark A. Berrill, berrillma@ornl.gov,
//    Oak Ridge National Laboratory
// Alfredo A. Correa, correaa@llnl.gov
//    Lawrence Livermore National Laboratory
// Miguel A. Morales, moralessilva2@llnl.gov 
//    Lawrence Livermore National Laboratory 
//
// File created by:
// Jeongnim Kim, jeongnim.kim@gmail.com,
//    University of Illinois at Urbana-Champaign
////////////////////////////////////////////////////////////////////////////////

#ifndef AFQMC_LAPACK_CPU_H
#define AFQMC_LAPACK_CPU_H

// generic header for blas routines
#include "Numerics/detail/Blasf.h"

namespace LAPACK_CPU
{

  inline static void gesvd(char *jobu, char *jobvt, int *m, int *n, float *a,
                           int *lda, float *s, float *u, int *ldu, float *vt,
                           int *ldvt, float *work, int *lwork, int *info)
  {
    sgesvd(jobu, jobvt, m, n, a, lda, s, u, ldu, vt, ldvt, work, lwork, info);
  }

  inline static void gesvd(char *jobu, char *jobvt, int *m, int *n, double *a,
                           int *lda, double *s, double *u, int *ldu, double *vt,
                           int *ldvt, double *work, int *lwork, int *info)
  {
    dgesvd(jobu, jobvt, m, n, a, lda, s, u, ldu, vt, ldvt, work, lwork, info);
  }

  inline static void geev(char *jobvl, char *jobvr, int *n, double *a, int *lda,
                          double *alphar, double *alphai, double *vl, int *ldvl,
                          double *vr, int *ldvr, double *work, int *lwork,
                          int *info)
  {
    dgeev(jobvl, jobvr, n, a, lda, alphar, alphai, vl, ldvl, vr, ldvr, work,
          lwork, info);
  }

  inline static void geev(char *jobvl, char *jobvr, int *n, float *a, int *lda,
                          float *alphar, float *alphai, float *vl, int *ldvl,
                          float *vr, int *ldvr, float *work, int *lwork,
                          int *info)
  {
    sgeev(jobvl, jobvr, n, a, lda, alphar, alphai, vl, ldvl, vr, ldvr, work,
          lwork, info);
  }

  inline static void ggev(char *jobvl, char *jobvr, int *n, double *a, int *lda,
                          double *b, int *ldb, double *alphar, double *alphai,
                          double *beta, double *vl, int *ldvl, double *vr,
                          int *ldvr, double *work, int *lwork, int *info)
  {
    dggev(jobvl, jobvr, n, a, lda, b, ldb, alphar, alphai, beta, vl, ldvl, vr,
          ldvr, work, lwork, info);
  }

  inline static void ggev(char *jobvl, char *jobvr, int *n, float *a, int *lda,
                          float *b, int *ldb, float *alphar, float *alphai,
                          float *beta, float *vl, int *ldvl, float *vr,
                          int *ldvr, float *work, int *lwork, int *info)
  {
    sggev(jobvl, jobvr, n, a, lda, b, ldb, alphar, alphai, beta, vl, ldvl, vr,
          ldvr, work, lwork, info);
  }

  inline static
  void hevr (char &JOBZ, char &RANGE, char &UPLO, int &N, float *A, int &LDA,
             float &VL, float &VU,int &IL, int &IU, float &ABSTOL, int &M, float *W,
             float* Z, int &LDZ, int* ISUPPZ, float *WORK,
             int &LWORK, float* RWORK, int &LRWORK, int* IWORK, int &LIWORK, int &INFO)
  {
    bool query = (LWORK==-1) or (LRWORK==-1) or (LIWORK==-1);
    if(query) {
      LWORK=-1;
      LRWORK=-1;
      LIWORK=-1;
    } 
    ssyevr (JOBZ,RANGE,UPLO,N,A,LDA,VL,VU,IL,IU,ABSTOL,M,W,Z,LDZ,ISUPPZ,
            RWORK,LRWORK,IWORK,LIWORK,INFO);
    if(query) {
      LWORK = int(WORK[0]);
      LRWORK = int(RWORK[0]);
      LIWORK = int(IWORK[0]);
    } 
  }

  inline static
  void hevr (char &JOBZ, char &RANGE, char &UPLO, int &N, double *A, int &LDA,
             double &VL, double &VU,int &IL, int &IU, double &ABSTOL, int &M, double *W,
             double* Z, int &LDZ, int* ISUPPZ, double *WORK,
             int &LWORK, double* RWORK, int &LRWORK, int* IWORK, int &LIWORK, int &INFO)
  {
    bool query = (LWORK==-1) or (LRWORK==-1) or (LIWORK==-1);
    if(query) {
      LWORK=-1;
      LRWORK=-1;
      LIWORK=-1;
    }
    dsyevr (JOBZ,RANGE,UPLO,N,A,LDA,VL,VU,IL,IU,ABSTOL,M,W,Z,LDZ,ISUPPZ,
            RWORK,LRWORK,IWORK,LIWORK,INFO);
    if(query) {
      LWORK = int(WORK[0]);
      LRWORK = int(RWORK[0]);
      LIWORK = int(IWORK[0]);
    }
  }

  inline static
  void hevr (char &JOBZ, char &RANGE, char &UPLO, int &N, std::complex<float> *A, int &LDA,
             float &VL, float &VU,int &IL, int &IU, float &ABSTOL, int &M, float *W,
             std::complex<float>* Z, int &LDZ, int* ISUPPZ,std::complex<float> *WORK,
             int &LWORK, float* RWORK, int &LRWORK, int* IWORK, int &LIWORK, int &INFO)
  {
    bool query = (LWORK==-1) or (LRWORK==-1) or (LIWORK==-1);
    if(query) {
      LWORK=-1;
      LRWORK=-1;
      LIWORK=-1;
    }
    cheevr (JOBZ,RANGE,UPLO,N,A,LDA,VL,VU,IL,IU,ABSTOL,M,W,Z,LDZ,ISUPPZ,WORK,LWORK,
            RWORK,LRWORK,IWORK,LIWORK,INFO);
    if(query) {
      LWORK = int(real(WORK[0]));
      LRWORK = int(RWORK[0]);
      LIWORK = int(IWORK[0]);
    }
  }

  inline static
  void hevr (char &JOBZ, char &RANGE, char &UPLO, int &N, std::complex<double> *A, int &LDA,
             double &VL, double &VU,int &IL, int &IU, double &ABSTOL, int &M, double *W,
             std::complex<double>* Z, int &LDZ, int* ISUPPZ,std::complex<double> *WORK,
             int &LWORK, double* RWORK, int &LRWORK, int* IWORK, int &LIWORK, int &INFO)
  {
    bool query = (LWORK==-1) or (LRWORK==-1) or (LIWORK==-1);
    if(query) {
      LWORK=-1;
      LRWORK=-1;
      LIWORK=-1;
    }
    zheevr (JOBZ,RANGE,UPLO,N,A,LDA,VL,VU,IL,IU,ABSTOL,M,W,Z,LDZ,ISUPPZ,WORK,LWORK,
           RWORK,LRWORK,IWORK,LIWORK,INFO);
    if(query) {
      LWORK = int(real(WORK[0]));
      LRWORK = int(RWORK[0]);
      LIWORK = int(IWORK[0]);
    }
  }

  inline static
  void getrf_bufferSize (const int n, const int m, float* a, const int lda, int *lwork) { *lwork = 0; } 
  inline static
  void getrf_bufferSize (const int n, const int m, double* a, const int lda, int *lwork) { *lwork = 0; } 
  inline static
  void getrf_bufferSize (const int n, const int m, std::complex<float>* a, const int lda, int *lwork) { *lwork = 0; } 
  inline static
  void getrf_bufferSize (const int n, const int m, std::complex<double>* a, const int lda, int *lwork) { *lwork = 0; } 

  void static getrf(
	const int &n, const int &m, double *a, const int &n0, int *piv, int &st, double* work=nullptr)
  {
	dgetrf(n, m, a, n0, piv, st);
  }

  void static getrf(
	const int &n, const int &m, float *a, const int &n0, int *piv, int &st, float* work=nullptr)
  {
	sgetrf(n, m, a, n0, piv, st);
  }

  void static getrf(const int &n, const int &m,  
	std::complex<double> *a, const int &n0, int *piv, int &st, std::complex<double>* work=nullptr)
  {
	zgetrf(n, m, a, n0, piv, st);
  }

  void static getrf(const int &n, const int &m,
	std::complex<float> *a, const int &n0, int *piv, int &st, std::complex<float>* work=nullptr)
  {
	cgetrf(n, m, a, n0, piv, st);
  }

  inline static void getrfBatched (const int n, float ** a, int lda, int * piv, int * info, int batchSize)
  {
    for(int i=0; i<batchSize; i++)
      getrf(n,n,a[i],lda,piv+i*n,*(info+i));
  }

  inline static void getrfBatched (const int n, double ** a, int lda, int * piv, int * info, int batchSize)
  {
    for(int i=0; i<batchSize; i++)
      getrf(n,n,a[i],lda,piv+i*n,*(info+i));
  }

  inline static void getrfBatched (const int n, std::complex<double> ** a, int lda, int * piv, int * info, int batchSize)
  {
    for(int i=0; i<batchSize; i++)
      getrf(n,n,a[i],lda,piv+i*n,*(info+i));
  }

  inline static void getrfBatched (const int n, std::complex<float> ** a, int lda, int * piv, int * info, int batchSize)
  {
    for(int i=0; i<batchSize; i++)
      getrf(n,n,a[i],lda,piv+i*n,*(info+i));
  }

  inline static
  void getri_bufferSize (int n, float* restrict a, int lda, int& lwork)
  {
    float work;
    int status;
    lwork = -1;
    sgetri(n, a, lda, nullptr, &work, lwork, status);
    lwork = int(work); 
  }

  inline static
  void getri_bufferSize (int n, double* restrict a, int lda, int& lwork)
  {
    double work;
    int status;
    lwork = -1;
    dgetri(n, a, lda, nullptr, &work, lwork, status);
    lwork = int(work);
  }

  inline static
  void getri_bufferSize (int n, std::complex<float>* restrict a, int lda, int& lwork)
  {
    std::complex<float> work;
    int status;
    lwork = -1;
    cgetri(n, a, lda, nullptr, &work, lwork, status);
    lwork = int(real(work));
  }

  inline static
  void getri_bufferSize (int n, std::complex<double>* restrict a, int lda, int& lwork)
  {
    std::complex<double> work;
    int status;
    lwork = -1;
    zgetri(n, a, lda, nullptr, &work, lwork, status);
    lwork = int(real(work));
  }


  void static getri(int n, float* restrict a, int n0, int const* restrict piv, float* restrict work, int& n1, int& status)
  {
        bool query = (n1==-1);
	sgetri(n, a, n0, piv, work, n1, status);
        if(query) n1 = int(work[0]);
  }

  void static getri(int n, double* restrict a, int n0, int const* restrict piv, double* restrict work, int& n1, int& status)
  {
        bool query = (n1==-1);
	dgetri(n, a, n0, piv, work, n1, status);
        if(query) n1 = int(work[0]);
  }

  void static getri(int n, std::complex<float>* restrict a, int n0, int const* restrict piv, std::complex<float>* restrict work, int& n1, int& status)
  {
        bool query = (n1==-1);
	cgetri(n, a, n0, piv, work, n1, status);
        if(query) n1 = int(real(work[0]));
  }

  void static getri(int n, std::complex<double>* restrict a, int n0, int const* restrict piv, std::complex<double>* restrict work, int& n1, int& status)
  {
        bool query = (n1==-1);
	zgetri(n, a, n0, piv, work, n1, status);
        if(query) n1 = int(real(work[0]));
  }

  inline static void getriBatched (int n, float ** a, int lda, int * piv, float ** work, int& lwork, int * info, int batchSize)
  {
    for(int i=0; i<batchSize; i++)
      getri(n,a[i],lda,piv+i*n,work[i],lwork,*(info+i));
  }

  inline static void getriBatched (int n, double ** a, int lda, int * piv, double ** work, int& lwork, int * info, int batchSize)
  {
    for(int i=0; i<batchSize; i++)
      getri(n,a[i],lda,piv+i*n,work[i],lwork,*(info+i));
  }

  inline static void getriBatched (int n, std::complex<float> ** a, int lda, int * piv, std::complex<float> ** work, int& lwork, int * info, int batchSize)
  {
    for(int i=0; i<batchSize; i++)
      getri(n,a[i],lda,piv+i*n,work[i],lwork,*(info+i));
  }

  inline static void getriBatched (int n, std::complex<double> ** a, int lda, int * piv, std::complex<double> ** work, int& lwork, int * info, int batchSize)
  {
    for(int i=0; i<batchSize; i++)
      getri(n,a[i],lda,piv+i*n,work[i],lwork,*(info+i));
  }

  void static geqrf(int M, int N, std::complex<double> *A, const int LDA, std::complex<double> *TAU, std::complex<double> *WORK, int LWORK, int& INFO)
  {
#ifdef __NO_QR__
INFO=0;
WORK[0]=0;
#else
	zgeqrf(M, N, A, LDA, TAU, WORK, LWORK, INFO);
#endif
  }

  void static geqrf(int M, int N, double *A, const int LDA, double *TAU, double *WORK, int LWORK, int& INFO)
  {
#ifdef __NO_QR__
INFO=0;
WORK[0]=0;
#else
	dgeqrf(M,N,A,LDA,TAU,WORK,LWORK,INFO);
#endif
  }

  void static geqrf(int M, int N, std::complex<float> *A, const int LDA, std::complex<float> *TAU, std::complex<float> *WORK, int LWORK, int& INFO)
  {
#ifdef __NO_QR__
INFO=0;
WORK[0]=0;
#else
	cgeqrf(M,N,A,LDA,TAU,WORK,LWORK,INFO);
#endif
  }

  void static geqrf(int M, int N, float *A, const int LDA, float *TAU, float *WORK, int LWORK, int& INFO)
  {
#ifdef __NO_QR__
INFO=0;
WORK[0]=0;
#else
	sgeqrf(M,N,A,LDA,TAU,WORK,LWORK,INFO);
#endif
  }

  void static gelqf(int M, int N, std::complex<double> *A, const int LDA, std::complex<double> *TAU, std::complex<double> *WORK, int LWORK, int& INFO)
  {
#ifdef __NO_QR__
INFO=0;
WORK[0]=0;
#else
	zgelqf(M,N,A,LDA,TAU,WORK,LWORK,INFO);
#endif
  }

  void static gelqf(int M, int N, double *A, const int LDA, double *TAU, double *WORK, int LWORK, int& INFO)
  {
#ifdef __NO_QR__
INFO=0;
WORK[0]=0;
#else
	dgelqf(M,N,A,LDA,TAU,WORK,LWORK,INFO);
#endif
  }

  void static gelqf(int M, int N, std::complex<float> *A, const int LDA, std::complex<float> *TAU, std::complex<float> *WORK, int LWORK, int& INFO)
  {
#ifdef __NO_QR__
INFO=0;
WORK[0]=0;
#else
	cgelqf(M,N,A,LDA,TAU,WORK,LWORK,INFO);
#endif
  }

  void static gelqf(int M, int N, float *A, const int LDA,  float *TAU, float *WORK, int LWORK, int& INFO)
  {
#ifdef __NO_QR__
INFO=0;
WORK[0]=0;
#else
	sgelqf(M,N,A,LDA,TAU,WORK,LWORK,INFO);
#endif
  }

  void static gqr(int M, int N, int K, std::complex<double> *A, const int LDA, std::complex<double> *TAU, std::complex<double> *WORK, int LWORK, int& INFO)
  {
#ifdef __NO_QR__
INFO=0;
WORK[0]=0;
#else
	zungqr(M,N,K,A,LDA,TAU,WORK,LWORK,INFO);
#endif
  }

  void static gqr(int M, int N, int K, double *A, const int LDA, double *TAU, double *WORK, int LWORK, int& INFO)
  {
#ifdef __NO_QR__
INFO=0;
WORK[0]=0;
#else
	dorgqr(M,N,K,A,LDA,TAU,WORK,LWORK,INFO);
#endif
  }

  void static gqr(int M, int N, int K, std::complex<float> *A, const int LDA, std::complex<float> *TAU, std::complex<float> *WORK, int LWORK, int& INFO)
  {
#ifdef __NO_QR__
INFO=0;
WORK[0]=0;
#else
	cungqr(M,N,K,A,LDA,TAU,WORK,LWORK,INFO);
#endif
  }

  void static gqr(int M, int N, int K, float *A, const int LDA, float *TAU, float *WORK, int LWORK, int& INFO)
  {
#ifdef __NO_QR__
INFO=0;
WORK[0]=0;
#else
	sorgqr(M,N,K,A,LDA,TAU,WORK,LWORK,INFO);
#endif
  }

  void static glq(int M, int N, int K, std::complex<double> *A, const int LDA, std::complex<double> *TAU, std::complex<double> *WORK, int LWORK, int& INFO)
  {
#ifdef __NO_QR__
INFO=0;
WORK[0]=0;
#else
	zunglq(M,N,K,A,LDA,TAU,WORK,LWORK,INFO);
#endif
  }

  void static glq(int M, int N, int K, double *A, const int LDA, double *TAU, double *WORK, int LWORK, int& INFO)
  {
#ifdef __NO_QR__
INFO=0;
WORK[0]=0;
#else
	dorglq(M,N,K,A,LDA,TAU,WORK,LWORK,INFO);
#endif
  }

  void static glq(int M, int N, int K, std::complex<float> *A, const int LDA, std::complex<float> *TAU, std::complex<float> *WORK, int LWORK, int& INFO)
  {
#ifdef __NO_QR__
INFO=0;
WORK[0]=0;
#else
	cunglq(M,N,K,A,LDA,TAU,WORK,LWORK,INFO);
#endif
  }
    
  void static glq(int M, int N, int K, float *A, const int LDA, float *TAU, float *WORK, int const LWORK, int& INFO){ 
#ifdef __NO_QR__
INFO=0;
WORK[0]=0;
#else
	sorglq(M,N,K,A,LDA,TAU,WORK,LWORK,INFO);
#endif
  }

}

#endif // OHMMS_BLAS_H