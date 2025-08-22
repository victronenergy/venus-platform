#include "network-resetter-app.h"
#include <iostream>


int main(int argc, char *argv[])
{
	if (argc > 1)
	{
		std::cout << "Usage: " << argv[0] << R"(

* Turns off venus-platform and connman.
* Removes network settings.
* Sets access points to defaults.
* Turn off localsettings (which should also save and fsync).

No arguments required.

)" << std::endl;

		return 1;
	}

	NetworkResetterApp app(argc, argv);
	return app.exec();
}
