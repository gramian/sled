#define NAME "sled"
#define VERSION "0.3"
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

#define NNODES     (12288 + 80)  // Number of nodes
#define SYMTAB     (2048 + 240)  // Maximum total symbol characters
#define SYMLEN     16            // Maximum symbol length
#define BUFLEN     120           // Input buffer sizes
#define PRDEPTH    128           // Maximum print and parse recursion depth
#define NMODES     1024          // Maximum mode stack entries
#define NLOAD      2             // Maximum nested loads

// Enumerations ################################################################

enum { SPCL = NNODES, UNDEF, RPAREN, DOT, EOT, ERR, NIL };  // Sentinels (live above the node heap)

enum { MHALT = 0, MEXPR, MLIST, MCALL, MRETN, MPRED, MOR, MSET, MBEGIN, MLET };  // Trampoline Modes

enum { S_BEGIN = 0 , S_DEFINE, S_IF, S_IFNIL, S_LAMBDA, S_LET, S_QUOTE,  // Special Forms
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
node Env = NIL;      // Lexical environment (association list)
node Vstack = NIL;   // Value stack
node Tmpcar = NIL;   // Scratch cons cell head
node Tmpcdr = NIL;   // Scratch cons cell tail
node Symbols = NIL;  // Symbol table
node StdDef = NIL;   // Standard library symbols
node UserDef = NIL;  // User defined symbols

node Restartsym = UNDEF;    // UNDEF: none, NIL: bare restart, else file symbol
char Restartbuf[SYMLEN+1];

// Mode Stack
unsigned int Mstack = 0;
char Modes[NMODES];

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
int Rdlines;                // state of the input reader used by (read)
volatile bool Interrupted;  // set by on_break() Ctrl+C handler and by rdch() if error
bool Quit;                  // flag signaling that an (exit) was evaluated
bool Fixed;                 // flag that once true new defines register in UserDef
bool Skip;                  // flag signaling an open block comment

// Symbols
node S_ans, S_self, S_ver;

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
    while (*s && *s == *t) { s++; t++; }
    return *s == *t;
}

// System I/O ##################################################################

// Call DOS interrupt 21h (small memory model only)
int int21h(unsigned int a, unsigned int b, unsigned int c, const char* d) {
    int r;
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
    if (*msg) int21h(0x4000, 1, strend(msg) - msg, msg);
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
        _asm {  // restore old int 1Bh handler
            push ds
            mov ax, 251Bh
            lds dx, DWORD PTR old1b
            int 21h
            pop ds
        }
    }
}

// Accessors ###################################################################

#define car(x)         (Heap[x].car & PTR_MASK)
#define cdr(x)         (Heap[x].cdr)

#define caar(x)        car(car(x))
#define cadr(x)        car(cdr(x))       // second
#define cdar(x)        cdr(car(x))
#define cddr(x)        cdr(cdr(x))

#define cadar(x)       car(cdr(car(x)))
#define caddr(x)       car(cdr(cdr(x)))  // third

#define set_car(x, v)  (Heap[x].car = (v))
#define set_cdr(x, v)  (Heap[x].cdr = (v))

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

// Convert symbol to string
#define symstr(n) (Symtab + car(n))

