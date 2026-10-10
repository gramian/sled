# SLED - Schemy LISP en DOS

## Version (0.6) Features

* Scheme-inspired
* LISP Interpreter
* for DOS (or DOSbox)
* in 16-bit Real Mode
* Small Memory Model
* Tail-Call Optimization
* Lexical Scoping
* Mark & Sweep Garbage Collector
* Trampoline Evaluator
* ~1.3K Lines of Code (C89)
* 9 Special Forms
* 21 Builtin Functions
* 21 Function Standard Library
* 13K Executable
* Open-Source (0BSD)

## Table of Contents

* [About](#about)
* [Data](#data)
* [Code](#code)
* [System](#system)
* [Index](#index)
* [Usage](#usage)
* [Links](#links)
* [Extra](#extra)

## About

SLED (**S**chemy **L**isp **e**n **D**OS) is a purely symbolic **LISP** (LISt
Processor) with functionality (largely) inspired by **Scheme**. Originally
derived from the fantastic [Kilo LISP](https://t3x.org/klisp), with some
features (such as macros) removed, and enhanced with others, SLED can be
classified as an Ur-Lisp and runs on a **DOS** (Disk Operating System) such as
[FreeDOS](https://freedos.org) or MS-DOS, as well as on DOS-emulators like
[DOSBox](https://dosbox.com), [DOSBox-X](https://dosbox-x.com), or
[DOSBox-Staging](https://www.dosbox-staging.org).
For an overview of provided symbols, special forms, builtin functions, and
standard library see the [index](#index).
Get SLED:

* [Release Download (including compiled binary)](https://codeberg.org/gramian/sled/releases/latest)
* [Source Code Repository](https://codeberg.org/gramian/sled)
* [Backup Repository](https://github.com/gramian/sled)

Overall, SLED is a LISP for DOS.

### Language

Lists are a computer language in a most general sense: they can be a programming
language, a data format, or a serialization. Thus lists are a fundamental
representation of data and code. In Lisp, a list is enclosed by parentheses `(`,
`)`, and list elements are delimited by a space ` `.

```
(begin lisp)
```

If a list represents code, its first element denotes the action (function),
as in Polish notation or verb-subject-object languages in general. LISP is such
a computer language. Typically, Scheme LISP classifies as a functional
programming language.

## Data

There are two fundamental data types: **Pairs** and **Atoms** (not-pairs). Atoms
come in three variants: **Symbols**, **Closures** (functions), and some
**Special Symbols**.

### Symbols

**Symbols** are unique names and consist of one to sixteen of the following
forty characters:

```
a b c d e f g h i j k l m n o p q r s t u v w x y z 0 1 2 3 4 5 6 7 8 9 _ - ? .
```

where an unescaped `.` cannot be the leading character.

Additionally, most ASCII character can be part of a symbol when prefixed with
the escape character `\` (backslash), with the exception of `(`, `)`, `'`, and
`$` which cannot be escaped. However, using printable ASCII characters for
symbols is recommended.

```
This\ is\ a\ sym\!
```

Furthermore, uppercase letters are accepted but converted to lowercase unless
the character is escaped.

### Special Symbols

There are some predefined special symbols managed by SLED, for example `nil`
which means "empty list". See the [index](#index) for details.

#### Quote

A quote means "do not evaluate" and is declared by the `quote` special form:

```
(quote sym)
```

For convenience the `'` short form syntax may be used:

```
'sym
```

Essentially quoting declares something as data instead of code.

#### Empty List

The `nil` special symbol has multiple meanings: It represents the empty list, so
it is equivalent to `'()`; it is also the only value evaluating to "false".

### Pairs

Pairs consist of a **head** and a **tail**, each holding either an `atom` or
another `pair`. A pair can be created as data using the `.` (dot):

```
'(a . x)
```

or as the result of the `cons` builtin function:

```
(cons 'a 'x)
```

Pair elements (head and tail) are immutable.

### Lists

A list is a sequence of pairs where each tail points to a distinct other pair
except one (the last) whose tail is the `nil` value, which is equivalent to
`()`. Here are some lists:

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

or as the result of the (standard library) `list` function:

```
(list 'a 'b)
```

#### Improper List

An improper list does not terminate in `nil`, for example:

```
(a . (b . (c . d)))
```

#### Association List

An association list is a list where each element is a pair (association):

```
((a . x) (b . y) (c . z))
```

The head part of such a pair element is called **key** and the tail is called
**value**.

### S-Expressions

A symbolic expression (S-expression) is a data structure defined as:
An S-expression is either an atom or a pair of S-expressions.
In Lisp, Scheme, and in particular in SLED, S-expressions are used for data as
well as source code.

### Numbers

The SLED system does not feature numeric types. Yet natural numbers
(non-negative integers) can be emulated using lists:

```
'()             ; zero
'(nil)          ; one
'(nil nil)      ; two
'(nil nil nil)  ; three
```

These are tally numerals, so cardinality represents the magnitude, which is
similar to unary tally numbers. The functions `inc`, `dec`, and `zero?`
help with counting tasks.

## Code

In LISP, unquoted data is evaluated as code.

### Expressions

Expressions can be evaluated, like:

* Bindings
* Functions
* Special Forms

### Bindings

A **binding** links a symbol to some data payload, and is created via the
`define` special form:

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

The first element of an unquoted list is interpreted as an expression that
evaluates to a function and the remaining elements as arguments to that
function:

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
works for `lambda`, `let`, `begin`, `if`, `ifnil`, and `apply`.
Additionally, TCO works for `cons` if the recursion runs in the second argument.

### Errors

An error during evaluation of an expression jumps back to the top-level, where
an error occurrence can be tested. An error cannot be caught inside an
expression except for errors inside the `load` function.

### Builtin Functions

A set of functions is built into the SLED executable to enable interaction with
the system and core functionality, for details see the [index](#index).

### Standard Library

Beyond the core functions a set of typical functions is implemented as a
standard library in the file `sled.scm`. For details see the [index](#index).
The standard library may be extended with additional custom definitions.

### Special Forms

Certain forms appear like functions but are not. These so-called special forms
do not follow the function behavior, but use the same syntax as functions.
For example, `if` does not evaluate its arguments before resolving the form.
Furthermore, special forms must appear in the first position, and cannot neither
be argument to `apply` nor aliased. For details see the [index](#index).

### Immutability

Special symbols, special forms, builtin functions, and standard library contents
are immutable in SLED. Furthermore, special forms and builtin functions cannot
be shadowed. Standard library and adjacent custom definitions cannot be
redefined.

## System

The SLED LISP system is a DOS application.

### File Names

File names should follow DOS 8.3 naming (maximum 8 characters for the file name,
a dot, maximum 3 characters for the file extension).

The recommended file extension for scripts running on this LISP is `.scm` due to
syntactic similarity to `Scheme`; for instance, the standard library is named
`sled.scm`. However, the interpreter does not check the file extension.

### Startup

The first action `sled` takes is loading its standard library, which has
to have the name `sled.scm` and is expected in the same directory as the
`SLED.EXE` interpreter executable. All symbols and their values loaded from the
standard library become immutable.

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

The third is a "batch mode" switch `/B`, which exits after execution:

```cmd
C:\> sled /B code.scm
```

The fourth is an "ignore errors" switch `/I`, which behaves like `/B` but
continues execution after an error occurs:

```cmd
C:\> sled /I code.scm
```

The file path has to be the last argument and extra arguments are ignored.

### File Path

In the interpreter, if the path contains backslashes these need to be escaped,
since the path becomes a symbol and the backslash `\` alone is not an admissible
symbol character.

```cmd
C:\> sled to\my\code.scm
```

```
(load 'to\\my\\code.scm)
```

The file path also falls under the 16 character limit.

### REPL

Once `sled` has started, the read-eval-print-loop (REPL) begins with a prompt:

```
sled>
```

It reads input, evaluates it, prints the result, and prompts again.

Now have fun:

```
sled> (println 'hello _ 'world)
```

#### Extended Characters

A pitfall is extended (two-byte) characters which are not supported. An example
is using the arrow keys in the REPL, resulting in an `α` (alpha) in the standard
input echo. These extended characters pollute the input stream and can cause an
error in an input line even if deleted. Hence, a line containing an extended
character throws an error and discards the line.

### Exiting

There are two regular ways to exit `sled`.

The first is the dollar symbol `$` on the top-level, which tells the parser to
exit:

```
sled> $
```

The second is the `(exit)` builtin function, which upon evaluation exits:

```
(exit)
```

`quit` is an alias for `exit`.

### Comments

Comments are ignored by the parser. A comment is introduced by a semicolon:

```
;
```

All characters until the next line break are ignored by the parser.
Traditionally, the number of consecutive semicolons conveys semantics,
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

Note that a comment form cannot be quoted directly:

```
'(comment test)           ; → error
'(a (comment ignored) b)  ; (a b)
```

### Breaking

To break pure computation, use `CTRL+Break`, for breaking input `CTRL+C` is
available.

### Help

In the REPL, the symbol `?` can be used to list special forms, builtin
functions, and standard library symbols.

### Limits

As a real-mode DOS program, SLED has multiple constraints:

* The heap has 12288 nodes
* The symbol table has 4096 characters

The standard library consumes about 5% of nodes and characters.

## Index

**Special Symbols**

|                           |                 |                 |
|---------------------------|-----------------|-----------------|
| [`$`](#exit)              | [`?`](#help-1)  | [`'`](#quote-1) |
| [`ans`](#ans)             | [`err`](#err)   | [`nil`](#nil)   |
| [`self`](#self-arg1-argn) | [`true`](#true) | [`ver`](#ver)   |

**Special Forms**

|                                     |                                 |                                           |
|-------------------------------------|---------------------------------|-------------------------------------------|
| [`begin`](#begin-body1-bodyn)       | [`comment`](#comment-arg1-argn) | [`define`](#define-sym-arg)               |
| [`if`](#if-arg1-arg2-arg3)          | [`ifnil`](#ifnil-arg1-arg2)     | [`lambda`](#lambda-arg1-argn-body1-bodyn) |
| [`let`](#let-arg1-arg2-body1-bodyn) | [`quote`](#quote-arg)           | [`value`](#value-sym)                     |

**Builtin Functions**

|                            |                              |                             |
|----------------------------|------------------------------|-----------------------------|
| [`apply`](#apply-fun-lst)  | [`atom?`](#atom-arg)         | [`cons`](#cons-arg1-arg2)   |
| [`defined?`](#defined-sym) | [`empty?`](#empty-arg)       | [`env`](#env)               |
| [`eof?`](#eof-arg)         | [`equiv?`](#equiv-arg1-arg2) | [`error`](#error-sym-arg)   |
| [`exit`](#exit-1)          | [`gc`](#gc-arg)              | [`head`](#head-arg)         |
| [`load`](#load-sym-arg)    | [`newline`](#newline)        | [`print`](#print-arg1-argn) |
| [`proc?`](#proc-arg)       | [`read`](#read)              | [`restart`](#restart-sym)   |
| [`scm`](#scm-arg1-argn)    | [`symbol?`](#symbol-arg)     | [`tail`](#tail-arg)         |

**Standard Aliases**

|                   |                 |                      |
|-------------------|-----------------|----------------------|
| [`_`](#_-space)   | [`br`](#br)     | [`nil?`](#nil-arg)   |
| [`not`](#not-arg) | [`quit`](#quit) | [`zero?`](#zero-arg) |

**Standard Library**

|                                     |                                 |                                  |
|-------------------------------------|---------------------------------|----------------------------------|
| [`and?`](#and-arg1-arg2)            | [`append`](#append-lst1-lst2)   | [`assert`](#assert-arg-sym)      |
| [`compose`](#compose-arg-fun1-funn) | [`dec`](#dec-arg)               | [`equal?`](#equal-arg1-arg2)     |
| [`error?`](#error)                  | [`get`](#get-sym-lst)           | [`id`](#id-arg)                  |
| [`inc`](#inc-arg)                   | [`list`](#list-arg1-argn)       | [`list?`](#list-arg)             |
| [`map`](#map-fun-lst)               | [`member`](#member-arg-lst)     | [`or?`](#or-arg1-arg2)           |
| [`pair?`](#pair-arg)                | [`printid`](#printid-arg1-arg2) | [`println`](#println-arg1-argn)  |
| [`put`](#put-sym-arg-lst)           | [`revpend`](#revpend-lst1-lst2) | [`shorter?`](#shorter-lst1-lst2) |

---

### `$` {exit}

This **special symbol** exits the REPL. This is **not** a short form of
`(exit)`, as it is resolved by the parser. In a loaded file it ends loading
of that file, ignoring everything after it.

---

### `?` {help}

This **special symbol** prints an overview of special forms, builtin functions,
and the standard library. Works only from the REPL and precedes evaluation.

---

### `'` {quote}

This **special symbol** is an alias for the `quote` special form.

---

### `_` {space}

This **standard alias** is for the space character `'\ `.

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

This **special symbol** contains the result of the last top-level form that
produced a value; in case of an error, `ans` is set to `err`, which can be
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
(append '(a) 'b)              ; → (a . b)
```

---

### `apply <fun> <lst>`

This **builtin** binary function evaluates the first argument function with the
second argument list as arguments.

```
(apply list '(a b c))  ; → (a b c)
```

---

### `assert <arg> <sym>`

This **standard library** binary procedure prints the second argument and causes
an error, if the first argument evaluates to `nil`, otherwise the first argument
is returned.

```
(assert nil 'list\ empty)
```

---

### `atom? <arg>`

This **builtin** unary predicate answers if the argument is an atom.

```
(atom? nil)   ; → true
(atom? true)  ; → true
(atom? '(a))  ; → nil
```

---

### `begin <body1> ... <bodyN>`

This **special form** evaluates its arguments in sequence and returns the last
argument's return value.

```
(begin (print 'a) (print 'b) 'c)  ; → c
(begin)                           ; → nil
```

---

### `br`

This **standard alias** is for `newline`.

---

### `comment <arg1> ... <argN>`

This **special form** is not evaluated. Use as block comment.
Unlike the other special forms, this is decoded by the parser before evaluation.

```
(comment this :-D is ignored)
```

> **NOTE:** Parentheses inside `comment` have to be balanced.

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

This **standard library** unary function returns the tail of a list if not
empty; use to decrement tally numbers.

```
(dec '(nil . nil))  ; → nil
```

---

### `define <sym> <arg>`

This **special form** creates a new binding of the second argument to the first
argument symbol, and returns the second argument. Bindings can be re-`defined`,
except special symbols, special forms, builtin functions, or standard library
symbols.

```
(define hello 'world)  ; → world
```

> **NOTE:** `define` always affects the global binding, also when used inside
            `let` or `lambda`.

---

### `defined? <sym>`

This **builtin** unary predicate answers if the argument symbol is already
defined at global scope.

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
(env)  ; → nil
```

---

### `eof? <arg>`

This **builtin** unary predicate answers if its argument evaluates to an
end-of-file (EOF) or end-of-transmission (EOT) marker.

```
(ifnil (eof? (read)) 'none)
```

---

### `equal? <arg1> <arg2>`

This **standard library** binary predicate answers if the arguments are
recursively equal. Use to compare pairs and lists, however `equal?` falls back
to `equiv?` for atoms.

```
(equal? '(nil nil) (list nil nil))  ; → true
(equal? nil nil)                    ; → true
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
(head nil)        ; → ? type
(equiv? err ans)  ; → true
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
(head nil)  ; → ? type
(error?)    ; → true
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
(gc)       ; → nil
(gc nil)   ; → nil
(gc true)  ; → nil
```

---

### `get <sym> <lst>`

This **standard library** binary function returns the value paired to the first
argument symbol if found in the second argument association list, or `nil`
otherwise.

```
(get 'a '((a . x) (b . y)))  ; → x
(get 'z '((a . x) (b . y)))  ; → nil
```

> **NOTE:** Uses `equiv?` for key comparisons.

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
argument is evaluated and returned, or `nil` if no third argument is given.

```
(if 'ok 'con 'alt)  ; → con
(if nil 'con 'alt)  ; → alt
(if 'ok 'con)       ; → con
(if nil 'con)       ; → nil
```

---

### `ifnil <arg1> <arg2>`

This **special form** evaluates the first argument and returns its result
if it is not `nil`, otherwise the second argument is evaluated and returned.

```
(ifnil 'ok 'alt)  ; → ok
(ifnil nil 'alt)  ; → alt
```

---

### `inc <arg>`

This **standard library** unary function prepends `nil` to a list. Use to
increment tally numbers.

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
            Multiple bindings can be realized by nesting `let`s.

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

This **builtin** function evaluates the contents of the file given by the
argument symbol (path) and returns the last answer. If a second argument, which
is not `nil`, is given then errors are ignored during loading.

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

> **NOTE:** Uses `equiv?` for list element comparisons.

---

### `newline`

This **builtin** thunk prints a line break; use for output formatting.

```
(newline)  ; → nil
```

---

### `nil`

This **special symbol** represents the empty list; and is also the only value
evaluating to "false". Equivalently, `'()` can be used for `nil`.

```
nil  ; → nil
'()  ; → nil
```

---

### `nil? <arg>`

This is a **standard alias** for `empty?`; use as test for `nil`.

```
(nil? nil)     ; → true
(nil? '())     ; → true
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
(print 'hi)              ; → nil
(print 'hello _ 'world)  ; → nil
(print '(a b))           ; → nil
```

---

### `printid <arg1> [<arg2>]`

This **standard library** function returns the first argument after printing
it and a line break; if given, the second argument is printed before the first,
followed by `: `.

```
(printid '42 'answer)  ; → 42
```

---

### `println <arg1> ... <argN>`

This **standard library** variadic procedure prints its arguments to the
standard output and appends a line break.

```
(println 'hi)              ; → nil
(println 'hello _ 'world)  ; → nil
(println '(a b))           ; → nil
```

---

### `proc? <arg>`

This **builtin** unary predicate answers if its argument is a closure or builtin
function. For special forms, this predicate returns `nil`.

```
(proc? map)    ; → true
(proc? proc?)  ; → true
(proc? nil)    ; → nil
(proc? if)     ; → nil
```

---

### `put <sym> <arg> <lst>`

This **standard library** function returns a copy of the third argument
association list but setting the tail of the first matching pair with the first
argument symbol as head, or adding a pair with the first argument symbol as head
and the second argument as tail, if no pair with the first argument symbol as
head is listed in the third argument.

```
(put 'hello 'world nil)               ; → ((hello . world))
(put 'one 'uno '((one . a)))          ; → ((one . uno))
(put 'two 'b '((one . a)))            ; → ((one . a) (two . b))
(put 'two 'z '((one . a) (two . b)))  ; → ((one . a) (two . z))
```

---

### `quit`

This **standard alias** is for `exit`.

---

### `quote <arg>`

This **special form** returns its argument unevaluated.

```
(quote a)  ; → a
'a         ; → a
```

---

### `read`

This **builtin** thunk reads one symbol of input from the keyboard terminated by
a white space or line break via return key / enter key and returns it.

```
(read)
```

> **NOTE:** `read` prompts with `'` and reads one symbol from the keyboard - not
            standard input. It re-prompts on empty or invalid input instead of
            erroring and returns the `EOT` on `Ctrl+Z` (EOF).

---

### `restart [<sym>]`

This **builtin** function resets and restarts the interpreter and optionally
loads a file specified by the symbol argument. All definitions are lost!

```
(restart 'next.scm)
```

---

### `revpend <lst1> [<lst2>]`

This **standard library** binary function reverses its first argument list and
appends the second argument list. Use also to reverse a list.

```
(revpend (list 'a 'b 'c) '(d e))  ; → (c b a d e)
(revpend (list 'a 'b 'c))         ; → (c b a)
```

---

### `scm <arg1> ... <argN>`

This **builtin** function provides "super character mode" for low-resolution
graphics programming.

For details see the [`scm` documentation](#super-character-mode).

---

### `self <arg1> ... <argN>`

This **special symbol** enables anonymous recursion. Inside any closure it
resolves to the enclosing `lambda`. Outside a `lambda`, `self` is undefined.

```
((lambda (x)
  (if (empty? x) nil
                 (self (tail x)))) (list nil nil))  ; → nil
```

> **NOTE:** `self` always refers to the innermost `lambda`.

---

### `shorter? <lst1> <lst2>`

This **standard library** binary predicate answers if the first argument list
has fewer elements than the second argument list.

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

This **special symbol** evaluates to true. Use as a generic "true" value.

```
true  ; → true
```

---

### `value <sym>`

This **special form** evaluates its argument, which must yield a symbol, and
returns that symbol's value. Respects lexical scope.

```
(value 'ver)  ; → sled-0.6
```

---

### `ver`

This **special symbol** evaluates to a symbol identifying the version of SLED.

```
ver  ; → sled-0.6
```

---

### `zero? <arg>`

This is a **standard alias** for `empty?`; use for testing tally numbers.

```
(zero? nil)     ; → true
(zero? '(nil))  ; → nil
```

## Usage

* SLED is made for a disk operating system like **FreeDOS** or **MS-DOS** (>=3)
* Outside DOS, a DOS emulator like **DOSBox**, **DOSBox-X**, **DOSBox-Staging** is required
* No libc, all I/O goes through `int 21h` directly
* Uses real mode "small memory model" (64K data segment and 64K code segment)
* Minimally, a 286 CPU with 256K RAM should be sufficient
* Run the shell script `./sled.sh` (auto-selects the installed DOSBox) on Linux, BSD, macOS, or Unix
* Some sample code is in the `demo` folder; for example, run `(load 'demo\\fractal.scm)`
* Building SLED requires [Microsoft C Compiler](https://github.com/davidly/dos_compilers) or [Open Watcom](https://github.com/open-watcom/open-watcom-v2), as well as `make`
* Build with MS C 6.0A: `make build_msc` (Compiler location via `MSC`)
* Build with Open Watcom v2: `make build_owc` (Compiler location via `OWC`)
* Run build: `make run`
* Run tests: `make tests`
* Run benchmark: `make bench` (Takeuchi function, see [this](https://archive.org/details/AcornUser047-Jun86/page/n179/mode/2up) and [that](https://archive.org/details/AcornUser052-Nov86/page/n197/mode/2up))

## Links

* [DOS](https://permacomputing.net/DOS/)

* [Kilo LISP](https://t3x.org/klisp/)

* [PC Scheme](https://conservatory.scheme.org/pcs/)

* [The implementation of PC Scheme](https://doi.org/10.1145/319838.319852)

* [A Comparison of Three LISP Interpreters for MS-DOS-Based Microcomputers](https://pmc.ncbi.nlm.nih.gov/articles/PMC2577970/)

* [Free Lisp development environments for DOS](https://web.archive.org/web/20251225014422/https://www.streetinfo.lu/computing/programming/dos/dos_lisp.html)

* [S-expressions](https://web.archive.org/web/20251011064346/https://www.s-expressions.org/home)

## Extra

### Super Character Mode

SCM (Super Character Mode) is a graphics extension for SLED and is meant for
DOS (event-driven) game development. SCM is inspired by C64's
[standard character mode](https://www.c64-wiki.com/wiki/Standard_Character_Mode),
uses BIOS video and keyboard services, and writes to video memory directly.

* SCM provides pixel graphics in 80x48@16 (80 by 48 pixels at 16 colors).
* This builtin extension uses text mode 3 (80x25@16),
* which requires EGA or VGA,
* and needs to be set before starting SLED (DOS default video mode).
* Using extended ASCII (CP437) character 223 `▀` (upper half block),
* each text row becomes two rows of pixels:
* the foreground color sets the upper pixel,
* the background color sets the lower pixel.
* The screen's bottom row is excluded and can be used as a status text line.
* A restart of SLED also resets the SCM extension.

#### Color Palette

The color palette is the default 16 EGA colors:

| Hex | Color          | RGB       | Pixel                                 |
|:---:|----------------|-----------|:-------------------------------------:|
|   0 | Black          | `#000000` | <span style="color:#000000">██</span> |
|   1 | Blue           | `#0000AA` | <span style="color:#0000AA">██</span> |
|   2 | Green          | `#00AA00` | <span style="color:#00AA00">██</span> |
|   3 | Cyan           | `#00AAAA` | <span style="color:#00AAAA">██</span> |
|   4 | Red            | `#AA0000` | <span style="color:#AA0000">██</span> |
|   5 | Magenta        | `#AA00AA` | <span style="color:#AA00AA">██</span> |
|   6 | Brown          | `#AA5500` | <span style="color:#AA5500">██</span> |
|   7 | Light Gray     | `#AAAAAA` | <span style="color:#AAAAAA">██</span> |
|   8 | Dark Gray      | `#555555` | <span style="color:#555555">██</span> |
|   9 | Bright Blue    | `#5555FF` | <span style="color:#5555FF">██</span> |
|   A | Bright Green   | `#55FF55` | <span style="color:#55FF55">██</span> |
|   B | Bright Cyan    | `#55FFFF` | <span style="color:#55FFFF">██</span> |
|   C | Bright Red     | `#FF5555` | <span style="color:#FF5555">██</span> |
|   D | Bright Magenta | `#FF55FF` | <span style="color:#FF55FF">██</span> |
|   E | Yellow         | `#FFFF55` | <span style="color:#FFFF55">██</span> |
|   F | White          | `#FFFFFF` | <span style="color:#FFFFFF">██</span> |

#### `hex` Image File Format

* SCM reads images in the `hex`-format.
* A `.hex` file is a plain text file.
* It consists of lines of text of the same length,
* each character corresponds to a pixel,
* allowed characters: `0`,`1`,`2`,`3`,`4`,`5`,`6`,`7`,`8`,`9`,`A`,`B`,`C`,`D`,`E`,`F`,
* in particular only uppercase letters for A-F,
* and `.` for a transparent pixel.
* All lines need to have the same length.
* Maximum size is 80x48.
* Blank lines are ignored.

Example image:

```
........
..EEEE..
.ECEECE.
.EEEEEE.
.E0EE0E.
.EE00EE.
..EEEE..
........
```

#### Edge Stacking

* The `draw` command draws to the double buffer (an offscreen area in VRAM).
* An image can be drawn to the left, right, top, bottom edge, or to the center;
* in any case the image is centered with respect to the edge.
* Images are clipped against the screen edges,
* and may optionally be horizontally flipped.
* Images stack inwards,
* so the next image drawn to the same edge is offset by the width or height of the previous.
* The stack front can be reset manually (`reset` command),
* or is reset when flushed to screen (`show` command).
* Drawing to the center does not change edge stacks.

#### Double Buffering

* The `show` command renders to the screen.
* Images are drawn to the double buffer.
* The double buffer is then flushed to the screen buffer.
* The screen buffer is text page 0 (`B800:0000`),
* the double buffer starts on text page 1 (`B800:1000`),
* the offscreen buffer then starts at `B800:1F00` and is 24K.

#### Status Row

* The bottom row can be used for printing text.
* Avoid using `newline`, as that would scroll the screen.
* The status row can use 79 characters before scrolling.

#### Key Polling

The `key` command waits for one of the following keys to be pressed:

| Key         | Symbol |
|-------------|--------|
| Escape      |  `es`  |
| Enter       |  `en`  |
| Up-Arrow    |  `up`  |
| Down-Arrow  |  `dn`  |
| Left-Arrow  |  `lt`  |
| Right-Arrow |  `rt`  |
| z           |  `b1`  |
| x           |  `b2`  |

#### Commands

|                                         |                         |                                    |
|-----------------------------------------|-------------------------|------------------------------------|
| [`scm 'draw`](#scm-draw-sym1-sym2-sym3) | [`scm 'key`](#scm-key)  | [`scm 'load`](#scm-load-sym1-sym2) |
| [`scm 'reset`](#scm-reset)              | [`scm 'set?`](#scm-set) | [`scm 'show`](#scm-show)           |

---

##### `scm 'draw <sym1> <sym2> [<sym3>]`

This **scm** command draws a loaded image identified by the first symbol argument
to the double buffer. It is stacked on the left (`'l`), right (`'r`),
top (`'t`), bottom (`'b`), or centered (`'c`) specified by the second symbol
argument, and optionally flipped via a non-`nil` third symbol argument. On
success `nil` is returned.

```
(scm 'draw 'img 'c)  ; → nil
```

---

##### `scm 'key`

This **scm** command waits for one of eight keys and returns an associated symbol:
"Escape" `'es`, "Enter", `'en`, "Up" `'up`, "Down" `'dn`, "Left" `'lt`,
"Right" `'rt`, "z", `'b1`, "x", `'b2`.

```
(scm 'key)  ; → es
```

---

##### `scm 'load <sym1> <sym2>`

This **scm** command loads an image with the file path and associates the second
symbol argument to the symbol in the first argument. On success `nil` is
returned.

```
(scm 'load 'img 'img.hex)  ; → nil
```

---

##### `scm 'reset`

This **scm** command resets the edge counters.

```
(scm 'reset)  ; → nil
```

---

##### `scm 'set?`

This **scm** command answers if the required text mode is set. Use to detect if
`scm` is available and ready.

```
(scm 'set?)  ; → true
```

---

##### `scm 'show`

This **scm** command copies the double buffer to the screen buffer. Also the edge
counters are reset, the double buffer is cleared, page 0 is set, and the cursor is set to row 24.

```
(scm 'show)  ; → nil
```

---

This project by [gramian](https://fosstodon.org/@gramian) is licensed under the 0BSD (Zero-Clause BSD) license.
