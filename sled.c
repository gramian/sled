#define NAME "sled"
#define VERSION "0.4"
#define TITLE "SLED - Schemy LISP en DOS (" VERSION ")"

#if !defined(__SMALL__) && !defined(M_I86SM) && !defined(_M_I86SM)
#error "Build only with the small memory model!"
#endif

// Typedefs ####################################################################

typedef unsigned char byte;
typedef int bool;
typedef unsigned int node;

// Configuration ###############################################################

#define NONE       0             // Null Pointer
#define TRUE       1
#define FALSE      0

#define DOS_EOF    0x1A
#define DOS_EOL    "\r\n"

#define PROMPT     NAME "> "
#define STDLIB     NAME ".scm"
#define BAR_WIDTH  20

#define NNODES     (12288 + 72)  // Number of nodes
#define SYMTAB     (2048 + 220)  // Symbol table size
#define SYMLEN     16            // Maximum symbol length
#define BUFLEN     120           // Input buffer size
#define PRDEPTH    64            // Maximum print and parse recursion depth
#define NMODES     512           // Maximum mode stack entries
#define NLOAD      2             // Maximum nested loads

// Enumerations ################################################################

enum { SPCL = NNODES, UNDEF, RPAREN, DOT, EOT, ERR, NIL };  // Sentinels (live above the node heap)

enum { MHALT = 0, MCHAIN, MRETN, MOR, MPRED, MEXPR, MLIST, MCALL, MLET, MBEGIN, MSET, MCONS };  // Trampoline Modes

enum { S_BEGIN = 0, S_DEFINE, S_IF, S_IFNIL, S_LAMBDA, S_LET, S_QUOTE,  // Special Forms
       NSPECIAL };

enum { F_APPLY = NSPECIAL, F_ATOM, F_CONS, F_DEFINED, F_EMPTY, F_ENV, F_EOF,  // Builtin Functions
       F_EQUIV, F_ERROR, F_EXIT, F_GC, F_HEAD, F_LOAD, F_NEWLINE,
       F_PRINT, F_PROC, F_READ, F_RESTART, F_SYMBOL, F_TAIL, F_VALUE,
       NBUILTIN };

enum { K_COMMENT = NBUILTIN, K_ERR, K_HELP, K_TRUE, K_VERSTR,  // Special Symbols
       NRESERVED };

// Globals #####################################################################

// Heap
struct { node car, cdr; } Heap[NNODES];  // Each node is two 16-bit words
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
struct { int fd, pos, len; char buf[BUFLEN]; int next; } IOstate[1 + NLOAD];
int IOdepth;
#define IO IOstate[IOdepth]

// Control
int Parens;                 // counter for open parenthesis
volatile bool Interrupted;  // set by on_break() Ctrl+C handler and by read_char() if error
bool Quit;                  // flag signaling that an "exit" was evaluated
bool Restart;               // flag signaling that a "restart" was evaluated
bool Fixed;                 // flag signaling end of StdDef and start of UserDef
bool Skip;                  // flag signaling an open block comment

// Symbols
node S_ans, S_self, S_ver;

// Most common error messages
static const char Badpair[] = "bad pair";
static const char Syntax[] = "syntax";
static const char Type[] = "type";
static const char Undefined[] = "undefined";

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
bool _fastcall streq(const char *s, const char* t) /*pure*/ {
    while (*s && (*s == *t)) { s++; t++; }
    return *s == *t;
}

// Copy string (return destination terminator position)
char* _fastcall strcp(char* s, const char* t) {
    while ((*s = *t) !=0) { s++; t++; }
    return s;
}

// System I/O ##################################################################

void _cdecl _setenvp(void) { }  // Prevent environment copying on start

// Call DOS interrupt 21h (small memory model only)
int int21h(unsigned int a, unsigned int b, unsigned int c, const char* d) {
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
    if (n) int21h(0x4000, 1, n, s);
}

// Print newline
void sys_newline(void) {
    sys_write(DOS_EOL, sizeof(DOS_EOL) - 1);
}

// Print string
void sys_print(const char* msg) {
    sys_write(msg, strend(msg) - msg);
}

// Print literal
#define sys_stamp(lit) sys_write(lit, sizeof("" lit) - 1)

// Exit program with error status
#define sys_abort() int21h(0x4C01, 0, 0, NONE)

// Open file
#define sys_open(pth) int21h(0x3D00, 0, 0, pth)

