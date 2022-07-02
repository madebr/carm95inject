from conans import ConanFile, CMake


class CarmaBypass(ConanFile):
    settings = "os", "arch", "compiler", "build_type"
    requires = "detours/cci.20220630"
    generators = "cmake"

    def validate(self):
        assert self.settings.os == "Windows"
        assert self.settings.arch == "x86"
        assert self.settings.compiler == "Visual Studio"
        assert self.settings.build_type == "Release"

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
