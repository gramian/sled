# SLED - Schemy LISP en DOS

## Version (0.2) Features

* Scheme-like
* LISP Interpreter
* for DOS (or DOSbox)
* in 16-bit Real Mode
* Small Memory Model
* Tail-Call Optimization
* Lexical Scoping
* Mark & Sweep Garbage Collector
* Trampoline Evaluator
* ~1000 Lines of Code (C89)
* 9 Special Forms
* 18 Builtin Functions
* 21 Function Standard Library
* 12K Executable
* Open-Source (0BSD)

## Table of Contents

* [About](#about)
* [Data](#data)
* [Code](#code)
* [System](#system)
* [Index](#index)
* [Usage](#usage)
* [Links](#links)

## About

SLED is a purely symbolic **LISP** (LISt Processor) with functionality (largely)
inspired by **Scheme**. Originally derived from the fantastic
[Kilo LISP](https://t3x.org/klisp), but reduced by some features such as macros,
and enhanced with others. SLED can be classified as an Ur-Lisp.

SLED runs on a **DOS** (Disk Operating System) such as
[FreeDOS](https://freedos.org) or MS-DOS, as well as on DOS-emulators like
[DOSBox](https://dosbox.com), [DOSBox-X](https://dosbox-x.com), or
[DOSBox-Staging](https://www.dosbox-staging.org).

For an overview of provided symbols, special forms, builtin functions, and
standard library see the [index](#index).

* [Source Code Repository](https://codeberg.org/gramian/sled)
* [Release Download](https://codeberg.org/gramian/sled/releases/download/v0.2/SLED-0_2.ZIP)
* [Backup Repository](https://github.com/gramian/sled)

## Data

There are two fundamental data types: **Pairs** and **Atoms** (not pairs). Atoms
come in two variants: **Symbols** and **Closures** (functions).

### Symbols

**Symbols** are unique names and consist of any combination of maximum 16 of
the following characters:

```
a b c d e f g h i j k l m n o p q r s t u v w x y z 1 2 3 4 5 6 7 8 9 0 - . ? _ 
```

where `.` cannot be the leading character.

Additionally, any printable ASCII character can be part of a symbol when
prefixed with the escape character `\` (backslash).

```
This\ is\ a\ sym\!
```

Furthermore, uppercase letters are accepted but converted to lowercase unless
the character is escaped.

### Special Symbols

There are some predefined special symbols managed by SLED, for example `nil`
which means "empty list". See the [index](#index) for details.

#### Quote

A quote means "do not evaluate". Via the `quote` special form a symbol is
registered:

```
(quote sym)
```

For convenience the `'` short form syntax may be used:

```
'sym
```

Essentially quoting declares something as data and not code.

### Pairs

Pairs consist of a **head** and a **tail**, holding an `atom` or a `pair`. A
pair can be created as data using the `.`:

```
'(a . x)
```

or as result of the `cons` builtin function:

```
(cons 'a 'x)
```

Pair elements (head and tail) are immutable.

### Lists

A list is a set of pairs where each tail points to a distinct other pair except
one (the last) whose tail is the `nil` symbol, which is equivalent to `()`. Here
are some lists:

```
nil
'()
'(a . nil)
'(a . (b . nil))
```

A list can be created as data also by:

```
'(a b)
```

or as result of the `list` function:

```
(list 'a 'b)
```

#### Improper List

An improper list does not terminate by `nil`, for example:

```
(a . (b . (c . d)))
```

#### Association List

An association list is a list where each element is a pair (association):

```
((a . x) (b . y) (c . z))
```

The head part of such a pair is called key and the tail is called value.

### S-Expressions

A symbolic expression (S-expression) is a data structure defined as:
An S-expression is either an atom or a pair of S-expressions.
In Lisp, Scheme, and in particular in SLED, S-expressions are used for data and
source code.

### von Neumann Ordinal Numbers

The SLED system does not feature numeric types. Yet natural numbers (positive
integers) can be emulated using lists:

```
'()             ; zero
'(nil)          ; one
'(nil nil)      ; two
'(nil nil nil)  ; three
```

These are so-called von Neumann ordinals. The functions `inc`, `dec`, and
`zero?` facilitate counting tasks.

## Code

### Expressions

Expressions can be evaluated, like:

* Bindings
* Functions
* Special Forms

### Bindings

A binding links a symbol to some data payload, and is created via the `define`
special form:

```
(define a 'x)
```

### Closures

**Closures** are functions together with an environment, and result from the
`lambda` special form:

```
(define fun (lambda (arg1 arg2) (print arg1) (print arg2)))
```

### Function Application

The first element of an unquoted list is interpreted as a function name and the
remaining elements as arguments to that function:

```
(fun arg1 arg2)
```

The function evaluation is eager; so first, the argument expressions are
evaluated, then the function application is using the evaluated arguments.

### Arguments

Arguments are evaluated and passed as a list of values to a function.

This means the function parameters can be set up in various ways:

```
(lambda x ...)          ; x is a list
(lambda (x y) ...)      ; destructured list with elements x and y
(lambda (x y . z) ...)  ; z is a list (which is by default nil)
```

Optional arguments can be passed as a list, like `z` above.

### Recursion

LISPs rely on recursion instead of iteration. Recursion refers to a function
calling itself. Two features of SLED help avoid a stack overflow in deep
recursions: the trampoline evaluator and tail-call optimization (TCO). TCO
works for `lambda`, `let`, and `begin`.

### Errors

An error during evaluation of an expression jumps back to the top-level, where
an error occurance can be tested. An error cannot be caught inside an
expression.

### Builtin Functions

A set of functions is built into the SLED executable to enable interaction with
the system and core functionality, for details see the [index](#index).

### Standard Library

Beyond the core functions a set of typical functions is implemented as a
standard library in the file `sled.scm`. For details see the [index](#index).

#### Custom Standard

Custom functions a user wants to reuse with SLED can be appended to the standard
library.

### Special Forms

Certain forms appear like functions but are not. These so-called special forms
do not follow the function behavior, but use the same syntax as functions.
For example, `if` does not evaluate its arguments before resolving the form. 
For details see the [index](#index).

### Immutability

Special symbols, special forms, builtin functions, and standard library contents
are immutable in SLED and cannot be shadowed. This includes custom extensions
to the standard library.

## System

### File Names

File names should follow DOS 8.3 naming (maximum 8 characters for the file name,
a dot, maximum 3 characters for the file extension).

The recommended file extension for scripts running on this LISP is `.scm` due to
syntactic similarity to `Scheme`; for instance, the standard library is named
`sled.scm`. However, the interpreter does not check the file extension.

### Startup

The first action `sled` takes is loading its standard library, which has
to have the name `sled.scm`. This file may be extended with additional custom
definitions.

All symbols and their values loaded from the standard library become immutable
after they are loaded.

### Command-Line Arguments

The `sled` binary has four mutually exclusive command-line arguments.
The first just displays a help page:

```cmd
C:\> sled /?
```

The second is a path to a Lisp source file to be loaded before the REPL starts,
but after the standard library loaded:

```cmd
C:\> sled code.scm
```

The third is an "batch mode" switch `/b`, which exits after execution:

```cmd
C:\> sled /b code.scm
```

The fourth is a "ignore errors" switch `/i`, which behaves like `/b` but
continues after an error occurs:

```cmd
C:\> sled /i code.scm
```

The file path has to be the last argument.

### File Path

If the path contains backslashes these need to be escaped, since the path
becomes a symbol and the backslash `\` alone is not an admissible symbol
character.

```cmd
C:\> sled to\\my\\code.scm
```

```
(load 'to\\my\\code.scm)
```

### REPL

Once `sled` started, the read-eval-print-loop (REPL) begins with a prompt:

```
sled> 
```

It reads input, evaluates it, prints the result, and prompts again.

Now have fun:

```
sled> (println 'hello\ world)
```

#### Extended Characters

A pitfall is extended (two-byte) characters which are not supported. An example
is using the arrow keys in the REPL, resulting in an `α` (alpha). These extended
characters pollute the input stream and can cause an error in an input line even
if deleted. A new prompt via `CTRL+C` cleans the input stream.

### Exiting

There are two regular ways to exit `sled`.

The first is the dollar symbol `$`, which tells the parser to exit:

```
sled> $
```

The second is the `(exit)` builtin function, which upon evaluation exits:

```
(exit)
```

### Comments

Comments are ignored by the parser. A comment is introduced by a semicolon:

```
;
```

All characters until the next line break are ignored by the parser.
Traditionally, the number of consecutive semicolons convey semantics,
similar to Markdown headings:

```
;;;; main title

;;; section title

;; start of line

; end of line
```

Furthermore, block comments are realized via a special form named `comment`:

```
(comment ...)
```

Parentheses inside a comment form need to be balanced:

```
(comment ())  ; OK
(comment ()   ; → error
```

Note that a comment form cannot be quoted:

```
'(comment test)  ; → error
```

### Breaking

To break pure computation, use `CTRL+Break`, for breaking input `CTRL+C` is
available.

### Help

In the REPL, the symbol `?` can be used to list special forms, builtin
functions, and standard library symbols.

## Index

**Special Symbols**

|                           |                 |                 |
|---------------------------|-----------------|-----------------|
| [`$`](#exit)              | [`?`](#help-1)  | [`_`](#_-space) |
| [`ans`](#ans)             | [`err`](#err)   | [`nil`](#nil)   |
| [`self`](#self-arg1-argn) | [`true`](#true) | [`ver`](#ver)   |

**Special Forms**

|                                           |                             |                                 |
|-------------------------------------------|-----------------------------|---------------------------------|
| [`apply`](#apply-fun-lst)                 | [`begin`](#begin-arg1-argn) | [`comment`](#comment-arg1-argn) |
| [`define`](#define-sym-arg)               | [`if`](#if-arg1-arg2-arg3)  | [`ifnil`](#ifnil-arg1-arg2)     |
| [`lambda`](#lambda-arg1-argn-body1-bodyn) | [`let`](#let)               | [`quote`](#quote-sym)           |

**Builtin Functions**

|                              |                             |                            |
|------------------------------|-----------------------------|----------------------------|
| [`atom?`](#atom-arg)         | [`cons`](#cons-arg1-arg2)   | [`defined?`](#defined-sym) |
| [`empty?`](#empty-arg)       | [`env`](#env)               | [`eof?`](#eof-arg)         |
| [`equiv?`](#equiv-arg1-arg2) | [`error`](#error-sym-arg)   | [`exit`](#exit-1)          |
| [`gc`](#gc-arg)              | [`head`](#head-arg)         | [`load`](#load-sym-arg)    |
| [`newline`](#newline)        | [`print`](#print-arg1-argn) | [`proc?`](#proc-arg)       |
| [`read`](#read)              | [`symbol?`](#symbol-arg)    | [`tail`](#tail-arg)        |

**Standard Aliases**

|                    |                   |                      |
|--------------------|-------------------|----------------------|
| [`nil?`](#nil-arg) | [`not`](#not-arg) | [`zero?`](#zero-arg) |

**Standard Library**

|                                     |                                 |                                  |
|-------------------------------------|---------------------------------|----------------------------------|
| [`and?`](#and-arg1-arg2)            | [`append`](#append-lst1-lst2)   | [`assert`](#assert-arg-sym)      |
| [`compose`](#compose-arg-fun1-funn) | [`equal?`](#equal-arg1-arg2)    | [`dec`](#dec-arg)                |
| [`error?`](#error-1)                | [`get`](#get)                   | [`id`](#id-arg)                  |
| [`inc`](#inc-arg)                   | [`list`](#list-arg1-argn)       | [`list?`](#list-arg)             |
| [`map`](#map-fun-lst)               | [`member`](#member)             | [`or?`](#or-arg1-arg2)           |
| [`pair?`](#pair-arg)                | [`printid`](#printid-arg1-arg2) | [`println`](#println-arg1-argn)  |
| [`put`](#put-lst-sym-arg)           | [`reverse`](#reverse-lst)       | [`shorter?`](#shorter-lst1-lst2) |

---

### `$` {exit}

This **special symbol** exits the REPL. This is **not** a short form of
`(exit)`, as it is resolved by the parser. Works only from the prompt.

---

### `?` {help}

This **special symbol** prints an overview of special forms, builtin functions,
and the standard library. Works only from the prompt.

---

### `_` {space}

This **special symbol** is an alias for a space character `'\ `.

---

### `and? <arg1> <arg2>`

This **standard library** binary predicate answers if both arguments are not
`nil`; use to make compound conditionals simpler.

```
(and? nil nil)    ; → nil
(and? nil true)   ; → nil
(and? true nil)   ; → nil
(and? true true)  ; → true
```

> **NOTE:** Unlike in Scheme, this is not a special form of variadic arguments
            which are evaluated sequentially until a false result, but a
            binary function that evaluates both arguments.

---
            
### `ans`

This **special symbol** contains the result of the previously evaluated
top-level form; in case of an error, the `err` symbol is set, which can be
tested for with `error?`.

```
ans
```

---

### `append <lst1> <lst2>`

This **standard library** binary function returns a list consisting of the
second argument list concatenated to the end of the first argument list.

```
(append nil (list 'a))        ; → (a)
(append (list 'a) nil)        ; → (a)
(append (list 'a) (list 'b))  ; → (a b)
(append '(a b) '(c d))        ; → (a b c d)
```

---

### `apply <fun> <lst>`

This **special form** evaluates the first argument function with the second
argument list as arguments.

```
(apply list '(a b c))  ; → (a b c)
```

---

### `assert <arg> <sym>`

This **standard library** binary procedure prints the second argument and causes
an error, if the first argument evaluates to `nil`.

```
(assert nil 'list\ empty)
```

---

### `atom? <arg>`

This **builtin** unary predicate answers if the argument is not a pair.

```
(atom? nil)   ; → true
(atom? true)  ; → true
(atom? '(a))  ; → nil
```

---

### `begin <arg1> ... <argN>`

This **special form** evaluates its arguments in sequence and returns the last
argument's return value.

```
(begin (print 'a) (print 'b) 'c)  ; → c
(begin)                           ; → nil
```

---

### `comment <arg1> ... <argN>`

This **special form** is ignored by the parser. Use as block comment.

```
(comment this is ignored)
```

> **NOTE:** Parentheses inside comment have to be balanced.

---
 
### `compose <arg> <fun1> ... <funN>`

This **standard library** variadic function pipelines unary functions:
the second argument is applied to the first argument,
the third argument is then applied to the previous return value and so on;
the final argument's return value is returned as result.

```
(compose nil inc inc)  ; → (nil nil)
```

---

### `cons <arg1> <arg2>`

This **builtin** binary function returns a pair with the first argument as head
and second argument as tail.

```
(cons 'a 'b)  ; → (a . b)
```

---

### `dec <arg>`

This is a **standard library** unary function that returns the tail of a list if
not empty; use to decrement von Neumann ordinals.

```
(dec '(nil . nil))  ; → nil
```

---

### `define <sym> <arg>`

This **special form** creates a new binding of the second argument to the first
argument symbol, and returns the second argument. Bindings can be re-`defined`.

```
(define hello 'world)  ; → world
```

> **NOTE:** `define` always affect the global binding, also when used inside
            `let` or `lambda`.

---

### `defined? <sym>`

This **builtin** unary predicate answers if the argument symbol is already
defined.

```
(defined? 'defined?)   ; → true
(defined? 'undefined)  ; → nil
```

---

### `empty? <arg>`

This **builtin** unary predicate answers if its argument is the empty list.

```
(empty? nil)  ; → true
```

---

### `env`

This **builtin** thunk prints the current user-defined symbols.

```
(env)
```

---

### `eof? <arg>`

This **builtin** unary predicate answers if its argument evaluates to an
end-of-file (EOF) or end-of-transmission (EOT) symbol.

```
(ifnil (eof? (read)) 'none)
```

---

### `equal? <arg1> <arg2>`

This **standard library** binary predicate answers if the arguments are
recursively equal. Use to compare pairs and lists.

```
(equal? '(nil nil) (list nil nil))  ; → true
```

---

### `equiv? <arg1> <arg2>`

This **builtin** binary predicate answers if the arguments are shallowly equal.
Use to compare atoms.

```
(equiv? nil nil)  ; → true
```

---

### `err`

This **special symbol** marks an error state.

```
(head nil) (equiv? err ans)  ; → true
```

---

### `error <sym> [<arg>]`

This **builtin** procedure throws an error and thus causes a break in 
evaluation of the current form. Furthermore, the first argument symbol (error
message) and the optional second argument expression (error reason) are printed.

```
(error 'bad\ error (list 'not 'right))
```

---

### `error?`

This **standard library** thunk predicate answers if the previous evaluation
resulted in an error.

```
(define x) (error?)  ; → true
```

---

### `exit`

This **builtin** thunk quits the interpreter or REPL.

```
(exit)  ; back to DOS
```

---

### `gc [<arg>]`

This **builtin** procedure triggers garbage collection. Node usage is printed if
an argument is provided which is not `nil`.

```
(gc)       ; → 
(gc nil)   ; →
(gc true)  ; → (prints node usage)
```

---

### `get <sym> <lst>`

This **standard library** binary function returns the value paired to the first
argument symbol if found in the second argument association list, or `nil`
otherwise.

```
(get 'a '((a . 1) (b . 2)))  ; → 1
(get 'z '((a . 1) (b . 2)))  ; → nil
```

---

### `head <arg>`

This **builtin** unary function returns the head part of a cons cell or list.

```
(head (cons 'a 'b))  ; → a
```

> **NOTE:** This function corresponds to `car` in classic LISP and Scheme.

---

### `id <arg>`

This **standard library** unary function returns its argument. Use as identity
function.

```
(id 'x)  ; → x
```

---

### `if <arg1> <arg2> [<arg3>]`

This **special form** evaluates the first argument; if it does not evaluate to
`nil`, the second argument is evaluated and returned, otherwise the third
argument is evaluated and returned or nothing if no third argument is given.

```
(if 'ok 'con 'alt)  ; → con
(if nil 'con 'alt)  ; → alt
(if 'ok 'con)       ; → con
(if nil 'con)       ; → 
```

---

### `ifnil <arg1> <arg2>`

This **special form** evaluates the first argument and returns its result,
if it is not `nil`, otherwise the second argument is evaluated and returned.

```
(ifnil 'ok 'alt)  ; → ok
(ifnil nil 'alt)  ; → alt
```

---

### `inc <arg>`

This **standard library** unary function prepends `nil` to a list. Use to
increment von Neumann ordinals.

```
(inc nil)     ; → (nil)
(inc '(nil))  ; → (nil nil)
```

---

### `lambda (<arg1> ... <argN>) <body1> ... <bodyN>`

This **special form** creates a function with arguments as destructured list
and a sequentially evaluated body, whose last expression is the return value.

```
(lambda (x y) (print x) y)  ; → function that prints x and returns y
```

---

### `let (<arg1> <arg2>) <body1> ... <bodyN>`

This **special form** creates a scope with a local binding and evaluates its
body sequentially.

```
(let (x 'y) x)  ; → y
```

> **NOTE:** Unlike Scheme, this special form allows only a single local binding.

---

### `list <arg1> ... <argN>`

This **standard library** variadic function constructs a list of its arguments.

```
(list)        ; → nil
(list 'a)     ; → (a)
(list 'a 'b)  ; → (a b)
```

---

### `list? <arg>`

This **standard library** unary predicate answers if the argument is a proper
list.

```
(list? nil)     ; → true
(list? true)    ; → nil
(list? '(x y))  ; → true
```

---

### `load <sym> [<arg>]`

This **builtin** unary function evaluates the contents of the file given by the
argument symbol (path) and returns the last answer. If a second argument which
is not `nil` is given then errors are ignored during loading.

```
(load 'myscript.scm)
(load 'myscript.scm true)
```

> **NOTE:** Loads can be nested twice at most.

---

### `map <fun> <lst>`

This **standard library** binary function applies the first argument unary
function to each element of the second argument list and returns the list of
return values.

```
(map inc '(nil (nil)))  ; → ((nil) (nil nil))
```

---

### `member <arg> <lst>`

This **standard library** binary function returns the pair from the second
argument list whose head is the first argument; otherwise `nil` is returned.

```
(member 'b '(a b c))  ; → (b c)
(member 'd '(a b c))  ; → nil
```

---

### `newline`

This **builtin** thunk prints a line break; use for output formatting.

```
(newline)
```

---

### `nil`

This **special symbol** represents the empty list; and is also the only symbol
evaluating to false.

```
nil  ; → nil
```

---

### `nil? <arg>`

This is a **standard alias** for `empty?`; use as test for `nil`.

```
(nil? nil)     ; → true
(nil? _)       ; → nil
(nil? (list))  ; → true
```

---

### `not <arg>`

This is a **standard alias** for `empty?`; use for inverting predicate results.

```
(not nil)   ; → true
(not true)  ; → nil
```

---

### `or? <arg1> <arg2>`

This **standard library** binary predicate answers if any argument is not `nil`;
use to simplify compound conditionals.

```
(or? nil nil)    ; → nil
(or? nil true)   ; → true
(or? true nil)   ; → true
(or? true true)  ; → true
```

> **NOTE:** Unlike in Scheme, this is not a special form of variadic arguments
            which are evaluated sequentially until a true result, but a
            binary function that evaluates both arguments.

---
            
### `pair? <arg>`

This **standard library** unary predicate answers if the argument is not an
atom.

```
(pair? 'a)      ; → nil
(pair? '(a b))  ; → true
(pair? nil)     ; → nil
```

---

### `print <arg1> ... <argN>`

This **builtin** variadic procedure prints its arguments to the standard output.

```
(print 'hi)
(print 'hello _ 'world)
(print '(a b))
```

---

### `printid <arg1> [<arg2>]`

This **standard library** function returns the first argument after printing
it and a line break; if given the second argument is printed before the first.

```
(printid ans 'answer)
```

---

### `println <arg1> ... <argN>`

This **standard library** variadic procedure prints its arguments to the
standard output and appends a line break.

```
(println 'hi)
(println 'hello _ 'world)
(println '(a b))
```

---

### `proc? <arg>`

This **builtin** unary predicate answers if its argument is the return value of
a lambda. For special forms and builtin functions this predicate returns `nil`.

```
(proc? map)    ; → true
(proc? nil)    ; → nil
(proc? proc?)  ; → nil
```

---

### `put <lst> <sym> <arg>`

This **standard library** function returns an updated first argument association
list by setting the tail of the pair with the second argument symbol as head, or
adding a pair with the second argument symbol as head and the third argument as
tail, if no pair with the second argument symbol as head is listed in the first
argument.

```
(put nil 'hello 'world)       ; → ((hello . world))
(put '((one . 1)) 'one 'uno)  ; → ((one . uno))
(put '((one . 1)) 'two '2)    ; → ((two . 2) (one . 1))
```

---

### `quote <sym>`

This **special form** returns its argument unevaluated. The `'` (single quote)
can be used as short syntax to create symbols.

```
(quote a)  ; → a
'a         ; → a
```

> **NOTE:** The symbol length is limited to 16 characters.

---

### `read`

This **builtin** thunk reads a line of input from the standard input source
terminated by a line break via return key / enter key.

```
(read)
```

---

### `reverse <lst>`

This **standard library** unary function reverses its list argument.

```
(reverse (list 'a 'b 'c))  ; → (c b a)
```

---

### `self <arg1> ... <argN>`

This **special symbol** enables anonymous recursion. Inside any closure it
resolves to the enclosing `lambda`. Outside a `lambda`, `self` is undefined.

```
((lambda (x)
  (if (empty? x) nil
                 (self (tail x)))) (list nil nil))  ; → nil
```

---

### `shorter? <lst1> <lst2>`

This **standard library** binary predicate answers if the first argument list
has less elements than the second argument list.

```
(shorter? nil (list nil))  ; → true
(shorter? (list nil) nil)  ; → nil
```

---

### `symbol? <arg>`

This **builtin** unary predicate answers if its argument is a symbol.

```
(symbol? 'a)  ; → true
```

---

### `tail <arg>`

This **builtin** unary function returns the tail part of a cons cell.

```
(tail (cons 'a 'b))  ; → b
```

> **NOTE:** This function corresponds to `cdr` in classic LISP and Scheme.

---

### `true`

This **special symbol** evaluates to true. Use as a generic true value.

```
true  ; → true
```

---

### `ver`

This **special symbol** evaluates to a symbol pinpointing the version of SLED.

```
ver  ; → sled-0.2
```

---

### `zero? <arg>`

This is a **standard alias** for `empty?`; use for testing von Neumann ordinals.

```
(zero? nil)    ; → true
(zero? (nil))  ; → nil
```

## Usage

* SLED is made for a disk operating system like **FreeDOS** or **MS-DOS**
* Outside DOS, a DOS emulator like **DOSBox**, **DOSBox-X**, **DOSBox-Staging** is required
* Run the shell script `./sled.sh` (auto-selects the installed DOSBox) on Linux, BSD, or MacOS
* Building SLED requires [Microsoft C Compiler](https://github.com/davidly/dos_compilers) or [Open Watcom](https://github.com/open-watcom/open-watcom-v2), as well as `make`
* Build with MS C 6.0A: `make build_msc` (Compiler location via `MSC`)
* Build with Open Watcom V2: `make build_owc` (Compiler location via `OWC`)
* Run tests: `make tests`
* Run benchmark: `make bench`
* Run build `make run`

## Links

* [DOS](https://permacomputing.net/DOS/)

* [Kilo LISP](https://t3x.org/klisp/)

* [PC Scheme](https://conservatory.scheme.org/pcs/)

* [The implementation of PC Scheme](https://doi.org/10.1145/319838.319852)

* [A Comparison of Three LISP Interpreters for MS-DOS-Based Microcomputers](https://pmc.ncbi.nlm.nih.gov/articles/PMC2577970/)

* [Free Lisp development environments for DOS](https://web.archive.org/web/20251225014422/https://www.streetinfo.lu/computing/programming/dos/dos_lisp.html)

* [S-expressions](https://web.archive.org/web/20251011064346/https://www.s-expressions.org/home)

---

This project is licensed under the 0BSD (Zero-Clause BSD) license.
