# CS 106: Virtual Machines & Language Translation

## Course Overview
This course teaches the translation of high-level functional programming languages to virtual-machine code through a series of incremental translation passes. Students build both a Simple Virtual Machine (SVM) and a Universal Forward Translator (UFT), implementing the complete pipeline from high-level Scheme code to executable bytecode.

## Key Topics
- **Virtual Machine Implementation**: Register-based VM design, bytecode interpretation, VM state management
- **Code Loading**: Virtual object code syntax and semantics, parsing, code loading
- **Memory Management**: Automated garbage collection
- **Translation Techniques**: 
  - Closure conversion for higher-order functions
  - K-normal form for naive register allocation
  - Code generation from K-normal form to virtual assembly
- **Functional Programming for Compilation**: Error monads for composing translation passes, parsing combinators
- **Language Processing**: Parsing and unparsing, parsing combinators

## Assignments/Projects
The course is module-based with weekly deliverables:
- **Module 1 (SVM)**: Build intraprocedural code execution and VM state
- **Module 2 (SVM)**: Implement the VM loader for virtual object code
- **Module 3+ (UFT)**: Build parsing, unparsing, and assembly components
- **Modules 5-10**: Implement higher-level language translations (K-normal form, first-order vScheme, unambiguous vScheme, full vScheme)

Students submit homework Monday nights and reflections Tuesday nights documenting learning outcomes.

## Course Structure
- **Labs**: Thursdays, supervised coding sessions to start module work
- **Code Reviews**: Tuesdays, peer review in small groups and plenary panels
- **Grading**: Based on project points (homework completion), participation points (code review engagement), and depth points (reflection quality)
- **Final Assessment**: End-of-term workshop with oral presentations to peer and expert jury, followed by final reflection
- **No exams or papers**

## Prerequisites
- **For SVM**: Comfort with bits, bytes, pointers, and machine emulation (CS 40 recommended)
- **For UFT**: Experience with functional programming in languages like Haskell, OCaml, Racket, or Standard ML (CS 105 recommended)
- Operational semantics knowledge helpful but not required
