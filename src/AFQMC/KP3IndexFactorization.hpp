//////////////////////////////////////////////////////////////////////
// This file is distributed under the University of Illinois/NCSA Open Source
// License.  See LICENSE file in top directory for details.
//
// Copyright (c) 2016 Jeongnim Kim and QMCPACK developers.
//
// File developed by:
// Miguel A. Morales, moralessilva2@llnl.gov 
//    Lawrence Livermore National Laboratory 
//
// File created by:
// Miguel A. Morales, moralessilva2@llnl.gov 
//    Lawrence Livermore National Laboratory 
////////////////////////////////////////////////////////////////////////////////

#ifndef QMCPLUSPLUS_AFQMC_HAMILTONIANOPERATIONS_KP3INDEXFACTORIZATION_HPP
#define QMCPLUSPLUS_AFQMC_HAMILTONIANOPERATIONS_KP3INDEXFACTORIZATION_HPP

#include <vector>
#include <type_traits>
//#include<mutex>
#include <random>

#include "Configuration.h"
#include "multi/array.hpp"
#include "multi/array_ref.hpp"
#include "Numerics/ma_operations.hpp"

#include "Utilities/type_conversion.hpp"
#include "Kernels/ajw_to_waj.cuh"
#include "Kernels/dot_wabn_wban.cuh"

namespace qmcplusplus
{

namespace afqmc
{

template<class Alloc, class Alloc_shared> //, class shm_mutex>
class KP3IndexFactorization
{

//  using shared_mutex = shm_mutex;

  // allocators
  using Allocator = Alloc;
  using Allocator_shared = Alloc_shared;
  using SpAllocator = typename Allocator::template rebind<SPComplexType>::other;
  using SpAllocator_shared = typename Allocator_shared::template rebind<SPComplexType>::other;
  using IAllocator_shared = typename Allocator_shared::template rebind<int>::other;

  // type defs
  using pointer = typename Allocator::pointer;
  using const_pointer = typename Allocator::const_pointer;
  using sp_pointer = typename SpAllocator::pointer;
  using const_sp_pointer = typename SpAllocator::const_pointer;
  using pointer_shared = typename Allocator_shared::pointer;
  using const_pointer_shared = typename Allocator_shared::const_pointer;
  using sp_pointer_shared = typename SpAllocator_shared::pointer;
  using const_sp_pointer_shared = typename SpAllocator_shared::const_pointer;

  using CVector = ComplexVector<Allocator>;  
  using IMatrix = IntegerMatrix<Allocator>;  
  using CMatrix = ComplexMatrix<Allocator>;  
  using C3Tensor = boost::multi::array<ComplexType,3,Allocator>;

  using SpVector = SPComplexVector<SpAllocator>;  
  using SpMatrix = SPComplexMatrix<SpAllocator>;  
  using Sp3Tensor = boost::multi::array<SPComplexType,3,SpAllocator>;

  using CMatrix_cref = boost::multi::const_array_ref<ComplexType,2,const_pointer>;
  using CVector_ref = ComplexVector_ref<pointer>;
  using CMatrix_ref = ComplexMatrix_ref<pointer>;
  using C3Tensor_cref = boost::multi::const_array_ref<ComplexType,3,const_pointer>;

  using SpMatrix_cref = boost::multi::const_array_ref<SPComplexType,2,sp_pointer>;
  using SpVector_ref = SPComplexVector_ref<sp_pointer>;
  using SpMatrix_ref = SPComplexMatrix_ref<sp_pointer>;
  using Sp3Tensor_ref = SPComplex3Tensor_ref<sp_pointer>;
  using Sp4Tensor_ref = SPComplexArray_ref<4,sp_pointer>;

  using shmCVector = ComplexVector<Allocator_shared>;  
  using shmCMatrix = ComplexMatrix<Allocator_shared>;  
  using shmIMatrix = IntegerMatrix<IAllocator_shared>;  
  using shmC3Tensor = Complex3Tensor<Allocator_shared>;  

  using shmSpVector = SPComplexVector<SpAllocator_shared>;  
  using shmSpMatrix = SPComplexMatrix<SpAllocator_shared>;  
  using shmSp3Tensor = SPComplex3Tensor<SpAllocator_shared>;  

//  using communicator = boost::mpi3::shared_communicator;

  using this_t = KP3IndexFactorization<Alloc,Alloc_shared>; //,shm_mutex>;

  public:

    //KP3IndexFactorization(communicator& c_,
    KP3IndexFactorization(
                 WALKER_TYPES type,
                 std::vector<int>&& nopk_,
                 std::vector<int>&& ncholpQ_,
                 std::vector<int>&& kminus_,
//                 shmIMatrix&& nelpk_,
//                 shmIMatrix&& QKToK2_,   
                 boost::multi::array<int,2>&& nelpk_,
                 boost::multi::array<int,2>&& QKToK2_,   
                 shmC3Tensor&& hij_,       
                 shmCMatrix&& h1, 
                 std::vector<shmSpMatrix>&& vik, 
                 std::vector<shmSpMatrix>&& vak, 
                 shmC3Tensor&& vn0_, 
                 std::vector<RealType>&& gQ_,
                 int nsampleQ_,
                 ValueType e0_,
                 Allocator alloc_,
                 Allocator_shared alloc_shared_,
                 int gncv): 
//        comm(std::addressof(c_)),
        allocator_(alloc_),
        sp_allocator_(alloc_),
        allocator_shared_(alloc_shared_),
        sp_allocator_shared_(alloc_shared_),
        iallocator_shared_(alloc_shared_),
        walker_type(type),
        global_nCV(gncv),
        E0(e0_),
        H1(std::move(hij_)),
        haj(std::move(h1)),
        nopk(std::move(nopk_)),
        ncholpQ(std::move(ncholpQ_)),
        kminus(std::move(kminus_)),
        nelpk(std::move(nelpk_)),
        QKToK2(std::move(QKToK2_)),
        LQKikn(std::move(vik)),
        LQKank(std::move(vak)),
        vn0(std::move(vn0_)),
        gQ(std::move(gQ_)),
        Qwn({1,1}),
        generator(),
        distribution(gQ.begin(),gQ.end()), 
        nsampleQ(nsampleQ_),
        SM_TMats({1,1},sp_allocator_shared_),
        TMats({1,1},sp_allocator_),
        EQ(nopk.size()+2)
//        mutex(0)
    {
      local_nCV = std::accumulate(ncholpQ.begin(),ncholpQ.end(),0); 
//      mutex.reserve(ncholpQ.size());
//      for(int nQ=0; nQ<ncholpQ.size(); nQ++)
//          mutex.emplace_back(std::make_unique<shared_mutex>(*comm));
      std::fill_n(EQ.data(),EQ.size(),0);
      int nkpts = nopk.size(); 
      Q0=-1;  // if K=(0,0,0) exists, store index here
      for(int Q=0; Q<nkpts; Q++) {
        if(kminus[Q]==Q) {
          bool found=true;
          for(int KI=0; KI<nkpts; KI++)
            if(KI != QKToK2[Q][KI]) {
              found = false;
              break;
            }
          if(found) {
            Q0=Q;
            break;
          }
        }
      }
      //comm->barrier();
    }

    ~KP3IndexFactorization() {}
    
    KP3IndexFactorization(const KP3IndexFactorization& other) = delete;
    KP3IndexFactorization& operator=(const KP3IndexFactorization& other) = delete;
    KP3IndexFactorization(KP3IndexFactorization&& other) = default; 
    KP3IndexFactorization& operator=(KP3IndexFactorization&& other) = default;

    double getE0() const { return real(E0); }

    template<class Mat, class MatB>
    RealType energy(Mat&& E, MatB const& G, int k=0, bool addH1=true, bool addEJ=true, bool addEXX=true) {
      MatB* Kr(nullptr);
      MatB* Kl(nullptr);
      return energy(E,G,k,Kl,Kr,addH1,addEJ,addEXX);
    }

    // KEleft and KEright must be in shared memory for this to work correctly  
    template<class Mat, class MatB, class MatC, class MatD>
    RealType energy(Mat&& E, MatB const& Gc, int nd, MatC* KEleft, MatD* KEright, bool addH1=true, bool addEJ=true, bool addEXX=true) {
      if(nsampleQ > 0)
        return energy_sampleQ(E,Gc,nd,KEleft,KEright,addH1,addEJ,addEXX);
      else
        return energy_exact(E,Gc,nd,KEleft,KEright,addH1,addEJ,addEXX);
    }

