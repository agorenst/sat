# README
This is my personal attempt at a SAT solver.
Every 6-18 months it seems I have a renewed interest in working on this.
I have non-github'd attempts, and other earlier ones, but this was the first that finally "beat" minisat on some motivating benchmarks
# Getting Started
## Building
The only dependency is a C++ compiler with C++20 support. I forget why I set it to C++20.
My main development is on WSL2 with Ubuntu on Windows 10 using VSCode and clang++.
My secondary development is on Windows with MSVC (Visual Studio).

There are no dependencies other than the build systems and compiler toolsets.
## User Interface
## Testing
All testing is maintained in the Makefile definitions.
The "build" of the test targets succeeding indicates that the tests pass.
# Development
Coding style is imperfectly defined with a clang-format file (committed to the repo).
Experimentally I have found this still allows for some arbitrary whitespace between certain statements--I don't know if that's an option I can control. I try to keep related statements together.
# Correctness
Correctness is obviously paramount (we want to get correct answers).
## In-Line Asserts and Invariants
Building with 
## Sanitizers
## Fuzzing
## Static Analysis
## Unit Tests
## Logging
I include a small logging library that tries to record essentially every semantic action.

# Performance
# Tests and Benchmarks
## Correctness Tests
# Documentation