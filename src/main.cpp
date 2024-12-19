#include <iostream>
#include <src/application.hpp>

static const char *version = "1.63-r1";

int main(int argc, char *argv[])
{
	std::cout << "Version: " << version << std::endl;

	Application app(argc, argv);

	return app.exec();
}
