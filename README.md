# README
This is my personal attempt at a SAT solver.
Every 6-18 months it seems I have a renewed interest in working on this.
I have non-github'd attempts, and other earlier ones, but this was the first that finally "beat" minisat on some motivating benchmarks.

Of course the goal is to solve CNF instances quickly, but more than that I want to write the explanations and demonstration code I "wish I had" when I started diving into this material.
# Getting Started
## Building
The only dependency is a C++ compiler with C++17 support and a build system. I forget why I set it to C++17, probably some libraries requirement.
My main development is on WSL2 with Ubuntu on Windows 10 using VSCode and clang++.
My secondary development is on Windows with MSVC (Visual Studio).

There are no dependencies other than the build systems and compiler toolsets. Additional features, notably the automated fuzzing system, requires [AFL++](https://github.com/AFLplusplus/AFLplusplus) currently only available on Linux.
### Build Targets
There is really only one executable built from this project, which is the main driver and, well, SAT solver. It is naturally called "sat".
There are several variations:
1. **sat** builds the executable with full optimizations, and is what you'd want to use if you actually have a CNF you'd like to solve.
2. **sat.dbg** builds the executable in debug mode: no optimizations, and lots of asserts.
3. **sat.san** builds the executable with optimizations, but also with available sanitizer flags on.
4. **sat.afl** builds the executable particularly with AFL instrumentation.
### Building In Linux
Navigate to the [build directory](build), and "make -j \<target\>".
So, ```make -j sat.dbg``` is enough to build the sat.dbg executable.
Do you need the ```-j``` parameter? No, but it's a rare enough case when we have the opportunity to use all our cores, so let's do it.
### Building In Windows
The [build directory](build) has a straightforward, more-or-less automatically generated solution file that can be opened with Visual Studio and built.

## User Interface
Of course the main application of the program is to be, well, a SAT solver.
1. The mode ```--solve``` is the default.
2. The mode ```--verify``` takes a CNF and a solution file and validates that it's true.
3. Clause minimization takes a trail and validates that it's doing the minimzation the right way.
# Development
Coding style is imperfectly defined with a clang-format file (committed to the repo).
Experimentally I have found this still allows for some arbitrary whitespace between certain statements--I don't know if that's an option I can control. I try to keep related statements together.
# Correctness
Correctness is obviously paramount (we want to get correct answers).
## In-Line Asserts and Invariants
## Sanitizers
Both MSVC and Clang natively support ASan (I would know).
## Fuzzing
## Static Analysis
Using the solution file it's easy to run the static analysis 
## Unit Tests
## Logging
I include a small logging library that tries to record essentially every semantic action.

# Performance
Obviously perfomance is paramount.
# Tests and Benchmarks
## Correctness Tests
# Documentation
# TODO
- [ ] Make more rigorous the DIMACs parser
- [ ] Clean up the EMA restart policy
- [ ] Minimal unsat core generation, for full validation.
