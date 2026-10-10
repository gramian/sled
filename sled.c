#define NAME "sled"
#define VERSION "0.6"
#define TITLE "SLED - Schemy LISP en DOS (" VERSION ")"

#if !defined(__SMALL__) && !defined(M_I86SM) && !defined(_M_I86SM)
#error "Build only with the small memory model!"
#endif

// Typedefs ####################################################################

typedef unsigned char byte;
typedef int bool;
typedef unsigned int node;

// Configuration ###############################################################

#define NONE       0             // C Null Pointer
#define TRUE       1
#define FALSE      0

#define DOS_EOF    0x1A
#define DOS_EOL    "\r\n"
#define PATHLEN    80

#define PROMPT     NAME "> "
#define STDLIB     NAME ".scm"
#define BAR_WIDTH  20

#define NRESERVE   32
#define NNODES     (12288 + 40 + NRESERVE)  // Number of nodes (compensated for internal and reserve)
#define SYMTAB     (4096 + 220)  // Symbol table size (compensated for internal)
#define SYMLEN     16            // Maximum symbol length
#define BUFLEN     120           // Input buffer size
#define PRDEPTH    64            // Maximum print and parse recursion depth
#define NMODES     1024          // Maximum mode stack entries
#define NLOAD      2             // Maximum nested loads

// Enumerations ################################################################

enum { SPCL = NNODES, UNDEF, RPAREN, DOT, EOT, ERR, NIL };  // Sentinels (live above the node heap)

enum { MHALT = 0, MCHAIN, MRETN, MOR, MPRED, MEXPR, MLIST, MCALL, MLET, MBEGIN, MSET, MCONS, MVALUE };  // Trampoline Modes (do not reorder!)

enum { S_BEGIN = 0, S_DEFINE, S_IF, S_IFNIL, S_LAMBDA, S_LET, S_QUOTE, S_VALUE,  // Special Forms
       NSPECIAL };

enum { F_APPLY = NSPECIAL, F_ATOM, F_CONS, F_DEFINED, F_EMPTY, F_ENV, F_EOF,  // Builtin Functions
       F_EQUIV, F_ERROR, F_EXIT, F_GC, F_HEAD, F_LOAD, F_NEWLINE,
       F_PRINT, F_PROC, F_READ, F_RESTART, F_SYMBOL, F_TAIL, F_SCM,
       NBUILTIN };

enum { K_COMMENT = NBUILTIN, K_ERR, K_HELP, K_TRUE, K_VERSTR,  // Special Symbols
       NRESERVED };

// Globals #####################################################################

// Heap
struct { node a, d; } Heap[NNODES];  // Each node is two 16-bit words
node Freelist;

// Registers (GC Roots)
node Acc = NIL;      // Current expression
node Env = NIL;      // Lexical environments (association list)
node Vstack = NIL;   // Value stack
node Tmpcar = NIL;   // Scratch cons cell head
node Tmpcdr = NIL;   // Scratch cons cell tail
node Symbols = NIL;  // Symbol table
node StdDef = NIL;   // Standard library symbols
node UserDef = NIL;  // User defined symbols

// Mode Stack
unsigned int Mstack = 0;
char Modes[NMODES];

// Symbol Table
unsigned int Symtop = 0;
unsigned int Pooltop = 0;
char Symtab[SYMTAB];

char Restartbuf[SYMLEN+1];

// Reader
struct { int fd, pos, len, next; char buf[BUFLEN]; } IOstack[2 + NLOAD];
int IOdepth;
#define IO IOstack[IOdepth]

// Control
int Parens;                 // counter for open parenthesis
volatile bool Interrupted;  // set by break_handler() Ctrl+C handler and by read_char() if error
bool Quit;                  // flag signaling that an "exit" was evaluated
bool Restart;               // flag signaling that a "restart" was evaluated
bool Restarted;             // flag signaling that a restart happened to "scm"
bool Fixed;                 // flag signaling end of StdDef and start of UserDef
bool Fresh;                 // flag signaling if current output line is fresh

// Symbols
node S_ans, S_self, S_ver;

// Most common error messages (manual string pooling)
static const char Badpair[] = "bad pair";
static const char Syntax[] = "syntax";
static const char Type[] = "type";
static const char Undefined[] = "undefined";
static const char Badsymbol[] = "bad symbol";
static const char Missingparen[] = "missing paren";
static const char Readdepth[] = "read depth";
static const char Outofnodes[] = "out of nodes";

// Bitmasks ####################################################################

#define PTR_MASK  0x3FFF  // 0b0011111111111111 , car and cdr words have 14-bit payloads
#define TAG_MASK  0xC000  // 0b1100000000000000 , car and cdr words have 2-bit tags

#define CONS_TAG  0x0000  // 0b0000000000000000 , car tag: payload is node index
#define ATOM_TAG  0x4000  // 0b0100000000000000 , car tag: payload is sentinel or raw integer
#define CLOS_TAG  0x8000  // 0b1000000000000000 , car tag: payload is index of parameter list
#define SYMB_TAG  0xC000  // 0b1100000000000000 , car tag: payload is symbol table index

#define SWAP_TAG  0x4000  // 0b0100000000000000 , cdr tag: only used during gc
#define MARK_TAG  0x8000  // 0b1000000000000000 , cdr tag: only used during gc

// Utilities ###################################################################

// Return string terminator position
const char* _fastcall strend(const char* s) /*pure*/ {
    while (*s) s++;
    return s;
}

