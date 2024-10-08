# The ExprOpt Library

is used to optimise the sets of expressions (data pass only) by using external synthesis tools. It also returns metadata like delay, power and area for the mapped data object.

## Things to be Aware of

 - abc does not consider wiring RC during mapping and metadata extraction, the metadata is from the used cells only
 - abc and therefore yosys can not handle cells with more than one output like fulladder and therefore will ignore them during mapping

## Install
To use it, make sure you have act properly installed and $ACT_HOME is pointing to your install.

Use:
```
./build.sh
```

## Dependencies

This library is an addon for the ACT language and core tools (https://github.com/asyncvlsi/act), so they need to be installed, and the tool can not be built without them.

The library requires the standard Unix tool sed (check you package manager) installed on your system and the excutables to be in your $path during runtime use.

The library has been parameterized so that it picks up any logic synthesis
tool for which the appropriate shared object modules have been installed on
your system. By default, we provide these modules for both abc and yosys.
The yosys module requires that the yosys logic synthesis tool has been installed
on your system.


## Example and Test Dependencies

For the example to run, you need some library files. They are installed when you install chp2prs from the sdtcore branch (not yet in master) on your system. https://github.com/asyncvlsi/chp2prs

The automated tests use the example program to test the API.

## Documentation

Have a peek at the header file for descriptions of the functions.

## Example Use

Your header will be installed in $ACT_HOME/include/act/expropt.h and the static library in $ACT_HOME/lib/expropt.a.

The sample config will be installed in the $ACT_HOME/config folder.

If you want to see how to use it, check the folder example, which contains a program that extracts expr out of chp. Use the makefile in the folder to build the example.
