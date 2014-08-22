#include <boost/mpi/environment.hpp>
#include <boost/mpi/communicator.hpp>
#include <iostream>

#include "client.h"
#include "server.h"

using namespace std;
namespace mpi = boost::mpi;

int main(int argc, char* argv[])
{
	int provided_MPI;
	MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided_MPI );

	mpi::environment env(argc, argv);
	mpi::communicator world;
	INode* node = 0;

	if (world.rank() == 0) {
		cout << "Welcome to the Parallel Ninja v1.0 <c> http://k0rnev.blogspot.com" << endl;
		cout << "Connected " << world.size() << " nodes" << endl;
	}
	if (world.size() == 1) {
		cout << "Error, can't run bruteforce without nodes!" << endl;
		return 0;
	}

	try {
		if (world.rank() == 0)
			node = new CBruteServer(env, world);
		else
			node = new CBruteClient(env, world);

		node->init();
		world.barrier();
		node->start();
	} catch (exception const& e) {
		cout << "[node #" << world.rank() << "] unhandled exception: " << e.what() << endl;
	} catch (...) {
		cout << "[node #" << world.rank() << "] unhandled exception: unknown" << endl;
	}

	if (node != 0)
		delete node;

	cout << "[node #" << world.rank() << "] aborted" << endl;
	getchar();
	return 0;
}