// Test if two strings are equal
bool _fastcall streq(const char* s, const char* t) /*pure*/ {
    while (*s && (*s == *t)) { s++; t++; }
    return *s == *t;
}

// Copy string (return destination terminator position)
char* _fastcall strcp(char* s, const char* t) {
    while ((*s = *t) != 0) { s++; t++; }
    return s;
}

// System I/O ##################################################################

void exit(int);  // abort program

void _cdecl _setenvp(void) { }  // Prevent environment copying on start

// int 23h handler
void _interrupt _far break_handler(void) {
    Interrupted = TRUE;
}

// Set CTRL+C and CTRL+Break handler
void _cdecl sys_break(void) {
    _asm {  // set new int 24h and int 23h handlers
        push ds
        push cs
        pop ds
        call setup  // push current ip on to stack and jump to target
        mov al, 3
        iret
        setup:
        pop dx
        mov ax, 0x2524
        int 21h
        mov dx, OFFSET break_handler
        mov ax, 0x2523
        int 21h
        pop ds
    }
}

// Call DOS interrupt 21h (small memory model only)
int _cdecl int21h(unsigned int a, unsigned int b, unsigned int c, const char* d) {
    int r = -1;
    _asm {
        mov ax, a  // function:subfunction codes
        mov bx, b  // usually a handle
        mov cx, c  // usually a length
        mov dx, d  // usually a pointer
        int 21h
        jc fail
        mov r, ax
        fail:
    }
    return r;
}

// Print to standard output
void sys_write(const char* s, unsigned int n) {
    if (n) {
        int21h(0x4000, 1, n, s);
        Fresh = (s[n-1] == '\n');
    }
}

// Print newline
void sys_newline(void) {
    sys_write(DOS_EOL, sizeof(DOS_EOL) - 1);
}

// Print string
void sys_print(const char* msg) {
    sys_write(msg, strend(msg) - msg);
}

// Print string at the beginning of a line
void sys_fresh(const char* s) {
    if (!Fresh) sys_newline();
    sys_print(s);
}

// Print literal
#define sys_stamp(lit) sys_write(lit, sizeof("" lit) - 1)

// Open file
#define sys_open(pth) int21h(0x3D00, 0, 0, pth)

// Close file
#define sys_close(hdl) int21h(0x3E00, hdl, 0, NONE)

// Read file
#define sys_read(hdl, len, buf) int21h(0x3F00, hdl, len, buf)

// Check for Break
#define sys_poll() int21h(0x0B00, 0, 0, NONE)

// Accessors ###################################################################

#define car(x)        (Heap[(x)].a & PTR_MASK)
#define cdr(x)        (Heap[(x)].d)

#define caar(x)       car(car(x))
#define cadr(x)       car(cdr(x))       // second
#define cdar(x)       cdr(car(x))
#define cddr(x)       cdr(cdr(x))

#define caddr(x)      car(cdr(cdr(x)))  // third

#define set_car(x, v) (Heap[(x)].a = (v))
#define set_cdr(x, v) (Heap[(x)].d = (v))

#define symstr(n)     (Symtab + car(n))  // Convert symbol to string

// Predicates ##################################################################

// Test if admissible symbol character
bool _fastcall isSymbolic(int c) /*const*/ {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c == '?') ||
           (c >= '0' && c <= '9') || (c == '-') || (c == '_') || (c == '\\') ||
           (c == '.');
}

// Test if atom
bool _fastcall isAtom(node n) /*pure*/ {
    return (n >= SPCL) || ((Heap[n].a & TAG_MASK) != 0);
}

// Test if symbol
bool _fastcall isSymbol(node n) /*pure*/ {
    return (n < SPCL) && ((Heap[n].a & TAG_MASK) == SYMB_TAG);
}

// Test if closure
bool _fastcall isClosure(node n) /*pure*/ {
    return (n < SPCL) && ((Heap[n].a & TAG_MASK) == CLOS_TAG);
}

// Printing ####################################################################

// Print node semi-recursively
void print_node(node n, int d) {
    if (n == NIL)          sys_stamp("nil");
    else if (isSymbol(n))  sys_print(symstr(n));
    else if (n == EOT)     sys_stamp("*eot*");
    else if (n == UNDEF)   sys_stamp("*undef*");
    else if (isClosure(n)) sys_stamp("*closure*");
    else if (isAtom(n))    sys_stamp("*atom*");
    else if (d < 0)        sys_stamp("*list*");
    else if (d >= PRDEPTH) sys_stamp("...");
    else {
        sys_stamp("(");
        for (;;) {
            if (Interrupted) break;
            print_node(car(n), d+1);
            n = cdr(n);
            if (n == NIL) break;
            if (isAtom(n)) {
                sys_stamp(" . ");
                print_node(n, d+1);
                break;
            }
            sys_stamp(" ");
        }
        sys_stamp(")");
    }
}

// Print user-defined environment
void print_env(void) {
    node u;
    for (u = UserDef; (u != NIL) && !Interrupted; u = cdr(u)) {
        sys_print(symstr(car(u)));
        sys_stamp("\t");
        print_node(cdar(u), -1);
        sys_newline();
    }
}

