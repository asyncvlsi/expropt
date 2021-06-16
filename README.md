# the ExprOpt library

is used the optimise sets of expressions (data pass only) by using external systesis tools. It also return metadata like delay, power and area for the mapped data object.

## things to be aware of

 - abc does not consider wiring RC while mapping and metadata exstraction, the metadata is from the used cells only
 - abc and therefore yosys can not handle cells with more than one output like fulladder and therefor will ignore them during mapping

## install
to use it make sure you have act properly installed and $ACT_HOME is pointing to your install.

use
```
./configure
make depend
make 
make install
```
## dependencies

the library is an addon for The ACT language and core tools (https://github.com/asyncvlsi/act), so they need to be installed the tool can not be build without.

the library requires you to have yosys (https://github.com/cliffordwolf/yosys.git) and abc (https://github.com/berkeley-abc/abc) and sed (check you package manager) installed on your system and the excutables to be in your $path during runtime use.

for commercial syntesis check the exproptcommercial library (closed source because tool APIs are under NDA), it will be automaticly picked up if installed on your system as a shared library.

## example and test dependencies

for the example to run you need some library files, they are installed when you install chp2prs form the sdtcore branch (not yet in master) on your system. https://github.com/asyncvlsi/chp2prs

the automated tests use the example program to test the API.

## documentaion
have a peek at the header file on descriptions of the functions.

## example use

your header will be installed in $ACT_HOME/include/act/expropt.h and the static library in $ACT_HOME/lib/expropt.a
the sample config will be installed in the $ACT_HOME/config folder

if you want to see how to use it see in the folder example a program that extracts expr out of chp.
use the make file in the folder to build the example, the the readme inside
