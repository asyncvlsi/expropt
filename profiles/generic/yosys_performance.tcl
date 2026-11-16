#yosys performance profile: minimize the delay of each expression block 
yosys read_verilog @VIN@
yosys synth -noabc -top @TOP@
yosys abc -liberty @LIB@ -constr @SDC@ -script {+strash;dch -f;map;topo;buffer -p;upsize;dnsize;upsize;dnsize;stime -p}
yosys write_verilog -nohex -nodec @VOUT@