// Print fixed symbols
void print_help(void) {
    node u;
    sys_stamp("special:" DOS_EOL);
    for (u = 0; u < NSPECIAL; u++) { sys_stamp(" "); sys_print(symstr(u)); }
    sys_stamp(DOS_EOL "builtin:" DOS_EOL);
    for (u = NSPECIAL; u < NBUILTIN; u++) { sys_stamp(" "); sys_print(symstr(u)); }
    sys_stamp(DOS_EOL "standard:" DOS_EOL);
    for (u = StdDef; u != NIL; u = cdr(u)) { sys_stamp(" "); sys_print(symstr(car(u))); }
    sys_newline();
}

// Print memory usage
void print_usage(unsigned int used) {
    static const char Bar[] = "####################"   // BAR_WIDTH '#'
                              "....................";  // BAR_WIDTH '.'
    unsigned int filled = used / (NNODES / BAR_WIDTH);

    sys_stamp("[");
    sys_write(Bar + BAR_WIDTH - filled, BAR_WIDTH);
    sys_stamp("]" DOS_EOL);
}

// Error Handling ##############################################################

// Handle unrecoverable error
void fatal(const char* m) {
    sys_fresh("! fatal: ");
    sys_print(m);
    sys_newline();
    exit(1);
}

// Handle recoverable error
node error(const char *m, node n) {
    if (!Interrupted) {
        sys_fresh("? ");
        sys_print(m);
        if (n != UNDEF) {
            sys_stamp(": ");
            print_node(n, 0);
        }
        sys_newline();
    }
    return ERR;
}

// Guard macro
#define ERROR_IF(cc, mm, nn) do { if (cc) return error(mm, nn); } while(0)

// Breaking guard macro
#define BREAK_IF(cc, dd, mm, nn) if (cc) { dd = error(mm, nn); break; }

// Test if number of arguments is wrong
bool _fastcall badarity(node x, int k0, int kn) /*pure*/ {
    int i = 0;
    if (kn == -1) kn = NNODES;  // use NNODES as upper limit for arguments
    for (x = cdr(x); !isAtom(x); x = cdr(x), i++)
        if (i >= kn) return TRUE;
    return (x != NIL) || (i < k0);
}

// Syntax guard macro
#define SYNTAX_IF(xx, mm, nn) do { if (badarity(xx, mm, nn)) return error(Syntax, xx); } while(0)

// Test if a binding name is bad
bool _fastcall badname(node q) /*pure*/ {
    return (q < NRESERVED) || (q == S_self) || !isSymbol(q);
}

// Test if lambda has bad parameters
bool _fastcall badparams(node p) /*pure*/ {
    for (; !isAtom(p); p = cdr(p)) {
        if (badname(car(p))) return TRUE;
    }
    return (p != NIL) && badname(p);
}

// Garbage Collection ##########################################################

// Deutsch–Schorr–Waite algorithm
void mark(node n) {
    register node p = NIL, t;
    unsigned int w;
    for (;; n = t) {
        if ((n < NRESERVED) || (n >= SPCL) || ((w = Heap[n].d) & MARK_TAG)) {  // reserved cdrs self-evaluate
            if (p == NIL) break;
            w = Heap[p].d;
            if (w & SWAP_TAG) {                // done with car, trace cdr now
                t = w & PTR_MASK;
                w = Heap[p].a;
                Heap[p].d = (w & PTR_MASK) | MARK_TAG;
                Heap[p].a = (w & TAG_MASK) | n;
            } else {                           // done with cdr, go to parent
                t = p;
                p = w & PTR_MASK;
                Heap[t].d = n | MARK_TAG;
            }
        } else {
            if ((t = Heap[n].a) & ATOM_TAG) {  // ATOM and SYMB cars are raw, trace cdr now
                Heap[n].d = p | MARK_TAG;
                t = w;
            } else {                           // trace CONS and CLOS cars (pointers) now, revisit cdr later
                Heap[n].a = (t & TAG_MASK) | p;
                Heap[n].d = w | (MARK_TAG | SWAP_TAG);
                t &= PTR_MASK;
            }
            p = n;
        }
    }
}

// Rebuild free list from unmarked nodes, clearing marks
unsigned int sweep(void) {
    register unsigned int i, nfree = 0;
    Freelist = NIL;
    for (i = NRESERVED; i < NNODES; i++) {
        if (!(Heap[i].d & MARK_TAG)) {
            Heap[i].d = Freelist;
            Freelist = i;
            nfree++;
        } else Heap[i].d &= ~(MARK_TAG | SWAP_TAG);
    }
    return nfree;
}

// Non-moving mark-and-sweep garbage collector
unsigned int gc(void) {
    sys_poll();
    mark(Acc);
    mark(Env);
    mark(Symbols);
    mark(Vstack);
    mark(Tmpcar);
    mark(Tmpcdr);
    mark(StdDef);
    mark(UserDef);
    return sweep();
}

// Memory ######################################################################

// Construct node (may invoke gc)
node _fastcall cons(node a, node d) {
    node n;
    if (Freelist == NIL) {
        Tmpcdr = d;
        Tmpcar = a;
        if (gc() < NRESERVE) {
            if (Freelist == NIL) fatal(Outofnodes);
            error(Outofnodes, UNDEF);
            Interrupted = TRUE;
        }
        Tmpcar = Tmpcdr = NIL;
    }
    n = Freelist;
    Freelist = cdr(n);
    Heap[n].a = a;
    Heap[n].d = d;
    return n;
}

// Push on to value stack
void _fastcall vpush(node n) {
    Vstack = cons(n, Vstack);
}

// Pop from value stack
node vpop(void) {
    node n;
    if (Vstack == NIL) fatal("vstack empty");
    n = car(Vstack);
    Vstack = cdr(Vstack);
    return n;
}