// Close file
#define sys_close(hdl) int21h(0x3E00, hdl, 0, NONE)

// Read file
#define sys_read(hdl, len, buf) int21h(0x3F00, hdl, len, buf)

// Set or unset CTRL+C and CTRL+Break handler
void sys_break(void (_interrupt _far *hdl)(void)) {
    static unsigned long old1b;

    if (hdl != NONE) {
        _asm {  // get old int 1Bh handler
            push es
            mov ax, 351Bh
            int 21h
            mov WORD PTR old1b, bx
            mov WORD PTR old1b+2, es
            pop es
        }
        _asm {  // set new int 23h and int 1Bh handlers
            push ds
            lds dx, DWORD PTR hdl  // int 21h, ah 25h preserves ds:dx
            mov ax, 2523h
            int 21h
            mov ax, 251Bh
            int 21h
            pop ds
        }
    } else {
        _asm {  // restore old int 1Bh handler (23h is restored by DOS)
            push ds
            mov ax, 251Bh
            lds dx, DWORD PTR old1b
            int 21h
            pop ds
        }
    }
}

// Accessors ###################################################################

#define car(x)        (Heap[(x)].car & PTR_MASK)
#define cdr(x)        (Heap[(x)].cdr)

#define caar(x)       car(car(x))
#define cadr(x)       car(cdr(x))       // second
#define cdar(x)       cdr(car(x))
#define cddr(x)       cdr(cdr(x))

#define caddr(x)      car(cdr(cdr(x)))  // third

#define set_car(x, v) (Heap[(x)].car = (v))
#define set_cdr(x, v) (Heap[(x)].cdr = (v))

#define symstr(n)     (Symtab + car(n))  // Convert symbol to string

// Predicates ##################################################################

// Test if admissible symbol character
bool _fastcall isSymbolic(int c, unsigned int i) /*const*/ {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c == '?') ||
           (c >= '0' && c <= '9') || (c == '-') || (c == '_') || (c == '\\') ||
           (c == '.' && i > 0);
}

// Test if atom
bool _fastcall isAtom(node n) /*pure*/ {
    return (n >= SPCL) || ((Heap[n].car & TAG_MASK) != 0);
}

// Test if symbol
bool _fastcall isSymbol(node n) /*pure*/ {
    return (n < SPCL) && ((Heap[n].car & TAG_MASK) == SYMB_TAG);
}

// Test if closure
bool _fastcall isClosure(node n) /*pure*/ {
    return (n < SPCL) && ((Heap[n].car & TAG_MASK) == CLOS_TAG);
}

// Printing ####################################################################

