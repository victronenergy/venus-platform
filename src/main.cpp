#include <iostream>
#include <src/application.hpp>

static const char *version = "2.03";

int main(int argc, char *argv[])
{
	std::cout << "Version: " << version << std::endl;

	Application app(argc, argv);

	return app.exec();
}