// Symbols #####################################################################

// Find symbol in symbol table
node find_sym(const char *s) /*pure*/ {
    node p, sym;
    const char *t;
    for (sym = 0; sym < Pooltop; sym++) {
        t = symstr(sym);
        if ((*t == *s) && streq(s, t)) return sym;
    }
    for (p = Symbols; p != NIL; p = cdr(p)) {
        sym = car(p);
        t = symstr(sym);
        if ((*t == *s) && streq(s, t)) return sym;
    }
    return NIL;
}

// Add symbol to symbol table or return existing symbol
node add_sym(const char *s, node v) {
    unsigned int off = Symtop;
    node n;
    if (streq(s, "nil")) return NIL;
    n = find_sym(s);
    if (n != NIL) return n;

    ERROR_IF((Symtop + SYMLEN + 1) > SYMTAB, "symbols full", UNDEF);
    Symtop = (strcp(Symtab + Symtop, s) - Symtab) + 1;

    if (v == SPCL) {  // take a reserved node (no GC)
        n = Pooltop++;
        Heap[n].a = off | SYMB_TAG;
        Heap[n].d = n;  // if SPCL, symbol's value cell self-evaluates
    } else {
        n = cons(NIL, v);
        Heap[n].a = off | SYMB_TAG;
        Symbols = cons(n, Symbols);
    }
    return n;
}

// Parsing #####################################################################

// Increase IO level
bool iopush(const char *path) {
    int fd = 0;
    if (path) {
        fd = sys_open(path);
        if (fd < 0) { error(path, UNDEF); return FALSE; }
        IOdepth++;
    }
    IO.fd = fd; IO.pos = IO.len = 0; IO.next = EOT;
    return TRUE;
}

// Decrease IO level
void iopop(void) {
    sys_close(IO.fd);
    IOdepth--;
}

// Read character (and print prompt)
int read_char(void) {
    int c;
    if (Interrupted) return EOT;
    if (IO.next != EOT) {
        c = IO.next;
        IO.next = EOT;
        return c;
    }
    if (IO.pos >= IO.len) {
        if ((IOdepth == 0) && (Parens == 0)) sys_fresh(PROMPT);  // print prompt if at top-level
        IO.len = sys_read(IO.fd, BUFLEN, IO.buf);
        IO.pos = 0;
        if (IOdepth == 0) Fresh = TRUE;
        if (Interrupted) return EOT;
        if (IO.len <= 0) {
            if (IO.len < 0) { error("read", UNDEF); Interrupted = TRUE; }
            return EOT;
        }
    }
    c = (byte) IO.buf[IO.pos++];
    if (c == DOS_EOF) { IO.pos--; return EOT; }
    if ((IOdepth == 0) && ((c == 0x00) || (c == 0xE0))) {  // extended char
        error("stray key", UNDEF);
        Interrupted = TRUE;
        return EOT;
    }
    return c;
}

// Read symbol
node read_sym(int c) {
    char sym[SYMLEN+1];
    unsigned int i = 0;
    const char* bad = NONE;
    while (isSymbolic(c)) {
        if ('\\' == c) {
            c = read_char();
            if (c == EOT) { bad = Badsymbol; break; }
            if ((c == 0) || (c == '\r') || (c == '\n') || (c == '$') ||
                (c == '(') || (c == ')') || (c == '\'')) bad = Badsymbol;
        } else if ((c >= 'A') && (c <= 'Z')) c += 32;
        if (i < SYMLEN) sym[i++] = c; else bad = "overlong";
        c = read_char();
    }
    sym[i] = 0;
    IO.next = c;
    if (Interrupted) return ERR;
    ERROR_IF(bad, bad, UNDEF);
    return add_sym(sym, UNDEF);  // add_sym checks if the symbol exists and if so returns its index
}

// Read symbol from the console
node read_input(void) {
    int c;
    node n = ERR;
    if (!iopush("CON")) return EOT;
    while ((ERR == n) && !Interrupted) {
        IO.pos = IO.len = 0; IO.next = EOT;
        sys_stamp("'");
        c = read_char();
        Fresh = TRUE;
        if (EOT == c) n = EOT;
        else if (isSymbolic(c) && ('.' != c)) n = read_sym(c);
        else if ('\r' != c) error(Badsymbol, UNDEF);
    }
    iopop();
    return n;
}

// Skip whitespace and line comments
int parse_skip(void) {
    int c;
    do {
        c = read_char();
        if (c == ';')
            while ((c != '\n') && (c != EOT)) c = read_char();
    } while ((c == ' ') || (c == '\t') || (c == '\n') || (c == '\r'));
    return c;
}

// Skip block comment up to its matching paren
node parse_comment(void) {
    int c, d = 1;
    do {
        c = parse_skip();
        ERROR_IF(EOT == c, Missingparen, UNDEF);
        if (c == '\\') read_char();
        else if (c == '(') d++;
        else if (c == ')') d--;
    } while (d);
    return UNDEF;
}

node parse(void);  // forward: parse <-> parse_list