// Print node semi-recursively
void print_node(node n, int d) {
    if (Interrupted) return;
    if (n == NIL)          sys_stamp("nil");
    else if (n == EOT)     sys_stamp("*eot*");
    else if (n == UNDEF)   sys_stamp("*undef*");
    else if (isSymbol(n))  sys_print(symstr(n));
    else if (isClosure(n)) sys_stamp("*closure*");
    else if (isAtom(n))    sys_stamp("*atom*");
    else if (d < 0)        sys_stamp("*list*");
    else if (d >= PRDEPTH) sys_stamp("...");
    else {
        sys_stamp("(");
        for (;;) {
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
    for (u = UserDef; u != NIL; u = cdr(u)) {
        sys_print(symstr(car(u)));
        sys_stamp("\t");
        print_node(cdar(u), -1);
        sys_newline();
    }
}

// Print fixed symbols
node print_help(void) {
    node u;
    sys_stamp("special forms:" DOS_EOL);
    for (u = 0; u < NSPECIAL; u++) { sys_stamp(" "); sys_print(symstr(u)); }
    sys_stamp(DOS_EOL "builtin functions:" DOS_EOL);
    for (u = NSPECIAL; u < NBUILTIN; u++) { sys_stamp(" "); sys_print(symstr(u)); }
    sys_stamp(DOS_EOL "standard library:" DOS_EOL);
    for (u = StdDef; u != NIL; u = cdr(u)) { sys_stamp(" "); sys_print(symstr(car(u))); }
    sys_newline();
    return UNDEF;
}

// Print memory usage
void print_usage(unsigned int used) {
    char buf[BAR_WIDTH + 3];
    unsigned int i, filled = used / (NNODES / BAR_WIDTH);
    if (filled > BAR_WIDTH) filled = BAR_WIDTH;

    buf[0] = '[';
    for (i = 1; i <= BAR_WIDTH; i++) buf[i] = (i <= filled) ? '#' : '.';
    buf[BAR_WIDTH + 1] = ']';
    buf[BAR_WIDTH + 2] = 0;

    sys_print(buf);
    sys_newline();
}

// Error Handling ##############################################################

// Handle unrecoverable error
void fatal(const char* m) {
    sys_stamp("! fatal: ");
    sys_print(m);
    sys_newline();
    sys_break(NONE);
    sys_abort();
}

// Handle recoverable error
node error(const char *m, node n) {
    sys_stamp("? ");
    sys_print(m);
    if ((n != UNDEF) && !Interrupted) {
        sys_stamp(": ");
        print_node(n, 0);
    }
    sys_newline();
    return ERR;
}

// Guard macro
#define ERROR_IF(cc, mm, nn) do { if (cc) return error(mm, nn); } while(0)

// Breaking guard macro
#define BREAK_IF(cc, dd, mm, nn) if (cc) { dd = error(mm, nn); break; }

// Test if number of arguments is wrong
bool _fastcall badarity(node x, int k0, int kn) /*pure*/ {
    int i;
    if (kn == -1) kn = NNODES;
    for (x = cdr(x), i = 0; !isAtom(x); x = cdr(x), i++)
        if (i >= kn) return TRUE;
    return (x != NIL) || (i < k0);
}

// Syntax error wrapper
#define syntax(xx) error(Syntax, xx)

// Syntax guard macro
#define SYNTAX_IF(xx, mm, nn) do { if (badarity(xx, mm, nn)) return syntax(xx); } while(0)

// Test if lambda is malformed (and "self" is not used as a parameter name)
bool _fastcall malformed(node p) /*pure*/ {
    node q;
    for (p = cadr(p); !isAtom(p); p = cdr(p)) {
        q = car(p);
        if (!isSymbol(q) || (q < NRESERVED) || (q == S_self)) return TRUE;
    }
    return (p != NIL) && (!isSymbol(p) || (p < NRESERVED) || (p == S_self));
}

// Garbage Collection ##########################################################

// Deutsch–Schorr–Waite algorithm
void mark(node n) {
    register node p = NIL, t;
    unsigned int w;
    for (;; n = t) {
        if ((n < NRESERVED) || (n >= SPCL) || ((w = Heap[n].cdr) & MARK_TAG)) {  // reserved cdrs self-evaluate
            if (p == NIL) break;
            w = Heap[p].cdr;
            if (w & SWAP_TAG) {                  // done with car, trace cdr now
                t = w & PTR_MASK;
                w = Heap[p].car;
                Heap[p].cdr = (w & PTR_MASK) | MARK_TAG;
                Heap[p].car = (w & TAG_MASK) | n;
            } else {                             // done with cdr, go to parent
                t = p;
                p = w & PTR_MASK;
                Heap[t].cdr = n | MARK_TAG;
            }
        } else {
            if ((t = Heap[n].car) & ATOM_TAG) {  // car is raw, trace cdr now
                Heap[n].cdr = p | MARK_TAG;
                t = w;
            } else {                             // trace car now, revisit cdr later
                Heap[n].car = (t & TAG_MASK) | p;
                Heap[n].cdr = w | (MARK_TAG | SWAP_TAG);
                t &= PTR_MASK;
            }
            p = n;
        }
    }
}

// Non-moving garbage collector
unsigned int gc(void) {
    register unsigned int i, nfree = 0;
    mark(Acc);
    mark(Env);
    mark(Symbols);
    mark(Vstack);
    mark(Tmpcar);
    mark(Tmpcdr);
    mark(StdDef);
    mark(UserDef);
    Freelist = NIL;
    for (i = NRESERVED; i < NNODES; i++) {
        if (!(Heap[i].cdr & MARK_TAG)) {
            Heap[i].cdr = Freelist;
            Freelist = i;
            nfree++;
        } else Heap[i].cdr &= ~(MARK_TAG | SWAP_TAG);
    }
    return nfree;
}

// Memory ######################################################################

// Construct typed node (may invoke gc)
node _fastcall cons_tag(node a, node d, node t) {
    node n;
    if (Freelist == NIL) {
        Tmpcdr = d;
        Tmpcar = (t == SYMB_TAG) ? NIL : a;
        if (gc() == 0) fatal("out of nodes");
        Tmpcar = Tmpcdr = NIL;
    }
    n = Freelist;
    Freelist = cdr(Freelist);
    Heap[n].car = a | t;
    Heap[n].cdr = d & PTR_MASK;
    return n;
}

// Short-hand macro for principal case
#define cons(a, d) cons_tag(a, d, CONS_TAG)

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
    for (p = Symbols; p != NIL; p = cdr(p)) {
        sym = car(p);
        t = symstr(sym);
        if ((*t == *s) && streq(s, t)) return sym;
    }
    return NIL;
}

// Add symbol to symbol table or return value of existing symbol
node add_sym(const char *s, node v) {
    unsigned int off = Symtop;
    node n;
    if (!s[0]) return NIL;
    n = find_sym(s);
    if (n != NIL) return n;

    ERROR_IF((Symtop + SYMLEN + 1) > SYMTAB, "symbols full", UNDEF);
    while ((Symtab[Symtop++] = *s++) != 0) ;  // string copy

    if (v == SPCL) {  // take a reserved node (no GC)
        n = Pooltop++;
        Heap[n].car = off | SYMB_TAG;
        Heap[n].cdr = n;  // if SPCL, symbol's value cell self-evaluates
    } else n = cons_tag(off, v, SYMB_TAG);

    Symbols = cons(n, Symbols);
    return n;
}

// Parsing #####################################################################

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
        if ((IOdepth == 0) && (Parens == 0)) sys_stamp(PROMPT);  // print prompt if at top-level
        IO.len = sys_read(IO.fd, BUFLEN, IO.buf);
        if (Interrupted) return EOT;
        if (IO.len <= 0) {
            if (IO.len < 0) { error("read", UNDEF); Interrupted = TRUE; }
            return EOT;
        }
        IO.pos = 0;
    }
    c = (byte) IO.buf[IO.pos++];
    if (c == DOS_EOF) return EOT;
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
    bool over = FALSE, bad = FALSE;
    while (isSymbolic(c, i)) {
        if ('\\' == c) {
            c = read_char();
            if (c == EOT) { bad = TRUE; break; }
            bad = bad || (c == 0) || (c == '$') || (c == '(') || (c == ')') || (c == '\'');
        } else if (c >= 'A' && c <= 'Z') c += 32;
        if (i < SYMLEN) sym[i++] = c; else over = TRUE;
        c = read_char();
    }
    sym[i] = 0;
    IO.next = c;
    if (Interrupted) return ERR;
    ERROR_IF(over, "overlong symbol", UNDEF);
    ERROR_IF(bad, "bad escape", UNDEF);
    return (streq(sym, "nil")) ? NIL : add_sym(sym, UNDEF);  // add_sym checks if the symbol exists and if so returns its index
}

