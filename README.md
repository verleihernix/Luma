---

# Luma

**Luma** is a lightweight, embeddable scripting language written in **C++20**, designed for easy integration into applications and game engines.

---

## Features

*  Embeddable C++ scripting engine
*  Dynamic typing (`number`, `string`, `list`, `map`, `function`)
*  Native function registration from C++
*  Simple C API for integration
*  Built-in standard library (print, math, strings, etc.)
*  Interactive REPL with multiline support
*  Script file execution
*  Fast and lightweight runtime design

---

## Project Structure

```
include/        # Public API (luma.h)
src/            # VM, parser, interpreter, lexer
examples/       # REPL and demo application
CMakeLists.txt  # Build system
```

---

## Build Instructions

### Requirements

* C++20 compatible compiler (MSVC, GCC, Clang)
* CMake 3.20+

### Build

```bash
git clone https://github.com/verleihernix/luma.git
cd luma
mkdir build
cd build
cmake ..
cmake --build .
```

---

## ▶️ Running Luma

### Start REPL

```bash
./luma
```

### Run a script file

```bash
./luma script.luma
```

### Execute inline code

```bash
./luma -e "print(1 + 2)"
```
## Execute code inside the console
```bash
luma
```
```lua
print("Hello from the Console")
```

---

## Example Code (Luma)

```lua
print("Hello from Luma!")

let x = 10
let y = 20

print(x + y)

let list = [1, 2, 3]
push(list, 4)

print(len(list))
```

---

## 🔌 Embedding in C++

Luma is designed to be embedded into your applications easily:

```cpp
#include "luma.h"

LumaVM* vm = luma_create();

luma_register_function(vm, "add", [](LumaVM*, std::span<LumaValue> args) {
    return LumaValue(args[0].as_number() + args[1].as_number());
});

luma_run(vm, "print(add(2, 3))");

luma_destroy(vm);
```

---