// Parse rooted tree
node parse_list(void) {
    node n, t = NIL;
    vpush(NIL);
    n = parse();
    if (K_COMMENT == n) n = parse_comment();
    else for (;; n = parse()) {
        if ((ERR == n) || (RPAREN == n)) break;
        BREAK_IF(EOT == n, n, Missingparen, UNDEF);
        if (UNDEF == n) continue;
        if (DOT == n) {
            BREAK_IF(NIL == t, n, Badpair, UNDEF);
            n = parse(); if (ERR == n) break;
            BREAK_IF((EOT == n) || (UNDEF == n) ||
                     (RPAREN == n) || (DOT == n), n, Badpair, UNDEF);
            set_cdr(t, n);
            n = parse();
            BREAK_IF((ERR != n) && (RPAREN != n), n, Badpair, UNDEF);
            break;
        }
        n = cons(n, NIL);
        if (NIL == t) set_car(Vstack, n); else set_cdr(t, n);
        t = n;
    }
    if (RPAREN == n) n = car(Vstack);
    vpop();
    return n;
}

// Parse quoted expression
node parse_quote(void) {
    node n = parse();
    if (ERR == n) return ERR;
    ERROR_IF((EOT == n) || (UNDEF == n) ||
             (RPAREN == n) || (DOT == n), Syntax, UNDEF);
    n = cons(n, NIL);
    return cons(S_QUOTE, n);
}

// Parser dispatch
node parse(void) {
    int c = parse_skip();
    node n;
    if ((EOT == c) || ('$' == c)) return EOT;  // top-level $ exits parser
    if (('(' == c) || ('\'' == c)) {
        ERROR_IF(Parens >= PRDEPTH, Readdepth, UNDEF);
        Parens++;
        n = ('(' == c) ? parse_list() : parse_quote();
        Parens--;
        return n;
    }
    if (')' == c) { ERROR_IF(!Parens, "extra paren", UNDEF); return RPAREN; }
    if ('.' == c) { ERROR_IF(!Parens, "free dot", UNDEF); return DOT; }
    if (isSymbolic(c)) return read_sym(c);
    return error(Syntax, UNDEF);
}

// Extension ###################################################################

#ifdef SCM
#include "scm.h"     // Super Character Mode extension
#else
#define scm(x) NIL   // (scm ...) returns nil without extension
#endif

// Virtual Machine #############################################################

// Symbol layout:      (SYMB_TAG | SymTab-offset . value)
// Closure layout:     (CLOS_TAG | params-index . (environment . (body ...)))
// Environment layout: ((symbol . value) ...)

// Set answer
#define set_ans(d) (set_cdr(S_ans, d))

// Prepend environment with binding
#define bind(ss, tt) (cons(cons((ss), (tt)), Env))

node load(const char *s, bool ie);  // forward: load -> eval -> builtin -> load

// Browse environment
node _fastcall lookup(node n) /*pure*/ {
    register node e, b;
    if (n >= NRESERVED) {
        for (e = Env; e != NIL; e = cdr(e)) {
            b = car(e);
            if (car(b) == n) return cdr(b);
        }
    }
    return cdr(n);
}

// Push on to mode stack
void _fastcall mpush(char m) {
    if (Mstack >= NMODES - 1) {
        error("mstack full", UNDEF);
        Interrupted = TRUE;
        return;
    }
    Modes[++Mstack] = m;
}

// Pop from mode stack
char mpop(void) {
    if (Mstack == 0) return MHALT;
    return Modes[Mstack--];
}

// Check tail position (MHALT, MCHAIN, MRETN are tail modes)
#define isTail() (Modes[Mstack] <= MRETN)

// Apply function (with tail-call optimization)
node funapp(node c, node a) {
    node v = car(c), b = cdr(c);
    if (!isTail()) { vpush(Env); mpush(MRETN); }
    Env = car(b);
    for (; !isAtom(v); a = cdr(a), v = cdr(v)) {  // bind args
        ERROR_IF(NIL == a, "missing args", Acc);
        Env = bind(car(v), car(a));
    }
    if (NIL != v) Env = bind(v, a);               // rest arg
    else ERROR_IF(NIL != a, "extra args", Acc);
    b = cdr(b);
    if (NIL != cdr(b)) { mpush(MBEGIN); vpush(cdr(b)); }
    return car(b);
}

// Special forms
node special(node x) {
    node hd = car(x), tl = cdr(x), ad = isAtom(tl) ? NIL : car(tl);

    switch(hd) {
        case S_QUOTE:   // (quote any)
            SYNTAX_IF(x, 1, 1);
            return ad;
        case S_IF:      // (if any any [any])
            SYNTAX_IF(x, 2, 3);
            mpush(MPRED);
            vpush(cdr(tl));  // (then, else)
            return ad;       // predicate
        case S_BEGIN:   // (begin . any)
            SYNTAX_IF(x, 0, -1);
            if (tl == NIL) return NIL;
            tl = cdr(tl);
            if (tl != NIL) { mpush(MBEGIN); vpush(tl); }
            return ad;
        case S_IFNIL:   // (ifnil any any)
            SYNTAX_IF(x, 2, 2);
            mpush(MOR);
            vpush(cdr(tl));
            return ad;
        case S_LAMBDA:  // (lambda lst any ...)
            SYNTAX_IF(x, 2, -1);
            ERROR_IF(badparams(ad), Syntax, x);
            tl = cons(Env, cdr(tl));
            tl = cons(ad, tl);
            Heap[tl].a |= CLOS_TAG;  // add closure tag
            hd = bind(S_self, tl);
            set_car(cdr(tl), hd);
            return tl;
        case S_VALUE:   // (value sym)
            SYNTAX_IF(x, 1, 1);
            mpush(MVALUE);
            return ad;
        case S_LET:     // (let (v e) body ...)
            SYNTAX_IF(x, 2, -1);
            ERROR_IF(isAtom(ad) || badarity(ad, 1, 1) || badname(car(ad)), Syntax, x);
            vpush(car(ad));
            vpush(cdr(tl));
            mpush(MLET);
            return cadr(ad);
        case S_DEFINE:  // (define sym any)
            SYNTAX_IF(x, 2, 2);
            ERROR_IF(!isSymbol(ad), Type, x);
            ERROR_IF(badname(ad) || (ad == S_ans) || (ad == S_ver), "reserved", ad);
            if (Fixed && (UNDEF != cdr(ad))) {
                for (hd = UserDef; hd != NIL; hd = cdr(hd))
                    if (car(hd) == ad) break;
                ERROR_IF(hd == NIL, "fixed", ad);
            }
            mpush(MSET);
            vpush(ad);        // name
            return cadr(tl);  // value
        default:        // unreachable
            return error(Syntax, x);
    }
}