    // KEleft and KEright must be in shared memory for this to work correctly  
    template<class Mat, class MatB, class MatC, class MatD>
    RealType energy_exact(Mat&& E, MatB const& Gc, int nd, MatC* KEleft, MatD* KEright, bool addH1=true, bool addEJ=true, bool addEXX=true) {

      using std::fill_n;
      int nkpts = nopk.size(); 
      assert(E.shape()[1]>=3);
      assert(nd >= 0 && nd < nelpk.size());  

      int nwalk = Gc.shape()[1];
      int nspin = (walker_type==COLLINEAR?2:1);
      int nmo_tot = std::accumulate(nopk.begin(),nopk.end(),0);
      int nmo_max = *std::max_element(nopk.begin(),nopk.end());
      int nocca_tot = std::accumulate(nelpk[nd].begin(),nelpk[nd].begin()+nkpts,0);
      int nocca_max = *std::max_element(nelpk[nd].begin(),nelpk[nd].begin()+nkpts);
      int nchol_max = *std::max_element(ncholpQ.begin(),ncholpQ.end());
      int nchol_tot = std::accumulate(ncholpQ.begin(),ncholpQ.end(),0);
      int noccb_tot = 0;
      if(walker_type==COLLINEAR) noccb_tot = std::accumulate(nelpk[nd].begin()+nkpts,
                                      nelpk[nd].begin()+2*nkpts,0);
      int getKr = KEright!=nullptr;
      int getKl = KEleft!=nullptr;
      if(E.shape()[0] != nwalk || E.shape()[1] < 3)
        APP_ABORT(" Error in AFQMC/HamiltonianOperations/sparse_matrix_energy::calculate_energy(). Incorrect matrix dimensions \n");

      size_t mem_needs(nwalk*nkpts*nkpts*nspin*nocca_max*nmo_max);
      size_t cnt(0);  
      if(addEJ) { 
        if(not getKr) mem_needs += nwalk*local_nCV;
        if(not getKl) mem_needs += nwalk*local_nCV;
      }
      set_shm_buffer(mem_needs);

      // messy
      sp_pointer Krptr, Klptr; 
      size_t Knr=0, Knc=0;
      if(addEJ) {
        Knr=nwalk;
        Knc=local_nCV;
        cnt=0;
        if(getKr) {
          assert(KEright->shape()[0] == nwalk && KEright->shape()[1] == local_nCV); 
          assert(KEright->strides()[0] == KEright->shape()[1]);
          Krptr = KEright->origin(); 
        } else {
          Krptr = SM_TMats.origin(); 
          cnt += nwalk*local_nCV;
        }
        if(getKl) {
          assert(KEleft->shape()[0] == nwalk && KEleft->shape()[1] == local_nCV); 
          assert(KEleft->strides()[0] == KEleft->shape()[1]);
          Klptr = KEleft->origin();
        } else {
          Klptr = SM_TMats.origin()+cnt; 
          cnt += nwalk*local_nCV;
        }
        //if(comm->root()) std::fill_n(Krptr,Knr*Knc,SPComplexType(0.0));
        //if(comm->root()) std::fill_n(Klptr,Knr*Knc,SPComplexType(0.0));
        fill_n(Krptr,Knr*Knc,SPComplexType(0.0));
        fill_n(Klptr,Knr*Knc,SPComplexType(0.0));
      } else if(getKr or getKl) {
        APP_ABORT(" Error: Kr and/or Kl can only be calculated with addEJ=true.\n");
      }   
      SpMatrix_ref Kl(Klptr,{Knr,Knc});
      SpMatrix_ref Kr(Krptr,{Knr,Knc});

      for(int n=0; n<nwalk; n++) 
        fill_n(E[n].origin(),3,ComplexType(0.));

      assert(Gc.num_elements() == nwalk*(nocca_tot+noccb_tot)*nmo_tot);
      C3Tensor_cref G3Da(Gc.origin(),{nocca_tot,nmo_tot,nwalk} );
      C3Tensor_cref G3Db(Gc.origin()+G3Da.num_elements()*(nspin-1),{noccb_tot,nmo_tot,nwalk} );


      Sp4Tensor_ref GKK(SM_TMats.origin()+cnt,
                        {nspin,nkpts,nkpts,nwalk*nmo_max*nocca_max});
      GKaKjw_to_GKKwaj(nd,Gc,GKK,nocca_tot,noccb_tot,nmo_tot,nmo_max*nocca_max);
      //comm->barrier();

      // one-body contribution
      // haj[ndet*nkpts][nocc*nmo]
      // not parallelized for now, since it would require customization of Wfn 
      if(addH1) {
        // must use Gc since GKK is is SP
        int na=0, nk=0, nb=0;
        for(int K=0; K<nkpts; ++K) {
#if defined(MIXED_PRECISION) 
          boost::multi::array_ref<ComplexType,2> haj_K(std::addressof(*haj[nd*nkpts+K].origin()),
                                                      {nelpk[nd][K],nopk[K]}); 
          for(int a=0; a<nelpk[nd][K]; ++a) 
            ma::product(ComplexType(1.),ma::T(G3Da[na+a].sliced(nk,nk+nopk[K])),haj_K[a],
                        ComplexType(1.),E({0,nwalk},0));
          na+=nelpk[nd][K];
          if(walker_type==COLLINEAR) {
            boost::multi::array_ref<ComplexType,2> haj_Kb(haj_K.origin()+haj_K.num_elements(),
                                                          {nelpk[nd][nkpts+K],nopk[K]});
            for(int b=0; b<nelpk[nd][nkpts+K]; ++b) 
              ma::product(ComplexType(1.),ma::T(G3Db[nb+b].sliced(nk,nk+nopk[K])),haj_Kb[b],
                        ComplexType(1.),E({0,nwalk},0));
            nb+=nelpk[nd][nkpts+K];
          }  
          nk+=nopk[K];  
#else
          nk = nopk[K];
          {
            na = nelpk[nd][K];
            CVector_ref haj_K(haj[nd*nkpts+K].origin(),{na*nk});
            SpMatrix_ref Gaj(GKK[0][K][K].origin(),{nwalk,na*nk});
            ma::product(ComplexType(1.),Gaj,haj_K,ComplexType(1.),E({0,nwalk},0));
          }
          if(walker_type==COLLINEAR) {
            na = nelpk[nd][nkpts+K];
            CVector_ref haj_K(haj[nd*nkpts+K].origin()+nelpk[nd][K]*nk,{na*nk});
            SpMatrix_ref Gaj(GKK[1][K][K].origin(),{nwalk,na*nk});
            ma::product(ComplexType(1.),Gaj,haj_K,ComplexType(1.),E({0,nwalk},0));
          }  
#endif
        }
      }

      // move calculation of H1 here	
      // NOTE: For CLOSED/NONCOLLINEAR, can do all walkers simultaneously to improve perf. of GEMM
      //       Not sure how to do it for COLLINEAR.
      if(addEXX) {  
        size_t local_memory_needs = 2*nwalk*nocca_max*nocca_max*nchol_max + 2*nchol_max*nwalk; 
        if(TMats.num_elements() < local_memory_needs) TMats.reextent({local_memory_needs,1});
        cnt=0; 
        SpMatrix_ref Kr_local(TMats.origin(),{nwalk,nchol_max}); 
        cnt+=Kr_local.num_elements();
        SpMatrix_ref Kl_local(TMats.origin()+cnt,{nwalk,nchol_max}); 
        cnt+=Kl_local.num_elements();
        fill_n(Kr_local.origin(),Kr_local.num_elements(),SPComplexType(0.0));
        fill_n(Kl_local.origin(),Kl_local.num_elements(),SPComplexType(0.0));
        RealType scl = (walker_type==CLOSED?2.0:1.0);
        size_t nqk=1;  
        for(int Q=0; Q<nkpts; ++Q) {
          bool haveKE=false;
          for(int Ka=0; Ka<nkpts; ++Ka) {
            int K0 = ((Q==Q0)?0:Ka);
            for(int Kb=K0; Kb<nkpts; ++Kb) {
              //if((nqk++)%comm->size() == comm->rank()) { 
              { 
                int nchol = ncholpQ[Q];
                int Qm = kminus[Q];
                int Qm_ = (Q==Q0?nkpts:Qm);
                int Kl = QKToK2[Qm][Kb];
                int Kk = QKToK2[Q][Ka];
                int nl = nopk[Kl];
                int nb = nelpk[nd][Kb];
                int na = nelpk[nd][Ka];
                int nk = nopk[Kk];

                SpMatrix_ref Gwal(GKK[0][Ka][Kl].origin(),{nwalk*na,nl});
                SpMatrix_ref Gwbk(GKK[0][Kb][Kk].origin(),{nwalk*nb,nk});
                SpMatrix_ref Lank(LQKank[nd*nspin*(nkpts+1)+Q][Ka].origin(),
                                                 {na*nchol,nk});
                SpMatrix_ref Lbnl(LQKank[nd*nspin*(nkpts+1)+Qm_][Kb].origin(),
                                                 {nb*nchol,nl});

                SpMatrix_ref Twban(TMats.origin()+cnt,{nwalk*nb,na*nchol});
                Sp4Tensor_ref T4Dwban(TMats.origin()+cnt,{nwalk,nb,na,nchol});
                SpMatrix_ref Twabn(Twban.origin()+Twban.num_elements(),{nwalk*na,nb*nchol});
                Sp4Tensor_ref T4Dwabn(Twban.origin()+Twban.num_elements(),{nwalk,na,nb,nchol});

                ma::product(Gwal,ma::T(Lbnl),Twabn);
                ma::product(Gwbk,ma::T(Lank),Twban);

                if(Q==Q0 || Ka==Kb)
                  kernels::dot_wabn_wban(nwalk,na,nb,nchol,ComplexType(-scl*0.5),
                                         to_address(Twabn.origin()),   
                                         to_address(Twban.origin()),   
                                         to_address(E[0].origin())+1,E.stride(0));
                else
                  kernels::dot_wabn_wban(nwalk,na,nb,nchol,ComplexType(-scl),
                                         to_address(Twabn.origin()),   
                                         to_address(Twban.origin()),   
                                         to_address(E[0].origin())+1,E.stride(0));

/*                
                for(int n=0; n<nwalk; ++n) {
                  ComplexType E_(0.0);
                  for(int a=0; a<na; ++a)
                    for(int b=0; b<nb; ++b)
                      E_ += ma::dot(T4Dwabn[n][a][b],T4Dwban[n][b][a]);
                  if(Q==Q0 || Ka==Kb)
                    E[n][1] -= scl*0.5*E_;
                  else
                    E[n][1] -= 2.0*scl*0.5*E_;
                }
*/

                // needs batched
                if(addEJ && Ka==Kb) {
                  haveKE=true;
                  for(int n=0; n<nwalk; ++n) 
                    for(int a=0; a<na; ++a) { 
                      ma::axpy(SPComplexType(1.0),T4Dwban[n][a][a],Kl_local[n].sliced(0,nchol));
                      ma::axpy(SPComplexType(1.0),T4Dwabn[n][a][a],Kr_local[n].sliced(0,nchol));
                    }
                }
              } // if

              if(walker_type==COLLINEAR) {

                //if((nqk++)%comm->size() == comm->rank()) { 
                {
                  int nchol = ncholpQ[Q];
                  int Qm = kminus[Q];
                  int Qm_ = (Q==Q0?nkpts:Qm);
                  int Kl = QKToK2[Qm][Kb];
                  int Kk = QKToK2[Q][Ka];
                  int nl = nopk[Kl];
                  int nb = nelpk[nd][nkpts+Kb];
                  int na = nelpk[nd][nkpts+Ka];
                  int nk = nopk[Kk];

                  SpMatrix_ref Gwal(GKK[1][Ka][Kl].origin(),{nwalk*na,nl});
                  SpMatrix_ref Gwbk(GKK[1][Kb][Kk].origin(),{nwalk*nb,nk});
                  SpMatrix_ref Lank(LQKank[nd*nspin*(nkpts+1)+nkpts+1+Q][Ka].origin(),
                                                 {na*nchol,nk});
                  SpMatrix_ref Lbnl(LQKank[nd*nspin*(nkpts+1)+nkpts+1+Qm_][Kb].origin(),
                                                 {nb*nchol,nl});

                  SpMatrix_ref Twban(TMats.origin()+cnt,{nwalk*nb,na*nchol});
                  Sp4Tensor_ref T4Dwban(TMats.origin()+cnt,{nwalk,nb,na,nchol});
                  SpMatrix_ref Twabn(Twban.origin()+Twban.num_elements(),{nwalk*na,nb*nchol});
                  Sp4Tensor_ref T4Dwabn(Twban.origin()+Twban.num_elements(),{nwalk,na,nb,nchol});

                  ma::product(Gwal,ma::T(Lbnl),Twabn);
                  ma::product(Gwbk,ma::T(Lank),Twban);
  
                if(Q==Q0 || Ka==Kb)
                  kernels::dot_wabn_wban(nwalk,na,nb,nchol,ComplexType(-scl*0.5),
                                         to_address(Twabn.origin()),
                                         to_address(Twban.origin()),
                                         to_address(E[0].origin())+1,E.stride(0));
                else
                  kernels::dot_wabn_wban(nwalk,na,nb,nchol,ComplexType(-scl),
                                         to_address(Twabn.origin()),
                                         to_address(Twban.origin()),
                                         to_address(E[0].origin())+1,E.stride(0));
/*
                  for(int n=0; n<nwalk; ++n) {
                    ComplexType E_(0.0);
                    for(int a=0; a<na; ++a)
                      for(int b=0; b<nb; ++b)
                        E_ += ma::dot(T4Dwabn[n][a][b],T4Dwban[n][b][a]);
                    if(Ka==Kb)
                      E[n][1] -= scl*0.5*E_;
                    else
                     E[n][1] -= 2.0*scl*0.5*E_;
                  } 
*/

                  if(addEJ && Ka==Kb) {
                    haveKE=true;
                    for(int n=0; n<nwalk; ++n) 
                      for(int a=0; a<na; ++a) {
                        ma::axpy(SPComplexType(1.0),T4Dwban[n][a][a],Kl_local[n].sliced(0,nchol));
                        ma::axpy(SPComplexType(1.0),T4Dwabn[n][a][a],Kr_local[n].sliced(0,nchol));
                      }
                  }

                } // if
              } // COLLINEAR
            } // Kb
          } // Ka
          if(addEJ && haveKE) {
//            std::lock_guard<shared_mutex> guard(*mutex[Q]);
            int nc0 = std::accumulate(ncholpQ.begin(),ncholpQ.begin()+Q,0);  
            for(int n=0; n<nwalk; n++) {
              ma::axpy(SPComplexType(1.0),Kr_local[n].sliced(0,ncholpQ[Q]),
                                        Kr[n].sliced(nc0,nc0+ncholpQ[Q])); 
              ma::axpy(SPComplexType(1.0),Kl_local[n].sliced(0,ncholpQ[Q]),
                                        Kl[n].sliced(nc0,nc0+ncholpQ[Q])); 
            }
          } // to release the lock
          if(addEJ && haveKE) { 
            fill_n(Kr_local.origin(),Kr_local.num_elements(),SPComplexType(0.0));
            fill_n(Kl_local.origin(),Kl_local.num_elements(),SPComplexType(0.0));
          }  
        } // Q
      }  

      if(addEJ) {
        if(not addEXX) {
          // calculate Kr
          APP_ABORT(" Error: Finish addEJ and not addEXX");
        }
        //comm->barrier();
        size_t nqk=0;  
        RealType scl = (walker_type==CLOSED?2.0:1.0);
        // needs batched
        for(int n=0; n<nwalk; ++n) {
          ma::adotpby(SPComplexType(0.5*scl*scl),Kl[n],Kr[n],
                      ComplexType(0.0),E[n].origin()+2);
        }
      }
      ComplexType Eav = ma::sum(E(E.extension(0),{0,3}));
      using std::real;
      return real(Eav)/nwalk + ((addH1)?real(E0):0.0);
    }

