# yosys "area" profile: minimize the area of each expression block
yosys read_verilog @VIN@
yosys synth -noabc -top @TOP@
yosys abc -liberty @LIB@ -constr @SDC@ -script {+strash;ifraig;dc2;strash;dch -f;map -a;topo;stime -p}
yosys write_verilog -nohex -nodec @VOUT@
