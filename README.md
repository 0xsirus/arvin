# arvin
<p align=center>
  <img src="https://www.cs.utah.edu/~sirus/dcfg.gif" />
</p>

Arvin is a stand-alone greybox fuzzer. It started as ZHARF project[1] which is our baseline framework of CFG-based fuzzing and 
then progressed to the this work. Arvin performs dynamic graph approximation 
to achieve more efficient coverage and higher number of bugs. Read the paper here:

[Arvin: Greybox Fuzzing Using Dynamic CFG Analysis](http://www.flux.utah.edu/paper/shahini-asiaccs23)

This repository contains the core functionality of Arvin. Including the fuzzing engine and DCFG libraries and three
default priority models.
 
# Prerequisites:

Arvin uses `angr` in its preprocessing stage before stating the fuzzing engine. Install `angr`:

`pip3 install angr`

# Compile and install the fuzzer:

```
make
make install
```

# Usage 

Arvin has lots of options. The basic usage is simple. Give it a binary, an input directory which
has at least one initial seed, an output directory to store the fuzzing result and start fuzzing:

`arvin -i input_dir -o output_dir program.elf`

Where `program.elf` is the executable you want to fuzz.

## Notes

Arvin reserves a limited capacity to build graphs in memory. The memory segment that is used by default is
large enough to adapt to most targets. However, if your target is unusually big, you will need to compile Arvin
in `ARV_BIG_TARGET` mode which comes at the cost a slightly degraded performance per iteration.

Note that it has been observed that `angr` fails to process some binaries in which case Arvin refuses to proceed 
to start the fuzzing engine. For those cases, you can send a bug report to `angr` repository directly:

[https://github.com/angr/angr](https://github.com/angr/angr)

Preferably don't compile your targets with `clang`. Specifically, Arvin will not work normally if your target has been 
compiled with LLVM ASAN due to inconsistencies of LLVM instrumentation with dynamic Arvin instrumentation.