    // KEleft and KEright must be in shared memory for this to work correctly  
    template<class Mat, class MatB, class MatC, class MatD>
    RealType energy_sampleQ(Mat&& E, MatB const& Gc, int nd, MatC* KEleft, MatD* KEright, bool addH1=true, bool addEJ=true, bool addEXX=true) {

      int nkpts = nopk.size(); 
      assert(E.shape()[1]>=3);
      assert(nd >= 0 && nd < nelpk.size());  

      int nwalk = Gc.shape()[1];
      int nspin = (walker_type==COLLINEAR?2:1);
      int nmo_tot = std::accumulate(nopk.begin(),nopk.end(),0);
      int nmo_max = *std::max_element(nopk.begin(),nopk.end());
      int nocca_tot = std::accumulate(nelpk[nd].begin(),nelpk[nd].begin()+nkpts,0);
      int nocca_max = *std::max_element(nelpk[nd].begin(),nelpk[nd].begin()+nkpts);
      int nchol_max = *std::max_element(ncholpQ.begin(),ncholpQ.end());
      int noccb_tot = 0;
      if(walker_type==COLLINEAR) noccb_tot = std::accumulate(nelpk[nd].begin()+nkpts,
                                      nelpk[nd].begin()+2*nkpts,0);
      int getKr = KEright!=nullptr;
      int getKl = KEleft!=nullptr;
      if(E.shape()[0] != nwalk || E.shape()[1] < 3)
        APP_ABORT(" Error in AFQMC/HamiltonianOperations/sparse_matrix_energy::calculate_energy(). Incorrect matrix dimensions \n");

      size_t mem_needs(nwalk*nkpts*nkpts*nspin*nocca_max*nmo_max);
      size_t cnt(0);  
      if(addEJ) { 
        if(not getKr) mem_needs += nwalk*local_nCV;
        if(not getKl) mem_needs += nwalk*local_nCV;
      }
      set_shm_buffer(mem_needs);

      // messy
      SPComplexType *Krptr, *Klptr; 
      size_t Knr=0, Knc=0;
      if(addEJ) {
        Knr=nwalk;
        Knc=local_nCV;
        cnt=0;
        if(getKr) {
          assert(KEright->shape()[0] == nwalk && KEright->shape()[1] == local_nCV); 
          assert(KEright->strides()[0] == KEright->shape()[1]);
          Krptr = std::addressof(*KEright->origin()); 
        } else {
          Krptr = std::addressof(*SM_TMats.origin()); 
          cnt += nwalk*local_nCV;
        }
        if(getKl) {
          assert(KEleft->shape()[0] == nwalk && KEleft->shape()[1] == local_nCV); 
          assert(KEleft->strides()[0] == KEleft->shape()[1]);
          Klptr = std::addressof(*KEleft->origin());
        } else {
          Klptr = std::addressof(*SM_TMats.origin())+cnt; 
          cnt += nwalk*local_nCV;
        }
        //if(comm->root()) std::fill_n(Krptr,Knr*Knc,SPComplexType(0.0));
        //if(comm->root()) std::fill_n(Klptr,Knr*Knc,SPComplexType(0.0));
        std::fill_n(Krptr,Knr*Knc,SPComplexType(0.0));
        std::fill_n(Klptr,Knr*Knc,SPComplexType(0.0));
      } else if(getKr or getKl) {
        APP_ABORT(" Error: Kr and/or Kl can only be calculated with addEJ=true.\n");
      }   
      SpMatrix_ref Kl(Klptr,{Knr,Knc});
      SpMatrix_ref Kr(Krptr,{Knr,Knc});

      for(int n=0; n<nwalk; n++) 
        std::fill_n(E[n].origin(),3,ComplexType(0.));

      assert(Gc.num_elements() == nwalk*(nocca_tot+noccb_tot)*nmo_tot);
      boost::multi::const_array_ref<ComplexType,3> G3Da(std::addressof(*Gc.origin()),
                                                        {nocca_tot,nmo_tot,nwalk} ); 
      boost::multi::const_array_ref<ComplexType,3> G3Db(std::addressof(*Gc.origin())+
                                                        G3Da.num_elements()*(nspin-1),
                                                        {noccb_tot,nmo_tot,nwalk} ); 

      Sp4Tensor_ref GKK(std::addressof(*SM_TMats.origin())+cnt,
                        {nspin,nkpts,nkpts,nwalk*nmo_max*nocca_max});
      cnt+=GKK.num_elements();
      GKaKjw_to_GKKwaj(nd,Gc,GKK,nocca_tot,noccb_tot,nmo_tot,nmo_max*nocca_max);
      //comm->barrier();

      // one-body contribution
      // haj[ndet*nkpts][nocc*nmo]
      // not parallelized for now, since it would require customization of Wfn 
      if(addH1) {
        // must use Gc since GKK is is SP
        int na=0, nk=0, nb=0;
        for(int n=0; n<nwalk; n++)
          E[n][0] = E0;  
        for(int K=0; K<nkpts; ++K) {
#if defined(MIXED_PRECISION)
          boost::multi::array_ref<ComplexType,2> haj_K(std::addressof(*haj[nd*nkpts+K].origin()),
                                                      {nelpk[nd][K],nopk[K]}); 
          for(int a=0; a<nelpk[nd][K]; ++a) 
            ma::product(ComplexType(1.),ma::T(G3Da[na+a].sliced(nk,nk+nopk[K])),haj_K[a],
                        ComplexType(1.),E({0,nwalk},0));
          na+=nelpk[nd][K];
          if(walker_type==COLLINEAR) {
            boost::multi::array_ref<ComplexType,2> haj_Kb(haj_K.origin()+haj_K.num_elements(),
                                                          {nelpk[nd][nkpts+K],nopk[K]});
            for(int b=0; b<nelpk[nd][nkpts+K]; ++b) 
              ma::product(ComplexType(1.),ma::T(G3Db[nb+b].sliced(nk,nk+nopk[K])),haj_Kb[b],
                        ComplexType(1.),E({0,nwalk},0));
            nb+=nelpk[nd][nkpts+K];
          }  
          nk+=nopk[K];  
#else
          nk = nopk[K];
          {
            na = nelpk[nd][K];
            CVector_ref haj_K(std::addressof(*haj[nd*nkpts+K].origin()),
                              {na*nk}); 
            SpMatrix_ref Gaj(std::addressof(*GKK[0][K][K].origin()),{nwalk,na*nk});
            ma::product(ComplexType(1.),Gaj,haj_K,ComplexType(1.),E({0,nwalk},0));
          }
          if(walker_type==COLLINEAR) {
            na = nelpk[nd][nkpts+K];
            CVector_ref haj_K(std::addressof(*haj[nd*nkpts+K].origin())+nelpk[nd][K]*nk,
                              {na*nk});
            SpMatrix_ref Gaj(std::addressof(*GKK[1][K][K].origin()),{nwalk,na*nk});
            ma::product(ComplexType(1.),Gaj,haj_K,ComplexType(1.),E({0,nwalk},0));
          }  
#endif
        }
      }

      // move calculation of H1 here	
      // NOTE: For CLOSED/NONCOLLINEAR, can do all walkers simultaneously to improve perf. of GEMM
      //       Not sure how to do it for COLLINEAR.
      if(addEXX) {  

        if(Qwn.shape()[0] != nwalk || Qwn.shape()[1] != nsampleQ)
          Qwn.reextent({nwalk,nsampleQ});
        //comm->barrier();
        //if(comm->root()) 
        {
          for(int n=0; n<nwalk; ++n) 
            for(int nQ=0; nQ<nsampleQ; ++nQ) {
              Qwn[n][nQ] = distribution(generator);
/*
              RealType drand = distribution(generator);
              RealType s(0.0);
              bool found=false;
              for(int Q=0; Q<nkpts; Q++) {
                s += gQ[Q];
                if( drand < s ) {
                  Qwn[n][nQ] = Q;
                  found=true;
                  break;
                }
              } 
              if(not found) 
                APP_ABORT(" Error: sampleQ Qwn. \n");  
*/
            }
        }
        //comm->barrier();

        size_t local_memory_needs = 2*nocca_max*nocca_max*nchol_max; 
        if(TMats.num_elements() < local_memory_needs) TMats.reextent({local_memory_needs,1});
        size_t local_cnt=0; 
        RealType scl = (walker_type==CLOSED?2.0:1.0);
        size_t nqk=1;  
        for(int n=0; n<nwalk; ++n) {
          for(int nQ=0; nQ<nsampleQ; ++nQ) {
            int Q = Qwn[n][nQ];
            for(int Ka=0; Ka<nkpts; ++Ka) {
              for(int Kb=0; Kb<nkpts; ++Kb) {
                //if((nqk++)%comm->size() == comm->rank()) 
                { 
                  int nchol = ncholpQ[Q];
                  int Qm = kminus[Q];
                  int Qm_ = (Q==Q0?nkpts:Qm);
                  int Kl = QKToK2[Qm][Kb];
                  int Kk = QKToK2[Q][Ka];
                  int nl = nopk[Kl];
                  int nb = nelpk[nd][Kb];
                  int na = nelpk[nd][Ka];
                  int nk = nopk[Kk];

                  SpMatrix_ref Gal(GKK[0][Ka][Kl].origin()+n*na*nl,{na,nl});
                  SpMatrix_ref Gbk(GKK[0][Kb][Kk].origin()+n*nb*nk,{nb,nk});
                  SpMatrix_ref Lank(std::addressof(*LQKank[nd*nspin*(nkpts+1)+Q][Ka].origin()),
                                                 {na*nchol,nk});
                  SpMatrix_ref Lbnl(std::addressof(*LQKank[nd*nspin*(nkpts+1)+Qm_][Kb].origin()),
                                                 {nb*nchol,nl});

                  SpMatrix_ref Tban(TMats.origin()+local_cnt,{nb,na*nchol});
                  Sp3Tensor_ref T3Dban(TMats.origin()+local_cnt,{nb,na,nchol});
                  SpMatrix_ref Tabn(Tban.origin()+Tban.num_elements(),{na,nb*nchol});
                  Sp3Tensor_ref T3Dabn(Tban.origin()+Tban.num_elements(),{na,nb,nchol});

                  ma::product(Gal,ma::T(Lbnl),Tabn);
                  ma::product(Gbk,ma::T(Lank),Tban);

                  ComplexType E_(0.0);
                  for(int a=0; a<na; ++a)
                    for(int b=0; b<nb; ++b)
                      E_ += ma::dot(T3Dabn[a][b],T3Dban[b][a]);
                  E[n][1] -= scl*0.5*E_/gQ[Q]/double(nsampleQ);

                } // if

                if(walker_type==COLLINEAR) {

                  //if((nqk++)%comm->size() == comm->rank()) 
                  { 
                    int nchol = ncholpQ[Q];
                    int Qm = kminus[Q];
                    int Qm_ = (Q==Q0?nkpts:Qm);
                    int Kl = QKToK2[Qm][Kb];
                    int Kk = QKToK2[Q][Ka];
                    int nl = nopk[Kl];
                    int nb = nelpk[nd][nkpts+Kb];
                    int na = nelpk[nd][nkpts+Ka];
                    int nk = nopk[Kk];

                    SpMatrix_ref Gal(GKK[1][Ka][Kl].origin()+n*na*nl,{na,nl});
                    SpMatrix_ref Gbk(GKK[1][Kb][Kk].origin()+n*nb*nk,{nb,nk});
                    SpMatrix_ref Lank(std::addressof(*LQKank[nd*nspin*(nkpts+1)+nkpts+1+Q][Ka].origin()),
                                                 {na*nchol,nk});
                    SpMatrix_ref Lbnl(std::addressof(*LQKank[nd*nspin*(nkpts+1)+nkpts+1+Qm_][Kb].origin()),
                                                 {nb*nchol,nl});

                    SpMatrix_ref Tban(TMats.origin()+local_cnt,{nb,na*nchol});
                    Sp3Tensor_ref T3Dban(TMats.origin()+local_cnt,{nb,na,nchol});
                    SpMatrix_ref Tabn(Tban.origin()+Tban.num_elements(),{na,nb*nchol});
                    Sp3Tensor_ref T3Dabn(Tban.origin()+Tban.num_elements(),{na,nb,nchol});
  
                    ma::product(Gal,ma::T(Lbnl),Tabn);
                    ma::product(Gbk,ma::T(Lank),Tban);
  
                    ComplexType E_(0.0);
                    for(int a=0; a<na; ++a)
                      for(int b=0; b<nb; ++b)
                        E_ += ma::dot(T3Dabn[a][b],T3Dban[b][a]);
                    E[n][1] -= scl*0.5*E_/gQ[Q]/double(nsampleQ);

                  } // if
                } // COLLINEAR 
              } // Kb 
            } // Ka
          } // nQ
        } // n 
      }  

      if(addEJ) {
        size_t local_memory_needs = 2*nchol_max*nwalk; 
        if(TMats.num_elements() < local_memory_needs) TMats.reextent({local_memory_needs,1});
        cnt=0; 
        SpMatrix_ref Kr_local(TMats.origin(),{nwalk,nchol_max}); 
        cnt+=Kr_local.num_elements();
        SpMatrix_ref Kl_local(TMats.origin()+cnt,{nwalk,nchol_max}); 
        cnt+=Kl_local.num_elements();
        std::fill_n(Kr_local.origin(),Kr_local.num_elements(),SPComplexType(0.0));
        std::fill_n(Kl_local.origin(),Kl_local.num_elements(),SPComplexType(0.0));
        size_t nqk=1;  
        for(int Q=0; Q<nkpts; ++Q) {
          bool haveKE=false;
          for(int Ka=0; Ka<nkpts; ++Ka) {
            //if((nqk++)%comm->size() == comm->rank()) 
            { 
              haveKE=true;
              int nchol = ncholpQ[Q];
              int Qm = kminus[Q];
              int Qm_ = (Q==Q0?nkpts:Qm);
              int Kl = QKToK2[Qm][Ka];
              int Kk = QKToK2[Q][Ka];
              int nl = nopk[Kl];
              int na = nelpk[nd][Ka];
              int nk = nopk[Kk];

              Sp3Tensor_ref Gwal(GKK[0][Ka][Kl].origin(),{nwalk,na,nl});
              Sp3Tensor_ref Gwbk(GKK[0][Ka][Kk].origin(),{nwalk,na,nk});
              Sp3Tensor_ref Lank(std::addressof(*LQKank[nd*nspin*(nkpts+1)+Q][Ka].origin()),
                                                 {na,nchol,nk});
              Sp3Tensor_ref Lbnl(std::addressof(*LQKank[nd*nspin*(nkpts+1)+Qm_][Ka].origin()),
                                                 {na,nchol,nl});

              // Twan = sum_l G[w][a][l] L[a][n][l]
              for(int n=0; n<nwalk; ++n) 
                for(int a=0; a<na; ++a)  
                  ma::product(SPComplexType(1.0),Lbnl[a],Gwal[n][a],
                              SPComplexType(1.0),Kl_local[n]);
              for(int n=0; n<nwalk; ++n) 
                for(int a=0; a<na; ++a)  
                  ma::product(SPComplexType(1.0),Lank[a],Gwbk[n][a],
                              SPComplexType(1.0),Kr_local[n]);
            } // if

            if(walker_type==COLLINEAR) {

              //if((nqk++)%comm->size() == comm->rank()) 
              { 
                haveKE=true;
                int nchol = ncholpQ[Q];
                int Qm = kminus[Q];
                int Qm_ = (Q==Q0?nkpts:Qm);
                int Kl = QKToK2[Qm][Ka];
                int Kk = QKToK2[Q][Ka];
                int nl = nopk[Kl];
                int na = nelpk[nd][nkpts+Ka];
                int nk = nopk[Kk];

                Sp3Tensor_ref Gwal(GKK[1][Ka][Kl].origin(),{nwalk,na,nl});
                Sp3Tensor_ref Gwbk(GKK[1][Ka][Kk].origin(),{nwalk,na,nk});
                Sp3Tensor_ref Lank(std::addressof(*LQKank[nd*nspin*(nkpts+1)+nkpts+1+Q][Ka].origin()),
                                                 {na,nchol,nk});
                Sp3Tensor_ref Lbnl(std::addressof(*LQKank[nd*nspin*(nkpts+1)+nkpts+1+Qm_][Ka].origin()),
                                                 {na,nchol,nl});

                // Twan = sum_l G[w][a][l] L[a][n][l]
                for(int n=0; n<nwalk; ++n)
                  for(int a=0; a<na; ++a)  
                    ma::product(SPComplexType(1.0),Lbnl[a],Gwal[n][a],
                                SPComplexType(1.0),Kl_local[n]);
                for(int n=0; n<nwalk; ++n)
                  for(int a=0; a<na; ++a)  
                    ma::product(SPComplexType(1.0),Lank[a],Gwbk[n][a],
                                SPComplexType(1.0),Kr_local[n]);

              } // if
            } // COLLINEAR
          } // Ka
          if(haveKE) {
//            std::lock_guard<shared_mutex> guard(*mutex[Q]);
            int nc0 = std::accumulate(ncholpQ.begin(),ncholpQ.begin()+Q,0);  
            for(int n=0; n<nwalk; n++) {
              ma::axpy(SPComplexType(1.0),Kr_local[n].sliced(0,ncholpQ[Q]),
                                        Kr[n].sliced(nc0,nc0+ncholpQ[Q])); 
              ma::axpy(SPComplexType(1.0),Kl_local[n].sliced(0,ncholpQ[Q]),
                                        Kl[n].sliced(nc0,nc0+ncholpQ[Q])); 
            }
          } // to release the lock
          if(haveKE) { 
            std::fill_n(Kr_local.origin(),Kr_local.num_elements(),SPComplexType(0.0));
            std::fill_n(Kl_local.origin(),Kl_local.num_elements(),SPComplexType(0.0));
          }  
        } // Q
        //comm->barrier();
        nqk=0;  
        RealType scl = (walker_type==CLOSED?2.0:1.0);
        for(int n=0; n<nwalk; ++n) {
          for(int Q=0; Q<nkpts; ++Q) {      // momentum conservation index   
            //if((nqk++)%comm->size() == comm->rank()) 
            {
              int nc0 = std::accumulate(ncholpQ.begin(),ncholpQ.begin()+Q,0);
              E[n][2] += 0.5*scl*scl*ma::dot(Kl[n]({nc0,nc0+ncholpQ[Q]}),
                                            Kr[n]({nc0,nc0+ncholpQ[Q]}));  
            }
          }
        }
      }
      ComplexType Eav = ma::sum(E(E.extension(0),{0,3}));
      using std::real;
      return real(Eav)/nwalk + ((addH1)?real(E0):0.0);
    }

