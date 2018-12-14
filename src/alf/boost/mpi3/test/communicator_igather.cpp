#if COMPILATION_INSTRUCTIONS
mpic++ -O3 -std=c++14 `#-Wfatal-errors` $0 -o $0x.x && time mpirun -n 8 $0x.x $@ && rm -f $0x.x; exit
#endif

#include "../../mpi3/main.hpp"
#include "../../mpi3/communicator.hpp"

namespace mpi3 = boost::mpi3;
using std::cout;

int mpi3::main(int, char*[], mpi3::communicator world){

	assert( world.size() > 1);

	{
		std::vector<double> small(10, 5.);
		std::vector<double> large;
		if(world.rank() == 0) large.resize(small.size()*world.size(), -1.);
		{
			auto req = world.igather(small.begin(), small.end(), large.begin(), 0);
			using namespace std::chrono_literals;
			std::this_thread::sleep_for(5s);
		}
		if(world.rank() == 0){
			assert( std::all_of(large.begin(), large.end(), [](auto& e){return 5. == e;}) );
		}
	}
	{
		std::vector<double> small(10, 5.);
		std::vector<double> large(small.size()*world.size(), -1.);
		{
			auto req = world.iall_gather(small.begin(), small.end(), large.begin());
			using namespace std::chrono_literals;
			std::this_thread::sleep_for(5s);
		}
		assert( std::all_of(large.begin(), large.end(), [](auto& e){return 5. == e;}) );
	}

	return 0;
}