// Built-in functions
node builtin(node x) {
    unsigned int u;
    node fn = car(x), ad = cdr(x), n;
    ad = isAtom(ad) ? NIL : car(ad);

    switch(fn) {
        case F_HEAD:       // (head lst)
            SYNTAX_IF(x, 1, 1);
            ERROR_IF(isAtom(ad), Type, x);
            return car(ad);
        case F_TAIL:       // (tail lst)
            SYNTAX_IF(x, 1, 1);
            ERROR_IF(isAtom(ad), Type, x);
            return cdr(ad);
        case F_CONS:       // (cons any any)
            SYNTAX_IF(x, 2, 2);
            return cons(ad, caddr(x));
        case F_EMPTY:      // (empty? lst)
            SYNTAX_IF(x, 1, 1);
            return (ad == NIL) ? K_TRUE : NIL;
        case F_EQUIV:      // (equiv? any any)
            SYNTAX_IF(x, 2, 2);
            return (ad == caddr(x)) ? K_TRUE : NIL;
        case F_ATOM:       // (atom? any)
            SYNTAX_IF(x, 1, 1);
            return isAtom(ad) ? K_TRUE : NIL;
        case F_SYMBOL:     // (symbol? any)
            SYNTAX_IF(x, 1, 1);
            return isSymbol(ad) ? K_TRUE : NIL;
        case F_PROC:       // (proc? any)
            SYNTAX_IF(x, 1, 1);
            return (isClosure(ad) || ((ad >= NSPECIAL) && (ad < NBUILTIN))) ? K_TRUE : NIL;
        case F_DEFINED:    // (defined? sym)  for globals only
            SYNTAX_IF(x, 1, 1);
            ERROR_IF(!isSymbol(ad), Type, x);
            return (UNDEF != cdr(ad)) ? K_TRUE : NIL;
        case F_EOF:        // (eof? any)
            SYNTAX_IF(x, 1, 1);
            return (EOT == ad) ? K_TRUE : NIL;
        case F_PRINT:      // (print . any)
            SYNTAX_IF(x, 0, -1);
            for (n = cdr(x); n != NIL; n = cdr(n)) print_node(car(n), 0);
            return NIL;
        case F_NEWLINE:    // (newline)
            SYNTAX_IF(x, 0, 0);
            sys_newline();
            return NIL;
        case F_LOAD:       // (load sym [any])
            SYNTAX_IF(x, 1, 2);
            ERROR_IF(!isSymbol(ad), Type, x);
            return load(symstr(ad), (cddr(x) != NIL) && (caddr(x) != NIL));
        case F_READ:       // (read)
            SYNTAX_IF(x, 0, 0);
            return read_input();
        case F_ERROR:      // (error sym [any])
            SYNTAX_IF(x, 1, 2);
            ERROR_IF(!isSymbol(ad), Type, x);
            return error(symstr(ad), cddr(x) == NIL ? UNDEF : caddr(x));
        case F_ENV:        // (env)
            SYNTAX_IF(x, 0, 0);
            print_env();
            return NIL;
        case F_GC:         // (gc [any])
            SYNTAX_IF(x, 0, 1);
            u = gc();
            if (ad != NIL) print_usage(NNODES - u);
            return NIL;
        case F_RESTART:    // (restart [sym])
            SYNTAX_IF(x, 0, 1);
            ERROR_IF((ad != NIL) && !isSymbol(ad), Type, x);
            if (ad != NIL) strcp(Restartbuf, symstr(ad)); else Restartbuf[0] = 0;
            Restart = Quit = TRUE;
            return UNDEF;
        case F_EXIT:       // (exit)
            SYNTAX_IF(x, 0, 0);
            Quit = TRUE;
            return UNDEF;
        case F_SCM:        // (scm sym [sym [sym [sym]]])
            return scm(x);
        default:
            return error("not callable", x);
    }
}