    template<class... Args>
    void fast_energy(Args&&... args)
    {
      APP_ABORT(" Error: fast_energy not implemented in KP3IndexFactorization. \n"); 
    }

    template<class MatA, class MatB,
             typename = typename std::enable_if_t<(std::decay<MatA>::type::dimensionality==1)>,
             typename = typename std::enable_if_t<(std::decay<MatB>::type::dimensionality==1)>,
             typename = void
            >
    void vHS(MatA& X, MatB&& v, double a=1., double c=0.) {
      using BType = typename std::decay<MatB>::type::element ;
      using AType = typename std::decay<MatA>::type::element ;
      using AAlloc = typename Allocator_shared::template rebind<AType>::other;
      using BAlloc = typename Allocator_shared::template rebind<BType>::other;
      using Aptr = typename AAlloc::pointer;
      using Bptr = typename BAlloc::pointer;
      boost::multi::array_ref<BType,2,Bptr> v_(v.origin(),{v.shape()[0],1});
      boost::multi::array_ref<AType,2,Aptr> X_(X.origin(),{X.shape()[0],1});
      return vHS(X_,v_,a,c);
    }

    template<class MatA, class MatB,
             typename = typename std::enable_if_t<(std::decay<MatA>::type::dimensionality==2)>,
             typename = typename std::enable_if_t<(std::decay<MatB>::type::dimensionality==2)>
            >
    void vHS(MatA& X, MatB&& v, double a=1., double c=0.) {
      int nkpts = nopk.size();
      int nwalk = X.shape()[1];
      assert(v.shape()[0]==nwalk);
      int nspin = (walker_type==COLLINEAR?2:1);
      int nmo_tot = std::accumulate(nopk.begin(),nopk.end(),0);
      int nmo_max = *std::max_element(nopk.begin(),nopk.end());
      int nchol_max = *std::max_element(ncholpQ.begin(),ncholpQ.end());
      assert(X.num_elements() == nwalk*2*local_nCV);
      assert(v.num_elements() == nwalk*nmo_tot*nmo_tot);
      SPComplexType one(1.0,0.0);
      SPComplexType im(0.0,1.0);
      size_t local_memory_needs = nmo_max*nmo_max*nwalk; 
      if(TMats.num_elements() < local_memory_needs) TMats.reextent({local_memory_needs,1});

      Sp3Tensor_ref v3D(v.origin(),{nwalk,nmo_tot,nmo_tot});

      // "rotate" X  
      //  XIJ = 0.5*a*(Xn+ -i*Xn-), XJI = 0.5*a*(Xn+ +i*Xn-)  
      for(int Q=0, nq=0; Q<nkpts; ++Q) { 
        int nc0=0, ncN=ncholpQ[Q];  
        //std::tie(nc0,ncN) = FairDivideBoundary(comm->rank(),ncholpQ[Q],comm->size()); 
        auto Xnp = X[nq+nc0].origin();
        auto Xnm = X[nq+ncholpQ[Q]+nc0].origin();
        for(int n=nc0; n<ncN; ++n) { 
          if(Q != kminus[Q] || Q==Q0) { 
// needs kernel
            for(int nw=0; nw<nwalk; ++nw, ++Xnp, ++Xnm) {
              ComplexType Xnp_ = 0.5*a*((*Xnp) -im*(*Xnm)); 
              *Xnm =  0.5*a*((*Xnp) + im*(*Xnm));
              *Xnp = Xnp_;
            }
          } else {
            // since rho(Q) == rho(-Q) when Q==(-Q) ( and Q!=Q0), contribution from Xnm is zero
            // and contribution from Xnp is 2X
// call scal
            for(int nw=0; nw<nwalk; ++nw, ++Xnp, ++Xnm) 
              *Xnp *= a;
          }  
        }  
        nq+=2*ncholpQ[Q];
      } 
      //comm->barrier();     
      //  then combine Q/(-Q) pieces
      //  X(Q)np = (X(Q)np + X(-Q)nm)
      for(int Q=0, nq=0; Q<nkpts; ++Q) {
        if(Q != kminus[Q]) {
          int Qm = kminus[Q];
          int nqm0 = 2*std::accumulate(ncholpQ.begin(),ncholpQ.begin()+Qm,0);
          int nc0=0, ncN=ncholpQ[Q];
          //std::tie(nc0,ncN) = FairDivideBoundary(comm->rank(),ncholpQ[Q],comm->size());
          auto Xnp = X[nq+nc0].origin();
          auto Xnm = X[nqm0+ncholpQ[Q]+nc0].origin();
// call axpy 
          for(int n=nc0; n<ncN; ++n) 
            for(int nw=0; nw<nwalk; ++nw, ++Xnp, ++Xnm) 
              *Xnp =  ((*Xnp) + (*Xnm));
        }
        nq+=2*ncholpQ[Q];
      }
      {
        // assuming contiguous
        ma::scal(c,v);
      }
      //comm->barrier();     
        
      using ma::T;  
      using ma::H;  
      size_t nqk=0;  
      for(int Q=0, nc0=0; Q<nkpts; ++Q) {      // momentum conservation index   
        for(int K=0; K<nkpts; ++K) {        // K is the index of the kpoint pair of (i,k)
          //if((nqk++)%comm->size() == comm->rank()) 
          {
            int nchol = ncholpQ[Q];
            int QK = QKToK2[Q][K];
            int ni = nopk[K];
            int ni0 = std::accumulate(nopk.begin(),nopk.begin()+K,0);
            int nk = nopk[QK];
            int nk0 = std::accumulate(nopk.begin(),nopk.begin()+QK,0);


            // v[nw][i(in K)][k(in Q(K))] += sum_n LQK[i][k][n] X[Q][n+][nw]
            if(Q < kminus[Q] || Q==Q0) {
              SpMatrix_ref vik(TMats.origin(),{nwalk,ni*nk});
              Sp3Tensor_ref vik3D(TMats.origin(),{nwalk,ni,nk});
              SpMatrix_ref Likn(LQKikn[Q][K].origin(),{ni*nk,nchol});
              ma::product(T(X.sliced(nc0,nc0+nchol)),T(Likn),vik);
              for(int nw=0; nw<nwalk; nw++)
                for(int i=0; i<ni; i++)
                  ma::axpy(one,vik3D[nw][i],v3D[nw][ni0+i].sliced(nk0,nk0+nk));
            } else if(Q > kminus[Q]) { // use L(-Q)(ki)*
              SpMatrix_ref vki(TMats.origin(),{nwalk,nk*ni});
              Sp3Tensor_ref vki3D(TMats.origin(),{nwalk,nk,ni});
              SpMatrix_ref Likn(LQKikn[kminus[Q]][QK].origin(),{nk*ni,nchol});
              ma::product(T(X.sliced(nc0,nc0+nchol)),H(Likn),vki);
              for(int nw=0; nw<nwalk; nw++) {
                auto&& vki_n(vki3D[nw]);
                for(int i=0; i<ni; i++) {
                  auto v3D_ni(v3D[nw][ni0+i].origin() + nk0);
                  for(int k=0; k<nk; k++, ++v3D_ni)
                    *v3D_ni += vki_n[k][i];
                }
              }
            } else { // Q==(-Q) and Q!=Q0
              if(K == QK) 
                APP_ABORT(" Error: Q!=Q0 and K==QK.\n"); 
              // find the position of (K,QK) pair on symmetric list
              int kpos(0); 
              for(int K_=0, kmin=std::min(K,QK); K_<kmin; K_++)
                if(K_ < QKToK2[Q][K_]) kpos++;
              if(K < QK) {
                SpMatrix_ref vik(TMats.origin(),{nwalk,ni*nk});
                Sp3Tensor_ref vik3D(TMats.origin(),{nwalk,ni,nk});
                SpMatrix_ref Likn(LQKikn[Q][kpos].origin(),{ni*nk,nchol});
                ma::product(T(X.sliced(nc0,nc0+nchol)),T(Likn),vik);
                for(int nw=0; nw<nwalk; nw++)
                  for(int i=0; i<ni; i++)
                    ma::axpy(one,vik3D[nw][i],v3D[nw][ni0+i].sliced(nk0,nk0+nk));
              } else {  
                SpMatrix_ref vki(TMats.origin(),{nwalk,nk*ni});
                Sp3Tensor_ref vki3D(TMats.origin(),{nwalk,nk,ni});
                SpMatrix_ref Likn(LQKikn[Q][kpos].origin(),{nk*ni,nchol});
                ma::product(T(X.sliced(nc0,nc0+nchol)),H(Likn),vki);
                for(int nw=0; nw<nwalk; nw++) {
                  auto&& vki_n(vki3D[nw]);
// needs kernel
                  for(int i=0; i<ni; i++) {
                    auto v3D_ni(v3D[nw][ni0+i].origin() + nk0);
                    for(int k=0; k<nk; k++, ++v3D_ni)
                      *v3D_ni += vki_n[k][i];
                  }
                }
              }
            }
          }
        }
        nc0+=2*ncholpQ[Q];
      }
      //comm->barrier();
      // adding second half of Q0
      nqk=0;
      if(Q0>=0) {  
        int nc0 = 2*std::accumulate(ncholpQ.begin(),ncholpQ.begin()+Q0,0);
        for(int K=0; K<nkpts; ++K) {        // K is the index of the kpoint pair of (i,k)
          //if((nqk++)%comm->size() == comm->rank()) 
          {
            int nchol = ncholpQ[Q0];
            int ni = nopk[K];
            int ni0 = std::accumulate(nopk.begin(),nopk.begin()+K,0);
            int nk = nopk[QKToK2[Q0][K]];
            int nk0 = std::accumulate(nopk.begin(),nopk.begin()+QKToK2[Q0][K],0);
            SpMatrix_ref Likn(LQKikn[Q0][K].origin(),{ni*nk,nchol});
            SpMatrix_ref vik(TMats.origin(),{nwalk,ni*nk});
            Sp3Tensor_ref vik3D(TMats.origin(),{nwalk,ni,nk});
            // v[nw][k(in Q(K))][i(in K)] += sum_n conj(LQK[i][k][n]) X[Q][n-][nw]
            ma::product(T(X.sliced(nc0+nchol,nc0+2*nchol)),H(Likn),vik);
            for(int nw=0; nw<nwalk; nw++) {
              auto&& vik3D_n = vik3D[nw];   
              for(int k=0; k<nk; k++) {
                ComplexType* v3D_nk = v3D[nw][nk0+k].origin() + ni0;    
// needs kernel
                for(int i=0; i<ni; i++, ++v3D_nk) 
                  *v3D_nk += vik3D_n[i][k];  
              }
            }
          }
        }
      }    
      //comm->barrier();
      // do I need to "rotate" back, can be done if necessary
    }