// Read symbol from line of standard input
node read_input(void) {
    char sym[SYMLEN+1];
    unsigned int i = 0;
    int save = IOdepth, c;
    IOdepth = 0; IO.next = EOT;
    if ((IO.pos < IO.len) && (IO.buf[IO.pos] == '\r')) IO.pos++;
    if ((IO.pos < IO.len) && (IO.buf[IO.pos] == '\n')) IO.pos++;
    Parens++;  // suppress the prompt
    while (((c = read_char()) != EOT) && (c != '\n')) {
        if ((c >= 'A') && (c <= 'Z')) c += 32;
        if ((c != '\r') && (i < SYMLEN)) sym[i++] = c;
    }
    Parens--;
    IOdepth = save;
    sym[i] = 0;
    if (Interrupted) return ERR;
    return i ? add_sym(sym, UNDEF) : EOT;
}

// Skip whitespace, comments, or block comments
int parse_skip(void) {
    int c;
    for (;;) {
        c = read_char();
        if (c == ';')
            while ((c != '\n') && (c != EOT)) c = read_char();
        if (Skip) {
            if (c == EOT || c == '(' || c == ')') return c;
            continue;
        }
        if ((c != ' ') && (c != '\t') && (c != '\n') && (c != '\r')) return c;
    }
}

node parse(void);  // forward: parse <-> parse_list