// Evaluate expression (caller must reset or restore Env)
node eval(node x) {
    node i, n;
    node vsave = Vstack;
    unsigned int msave = Mstack;
    char m = MEXPR;
    Acc = x;
    mpush(MHALT);

    while ((m != MHALT) && (ERR != Acc) && !Interrupted) {
        switch (m) {
            case MOR:     // resume after evaluating "ifnil"
                if (NIL != Acc) { vpop(); m = mpop(); break; }
                Acc = K_TRUE;  // true predicate result for MPRED
            case MPRED:   // resume after evaluating "if"
                n = vpop();
                if (NIL == Acc) n = cdr(n);
                if (NIL == n) { Acc = NIL; m = mpop(); break; }
                Acc = car(n);
            case MEXPR:   // evaluate Acc as an expression
                if (isAtom(Acc)) {
                    if (isSymbol(Acc)) {
                        n = Acc;
                        Acc = lookup(Acc);
                        BREAK_IF(UNDEF == Acc, Acc, Undefined, n);
                    }
                    m = mpop();
                    break;
                }
                n = car(Acc);
                if (n < NSPECIAL) {
                    Acc = special(Acc);
                    m = ((S_QUOTE == n) || (S_LAMBDA == n)) ? mpop() : MEXPR;
                    break;
                }
                if ((F_CONS == n) && isTail() && !badarity(Acc, 2, 2)) {  // tail-recursion via cons
                    n = cdr(Acc);
                    if (MCHAIN != Modes[Mstack]) {
                        i = cons(NIL, NIL);
                        vpush(i); vpush(i);  // Vstack: (last dummy-head ...)
                        mpush(MCHAIN);
                    }
                    vpush(cadr(n));
                    mpush(MCONS);
                    Acc = car(n);
                    m = MEXPR;
                    break;
                }
                vpush(cdr(Acc));  // unevaluated argument list
                vpush(NIL);       // result evaluated argument list
                Acc = n;
                if (n >= NRESERVED) {
                    if (!isSymbol(n)) { mpush(MLIST); m = MEXPR; break; }
                    Acc = lookup(n);
                    BREAK_IF(UNDEF == Acc, Acc, Undefined, n);
                }
            case MLIST:   // collect evaluated head and arguments
                set_car(Vstack, cons(Acc, car(Vstack)));
                n = cadr(Vstack);  // arguments not yet evaluated
                if (!isAtom(n)) {  // evaluate next
                    set_car(cdr(Vstack), cdr(n));
                    Acc = car(n);
                    mpush(MLIST);
                    m = MEXPR;
                    break;
                }
                BREAK_IF(n != NIL, Acc, "improper call", n);
                i = car(Vstack);
                for (Acc = NIL; i != NIL; i = n) {  // reverse
                    n = cdr(i);
                    set_cdr(i, Acc);
                    Acc = i;
                }
                vpop();
                vpop();
            case MCALL:   // application: builtin or closure
                n = car(Acc);
                if (F_APPLY == n) {   // (apply fn args) -> (fn . args), re-dispatch
                    BREAK_IF(badarity(Acc, 2, 2), Acc, Syntax, Acc);
                    Acc = cons(cadr(Acc), caddr(Acc));
                    BREAK_IF(badarity(Acc, 0, -1), Acc, Syntax, Acc);
                    m = MCALL;
                } else if (n < NBUILTIN) {
                    Acc = builtin(Acc);
                    if (Quit) { Acc = UNDEF; m = MHALT; } else m = mpop();
                } else {
                    BREAK_IF(!isClosure(n), Acc, Type, Acc);
                    Acc = funapp(n, cdr(Acc));
                    m = MEXPR;
                }
                break;
            case MLET:   // bind local, enter the body
                n = cadr(Vstack);
                if (isTail()) {
                    set_cdr(Vstack, cddr(Vstack));
                } else {
                    set_car(cdr(Vstack), Env);  // overwrite the name with the env to restore
                    mpush(MRETN);
                }
                Env = bind(n, Acc);
            case MBEGIN:  // sequencing of a body
                n = car(Vstack);
                Acc = car(n);
                n = cdr(n);
                if (NIL == n) vpop();
                else {
                    set_car(Vstack, n);
                    mpush(MBEGIN);
                }
                m = MEXPR;
                break;
            case MSET:    // resume after evaluating "define" value
                n = vpop();
                if (UNDEF == cdr(n)) {
                    if (Fixed) UserDef = cons(n, UserDef);
                    else       StdDef  = cons(n, StdDef);
                }
                set_cdr(n, Acc);
                m = mpop();
                break;
            case MCONS:   // evaluated head; append a cell, tail-eval the rest
                n = cdr(Vstack);
                Acc = cons(Acc, NIL);
                set_cdr(car(n), Acc);
                set_car(n, Acc);
                Acc = vpop();
                m = MEXPR;
                break;
            case MVALUE:    // argument evaluated
                BREAK_IF(!isSymbol(Acc), Acc, Type, Acc);
                m = MEXPR;
                break;
            case MCHAIN:  // close the cons chain
                set_cdr(vpop(), Acc);
                Acc = cdr(vpop());
                m = mpop();
                break;
            case MRETN:   // restore callers Env after a non-tail call
                Env = vpop();
                m = mpop();
                break;
        }
    }
    Vstack = vsave;
    Mstack = msave;
    return Interrupted ? ERR : Acc;
}

// Load script file
node load(const char *s, bool ie) {
    if (IOdepth >= NLOAD) { error("nested load", UNDEF); return K_ERR; }
    if (!iopush(s)) return K_ERR;

    vpush(Env);
    set_ans(NIL);  // Set "ans" in case of empty file
    while (!Quit && !Interrupted) {
        Env = NIL;
        Acc = parse();
        if (EOT == Acc) break;
        if (UNDEF == Acc) continue;
        if (ERR != Acc) Acc = eval(Acc);
        if (ERR == Acc) {
            set_ans(K_ERR);
            if (!ie) break;  // if ie, ignore errors
            continue;
        }
        if (UNDEF == Acc) continue;
        set_ans(Acc);
    }
    Env = vpop();

    iopop();
    if (Interrupted) set_ans(K_ERR);
    return cdr(S_ans);
}

