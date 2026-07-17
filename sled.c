#define NAME "sled"
#define VERSION "0.1"
#define TITLE "SLED - Schemy LISP en DOS (" VERSION ")"

#if !defined(__SMALL__) && !defined(M_I86SM) && !defined(_M_I86SM)
#error "Build only with the small memory model!"
#endif

// Typedefs ####################################################################

typedef int bool;
typedef unsigned int node;

// Configuration ###############################################################

#define NONE       ((void*)0)    // Null Pointer
#define TRUE       1
#define FALSE      0
#define DOS_EOF    0x1A
#define DOS_EOL    "\r\n"

#define PROMPT     NAME "> "
#define STDLIB     NAME ".scm"
#define BAR_WIDTH  20

#define NNODES     12288         // Number of nodes
#define SYMTAB     (2048 + 192)  // Maximum total symbol characters
#define SYMLEN     16            // Maximum symbol length
#define BUFLEN     128           // Input buffer sizes
#define PRDEPTH    128           // Maximum print and parse recursion depth
#define NLOAD      2             // Maximum nested loads

// Enumerations ################################################################

enum { SPCL = NNODES, UNDEF, RPAREN, DOT, EOT, ERR, NIL };  // Sentinels (live above the node heap)

enum { MHALT = 0, MEXPR, MLIST, MBETA, MRETN, MAPPL, MPRED, MSET, MBEGIN, MLET };  // Trampoline Modes

enum { S_APPLY = 0, S_BEGIN, S_DEFINE, S_IF, S_IFNIL, S_LAMBDA, S_LET, S_QUOTE,  // Special Forms
       NSPECIAL };

enum { F_ATOM = NSPECIAL, F_CONS, F_DEFINED, F_EMPTY, F_ENV, F_EOF, F_EQUIV, F_ERROR, F_EXIT,  // Builtin Functions
       F_GC, F_HEAD, F_LOAD, F_NEWLINE, F_PRINT, F_PROC, F_READ, F_SYMBOL, F_TAIL,
       K_COMMENT, K_ERR, K_HELP, K_TRUE, K_SPACE, K_VERSTR,  // Special Symbols
       NRESERVED };

// Globals #####################################################################

// Heap
struct { node car, cdr; } Heap[NNODES];  // Each node is two 16-bit words
node Freelist;

// Registers (GC Roots)
node Acc = NIL;      // Current expression
node Env = NIL;      // Lexical environment (association list)
node Vstack = NIL;   // Value stack
node Mstack = NIL;   // Mode stack (car(Mstack) holds the pending mode)
node Tmpcar = NIL;   // Scratch cons cell head
node Tmpcdr = NIL;   // Scratch cons cell tail
node Symbols = NIL;  // Symbol table
node StdDef = NIL;   // Standard library symbols
node UserDef = NIL;  // User defined symbols

// Symbol Table
unsigned int Symtop = 0;
unsigned int Pooltop = 0;
char Symtab[SYMTAB];

// Reader
struct { int fd, pos, len; char buf[BUFLEN]; int next; } IOstate[1 + NLOAD];
int IOdepth;
#define IO IOstate[IOdepth]

// Control
int Parens;                 // counter for open parenthesis
int Rdstate;                // state of the input reader used by (read)
volatile bool Interrupted;  // set by on_break() Ctrl+C handler and by rdch() if error
bool Quit;                  // flag signaling that an (exit) was evaluated
bool Fixed;                 // flag that once true new defines register in UserDef
bool Skip;                  // flag signaling an open block comment

// Symbols
node S_ans, S_self, S_space, S_ver;

// Bitmasks ####################################################################

#define PTR_MASK  0x3FFF  // 0b0011111111111111 , car and cdr words have 14-bit payloads
#define TAG_MASK  0xC000  // 0b1100000000000000 , car and cdr words have 2-bit tags

#define CONS_TAG  0x0000  // 0b0000000000000000 , car tag: payload is node index
#define ATOM_TAG  0x4000  // 0b0100000000000000 , car tag: payload is sentinel or raw integer
#define CLOS_TAG  0x8000  // 0b1000000000000000 , car tag: payload is index of parameter list
#define SYMB_TAG  0xC000  // 0b1100000000000000 , car tag: payload is symbol table index