// Parse rooted tree
node parse_list(void) {
    node n, c, t = NIL;
    bool entered = Skip;
    ERROR_IF(Parens >= PRDEPTH, "read depth", UNDEF);
    Parens++;
    vpush(NIL);
    for (;;) {
        n = parse(); if (ERR == n) break;
        BREAK_IF(EOT == n, n, "missing paren", UNDEF);
        if (RPAREN == n) break;
        if (UNDEF == n) continue;
        if (Skip) continue;
        if ((NIL == t) && (n == K_COMMENT)) { Skip = TRUE; continue; }
        if (DOT == n) {
            BREAK_IF(t == NIL, n, Badpair, UNDEF);
            n = parse(); if (ERR == n) break;
            BREAK_IF((n == DOT) || (n == RPAREN) ||
                     (n == EOT) || (n == UNDEF), n, Badpair, UNDEF);
            set_cdr(t, n);
            n = parse(); if (ERR == n) break;
            BREAK_IF(n != RPAREN, n, Badpair, UNDEF);
            break;
        }
        c = cons(n, NIL);
        if (NIL == t) set_car(Vstack, c); else set_cdr(t, c);
        t = c;
    }
    if (ERR != n) {
        if (entered) n = NIL;
        else if (Skip) n = UNDEF;
        else n = car(Vstack);
    }
    Skip = entered;
    Parens--;
    vpop();
    return n;
}

// Parse quoted expression
node parse_quote(void) {
    node n;
    ERROR_IF(Parens >= PRDEPTH, "read depth", UNDEF);
    Parens++;
    n = parse();
    Parens--;
    if (ERR == n) return ERR;
    ERROR_IF((EOT == n) || (UNDEF == n) ||
             (RPAREN == n) || (DOT == n), "missing expression", UNDEF);
    n = cons(n, NIL);
    return cons(S_QUOTE, n);
}

// Parser dispatch
node parse(void) {
    int c = parse_skip();
    if (Interrupted || (EOT == c) || ((Parens == 0) && (c == '$'))) return EOT;
    if ('$' == c) return error("missing paren", UNDEF);
    if ('(' == c) return parse_list();
    if ('\'' == c) return parse_quote();
    if (')' == c) { ERROR_IF(!Parens, "extra paren", UNDEF); return RPAREN; }
    if ('.' == c) { ERROR_IF(!Parens, "free dot", UNDEF); return DOT; }
    if (isSymbolic(c, 0)) return read_sym(c);
    return syntax(UNDEF);
}

// Virtual Machine #############################################################

// Symbol layout:      (SYMB_TAG | SymTab-offset . value)
// Closure layout:     (CLOS_TAG | params-index . (environment . (body ...)))
// Environment layout: ((symbol . value) ...)

// Set answer
#define set_ans(d) (set_cdr(S_ans, d))

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
        case F_VALUE:      // (value sym)
            SYNTAX_IF(x, 1, 1);
            ERROR_IF(!isSymbol(ad), Type, x);
            n = lookup(ad);
            ERROR_IF(UNDEF == n, Undefined, ad);
            return n;
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
        case F_DEFINED:    // (defined? sym)
            SYNTAX_IF(x, 1, 1);
            ERROR_IF(!isSymbol(ad), Type, x);
            return (UNDEF != cdr(ad)) ? K_TRUE : NIL;
        case F_EOF:        // (eof? any)
            SYNTAX_IF(x, 1, 1);
            return (EOT == ad) ? K_TRUE : NIL;
        case F_PRINT:      // (print . any)
            SYNTAX_IF(x, 0, -1);
            for (n = cdr(x); n != NIL; n = cdr(n)) print_node(car(n), 0);
            return UNDEF;
        case F_NEWLINE:    // (newline)
            SYNTAX_IF(x, 0, 0);
            sys_newline();
            return UNDEF;
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
            return UNDEF;
        case F_GC:         // (gc [any])
            SYNTAX_IF(x, 0, 1);
            u = gc();
            if (ad != NIL) print_usage(NNODES - u);
            return UNDEF;
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
        default:
            return error("not callable", x);
    }
}

// Bind values as arguments
node bindargs(node v, node a) {
    node n;
    for (; !isAtom(v); a = cdr(a), v = cdr(v)) {
        ERROR_IF(isAtom(a), "missing args", Acc);
        n = cons(car(v), car(a));
        Env = cons(n, Env);
    }
    if (v != NIL) {
        n = cons(v, a);
        Env = cons(n, Env);
    } else ERROR_IF(NIL != a, "extra args", Acc);
    return NIL;
}

// Check tail position
#define isTail() (Modes[Mstack] <= MRETN)

