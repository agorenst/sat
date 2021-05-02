# README
This is my personal attempt at a SAT solver.
Every 6-18 months it seems I have a renewed interest in working on this.
I have non-github'd attempts, and other earlier ones, but this was the first that finally "beat" minisat on some motivating benchmarks.

The [src](src) directory contains everything about the solver. At one point I had it neatly broken up into the solving algorithms, the underlying data types, and the sat driver, but the project just isn't big enough for that.
It is all in C++.

The [build](build) directory has the ways of building the sat project, as well as running tests (correctness and performance).
It uses Make.

The [tests](tests) directory has some old tests. A lot of the testing is subsumed by my ability to generate random SAT instances (see [satgen](tools/satgen.py), and how it is used in the [Makefile](build/Makefile). However, it also has a (perhaps slightly out-of-date) script for running an AFL-ized version of the solver.

The [tools](tools) directory contains various scripts (mostly Python, some bash) for interacting with the compiled SAT solver.

Of course the goal is to solve CNF instances quickly, but more than that I want to write the explanations and demonstration code I "wish I had" when I started diving into this material.
That writing, in LaTeX for ease-of-citation, can be found in [paper](paper).

# Getting Started
## Building
The only dependency is a C++ compiler with C++17 support and a build system. I forget why I set it to C++17, probably some standard libraries requirement.
My main development is on WSL2 with Ubuntu on Windows 10 using VSCode and clang++.

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
The solver takes a CNF from stdin, and output SATISFIABLE or UNSATISFIABLE to stdout.
There are a great many flags (see [settings.cpp](src/settings.cpp)) that control trace statements, correctness-checking, and optimizations options.
Such flags are a fairly frequent state of flux.
(In a far-off distant future, it'd be nice to isolate the solver as a library and replace the driver logic with a python script calling a static library, using something like [argparse](https://docs.python.org/3/library/argparse.html), which I use in my python scripts, to enable a more systematic flag parsing system.)

# Development
Coding style is imperfectly defined with a clang-format file (committed to the repo).
Experimentally I have found this still allows for some arbitrary whitespace between certain statements--I don't know if that's an option I can control. I try to keep related statements together.

The main correctness suite is accessed via ```make tests``` in the build directory.
You can run them in parallel! It's great. I just need one more excuse and I'll take the time to get an EPYC machine.
# Correctness
Correctness is obviously paramount (we want to get correct answers).
It is also very interesting: I want to learn about these algorithms and insights, and that requires doing it, uh, right.
So of course we want the right answers, but we also want to illuminate all the fine details of the interacting components.
## In-Line Asserts and Invariants
This is *by far* the main work on correctness.
In addition to correctness proper, it helps "fill in the gaps" of what a correct algorithm implementation really is.
By this I mean really filling out all the invariants.
The watched-literal scheme was a big one, as was the real clause-learning algorithm.
I'm not suggesting any additional deep insight, but "just" filling in the gaps.

It's been extremely useful, most recently with on-the-fly subsumption. Rather than reporting the wrong answer, we assert (all over the place).
## Sanitizers
Clang has ASan and UBSan (and others). I use those two sanitizers as a standard test build. They've been helpful.
## Fuzzing
I used AFL++ to fuzz the solver, as well as LLVM-fuzz.
They revealed that, e.g., if you say you have an instance with MAX_INT (or close to it) variables, I will instantiate some really titanic arrays (using the variables as indices).
This is unnecessary, you can imagine a "normalizing" phase that maps the variables {1, 4, 10000} to {1, 2, 3}, for instance.
So for now I've just verified that my parser is well-behaved.

I would like to use fuzzing, coupled with a deliberate assert, to find "minimal" inputs that trigger an "interesting" behavior.
For instance, have an assert for when non-chronological backtracking actually skips a decision.
I would think fuzzing would find a nice minimal instance.
Such an instance, and an illustrated trace of the solver's behavior on that instance, may be pedagogically very valuable.
## Warnings and Static Analysis
Need to hook up MSVC again for their static analysis.
Clang is warning-free.
At one point GCC was warning-free, I'm not sure at this point.
## Unit Tests
Not really done at this point.
There are some vestigial tests from LCM implementation.
## Logging
I include a small logging library that tries to record essentially every semantic action.

# Performance
Obviously perfomance is paramount.
There are so many interacting components it can be hard to anticipate the effect of an optimization.
The main tradeoff is "is the optimization worth the analysis?".
I can imagine the answer may vary depending on what *other* optimizations are enabled.
Haven't gotten to that part yet.
Run ```make benchmark1``` (no -j) to run the main benchmark suite.

The only performance benchmark I'm looking at is random-3-SAT.
Industrial instances have other considerations (namely, titanic CNF size), but random-3-SAT has a lot of advantages.
In particular, it's a class of problems that's easy to "tweak": you can make them bigger or smaller very naturally, and you can generate a *lot* of them.

# Source Structure
Everything is in the flat directory [src](src).
The entry point is [sat.cpp](src/sat.cpp), and the entry point for the solver is at the top of [solver.cpp](src/solver.cpp).
I tried to isolate every interesting function or algorithm in its own file, so there's lots of little ones.

This is definitely a constant work-in-progress.
For instance, I recently moved clause learning into solver.cpp, but there is still a few other attempts in [clause_learning.cpp](src/clause_learning.cpp) I'm keeping around.
One file is zero'd out as something that no longer works post-some-refactoring, but

## Data Structures
I tried really hard to get fancy cache behaving stuff going.
I decided to hamstring myself by working in WSL, where I basically don't get hardware counters.
Surprise surprise, while sometimes I would get small perf wins I never found they were really worth it.
While [clause.cpp](src/clause.cpp) has some weird "small-string-optimization-esque" clause definition, you can see at this point I've just returned to a vector.

The CNF is an intrusive doubly-linked-list on clauses.
At one point long ago it was actually a flat vector, but I shifted to this.

Overall I found myself in a two-step: I'd stop finding algorithmic improvements and fiddle with data structures, make or not make progress, then try another algorithmic improvement that more-or-less subsumed that improvement.
I am also trying to nicely modularize things, with limited success.
At this point I'm inclined to re-simplify all the data structures I can and really commit to only thinking about algorithmic improvements, but there are still these vestiges.

## Algorithms
The algorithms included here are best listed out in the [paper](paper/paper.pdf).
I am trying to get all the neat and valuable insights I can find.

## General Design
The solver object has the main solver loop.
"Plugins" are just a vector of function points where syntactic sugar.
The idea is that we count on certain fundamental actions in the solver (e.g., we want to make a decision, when there is no more unit propogation possible and the (CNF, trail) is still indeterminate) but enable those actions to be modular (e.g., set the "getDecisionLiteral" to be one function pointer or another depending on settings).
These plugins also function as listeners: VSIDS not only provides a "getDecisionLiteral", but hooks into "cdclResolve" so that it knows how to bump whichever literal.

This design is partly inspired by Nadel's PhD thesis, the same central ideas there. I want to be able to modularize the algorithms, both for experimental research and so we can distinguish what "really is" the algorithm, versus what's an incidental implementation detail.
It's not always obvious!

# Work in Progress
This is always going to be a work-in-progress.
It's one of my fun little hobby projects.