#define SWAP_TAG  0x4000  // 0b0100000000000000 , cdr tag: only used during gc
#define MARK_TAG  0x8000  // 0b1000000000000000 , cdr tag: only used during gc

// System I/O ##################################################################

// Call DOS interrupt 21h (small memory model only)
int int21h(unsigned int a, unsigned int b, unsigned int c, const char* d) {
    int r = -1;
    _asm {
        mov ax, a  // function:subfunction codes
        mov bx, b  // usually a handle
        mov cx, c  // usually a length
        mov dx, d  // usually a pointer
        int 21h
        jnc ok
        mov ax, -1
        ok:
        mov r, ax
    }
    return r;
}

// Print string to standard output
void sys_print(const char* msg) {
    int len;
    for (len = 0; msg[len]; len++) ;
    int21h(0x4000, 1, len, msg);
}

// Print newline to standard output
void sys_newline() {
    int21h(0x4000, 1, sizeof(DOS_EOL) - 1, DOS_EOL);
}

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
            mov ax, 351Bh
            int 21h
            mov WORD PTR old1b, bx
            mov WORD PTR old1b+2, es
        }
        _asm {  // set new int 23h and int 1Bh handler
            push ds
            mov ax, 2523h
            lds dx, DWORD PTR hdl
            int 21h
            mov ax, 251Bh
            lds dx, DWORD PTR hdl
            int 21h
            pop ds
        }
    } else {
        _asm {  // restore old int 1Bh handler
            push ds
            mov ax, 251Bh
            lds dx, DWORD PTR old1b
            int 21h
            pop ds
        }
    }
}

// Utilities ###################################################################

// Test if two strings are equal
bool _fastcall streq(const char *s, const char* t) /*pure*/ {
    while (*s && *s == *t) { s++; t++; }
    return *s == *t;
}

// Accessors ###################################################################

#define car(x)    (Heap[x].car & PTR_MASK)
#define cdr(x)    (Heap[x].cdr & PTR_MASK)

#define caar(x)   car(car(x))
#define cadr(x)   car(cdr(x))       // second
#define cdar(x)   cdr(car(x))
#define cddr(x)   cdr(cdr(x))
#define cadar(x)  car(cdr(car(x)))
#define caddr(x)  car(cdr(cdr(x)))  // third

// Mutators ####################################################################

// Set head of node
void _fastcall set_car(node x, node v) {
    Heap[x].car = (Heap[x].car & TAG_MASK) | (v & PTR_MASK);
}

// Set tail of node
void _fastcall set_cdr(node x, node v) {
    Heap[x].cdr = (Heap[x].cdr & TAG_MASK) | (v & PTR_MASK);
}

// Predicates ##################################################################

// Test if admissible symbol character
bool _fastcall isSymbolic(int c, unsigned int i) /*const*/ {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c == '?') ||
           (c >= '0' && c <= '9') || (c == '-') || (c == '_') || (c == '\\') ||
           (c == '.' && i > 0);
}

// Test if atom
bool _fastcall isAtom(node n) /*pure*/ {
    return (n >= SPCL) || !!(Heap[n].car & TAG_MASK);
}

// Test if symbol
bool _fastcall isSymbol(node n) /*pure*/ {
    return (n < SPCL) && ((Heap[n].car & TAG_MASK) == SYMB_TAG);
}

// Test if closure
bool _fastcall isClosure(node n) /*pure*/ {
    return (n < SPCL) && ((Heap[n].car & TAG_MASK) == CLOS_TAG);
}

// Test if top-level
bool _fastcall isToplevel(void) /*pure*/ {
    return (Mstack == NIL) || (cdr(Mstack) == NIL);  // only MHALT is on Mstack
}

// Printing ####################################################################

// Convert symbol to string
#define symstr(n) (Symtab + car(n))

// Print node semi-recursively
void print_node(node n, int d) {
    if (n == NIL)          sys_print("nil");
    else if (n == EOT)     sys_print("*eot*");
    else if (n == UNDEF)   sys_print("*undef*");
    else if (isSymbol(n))  sys_print(symstr(n));
    else if (isClosure(n)) sys_print("*closure*");
    else if (isAtom(n))    sys_print("*unprintable*");
    else if (d < 0)        sys_print("*list*");
    else if (d >= PRDEPTH) sys_print("...");
    else {
        sys_print("(");
        for (;;) {
            print_node(car(n), d+1);
            n = cdr(n);
            if (n == NIL) break;
            if (isAtom(n)) {
                sys_print(" . ");
                print_node(n, d+1);
                break;
            }
            sys_print(" ");
        }
        sys_print(")");
    }
}