    template<class MatA, class MatB,
             typename = typename std::enable_if_t<(std::decay<MatA>::type::dimensionality==1)>,
             typename = typename std::enable_if_t<(std::decay<MatB>::type::dimensionality==1)>,
             typename = void
            >
    void vbias(const MatA& G, MatB&& v, double a=1., double c=0., int k=0) {
      using BType = typename std::decay<MatB>::type::element ;
      using AType = typename std::decay<MatA>::type::element ;
      using AAlloc = typename Allocator_shared::template rebind<AType>::other;
      using BAlloc = typename Allocator_shared::template rebind<BType>::other;
      using Aptr = typename AAlloc::pointer;
      using Bptr = typename BAlloc::pointer;
      boost::multi::array_ref<BType,2,Bptr> v_(v.origin(),
                                        {v.shape()[0],1});
      boost::multi::const_array_ref<AType,2,Aptr> G_(G.origin(),
                                        {G.shape()[0],1});
      return vbias(G_,v_,a,c,k);  
    }

    template<class MatA, class MatB,
             typename = typename std::enable_if_t<(std::decay<MatA>::type::dimensionality==2)>,
             typename = typename std::enable_if_t<(std::decay<MatB>::type::dimensionality==2)>
            >
    void vbias(const MatA& G, MatB&& v, double a=1., double c=0., int nd=0) {
      int nkpts = nopk.size();
      assert(nd >= 0 && nd < nelpk.size());
      int nwalk = G.shape()[1];
      assert(v.shape()[0]==2*local_nCV);  
      assert(v.shape()[1]==nwalk);  
      int nspin = (walker_type==COLLINEAR?2:1);
      int nmo_tot = std::accumulate(nopk.begin(),nopk.end(),0);
      int nmo_max = *std::max_element(nopk.begin(),nopk.end());
      int nocca_tot = std::accumulate(nelpk[nd].begin(),nelpk[nd].begin()+nkpts,0);
      int nocca_max = *std::max_element(nelpk[nd].begin(),nelpk[nd].begin()+nkpts);
      int noccb_max = nocca_max; 
      int nchol_max = *std::max_element(ncholpQ.begin(),ncholpQ.end());
      int noccb_tot = 0;
      if(walker_type==COLLINEAR) {
        noccb_tot = std::accumulate(nelpk[nd].begin()+nkpts,
                                    nelpk[nd].begin()+2*nkpts,0);
        noccb_max = *std::max_element(nelpk[nd].begin()+nkpts,
                                      nelpk[nd].begin()+2*nkpts);
      }
      RealType scl = (walker_type==CLOSED?2.0:1.0);
      SPComplexType one(1.0,0.0);
      SPComplexType halfa(0.5*a*scl,0.0);
      SPComplexType minusimhalfa(0.0,-0.5*a*scl);
      SPComplexType imhalfa(0.0,0.5*a*scl);
      size_t local_memory_needs = 2*nchol_max*nwalk + std::max(nocca_max,noccb_max)*nwalk;
      if(TMats.num_elements() < local_memory_needs) TMats.reextent({local_memory_needs,1});
      SpMatrix_ref vlocal(TMats.origin(),{2*nchol_max,nwalk});
      fill_n(vlocal.origin(),vlocal.num_elements(),SPComplexType(0.0));

      assert(G.num_elements() == nwalk*(nocca_tot+noccb_tot)*nmo_tot);
      C3Tensor_cref G3Da(G.origin(),{nocca_tot,nmo_tot,nwalk} );
      C3Tensor_cref G3Db(G.origin()+G3Da.num_elements()*(nspin-1),{noccb_tot,nmo_tot,nwalk} );

      {  
        // assuming contiguous
        ma::scal(c,v);
      }
      //comm->barrier();

      size_t nqk=0;  // start count at 1 to "offset" the calcuation of E1 done at root
      for(int K=0; K<nkpts; ++K) {        // K is the index of the kpoint pair of (a,k)
        for(int Q=0; Q<nkpts; ++Q) {      // momentum conservation index   
          bool haveV=false;
          //if((nqk++)%comm->size() == comm->rank()) 
          {
            haveV=true;
            int nchol = ncholpQ[Q];
            int na = nelpk[nd][K];
            int na0 = std::accumulate(nelpk[nd].begin(),nelpk[nd].begin()+K,0);
            int nk = nopk[QKToK2[Q][K]];
            int nk0 = std::accumulate(nopk.begin(),nopk.begin()+QKToK2[Q][K],0);
            auto&& v1 = vlocal.sliced(0,nchol);

            Sp3Tensor_ref Lank(LQKank[nd*nspin*(nkpts+1)+Q][K].origin(),{na,nchol,nk});

            // v1[Q][n][nw] += sum_K sum_a_k LQK[a][n][k] G[a][k][nw]
            for(int a=0; a<na; ++a) 
              ma::product(one,Lank[a],G3Da[na0+a]({nk0,nk0+nk},{0,nwalk}),one,v1);

          }
          if(walker_type==COLLINEAR) {
            //if((nqk++)%comm->size() == comm->rank()) {
                APP_ABORT(" Error: Finish UHF vbias.\n");
            //}
          }
          if(haveV) {
            {
//              std::lock_guard<shared_mutex> guard(*mutex[Q]);
              int nc0 = 2*std::accumulate(ncholpQ.begin(),ncholpQ.begin()+Q,0);
              // v+ = 0.5*a*(v1+v2) 
              ma::axpy(halfa,vlocal.sliced(0,ncholpQ[Q]),v.sliced(nc0,nc0+ncholpQ[Q]));
              // v- = -0.5*a*i*(v1-v2) 
              ma::axpy(minusimhalfa,vlocal.sliced(0,ncholpQ[Q]),v.sliced(nc0+ncholpQ[Q],nc0+2*ncholpQ[Q]));
            }
            int Qm = kminus[Q];
            int nc0 = 2*std::accumulate(ncholpQ.begin(),ncholpQ.begin()+Qm,0);
            if(Q != Q0) {
//              std::lock_guard<shared_mutex> guard(*mutex[Qm]);
              // v+ = 0.5*a*(v1+v2) 
              ma::axpy(halfa,vlocal.sliced(0,ncholpQ[Qm]),v.sliced(nc0,nc0+ncholpQ[Qm]));
              // v- = -0.5*a*i*(v1-v2) 
              ma::axpy(imhalfa,vlocal.sliced(0,ncholpQ[Qm]),v.sliced(nc0+ncholpQ[Qm],nc0+2*ncholpQ[Qm]));
            }
          } // to release the lock
          if(haveV)
            fill_n(vlocal.origin(),vlocal.num_elements(),SPComplexType(0.0));
        }
      }
      // add second contribution to Q0
      if(Q0 >= 0) {  
        for(int K=0; K<nkpts; ++K) {        // K is the index of the kpoint pair of (a,k)
          bool haveV=false;
          //if((nqk++)%comm->size() == comm->rank()) 
          {
            haveV=true;
            int nchol = ncholpQ[Q0];
            int na = nelpk[nd][K];
            int na0 = std::accumulate(nelpk[nd].begin(),nelpk[nd].begin()+K,0);
            int nk = nopk[QKToK2[Q0][K]];
            int nk0 = std::accumulate(nopk.begin(),nopk.begin()+QKToK2[Q0][K],0);
            auto&& v1 = vlocal.sliced(0,nchol);

            Sp3Tensor_ref Lank(LQKank[nd*nspin*(nkpts+1)+nkpts][K].origin(),{na,nchol,nk});

            // v1[Q][n][nw] += sum_K sum_a_k LQK[a][n][k] G[a][k][nw]
            for(int a=0; a<na; ++a) 
              ma::product(one,Lank[a],G3Da[na0+a]({nk0,nk0+nk},{0,nwalk}),one,v1);

          }
          if(walker_type==COLLINEAR) {
            //if((nqk++)%comm->size() == comm->rank()) 
            {
                APP_ABORT(" Error: Finish UHF vbias.\n");
            }
          }
          if(haveV) {
            {
//              std::lock_guard<shared_mutex> guard(*mutex[Q0]);
              int nc0 = 2*std::accumulate(ncholpQ.begin(),ncholpQ.begin()+Q0,0);
              // v+ = 0.5*a*(v1+v2) 
              ma::axpy(halfa,vlocal.sliced(0,ncholpQ[Q0]),v.sliced(nc0,nc0+ncholpQ[Q0]));
              // v- = -0.5*a*i*(v1-v2) 
              ma::axpy(imhalfa,vlocal.sliced(0,ncholpQ[Q0]),v.sliced(nc0+ncholpQ[Q0],nc0+2*ncholpQ[Q0]));
            }
          } // to release the lock
          if(haveV)
            fill_n(vlocal.origin(),vlocal.num_elements(),SPComplexType(0.0));
        }
      }
      //comm->barrier();
    }

