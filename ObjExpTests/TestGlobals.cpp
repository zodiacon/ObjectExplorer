#include "pch.h"
#include <catch2/catch_session.hpp>
#include "AppSettings.h"

// declared by ObjExp's pch.h
CAppModule _Module;

// AppSettings::Get() returns the first instance; the defaults (nothing is loaded from the registry)
AppSettings g_Settings;

// vcpkg doesn't link Catch2Main (a "manual-link" library) automatically
int main(int argc, char* argv[]) {
	return Catch::Session().run(argc, argv);
}
