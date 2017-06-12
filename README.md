QConf v2.3
==========

Authors: Justin Karneges <justin@affinix.com>, Sergey Ilinykh <rion4ik@gmail.com>, Ivan Romanov <drizt@land.ru>  

QConf allows you to have a nice configure script for your qmake-based project. It is intended for developers who don't need (or want) to use the more complex GNU autotools. QConf also generates a configure.exe program for use on Windows.

Install
-------

```sh
./configure
make
make install
```

Usage
-----

First, create a project.qc file. It is in an XML format:

```xml
<qconf>
  <name>MyProject</name>         <!-- a friendly string  -->
  <profile>project.pro</profile> <!-- your qmake profile -->
</qconf>
```

Then, run qconf on your file:

```sh
qconf project.qc
```

Assuming all goes well, this will output `configure` and `configure.exe` programs. Simply copy these files into your application package. Make sure to `include(conf.pri)` in your project.pro file.

Tip: If qconf is launched with no arguments, it will use the first .qc file it can find in the current directory. If there is no .qc file, then it will look for a .pro file, and create a .qc for you based on it.

The Configure Programs
----------------------

Once the configure programs have been created, they are immediately usable. The programs perform the following tasks:

1. Check for a proper Qt build environment. This is done by compiling the `conf` program, which ensures that the Qt library, qmake, and necessary compiler tools are present and functioning.

2. `conf` is launched, which does any needed dependency checking and creates a suitable `conf.pri`. This operation also ensures that not only can Qt-based programs be successfully built, but launched as well.

3. qmake is invoked on the project's .pro file.

Assuming configuration was a success, a `Makefile` should be generated and the user can now run the make tool (e.g. `make`).

The script does not touch any of your project files. The only output is a `conf.pri` file. It is up to you to actually include `conf.pri` in your .pro file.

Tip: Passing the `--verbose` option to configure can aid in diagnosing configuration problems.

Q & A
-----

Q: How do I specify dependencies?  
A: List them in your .qc file using the `<dep>` element. Follow sampledeps.qc for a hint.

Q: My dependency is not supported!  
A: You will need to make it. Look in the `modules` folder to see how it is done. If you find that you need to make a lot of these, perhaps you should consider GNU autotools or CMake.

Q: How does qconf find modules?  
A: Modules are found in the `modules` subdirectory within the configured libdir (default is `/usr/local/share/qconf`). Additional directories can be specified using the `<moddir>` element. For instance:

```xml
<moddir>qcm</moddir>
```

The above element would cause qconf to look for modules in the relative directory `qcm`. This is useful if you want to bundle modules within your application distribution.

Q: How do I perform custom processing or add project-specific arguments?  
A: The recommended way of doing this is to create an extra.qcm file that does the processing you need, and then just add it to your .qc file like any normal dependency. Implement `checkString()` in your module to return an empty QString if you want to suppress output.

Q: How can I install more than just the binary with 'make install'?  
A: You need to specify the extra files using the qmake `INSTALLS` variable (see qmake docs for details).

Q: What environment variables are available?  
A: Main variables: `PREFIX`, `BINDIR`, `LIBDIR`, `QTDIR`. All other variables are written as `QC_FOO`, where `FOO` is the option name in all caps. Boolean variables are set to `Y` when flagged.
