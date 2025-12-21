# Example Programs

This directory contains sample C programs for testing the debugger.

## Files

| File | Description | Key Concepts |
|------|-------------|--------------|
| `01_simple.c` | Hello World | Basic program structure, printf |
| `02_variables.c` | Variable operations | Variable declaration, arithmetic |
| `03_loop.c` | For loop | Loop iteration, counter variable |
| `04_function.c` | Function calls | Function definition, parameters, return values |
| `05_recursive.c` | Recursive factorial | Recursion, base case |
| `06_array.c` | Array operations | Array declaration, iteration, sum |
| `07_pointer.c` | Pointer usage | Pointers, dereferencing, pass-by-reference |
| `08_conditional.c` | If-else statements | Conditionals, branches, ternary operator |
| `09_fibonacci.c` | Fibonacci sequence | Iterative algorithm, variable updates |
| `10_struct.c` | Structure usage | struct definition, members, functions |

## Usage

### From File Browser

1. Start the file browser:
   ```bash
   ./filebrowser
   ```

2. Navigate to the `examples/` directory

3. Select any `.c` file

4. Press `d` to compile and debug

5. Press `r` to run, then `n` to step through

### Manual Compilation

You can also compile and run these programs manually:

```bash
gcc -g -O0 -no-pie -o 02_variables 02_variables.c
./02_variables
```

## Learning Path

**Beginner:**
1. Start with `01_simple.c` - Learn basic debugging
2. Try `02_variables.c` - See how variables work
3. Move to `03_loop.c` - Understand loops

**Intermediate:**
4. `04_function.c` - Function calls
5. `06_array.c` - Arrays and iteration
6. `08_conditional.c` - Control flow

**Advanced:**
7. `05_recursive.c` - Recursion
8. `07_pointer.c` - Pointers and memory
9. `10_struct.c` - Complex data structures
10. `09_fibonacci.c` - Algorithm implementation

## Tips

- Use `n` (next) to step through line by line
- Watch the middle panel for program output
- The right panel shows current line and state
- Try modifying these programs and re-debugging them!