// Print user-defined environment
void print_env(void) {
    node u;
    for (u = UserDef; u != NIL; u = cdr(u)) {
        sys_print(symstr(car(u)));
        sys_print("\t");
        print_node(cdar(u), -1);
        sys_newline();
    }
}

// Print memory usage
void print_usage(unsigned int used) {
    char buf[BAR_WIDTH + 3];
    unsigned int i, filled = used / (NNODES / BAR_WIDTH);

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
    sys_print("! fatal: ");
    sys_print(m);
    sys_newline();
    sys_break(NONE);
    sys_abort();
}

// Handle recoverable error (assume syntax error for null message)
node error(const char *m, node n) {
    sys_print("? ");
    sys_print(m);
    if (n != UNDEF) {
        sys_print(": ");
        print_node(n, 0);
    }
    sys_newline();
    return ERR;
}

// Guard macro
#define ERROR_IF(c, m, n) do { if (c) return error(m, n); } while(0)

// Breaking guard macro
#define BREAK_IF(c, m, n) if (c) { err = error(m, n); break; }

// Syntax error wrapper
node syntax(node n) {
    return error("syntax", n);
}

// Test if number of arguments is wrong
bool _fastcall badarity(node x, int k0, int kn) /*pure*/ {
    int i;
    for (i = 0; !isAtom(x); x = cdr(x), i++)
        if ((kn != -1) && (i >= kn)) return TRUE;
    return (x != NIL) || (i < k0);
}

// Syntax guard macro
#define SYNTAX_IF(x, m, n) do { if (badarity(x, m, n)) return syntax(x); } while(0)

// Test if lambda is malformed (and "self" is not used as a parameter name)
bool malformed(node p) /*pure*/ {
    node q;
    for (p = cadr(p); !isAtom(p); p = cdr(p)) {
        q = car(p);
        if (!isSymbol(q) || (q < NRESERVED) || (q == S_self)) return TRUE;
    }
    return (p != NIL) && (!isSymbol(p) || (p < NRESERVED) || (p == S_self));
}

// Garbage Collection ##########################################################

