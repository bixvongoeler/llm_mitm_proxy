# CS 21- Concurrent Programming

## Course Overview

This course teaches concurrent programming, where multiple activities may happen simultaneously or in unpredictable orders. Students learn to write programs that model real-world concurrency, leverage modern multicore processors, and improve application responsiveness and performance.

## Key Topics

- **Concurrency vs. Parallelism**: Understanding the distinction between logical concurrency (activities appearing simultaneous) and true parallelism (activities occurring at the same time on hardware)
- **Shared-Memory Threads Programming**: Conventional threading models and their challenges
- **Alternative Concurrency Models**: Actors and/or Communicating Sequential Processes (CSP)
- **Classic Concurrency Problems**: Race conditions, deadlock, and non-determinism
- **Synchronization Mechanisms**: Semaphores, locks, and barriers
- **Non-deterministic Behavior**: Understanding why concurrent programs may produce different results on each run

## Core Concepts

The course emphasizes that in sequential programs, operations form a **total order** (every operation can be related to every other in time), while concurrent programs create **partial orders** where timing relationships between threads are unspecified. Students learn that even simple operations like `x++` can cause race conditions because they involve multiple underlying instructions.

**Why Concurrency Matters:**
- Responsiveness: Systems can respond to users while performing long computations
- Resource utilization: Hardware stays busy across threads
- Speed: Parallel execution on multiple cores
- Modularization: Complex systems built as interacting processes
- Reliability: Through redundancy and monitoring

## Assignments/Projects

- **Substantial Team Programming Project**: Students work in teams to apply concurrency tools and techniques to build a significant concurrent application
- **Class Presentations**: Teams present their project work to the class
- Assignments are due by midnight of specified dates (last valid timestamp at 23:59)

## Course Structure

The course uses a dynamic calendar updated throughout the term with lecture notes, assignments, and solutions. Students should regularly refresh the calendar page (clearing browser cache may be necessary). Schedule changes are announced in class and on Piazza.

## Prerequisites

Not explicitly stated in the materials, but the course assumes prior programming experience. Students should understand sequential programming concepts like control flow and determinism before exploring how concurrency changes these assumptions.