    bool distribution_over_cholesky_vectors() const{ return true; }
    int number_of_ke_vectors() const{ return std::accumulate(ncholpQ.begin(),ncholpQ.end(),0); }
    int local_number_of_cholesky_vectors() const{ return 2*std::accumulate(ncholpQ.begin(),ncholpQ.end(),0); } 
    int global_number_of_cholesky_vectors() const{ return global_nCV; }

    // transpose=true means G[nwalk][ik], false means G[ik][nwalk]
    bool transposed_G_for_vbias() const{return false;}
    bool transposed_G_for_E() const{return false;} 
    // transpose=true means vHS[nwalk][ik], false means vHS[ik][nwalk]
    bool transposed_vHS() const{return true;} 

    bool fast_ph_energy() const { return false; }

  private:

    int Q0;

//    communicator* comm;


    Allocator allocator_;
    SpAllocator sp_allocator_;
    Allocator_shared allocator_shared_;
    SpAllocator_shared sp_allocator_shared_;
    IAllocator_shared iallocator_shared_;

    WALKER_TYPES walker_type;

    int global_nCV;
    int local_nCV;

    ValueType E0;

    // bare one body hamiltonian
    shmC3Tensor H1;

    // (potentially half rotated) one body hamiltonian
    shmCMatrix haj;
    //std::vector<shmCVector> haj;

