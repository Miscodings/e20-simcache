# 🧮 E20 Processor Simulator with Cache Support

## 📋 State of Work
- ✅ all test cases pass successfully  
- 🧠 supports both **single-level (L1)** and **two-level (L1 + L2)** cache configurations  
- 🧩 implements all required **E20 instruction set** operations  
- ⚙️ correctly handles **cache hits, misses, and write-through operations**  
- 🧾 includes additional logic to cover potential **edge cases** not specified in the provided tests  

## 📚 Resources Used
- course lecture notes on cache architecture  
- official **E20 Processor Manual**  
- CA office hours for design clarifications  

---

## 🧠 Design Overview

### core structure
- utilizes a main **simulation function** that reuses the E20 simulator from project 1  
  - allows flexible use of **one or two cache layers** through parameterized configuration  
  - avoids redundancy by using a single simulation loop for all configurations  

### data representation
- **2D arrays** store cache metadata: tags, valid bits, and LRU counters  
- dynamic calculation of cache parameters (e.g., number of rows, tag bits) from configuration input  

### cache policies
- **write-through policy** for `sw` (store word) instructions  
- **LRU (least recently used)** replacement for cache lines  

### instruction simulation
- reads binary instruction files (`.bin`)  
- uses a **while loop** to iterate through instructions  
- **if / else-if** chains decode primary opcodes  
- nested conditionals handle instructions sharing opcodes  
- supports **sign-extension** and other bitwise operations for accuracy  

---

## 🏗️ Implementation Notes
- clear **separation of cache logic** from the E20 instruction handling  
- detailed **logging system** for cache events (as defined in the project specification)  
- configurable through **command-line parameters**, improving flexibility  
- heavily **commented codebase** for clarity and maintainability  

---

## 🔍 Strengths
- modular and readable structure  
- accurate cache logging and flexible configuration system  
- robust handling of expected and unexpected instruction behaviors  

## ⚠️ Potential Weaknesses
- minor untested **edge cases** in certain rare instruction scenarios  
- could benefit from **performance optimizations** for larger cache sizes  

---

## 💭 Overall Summary
this project provides a clean, well-structured implementation of the E20 processor simulator with fully integrated cache behavior.  
it successfully passes all known test cases and adheres closely to the required design principles, while maintaining clarity and extensibility.