// Print node semi-recursively
void print_node(node n, int d) {
    if (Interrupted) return;
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

// Handle recoverable error
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
#define ERROR_IF(cc, mm, nn) do { if (cc) return error(mm, nn); } while(0)

// Breaking guard macro
#define BREAK_IF(cc, dd, mm, nn) if (cc) { dd = error(mm, nn); break; }

// Syntax error wrapper
node syntax(node n) {
    return error("syntax", n);
}

// Test if number of arguments is wrong
bool _fastcall badarity(node x, int k0, int kn) /*pure*/ {
    int i;
    if (kn == -1) kn = NNODES;
    for (i = 0; !isAtom(x); x = cdr(x), i++)
        if (i >= kn) return TRUE;
    return (x != NIL) || (i < k0);
}

// Syntax guard macro
#define SYNTAX_IF(xx, mm, nn) do { if (badarity(xx, mm, nn)) return syntax(xx); } while(0)

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

// Deutsch–Schorr–Waite algorithm
void mark(node n) {
    register node p = NIL, t;
    unsigned int w;
    for (;; n = t) {
        if ((n < NRESERVED) || (n >= SPCL) || ((w = Heap[n].cdr) & MARK_TAG)) {
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
    if (Mstack >= NMODES - 1) fatal("mstack full");
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
        if ((IOdepth == 0) && (Parens == 0) && (Rdlines == 0))
            sys_print(PROMPT);  // print prompt if at top-level
        IO.len = sys_read(IO.fd, BUFLEN, IO.buf);
        if (Interrupted) return EOT;
        if (IO.len <= 0) {
            if (IO.len < 0) { error("read", UNDEF); Interrupted = TRUE; }
            return EOT;
        }
        IO.pos = 0;
    }
    c = (unsigned char) IO.buf[IO.pos++];
    if (c == DOS_EOF) return EOT;
    if ((IOdepth == 0) && ((c == 0x00) || (c == 0xE0))) {  // extended key
        error("extended char", UNDEF);
        Interrupted = TRUE;
        return EOT;
    }
    return c;
}

// Read symbol
node read_sym(int c) {
    char s[SYMLEN+1];
    unsigned int i = 0;
    bool over = FALSE;
    while (isSymbolic(c, i)) {
        if ('\\' == c) {
            c = read_char();
            ERROR_IF((c == EOT) || (c == '$') || (c == '(') || (c == ')') ||
                     (c == '\''), "bad escape", UNDEF);
        } else if (c >= 'A' && c <= 'Z') c += 32;
        if (i < SYMLEN) s[i++] = c; else over = TRUE;
        c = read_char();
    }
    s[i] = 0;
    IO.next = c;
    if (Interrupted) return ERR;
    ERROR_IF(over, "overlong symbol", UNDEF);
    return (streq(s, "nil")) ? NIL : add_sym(s, UNDEF);  // add_sym checks if the symbol exists and if so returns its index
}

// Skip whitespace, comments, or block comments
int parse_skip(void) {
    int c;
    for (;;) {
        c = read_char();
        if (Skip) {
            if (c == EOT || c == '(' || c == ')') return c;
            continue;
        }
        if (c == ';')
            while ((c != '\n') && (c != EOT)) c = read_char();
        if ((Parens == 0) && (c == '\n') && (Rdlines > 0)) {
            Rdlines--;
            if (Rdlines == 0) return EOT;
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
            BREAK_IF(t == NIL, n, "bad pair", UNDEF);
            n = parse(); if (ERR == n) break;
            BREAK_IF((n == DOT) || (n == RPAREN) ||
                     (n == EOT) || (n == UNDEF), n, "bad pair", UNDEF);
            set_cdr(t, n);
            n = parse(); if (ERR == n) break;
            BREAK_IF(n != RPAREN, n, "bad pair", UNDEF);
            break;
        }
        c = cons(n, NIL);
        if (NIL == t) set_car(Vstack, c); else set_cdr(t, c);
        t = c;
    }
    if (entered) n = NIL;
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
// Environment layout: (symbol ...)
// Closure layout:     (CLOS_TAG | params-index . (environment . (body ...)))

// Set answer
#define set_ans(d) (set_cdr(S_ans, d))

node load(const char *s, bool ie);  // forward: load -> eval -> builtin -> load

// Resolve symbol in current scope
node _fastcall lookup(node n) /*pure*/ {
    register node e, a;
    if (n >= NRESERVED)
        for (e = Env; e != NIL; e = cdr(e))
            for (a = car(e); a != NIL; a = cdr(a))
                if (caar(a) == n) return cdar(a);
    return cdr(n);  // The symbol's value is its own cdr
}

// Built-in functions
node builtin(node x) {
    unsigned int u;
    node fn = car(x), ad = cdr(x), n;
    ad = isAtom(ad) ? NIL : car(ad);

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
        case F_VALUE:      // (value sym)
            SYNTAX_IF(x, 2, 2);
            ERROR_IF(!isSymbol(ad), "type", x);
            n = lookup(ad);
            ERROR_IF(UNDEF == n, "undefined", ad);
            return n;
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
            return (isClosure(ad) || ((ad >= NSPECIAL) && (ad < NBUILTIN))) ? K_TRUE : NIL;
        case F_DEFINED:    // (defined? sym)
            SYNTAX_IF(x, 2, 2);
            ERROR_IF(!isSymbol(ad), "type", x);
            return (UNDEF != cdr(ad)) ? K_TRUE : NIL;
        case F_EOF:        // (eof? any)
            SYNTAX_IF(x, 2, 2);
            return (EOT == ad) ? K_TRUE : NIL;
        case F_PRINT:      // (print . any)
            SYNTAX_IF(x, 1, -1);
            for (n = cdr(x); n != NIL; n = cdr(n)) print_node(car(n), 0);
            return UNDEF;
        case F_NEWLINE:    // (newline)
            SYNTAX_IF(x, 1, 1);
            sys_newline();
            return UNDEF;
        case F_LOAD:       // (load sym)
            SYNTAX_IF(x, 2, 3);
            ERROR_IF(!isSymbol(ad), "type", x);
            return load(symstr(ad), (cddr(x) != NIL) && (caddr(x) != NIL));
        case F_READ:       // (read)
            SYNTAX_IF(x, 1, 1);
            Rdlines = 2;
            do { x = parse(); } while (UNDEF == x);
            Rdlines = 0;
            return x;
        case F_ERROR:      // (error sym [any])
            SYNTAX_IF(x, 2, 3);
            ERROR_IF(!isSymbol(ad), "type", x);
            return error(symstr(ad), cddr(x) == NIL ? UNDEF : caddr(x));
        case F_ENV:        // (env)
            SYNTAX_IF(x, 1, 1);
            print_env();
            return UNDEF;
        case F_GC:         // (gc [any])
            SYNTAX_IF(x, 1, 2);
            u = gc();
            if (ad != NIL) print_usage(NNODES - u);
            return UNDEF;
        case F_RESTART:    // (restart [sym])
            SYNTAX_IF(x, 1, 2);
            ERROR_IF((ad != NIL) && !isSymbol(ad), "type", x);
            Restartsym = ad;
            Quit = TRUE;
            return UNDEF;
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
        ERROR_IF(isAtom(a), "missing args", (vpop(), Acc));
        n = cons(car(v), car(a));
        e = cons(n, e);
        set_car(Vstack, e);
    }
    if (isSymbol(v)) {
        n = cons(v, a);
        e = cons(n, e);
    } else ERROR_IF(NIL != a, "extra args", (vpop(), Acc));
    Env = cons(e, Env);
    vpop();
    return NIL;
}

// Apply function (with tail-call optimization)
node funapp(node x) {
    node c = car(x), b;
    bool tc;
    ERROR_IF(!isClosure(c), "not a function", x);

    tc = Modes[Mstack] == MRETN;

    if (!tc) { vpush(Env); mpush(MRETN); }
    Env = cadr(c);
    if (ERR == bindargs(car(c), cdr(x))) {
        if (!tc) { Env = vpop(); mpop(); }
        return ERR;
    }
    b = cddr(c);
    if (NIL == cdr(b)) { mpush(MEXPR); return car(b); }
    vpush(b);
    mpush(MBEGIN);
    return NIL;
}

// Special forms
node special(node x) {
    node n, fn = car(x), ad = cdr(x);
    ad = isAtom(ad) ? NIL : car(ad);

    switch(fn) {
        case S_QUOTE:   // (quote any)
            SYNTAX_IF(x, 2, 2);
            return ad;
        case S_IF:      // (if any any [any])
            SYNTAX_IF(x, 3, 4);
            mpush(MPRED);
            vpush(cddr(x));  // (then, else)
            mpush(MEXPR);
            return ad;       // predicate
        case S_BEGIN:   // (begin . any)
            SYNTAX_IF(x, 1, -1);
            if (NIL == cdr(x)) return NIL;
            if (NIL != cddr(x)) { mpush(MBEGIN); vpush(cddr(x)); }
            mpush(MEXPR);
            return ad;
        case S_IFNIL:   // (ifnil any any)
            SYNTAX_IF(x, 3, 3);
            mpush(MOR);
            vpush(caddr(x));
            mpush(MEXPR);
            return ad;
        case S_LAMBDA:  // (lambda lst any ...)
            SYNTAX_IF(x, 3, -1);
            ERROR_IF(malformed(x), "syntax", x);
            n = cons(Env, cddr(x));
            n = cons_tag(ad, n, CLOS_TAG);
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
            vpush(car(ad));
            vpush(cddr(x));
            mpush(MLET);
            mpush(MEXPR);
            return cadr(ad);
        case S_DEFINE:  // (define sym any)
            SYNTAX_IF(x, 3, 3);
            ERROR_IF(!isSymbol(ad), "type", x);
            mpush(MSET);
            vpush(ad);        // name
            mpush(MEXPR);
            return caddr(x);  // value
        default:
            return syntax(x);
    }
}

// Evaluate expression (caller must reset or restore Env)
node eval(node x) {
    node i, n;
    node vsave = Vstack;
    unsigned int msave = Mstack;
    register char m = MEXPR;
    Acc = x;
    mpush(MHALT);

    while (!Interrupted && !Quit) {
        switch (m) {
            case MEXPR:   // evaluate Acc as an expression
                if (isSymbol(Acc)) {
                    n = Acc;
                    Acc = lookup(Acc);
                    BREAK_IF(UNDEF == Acc, Acc, "undefined", n);
                    m = mpop();
                } else if (isAtom(Acc)) m = mpop();
                else if (car(Acc) < NSPECIAL) { Acc = special(Acc); m = mpop(); }
                else {
                    vpush(cdr(Acc));  // unevaluated argument list
                    vpush(NIL);       // result evaluated argument list
                    Acc = car(Acc);
                    if (Acc < NRESERVED) m = MLIST; else mpush(MLIST);  // reserved heads self-evaluate: skip the MEXPR trip
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
                    BREAK_IF(i != NIL, Acc, "improper call", i);
                    Acc = car(Vstack);
                    vpop();
                    vpop();
                    m = MCALL;
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
            case MPRED:   // resume after evaluating "if"
                n = vpop();
                m = MEXPR;
                if (NIL != Acc) Acc = car(n);
                else if (NIL == cdr(n)) { Acc = UNDEF; m = mpop(); }
                else Acc = cadr(n);
                break;
            case MOR:     // resume after evaluating "ifnil"
                n = vpop();
                if (NIL == Acc) { Acc = n; m = MEXPR; }
                else m = mpop();
                break;
            case MCALL:   // application: builtin or closure
                n = car(Acc);
                if (isSymbol(n)) {
                    if (F_APPLY == n) {   // (apply fn args) -> (fn . args), re-dispatch
                        BREAK_IF(badarity(Acc, 3, 3), Acc, "syntax", Acc);
                        Acc = cons(cadr(Acc), caddr(Acc));
                        break;
                    }
                    Acc = builtin(Acc);
                } else Acc = funapp(Acc);
                m = mpop();
                break;
            case MHALT:   // returns from eval
                Vstack = vsave;
                Mstack = msave;
                return Acc;
            case MSET:    // resume after evaluating "define" value
                n = vpop();
                BREAK_IF(UNDEF == Acc, Acc, "undefined", n);
                BREAK_IF((n == S_ans) || (n == S_ver) || (n == S_self) ||
                         (n < NRESERVED), Acc, "reserved", n);  // The second last test catches SPCL symbols, the last test prevents immutable symbols
                if(Fixed) {
                    if (UNDEF == cdr(n)) UserDef = cons(n, UserDef);
                    else {
                        for (i = UserDef; i != NIL; i = cdr(i))
                            if (car(i) == n) break;
                        BREAK_IF(i == NIL, Acc,"fixed", n);
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
        if (ERR == Acc) break;
    }
    Vstack = vsave;
    Mstack = msave;
    return (Interrupted || (ERR == Acc)) ? ERR : UNDEF;
}

// Load script file
node load(const char *s, bool ie) {
    int fd;
    ERROR_IF(IOdepth >= NLOAD, "nested load", UNDEF);
    fd = sys_open(s);
    ERROR_IF(fd < 0, s, UNDEF);

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
    return (ERR == Acc) ? ERR : cdr(S_ans);
}

// Register reserved symbols (needs to be same sequence as in enum)
void reserved(void) {
    static const char* names[NRESERVED];
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
        if (add_sym(names[i], SPCL) != i) error("init reserved", UNDEF);

    S_ans   = add_sym("ans", NIL);
    S_self  = add_sym("self", UNDEF);
    S_ver   = add_sym("ver", K_VERSTR);
}

// Entrypoint ##################################################################

// Break interrupt handler
void _interrupt _far _loadds on_break(void) { Interrupted = TRUE; }

// Reset state
void reset(void) {
    Interrupted = 0;
    Rdlines = Parens = Skip = Mstack = 0;
    Acc = Env = Vstack = Tmpcar = Tmpcdr = NIL;
}

// Display help
node help(void) {
    node u;
    sys_print("special forms:" DOS_EOL);
    for (u = 0; u < NSPECIAL; u++) { sys_print(" "); sys_print(symstr(u)); }
    sys_print(DOS_EOL "builtin functions:" DOS_EOL);
    for (u = NSPECIAL; u < K_COMMENT; u++) { sys_print(" "); sys_print(symstr(u)); }
    sys_print(DOS_EOL "standard library:" DOS_EOL);
    for (u = StdDef; u != NIL; u = cdr(u)) { sys_print(" "); sys_print(symstr(car(u))); }
    sys_newline();
    return UNDEF;
}

void stdpath(char *buf, const char *exe) {
    const char *p;
    char *slash = buf;
    for (p = exe; *p; p++) {
        *buf++ = *p;
        if (*p == '\\') slash = buf;
    }
    buf = slash;
    for (p = STDLIB; *p; p++) *buf++ = *p;
    *buf = 0;
}

// Entrypoint
int main(int argc, char **argv) {
    register unsigned int i;
    bool ignore, batch, status = 0;
    char stdlib[82];

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

    // Restart anchor
    restart:

    // Init reader
    IOdepth = 0; IO.fd = IO.pos = IO.len = 0; IO.next = EOT;

    // Init heap
    for (i = NRESERVED; i < NNODES-1; i++) Heap[i].cdr = i+1;
    Heap[NNODES-1].cdr = NIL;
    Freelist = NRESERVED;
    reserved();

    // Load standard library
    stdpath(stdlib, argv[0]);
    if (ERR == load(stdlib, FALSE)) error("stdlib", UNDEF);
    Fixed = TRUE;

    // Load script CLI arguments
    if (Restartbuf[0]) {
        reset();
        load(Restartbuf, FALSE);
        Restartbuf[0] = 0;
    } else if (argc > 1) {
        reset();
        ignore = streq(argv[1], "/i");
        batch = streq(argv[1], "/b") || ignore;
        if (batch && (argc < 3)) {
            error("missing filename", UNDEF);
            status = 1;
            Quit = TRUE;
        } else {
            status = load(batch ? argv[2] : argv[1], ignore) == ERR;
            if (batch) Quit = TRUE;
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

    // Restart
    if (Restartsym != UNDEF) {
        Restartbuf[0] = 0;
        if (Restartsym != NIL)
            for (i = 0; Restartbuf[i] = symstr(Restartsym)[i]; i++) ;
        Restartsym = UNDEF;
        Symtop = Pooltop = 0;
        Symbols = StdDef = UserDef = NIL;
        Fixed = Quit = FALSE;
        reset();
        argc = 1;
        goto restart;
    }

    // Exit banner
    sys_print(DOS_EOL "Bye" DOS_EOL DOS_EOL);

    sys_break(NONE);
    return status;
}