// Entrypoint ##################################################################

// Register reserved symbols (needs to be same sequence as in enum)
void reserved(void) {
    const char* names[NRESERVED];
    unsigned int i;

    names[S_BEGIN]  = "begin";     names[S_DEFINE]  = "define";
    names[S_IF]     = "if";        names[S_IFNIL]   = "ifnil";
    names[S_LAMBDA] = "lambda";    names[S_LET]     = "let";
    names[S_QUOTE]  = "quote";     names[S_VALUE]   = "value";

    names[F_APPLY]  = "apply";     names[F_ATOM]    = "atom?";
    names[F_CONS]   = "cons";      names[F_DEFINED] = "defined?";
    names[F_EMPTY]  = "empty?";    names[F_ENV]     = "env";
    names[F_EOF]    = "eof?";      names[F_EQUIV]   = "equiv?";
    names[F_ERROR]  = "error";     names[F_EXIT]    = "exit";
    names[F_GC]     = "gc";        names[F_HEAD]    = "head";
    names[F_LOAD]   = "load";      names[F_NEWLINE] = "newline";
    names[F_PRINT]  = "print";     names[F_PROC]    = "proc?";
    names[F_READ]   = "read";      names[F_RESTART] = "restart";
    names[F_SYMBOL] = "symbol?";   names[F_TAIL]    = "tail";
    names[F_SCM]    = "scm";

    names[K_COMMENT] = "comment";  names[K_ERR]     = "err";
    names[K_HELP]    = "?";        names[K_TRUE]    = "true";
    names[K_VERSTR]  = NAME "-" VERSION;

    for (i = 0; i < NRESERVED; i++) add_sym(names[i], SPCL);

    S_ans   = add_sym("ans", NIL);
    S_self  = add_sym("self", UNDEF);
    S_ver   = add_sym("ver", K_VERSTR);
}

// Build standard library path
void stdpath(char* buf, const char* exe) {
    char *dir = buf, *p = buf, *end = buf + PATHLEN - sizeof(STDLIB);
    for (; *exe; exe++) {
        if (p >= end) { dir = buf; break; }
        *p++ = *exe;
        if ((*exe == '\\') || (*exe == ':')) dir = p;
    }
    strcp(dir, STDLIB);
}

// Reset state
void reset(void) {
    Interrupted = FALSE;
    Modes[0] = MHALT;
    Parens = Mstack = 0;
    Acc = Env = Vstack = NIL;
}

// Entrypoint
int _cdecl main(int argc, char **argv) {
    char opt = ((argc > 1) && (argv[1][0] == '/') && argv[1][1] && !argv[1][2]) ? argv[1][1] : 0;
    bool ignore = (opt == 'i') || (opt == 'I'),
         batch = ignore || (opt == 'b') || (opt == 'B'),
         status = 0;
    static char stdlib[PATHLEN];  // maximum full file path can be 79 chars in DOS

    // Welcome banner
    sys_stamp(DOS_EOL TITLE DOS_EOL DOS_EOL);

    // Help screen
    if (opt == '?') {
        sys_stamp("Usage: " NAME " [/?] [/B] [/I] [file]" DOS_EOL DOS_EOL
                  "  /?       This help" DOS_EOL
                  "  file     Load file" DOS_EOL
                  "  /B file  Load file, exit" DOS_EOL
                  "  /I file  Load file (ignore errors), exit" DOS_EOL DOS_EOL);
        return 0;
    }

    // Set break interrupt handler
    sys_break();

    restart:  // Restart anchor

    // Reset (for restarts)
    Restarted = TRUE;
    Symtop = Pooltop = status = 0;
    Symbols = StdDef = UserDef = NIL;
    Restart = Fixed = Quit = FALSE;

    // Init reader
    IOdepth = 0;
    iopush(NONE);

    // Init heap
    sweep();
    reset();
    reserved();

    // Load standard library
    stdpath(stdlib, argv[0]);
    load(stdlib, FALSE);
    Fixed = TRUE;

    // Load script CLI arguments
    if (Restartbuf[0]) {
        status = (load(Restartbuf, FALSE) == K_ERR);
    } else if (argc > 1) {
        if (batch && (argc < 3)) {
            sys_stamp("missing file" DOS_EOL);
            status = 1;
            Quit = TRUE;
        } else status = (load(batch ? argv[2] : argv[1], ignore) == K_ERR);
    }
    if (batch) Quit = TRUE;

    // REPL
    while (!Quit) {
        reset();
        Acc = parse();
        if ((EOT == Acc) && !Interrupted) break;
        if (K_HELP == Acc) { print_help(); continue; }
        if (UNDEF == Acc) continue;
        if ((ERR != Acc) && !Interrupted) Acc = eval(Acc);
        if ((ERR == Acc) || Interrupted) {
            set_ans(K_ERR);
            if (IO.pos && (IO.next != '\n'))
                while ((IO.pos < IO.len) && (IO.buf[IO.pos++] != '\n')) ;
            IO.next = EOT;
            continue;
        }
        if (UNDEF == Acc) continue;
        set_ans(Acc);
        sys_fresh("");
        print_node(Acc, 0);
        sys_newline();
    }

    // Restart
    if (Restart) {
        argc = 1;
        goto restart;
    }

    // Exit banner
    sys_fresh(DOS_EOL "Bye" DOS_EOL DOS_EOL);

    return status;
}
