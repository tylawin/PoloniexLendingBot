#include "PoloniexLendingBot.hpp"
#include "logging.hpp"

#include <thread>

#include <signal.h>

using namespace tylawin;
using namespace tylawin::poloniex;
using namespace std;

volatile sig_atomic_t g_sigint = false;
void interruptSignalHandler(int param)
{
	if(!g_sigint)
		g_sigint = true;
	else//second time really crash it instead of trying to exit cleanly
	{
		signal(SIGINT, SIG_DFL);
		raise(SIGINT);
	}
}

int main(int argc, char **argv)
{
	logInit();

	INFO << "Poloniex Lending Bot - Built at(" << __DATE__ << " " << __TIME__ << ") with cppVersion(" << __cplusplus << ")";

	PoloniexLendingBot poloLendBot([]() -> bool {
		if(g_sigint)
		{
			INFO << "^c - quitting.";
			return true;
		}

		return false;
	});

	for(size_t i = 0; i < argc; ++i)
	{
		if(strcmp(argv[i], "--clearAutoRenew") == 0)
		{
			poloLendBot.setAllAutoRenew(false);
			return EXIT_SUCCESS;
		}

		if(strcmp(argv[i], "--setAutoRenew") == 0)
		{
			poloLendBot.setAllAutoRenew(true);
			return EXIT_SUCCESS;
		}

		if(strcmp(argv[i], "--dryrun") == 0)
			poloLendBot.dryRun(true);
	}

	signal(SIGINT, interruptSignalHandler);

	try
	{
		poloLendBot.run();
	}
	catch(const std::exception &e)
	{
		ERROR << boost::current_exception_diagnostic_information();
		return EXIT_FAILURE;
	}
	catch(...)
	{
		ERROR << boost::current_exception_diagnostic_information();
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