    // number of orbitals per k-point
    std::vector<int> nopk;

    // number of cholesky vectors per Q-point
    std::vector<int> ncholpQ;

    // position of (-K) in kp-list for every K 
    std::vector<int> kminus;

    // number of electrons per k-point
    // nelpk[ndet][nspin*nkpts]
    //shmIMatrix nelpk;
    boost::multi::array<int,2> nelpk;

    // maps (Q,K) --> k2
    //shmIMatrix QKToK2; 
    boost::multi::array<int,2> QKToK2; 

    //Cholesky Tensor Lik[Q][nk][i][k][n]
    std::vector<shmSpMatrix> LQKikn;

    // half-tranformed Cholesky tensor
    std::vector<shmSpMatrix> LQKank;

    // one-body piece of Hamiltonian factorization
    shmC3Tensor vn0;

    int nsampleQ;
    std::vector<RealType> gQ;
    boost::multi::array<int,2> Qwn;
    std::default_random_engine generator;
    std::discrete_distribution<int> distribution;

    // shared buffer space
    // using matrix since there are issues with vectors
    shmSpMatrix SM_TMats;
    SpMatrix TMats;

//    std::vector<std::unique_ptr<shared_mutex>> mutex;

//    boost::multi::array<ComplexType,3> Qave;
//    int cntQave=0;
    std::vector<ComplexType> EQ;
//    std::default_random_engine generator;
//    std::uniform_real_distribution<RealType> distribution(RealType(0.0),Realtype(1.0));