// Apply function (with tail-call optimization)
node funapp(node c, node a) {
    node b = cdr(c);
    if (!isTail()) { vpush(Env); mpush(MRETN); }
    Env = car(b);
    if (ERR == bindargs(car(c), a)) return ERR;
    b = cdr(b);
    if (NIL != cdr(b)) { vpush(cdr(b)); mpush(MBEGIN); }
    return car(b);
}

// Special forms
node special(node x) {
    node hd = car(x), tl = cdr(x), ad = isAtom(tl) ? NIL : car(tl);

    switch(hd) {
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
            ERROR_IF(malformed(x), Syntax, x);
            tl = cons(Env, cdr(tl));
            tl = cons_tag(ad, tl, CLOS_TAG);
            vpush(tl);
            hd = cons(S_self, tl);
            hd = cons(hd, Env);
            set_car(cdr(tl), hd);
            return vpop();
        case S_LET:     // (let (v e) body ...)
            SYNTAX_IF(x, 2, -1);
            ERROR_IF(isAtom(ad) || badarity(ad, 1, 1), Syntax, x);
            hd = car(ad);
            ERROR_IF(!isSymbol(hd) || (hd < NRESERVED) || (hd == S_self), Syntax, x);
            vpush(hd);
            vpush(cdr(tl));
            mpush(MLET);
            return cadr(ad);
        case S_DEFINE:  // (define sym any)
            SYNTAX_IF(x, 2, 2);
            ERROR_IF(!isSymbol(ad), Type, x);
            ERROR_IF((ad < NRESERVED) || (ad == S_ans) || (ad == S_ver) || (ad == S_self), "reserved", ad);
            if (Fixed && (UNDEF != cdr(ad))) {
                for (hd = UserDef; hd != NIL; hd = cdr(hd))
                    if (car(hd) == ad) break;
                ERROR_IF(hd == NIL, "fixed", ad);
            }
            mpush(MSET);
            vpush(ad);        // name
            return cadr(tl);  // value
        default:        // unreachable
            return syntax(x);
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

    while ((m != MHALT) && (ERR != Acc)) {
        if (Interrupted) { Acc = ERR;   break; }
        if (Quit)        { Acc = UNDEF; break; }

        switch (m) {
            case MOR:     // resume after evaluating "ifnil"
                if (NIL != Acc) { vpop(); m = mpop(); break; }
                Acc = K_TRUE;
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
                    if (S_QUOTE == n) {
                        Acc = badarity(Acc, 1, 1) ? syntax(Acc) : cadr(Acc);
                        m = mpop();
                    } else {
                        Acc = special(Acc);
                        m = MEXPR;
                    }
                    break;
                }
                if ((F_CONS == n) && isTail() && !badarity(Acc, 2, 2)) {
                    n = cdr(Acc);
                    if (MCHAIN != Modes[Mstack]) {
                        i = cons(NIL, NIL);
                        vpush(i); vpush(i);
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
                    m = mpop();
                } else {
                    BREAK_IF(!isClosure(n), Acc, Type, Acc);
                    Acc = funapp(n, cdr(Acc));
                    m = MEXPR;
                }
                break;
            case MLET:
                n = cons(cadr(Vstack), Acc);
                if (isTail()) {
                    set_cdr(Vstack, cddr(Vstack));
                } else {
                    set_car(cdr(Vstack), Env);  // overwrite the name with the env to restore
                    mpush(MRETN);
                }
                Env = cons(n, Env);
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
                BREAK_IF(UNDEF == Acc, Acc, Undefined, n);
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
    return Acc;
}

// Load script file
node load(const char *s, bool ie) {
    int fd;
    if (IOdepth >= NLOAD) { error("nested load", UNDEF); return K_ERR; }
    fd = sys_open(s);
    if (fd < 0) { error(s, UNDEF); return K_ERR; }

    IOdepth++; IO.fd = fd; IO.pos = IO.len = 0; IO.next = EOT;

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
        } else if (Acc != UNDEF) set_ans(Acc);
    }
    Env = vpop();

    sys_close(fd);
    IOdepth--;
    if (Interrupted) set_ans(K_ERR);
    return cdr(S_ans);
}

// Entrypoint ##################################################################

// Break interrupt handler
void _interrupt _far _loadds on_break(void) { Interrupted = TRUE; }

// Register reserved symbols (needs to be same sequence as in enum)
void reserved(void) {
    const char* names[NRESERVED];
    unsigned int i;

    names[S_BEGIN]  = "begin";     names[S_DEFINE]  = "define";
    names[S_IF]     = "if";        names[S_IFNIL]   = "ifnil";
    names[S_LAMBDA] = "lambda";    names[S_LET]     = "let";
    names[S_QUOTE]  = "quote";

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
    names[F_VALUE]  = "value";

    names[K_COMMENT] = "comment";  names[K_ERR]    = "err";
    names[K_HELP]    = "?";        names[K_TRUE]   = "true";
    names[K_VERSTR]  = NAME "-" VERSION;

    for (i = 0; i < NRESERVED; i++)
        if (add_sym(names[i], SPCL) != i) fatal("init reserved");

    S_ans   = add_sym("ans", NIL);
    S_self  = add_sym("self", UNDEF);
    S_ver   = add_sym("ver", K_VERSTR);
}

// Build standard library path
void stdpath(char* buf, const char* exe) {
    char *dir = buf, *p = buf;
    for (; *exe; exe++) {
        *p++ = *exe;
        if ((*exe == '\\') || (*exe == ':')) dir = p;
    }
    strcp(dir, STDLIB);
}

// Reset state
void reset(void) {
    Interrupted = 0;
    Modes[0] = MHALT;
    Parens = Skip = Mstack = 0;
    Acc = Env = Vstack = Tmpcar = Tmpcdr = NIL;
}

// Entrypoint
int main(int argc, char **argv) {
    register unsigned int i;
    bool ignore, batch, status = 0;
    char stdlib[84];

    // Welcome banner
    sys_stamp(DOS_EOL TITLE DOS_EOL DOS_EOL);

    // Help screen
    if ((argc > 1) && streq(argv[1], "/?")) {
        sys_stamp("Usage: " NAME " [/?] [/B] [/I] [file]" DOS_EOL DOS_EOL
                  "  /?       This help" DOS_EOL
                  "  file     Load file" DOS_EOL
                  "  /B file  Load file, exit" DOS_EOL
                  "  /I file  Load file (ignore errors), exit" DOS_EOL DOS_EOL);
        return 0;
    }

    // Set break interrupt handler
    sys_break((void (_interrupt _far *)(void)) on_break);

    restart:  // Restart anchor

    // Init reader
    IOdepth = 0; IO.fd = IO.pos = IO.len = 0; IO.next = EOT;

    // Init heap
    for (i = NRESERVED; i < NNODES-1; i++) Heap[i].cdr = i+1;
    Heap[NNODES-1].cdr = NIL;
    Freelist = NRESERVED;
    reset();
    reserved();

    // Load standard library
    stdpath(stdlib, argv[0]);
    load(stdlib, FALSE);
    Fixed = TRUE;

    // Load script CLI arguments
    if (Restartbuf[0]) {
        load(Restartbuf, FALSE);
    } else if (argc > 1) {
        ignore = streq(argv[1], "/I") || streq(argv[1], "/i");
        batch = streq(argv[1], "/B") || streq(argv[1], "/b") || ignore;
        if (batch && (argc < 3)) {
            sys_stamp("missing filename" DOS_EOL);
            status = 1;
            Quit = TRUE;
        } else {
            status = (load(batch ? argv[2] : argv[1], ignore) == K_ERR);
            if (batch) Quit = TRUE;
        }
    }

    // REPL
    while (!Quit) {
        reset();
        Acc = parse();
        if ((EOT == Acc) && !Interrupted) break;
        if (K_HELP == Acc) Acc = print_help();
        if (UNDEF == Acc) continue;
        if ((ERR != Acc) && !Interrupted) Acc = eval(Acc);
        if ((ERR == Acc) || Interrupted) {
            set_ans(K_ERR);
            IO.pos = IO.len;
            IO.next = EOT;
        } else if (Acc != UNDEF) {
            set_ans(Acc);
            print_node(Acc, 0);
            sys_newline();
        }
    }

    // Restart
    if (Restart) {
        Symtop = Pooltop = 0;
        Symbols = StdDef = UserDef = NIL;
        Restart = Fixed = Quit = FALSE;
        reset();
        argc = 1;
        goto restart;
    }

    // Exit banner
    sys_stamp(DOS_EOL "Bye" DOS_EOL DOS_EOL);

    sys_break(NONE);
    return status;
}
