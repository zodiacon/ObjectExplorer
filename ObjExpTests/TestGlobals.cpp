#include "pch.h"
#include <catch2/catch_session.hpp>

// declared by ObjExp's pch.h
CAppModule _Module;

// vcpkg doesn't link Catch2Main (a "manual-link" library) automatically
int main(int argc, char* argv[]) {
	return Catch::Session().run(argc, argv);
}