    void set_shm_buffer(size_t N) {
      if(SM_TMats.num_elements() < N) 
        SM_TMats.reextent({N,1});
    }

    template<class MatA, class MatB>
    void GKaKjw_to_GKKwaj(int nd, MatA const& GKaKj, MatB && GKKaj, 
                          int nocca_tot, int noccb_tot, int nmo_tot, int akmax)
    {
      int nspin = (walker_type==COLLINEAR?2:1);
      int nwalk = GKaKj.shape()[1];
      int nkpts = nopk.size();
      assert(GKaKj.num_elements() == nocca_tot*nmo_tot*nwalk);
      assert(GKKaj.num_elements() == nkpts*nkpts*akmax*nwalk); 
      boost::multi::const_array_ref<ComplexType,3,const_pointer> Gca(GKaKj.origin(),
                                                       {nocca_tot,nmo_tot,nwalk} );
      boost::multi::const_array_ref<ComplexType,3,const_pointer> Gcb(GKaKj.origin()+
                                                       Gca.num_elements(),
                                                       {noccb_tot,nmo_tot,nwalk} );
      boost::multi::array_ref<SPComplexType,4,sp_pointer> GKK(GKKaj.origin(),
                                                   {nspin,nkpts,nkpts,nwalk*akmax} );
      // might need a kernel for this!!!  
      int na0 = 0;
      for(int Ka=0, Kaj=0; Ka<nkpts; Ka++) {
        int na = nelpk[nd][Ka];
        int nj0=0;
        for(int Kj=0; Kj<nkpts; Kj++, Kaj++) {
          int nj = nopk[Kj];
          kernels::ajw_to_waj(na,nj,nwalk,nmo_tot*nwalk,
                              to_address(Gca[na0][nj0].origin()),
                              to_address(GKK[0][Ka][Kj].origin()));
/*
          if(Kaj%comm->size() != comm->rank()) {
            nj0 += nj;  
            continue;
          }
          auto G_(std::addressof(*GKK[0][Ka][Kj].origin()));  
          int naj=na*nj;
          for(int a=0, aj=0; a<na; a++) {
            auto Gc_( std::addressof(*Gca[na0+a][nj0].origin()) );
            for(int j=0; j<nj; j++, aj++) { 
              for(int w=0, waj=0; w<nwalk; w++, ++Gc_, waj+=naj) 
#if defined(MIXED_PRECISION)
                G_[waj+aj] = static_cast<SPComplexType>(*Gc_);
#else
                G_[waj+aj] = (*Gc_);
#endif
            }
          }
*/
          nj0 += nj;  
        }
        na0 += na;
      }
      if(nspin>1) {
        na0 = 0;
        for(int Ka=0, Kaj=0; Ka<nkpts; Ka++) {
          int na = nelpk[nd][nkpts+Ka];
          int nj0=0;
          for(int Kj=0; Kj<nkpts; Kj++, Kaj++) {
            int nj = nopk[Kj];
            kernels::ajw_to_waj(na,nj,nwalk,nmo_tot*nwalk,
                              to_address(Gcb[na0][nj0].origin()),
                              to_address(GKK[1][Ka][Kj].origin()));
/*
            if(Kaj%comm->size() != comm->rank()) {
              nj0 += nj;  
              continue;
            }
            auto G_(std::addressof(*GKK[1][Ka][Kj].origin()));
            int naj=na*nj;
            for(int a=0, aj=0; a<na; a++) {
              auto Gc_( std::addressof(*Gcb[na0+a][nj0].origin()) );
              for(int j=0; j<nj; j++, aj++) {
                for(int w=0, waj=0; w<nwalk; w++, ++Gc_, waj+=naj)
#if defined(MIXED_PRECISION)
                  G_[waj+aj] = static_cast<SPComplexType>(*Gc_);
#else
                  G_[waj+aj] = (*Gc_);
#endif
              }
            }
*/
            nj0 += nj;
          }
          na0 += na;
        }
      }  
      //comm->barrier();  
    }

    // for testing purposes only
    template<class MatA, class MatB>
    void GwAK_to_GAKw(MatA const& GwAK, MatB && GAKw)
    {
      int nwalk = GwAK.shape()[0];
      int nAK = GwAK.shape()[1];
      for(int w=0; w<nwalk; w++) 
        for(int AK=0; AK<nAK; AK++) 
          GAKw[AK][w] = GwAK[w][AK]; 
    }


};

}

}

#endif