// Deutsch–Schorr–Waite algorithm  TODO
void mark(node n) {
    node p = NIL, t;
    if  ((n < NRESERVED) || (n >= SPCL)) return;
    for (;; n = t) {
        if ((n < NRESERVED) || (n >= SPCL) || (Heap[n].cdr & MARK_TAG)) {
            if (p == NIL) break;
            if (Heap[p].cdr & SWAP_TAG) {  // done with car, trace cdr now
                t = cdr(p);
                set_cdr(p, car(p));
                set_car(p, n);
                Heap[p].cdr &= ~SWAP_TAG;
            } else {                       // done with cdr, go to parent
                t = p;
                p = cdr(t);
                set_cdr(t, n);
            }
        } else {
            if (Heap[n].car & ATOM_TAG) {  // car is raw, trace cdr now
                t = cdr(n);
                set_cdr(n, p);
                Heap[n].cdr |= MARK_TAG;
            } else {                       // trace car now, revisit cdr later
                t = car(n);
                set_car(n, p);
                Heap[n].cdr |= (MARK_TAG | SWAP_TAG);
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
    mark(Mstack);
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
node _fastcall cons_t(node a, node d, node t) {
    node n;
    if (Freelist == NIL) {
        Tmpcdr = d;
        Tmpcar = (t == ATOM_TAG || t == SYMB_TAG) ? NIL : a;
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
#define cons(a, d) cons_t(a, d, CONS_TAG)

// Push on to mode stack
void _fastcall mpush(node n) {
    Mstack = cons_t(n, Mstack, ATOM_TAG);  // atom prevents GC from tracing raw enums (integers)
}

// Pop from mode stack
node mpop(void) {
    node n;
    if (Mstack == NIL) fatal("mstack empty");
    n = car(Mstack);
    Mstack = cdr(Mstack);
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

// Create two element list
node list2(node a, node b) {
    vpush(a);
    b = cons(b, NIL);
    a = vpop();
    return cons(a, b);
}

// Symbols #####################################################################

// Find symbol in symbol table
node find_sym(const char *s) /*pure*/ {
    node p, sym;
    for (p = Symbols; p != NIL; p = cdr(p)) {
        sym = car(p);
        if (streq(s, symstr(sym))) return sym;
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
    while (Symtab[Symtop++] = *s++) ;  // string copy

    if (v == SPCL) {  // take a reserved node (no GC)
        n = Pooltop++;
        Heap[n].car = off | SYMB_TAG;
        Heap[n].cdr = n;  // if SPCL, symbol's value cell self-evaluates
    } else n = cons_t(off, v, SYMB_TAG);

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
    } else {
        if (IO.pos >= IO.len) {
            if ((IOdepth == 0) && (Parens == 0) && (Rdstate == 0) &&
                isToplevel()) sys_print(PROMPT);  // print prompt if at top-level
            IO.len = sys_read(IO.fd, BUFLEN, IO.buf);
            if (Interrupted) return EOT;
            if (IO.len <= 0) {
                if (IO.len < 0) { error("read", UNDEF); Interrupted = TRUE; }
                return EOT;
            }
            IO.pos = 0;
        }
        c = (unsigned char) IO.buf[IO.pos++];
    }
    return (c == DOS_EOF) ? EOT : c;
}

// Read symbol
node read_sym(int c) {
    char s[SYMLEN+1];
    unsigned int i = 0;
    while (isSymbolic(c, i)) {
        if ('\\' == c) {
            c = read_char();
            ERROR_IF(c == EOT, "bad escape", UNDEF);
            ERROR_IF((c == 0xE0) || (c == 0x00), "extended char", UNDEF);
        } else if (c >= 'A' && c <= 'Z') c += 32;
        ERROR_IF(i >= SYMLEN, "overlong symbol", (IO.next = c, UNDEF));
        s[i++] = c;
        c = read_char();
        ERROR_IF((c == 0xE0) || (c == 0x00), "extended char", UNDEF);  // 2-byte characters can cause input corruption
    }
    s[i] = 0;
    IO.next = c;
    return (streq(s, "nil")) ? NIL : add_sym(s, UNDEF);  // add_sym checks if the symbol exists and if so returns its index
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
        if ((Parens == 0) && (c == '\n') && (Rdstate > 0)) {
            Rdstate--;
            if (Rdstate == 0) return EOT;
        }
        if ((c != ' ') && (c != '\t') && (c != '\n') && (c != '\r')) return c;
    }
}

node parse(void);

// Parse rooted tree TODO
node parse_list(void) {
    node n, c, t = NIL, err = NIL;
    bool entered = Skip;
    ERROR_IF(Parens >= PRDEPTH, "read depth", UNDEF);
    Parens++;
    vpush(NIL);
    for (;;) {
        n = parse(); if (ERR == n) break;
        BREAK_IF(EOT == n, "missing paren", UNDEF);
        if (RPAREN == n) break;
        if (UNDEF == n) continue;
        if (Skip) continue;
        if ((NIL == t) && (n == K_COMMENT)) { Skip = TRUE; continue; }
        if (DOT == n) {
            BREAK_IF(t == NIL, "bad pair", UNDEF);
            n = parse(); if (ERR == n) break;
            BREAK_IF((n == DOT) || (n == RPAREN) || (n == EOT), "bad pair", UNDEF);
            set_cdr(t, n);
            n = parse(); if (ERR == n) break;
            BREAK_IF(n != RPAREN, "bad pair", UNDEF);
            break;
        }
        c = cons(n, NIL);
        if (NIL == t) set_car(Vstack, c); else set_cdr(t, c);
        t = c;
    }
    if ((ERR == err) || (ERR == n)) n = ERR;
    else if (entered) n = NIL;
    else if (Skip) n = UNDEF;
    else n = car(Vstack);
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
    return list2(S_QUOTE, n);
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
// Environment layout: (symbol ...)
// Closure layout:     (CLOS_TAG | params-index . (environment . (body ...)))

// Set answer
#define set_ans(d) (set_cdr(S_ans, d))

node eval(node x);

// Load script file
node load(const char *s, bool ie) {
    int fd;
    ERROR_IF(IOdepth >= NLOAD, "nested load", UNDEF);
    fd = sys_open(s);
    ERROR_IF(fd < 0, s, UNDEF);

    IOdepth++; IO.fd = fd; IO.pos = IO.len = 0; IO.next = EOT;

    vpush(Env);
    set_cdr(S_ans, NIL);  // Set "ans" in case of empty file
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
    return (ERR == Acc) ? ERR : cdr(S_ans);
}

// Built-in functions
node builtin(node x) {
    unsigned int u;
    node fn = car(x), ad = cdr(x);
    if (ad != NIL) ad = car(ad);

    switch(fn) {
        case F_HEAD:       // (head lst)
            SYNTAX_IF(x, 2, 2);
            ERROR_IF(isAtom(ad), "type", x);
            return car(ad);
        case F_TAIL:       // (tail lst)
            SYNTAX_IF(x, 2, 2);
            ERROR_IF(isAtom(ad), "type", x);
            return cdr(ad);
        case F_CONS:       // (cons any any)
            SYNTAX_IF(x, 3, 3);
            return cons(ad, caddr(x));
        case F_EMPTY:      // (empty? lst)
            SYNTAX_IF(x, 2, 2);
            return (ad == NIL) ? K_TRUE : NIL;
        case F_EQUIV:      // (equiv? any any)
            SYNTAX_IF(x, 3, 3);
            return (ad == caddr(x)) ? K_TRUE : NIL;
        case F_ATOM:       // (atom? any)
            SYNTAX_IF(x, 2, 2);
            return isAtom(ad) ? K_TRUE : NIL;
        case F_SYMBOL:     // (symbol? any)
            SYNTAX_IF(x, 2, 2);
            return isSymbol(ad) ? K_TRUE : NIL;
        case F_PROC:       // (proc? any)
            SYNTAX_IF(x, 2, 2);
            return isClosure(ad) ? K_TRUE : NIL;
        case F_DEFINED:    // (defined? sym)
            SYNTAX_IF(x, 2, 2);
            ERROR_IF(!isSymbol(ad), "type", x);
            return (UNDEF != cdr(ad)) ? K_TRUE : NIL;
        case F_EOF:        // (eof? any)
            SYNTAX_IF(x, 2, 2);
            return (EOT == ad) ? K_TRUE : NIL;
        case F_PRINT:      // (print . any)
            SYNTAX_IF(x, 1, -1);
            for (u = cdr(x); u != NIL; u = cdr(u)) print_node(car(u), 0);
            return isToplevel() ? UNDEF : NIL;
        case F_NEWLINE:    // (newline)
            SYNTAX_IF(x, 1, 1);
            sys_newline();
            return isToplevel() ? UNDEF : NIL;
        case F_LOAD:       // (load sym)
            SYNTAX_IF(x, 2, 3);
            ERROR_IF(!isSymbol(ad), "type", x);
            return load(symstr(ad), (cddr(x) != NIL) && (caddr(x) != NIL));
        case F_READ:       // (read)
            SYNTAX_IF(x, 1, 1);
            Rdstate = 2;
            do { x = parse(); } while (UNDEF == x);
            Rdstate = 0;
            return x;
        case F_ERROR:      // (error sym [any])
            SYNTAX_IF(x, 2, 3);
            ERROR_IF(!isSymbol(ad), "type", x);
            return error(symstr(ad), cddr(x) == NIL ? UNDEF : caddr(x));
        case F_ENV:        // (env)
            SYNTAX_IF(x, 1, 1);
            print_env();
            return isToplevel() ? UNDEF : NIL;
        case F_GC:         // (gc [any])
            SYNTAX_IF(x, 1, 2);
            u = gc();
            if (ad != NIL) print_usage(NNODES - u);
            return isToplevel() ? UNDEF : NIL;
        case F_EXIT:       // (exit)
            SYNTAX_IF(x, 1, 1);
            Quit = TRUE;
            return UNDEF;
        default:
            return error("not callable", x);
    }
}

// Bind values as arguments
node bindargs(node v, node a) {
    node e = NIL, n;
    vpush(e);
    for (; !isAtom(v); a = cdr(a), v = cdr(v)) {
        ERROR_IF(NIL == a, "missing args", (vpop(), Acc));
        n = cons(car(v), car(a));
        e = cons(n, e);
        set_car(Vstack, e);
    }
    if (isSymbol(v)) {
        n = cons(v, a);
        e = cons(n, e);
        set_car(Vstack, e);
    } else ERROR_IF(NIL != a, "extra args", (vpop(), Acc));
    Env = cons(e, Env);
    vpop();
    return NIL;
}

// Apply function (with tail-call optimization)
node funapp(node x) {
    node c = car(x);
    bool tc;
    ERROR_IF(!isClosure(c), "not a function", x);

    tc = (car(Mstack) == MRETN);  // MBETA (caller) is reached with MRETN on top only if previous funapp left it there, but at least MHALT is on Mstack

    if (!tc) { vpush(Env); mpush(MRETN); }
    Env = cadr(c);
    if (ERR == bindargs(car(c), cdr(x))) {
        if (!tc) { Env = vpop(); mpop(); }
        return ERR;
    }
    vpush(cddr(c));
    mpush(MBEGIN);
    return NIL;
}

// Special forms
node special(node x, node *pm) {
    node n, fn = car(x), ad = cdr(x);
    if (ad != NIL) ad = car(ad);

    switch(fn) {
        case S_QUOTE:   // (quote any)
            SYNTAX_IF(x, 2, 2);
            *pm = mpop();
            return ad;
        case S_IF:      // (if any any [any])
            SYNTAX_IF(x, 3, 4);
            *pm = MEXPR;
            mpush(MPRED);
            vpush(cddr(x));  // (then, else)
            return ad;       // predicate
        case S_BEGIN:   // (begin . any)
            SYNTAX_IF(x, 1, -1);
            *pm = MEXPR;
            if (NIL == cdr(x)) return NIL;
            if (NIL == cddr(x)) return ad;
            mpush(MBEGIN);
            vpush(cddr(x));
            return ad;
        case S_IFNIL:   // (ifnil any any)
            SYNTAX_IF(x, 3, 3);
            *pm = MEXPR;
            mpush(MPRED);
            vpush(list2(UNDEF, caddr(x)));
            return ad;
        case S_LAMBDA:  // (lambda lst any ...)
            SYNTAX_IF(x, 3, -1);
            ERROR_IF(malformed(x), "syntax", x);
            *pm = mpop();
            n = cons(Env, cddr(x));
            n = cons_t(ad, n, CLOS_TAG);
            vpush(n);
            ad = cons(S_self, n);
            ad = cons(ad, NIL);
            ad = cons(ad, Env);
            set_car(cdr(n), ad);
            return vpop();
        case S_LET:     // (let (v e) body ...)
            SYNTAX_IF(x, 3, -1);
            ERROR_IF(isAtom(ad) || !isSymbol(car(ad)) || (car(ad) < NRESERVED)
                     || (car(ad) == S_self) || badarity(ad, 2, 2), "syntax", x);
            *pm = MEXPR;
            vpush(car(ad));
            vpush(cddr(x));
            mpush(MLET);
            return cadr(ad);
        case S_APPLY:   // (apply any any)
            SYNTAX_IF(x, 3, 3);
            *pm = MEXPR;
            mpush(MAPPL);
            vpush(caddr(x));
            vpush(ERR);
            return ad;
        case S_DEFINE:  // (define sym any)
            SYNTAX_IF(x, 3, 3);
            ERROR_IF(!isSymbol(ad), "type", x);
            *pm = MEXPR;
            mpush(MSET);
            vpush(ad);        // name
            return caddr(x);  // value
        default:
            return syntax(x);
    }
}

// Resolve symbol in current scope
node _fastcall lookup(node n) /*pure*/ {
    register node e, a;
    if (n >= NRESERVED)
        for (e = Env; e != NIL; e = cdr(e))
            for (a = car(e); a != NIL; a = cdr(a))
                if (caar(a) == n) return cdar(a);
    return cdr(n);  // The symbol's value is its own cdr
}

// Evaluate expression (caller must reset or restore Env)
node eval(node x) {
    node i, n, m = MEXPR, err = NIL;
    node sV = Vstack, sM = Mstack;
    Acc = x;
    mpush(MHALT);

    while (!Interrupted && !Quit) {
        switch (m) {
            case MEXPR:   // evaluate Acc as an expression
                if (isSymbol(Acc)) {
                    n = Acc;
                    Acc = lookup(Acc);
                    BREAK_IF(UNDEF == Acc, "undefined", n);
                    m = mpop();
                } else if (isAtom(Acc)) m = mpop();
                else if (car(Acc) < NSPECIAL) Acc = special(Acc, &m);
                else {
                    vpush(cdr(Acc));  // unevaluated argument list
                    vpush(NIL);       // result evaluated argument list
                    Acc = car(Acc);
                    mpush(MLIST);
                }
                break;
            case MLIST:   // accumulating an evaluated argument list
                n = cons(Acc, NIL);
                if (NIL == car(Vstack)) set_car(Vstack, n);
                else {
                    for(i = car(Vstack); cdr(i) != NIL; i = cdr(i)) ;
                    set_cdr(i, n);
                }
                i = cadr(Vstack);
                if (isAtom(i)) {  // all arguments evaluated or improper tail
                    BREAK_IF(i != NIL, "improper call", i);
                    Acc = car(Vstack);
                    vpop();
                    vpop();
                    m = MBETA;
                } else {          // evaluate next argument
                    Acc = car(i);
                    set_car(cdr(Vstack), cdr(i));
                    mpush(m);
                    m = MEXPR;
                }
                break;
            case MBEGIN:  // sequencing of a body
                if (NIL == cdar(Vstack)) Acc = car(vpop());
                else {
                    Acc = caar(Vstack);
                    set_car(Vstack, cdar(Vstack));
                    mpush(MBEGIN);
                }
                m = MEXPR;
                break;
            case MPRED:   // resume after evaluating "if" and "ifnil"
                n = vpop();
                m = MEXPR;
                if (NIL == Acc) Acc = (NIL == cdr(n)) ? UNDEF : cadr(n);
                else if (car(n) == UNDEF) m = mpop();
                else Acc = car(n);
                break;
            case MBETA:   // application: builtin or closure
                Acc = isSymbol(car(Acc)) ? builtin(Acc) : funapp(Acc);
                m = mpop();
                break;
            case MHALT:   // returns from eval
                return Acc;
            case MAPPL:   // (apply fn args): two-stage eval of fn then args
                if (ERR == car(Vstack)) {  // function evaluated
                    set_car(Vstack, Acc);
                    Acc = cadr(Vstack);
                    mpush(MAPPL);
                    m = MEXPR;
                } else {                   // arguments evaluated
                    n = vpop();
                    vpop();
                    for (i = Acc; !isAtom(i); i = cdr(i)) ;  // test for proper list
                    BREAK_IF(i != NIL, "improper args", Acc);
                    Acc = cons(n, Acc);
                    m = MBETA;
                }
                break;
            case MSET:    // resume after evaluating "define" value
                n = vpop();
                BREAK_IF((n == S_ans) || (n == S_ver) || (n == S_self) ||
                         (n == S_space) || (n < NRESERVED), "reserved", n);  // The second last test catches SPCL symbols, the last test prevents immutable symbols
                if(Fixed) {
                    if (UNDEF == cdr(n)) UserDef = cons(n, UserDef);
                    else {
                        for (i = UserDef; i != NIL; i = cdr(i))
                            if (car(i) == n) break;
                        BREAK_IF(i == NIL, "fixed", n);
                    }
                } else if (UNDEF == cdr(n)) StdDef = cons(n, StdDef);
                set_cdr(n, Acc);
                m = mpop();
                break;
            case MLET:
                n = cons(cadr(Vstack), Acc);
                n = cons(n, NIL);
                set_car(cdr(Vstack), Env);  // overwrite the pushed variable name with Env
                Env = cons(n, Env);
                mpush(MRETN);
                m = MBEGIN;
                break;
            case MRETN:   // restore callers Env after a non-tail call
                Env = vpop();
                m = mpop();
                break;
        }
        if ((ERR == Acc) || (ERR == err)) break;
    }
    Vstack = sV;
    Mstack = sM;
    return (Interrupted || (ERR == Acc) || (ERR == err)) ? ERR : UNDEF;
}

// Register reserved symbols (needs to be same sequence as in enum)
void reserved(void) {
    static const char* names[NRESERVED];
    unsigned int i;

    names[S_APPLY]   = "apply";     names[S_BEGIN]  = "begin";
    names[S_DEFINE]  = "define";    names[S_IF]     = "if";
    names[S_IFNIL]   = "ifnil";     names[S_LAMBDA] = "lambda";
    names[S_LET]     = "let";       names[S_QUOTE]  = "quote";

    names[F_ATOM]    = "atom?";     names[F_CONS]   = "cons";
    names[F_DEFINED] = "defined?";  names[F_EMPTY]  = "empty?";
    names[F_ENV]     = "env";       names[F_EOF]    = "eof?";
    names[F_EQUIV]   = "equiv?";    names[F_ERROR]  = "error";
    names[F_EXIT]    = "exit";      names[F_GC]     = "gc";
    names[F_HEAD]    = "head";      names[F_LOAD]   = "load";
    names[F_NEWLINE] = "newline";   names[F_PRINT]  = "print";
    names[F_PROC]    = "proc?";     names[F_READ]   = "read";
    names[F_SYMBOL]  = "symbol?";   names[F_TAIL]   = "tail";

    names[K_COMMENT] = "comment";   names[K_ERR]    = "err";
    names[K_HELP]    = "?";         names[K_TRUE]   = "true";
    names[K_SPACE]   = " ";         names[K_VERSTR]  = NAME "-" VERSION;

    for (i = 0; i < NRESERVED; i++)
        if (add_sym(names[i], SPCL) != i) error("init reserved", UNDEF);

    S_ans   = add_sym("ans", NIL);
    S_self  = add_sym("self", UNDEF);
    S_space = add_sym("_", K_SPACE);
    S_ver   = add_sym("ver", K_VERSTR);
}

// Entrypoint ##################################################################

// Break interrupt handler
void _interrupt _far _loadds on_break(void) { Interrupted = TRUE; }

// Reset state
void reset(void) {
    Rdstate = Parens = Interrupted = Skip = 0;
    Acc = Env = Vstack = Mstack = Tmpcar = Tmpcdr = NIL;
}

// Display help
node help(void) {
    int u;
    sys_print("special forms:" DOS_EOL);
    for (u = 0; u < NSPECIAL; u++) { sys_print(" "); sys_print(symstr(u)); }
    sys_print(DOS_EOL "builtin functions:" DOS_EOL);
    for (u = NSPECIAL; u < K_COMMENT; u++) { sys_print(" "); sys_print(symstr(u)); }
    sys_print(DOS_EOL "standard library:" DOS_EOL);
    for (u = StdDef; u != NIL; u = cdr(u)) { sys_print(" "); sys_print(symstr(car(u))); }
    sys_newline();
    return UNDEF;
}

// Entrypoint
int main(int argc, char **argv) {
    register unsigned int i;
    bool ignore, batch, status = 0;

    // Welcome banner
    sys_print(DOS_EOL TITLE DOS_EOL DOS_EOL);

    // Help screen
    if ((argc > 1) && streq(argv[1], "/?")) {
        sys_print("Usage: " NAME " [/?] [/b] [/i] [file]" DOS_EOL DOS_EOL
                  "  /?       This help" DOS_EOL
                  "  file     Load file" DOS_EOL
                  "  /b file  Load file, exit" DOS_EOL
                  "  /i file  Load file (ignore errors), exit" DOS_EOL DOS_EOL);
        return 0;
    }

    // Set break interrupt handler
    sys_break((void (_interrupt _far *)(void)) on_break);

    // Init reader
    IOdepth = 0; IO.fd = IO.pos = IO.len = 0; IO.next = EOT;

    // Init heap
    for (i = NRESERVED; i < NNODES-1; i++) Heap[i].cdr = i+1;
    Heap[NNODES-1].cdr = NIL;
    Freelist = NRESERVED;
    reserved();

    // Load standard library
    if (ERR == load(STDLIB, FALSE)) error("stdlib", UNDEF);
    Fixed = TRUE;

    // Load script CLI arguments
    if (argc > 1) {
        reset();
        ignore = (argc > 2) && streq(argv[1], "/i");
        batch = ((argc > 2) && streq(argv[1], "/b")) || ignore;
        if (batch && (argc < 3)) {
            error("missing filename", UNDEF);
            status = 1;
            Quit = TRUE;
        } else {
            status = load(batch ? argv[2] : argv[1], ignore) == ERR;
            Quit = batch;
        }
    }

    // REPL
    while (!Quit) {
        reset();
        Acc = parse();
        if ((EOT == Acc) && !Interrupted) break;
        if (K_HELP == Acc) Acc =  help();
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

    // Exit banner
    sys_print(DOS_EOL "Bye" DOS_EOL DOS_EOL);

    sys_break(NONE);
    return status;
}
