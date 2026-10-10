// Super Character Mode extension for SLED - Schemy LISP en DOS
//   80x48@16 via text mode 3 using "▀" as two pixels; plus text status row

// Configuration ###############################################################

#define SCM_W       80      // Screen width (one pixel per text column)
#define SCM_H       48      // Screen height (two pixels per text row; row 24 is the status row)
#define SCM_PIXEL   0xDF    // Pixel pair character (extended ASCII character 223 "upper half block")
#define SCM_TRANS   0x10    // Non-color is transparent
#define SCM_BLANK   0x0720  // Space, light gray on black
#define SCM_HDR     4       // Image header: name (Symtab offset), width, height
#define SCM_WID     2       // Header index of width
#define SCM_HGT     3       // Header index of height

// Constants ###################################################################

#define SCM_SIZE    (0x7000u - SCM_W * SCM_H)             // Pool size (EGA/VGA)
#define SCM_SCREEN  ((volatile byte _far*) 0xB8000000L)   // Screen buffer (page 0)
#define SCM_DOUBLE  ((byte _far*) 0xB8001000L)            // Double buffer (page 1)
#define SCM_POOL    (SCM_DOUBLE + SCM_W * SCM_H)          // Offscreen Image pool

#define SCM_MODE    (*(volatile byte _far*) 0x00400049L)  // BIOS video mode
#define SCM_COLS    (*(volatile unsigned int _far*) 0x0040004AL)  // EGA/VGA cols
#define SCM_ROWS    (*(volatile byte _far*) 0x00400084L)  // EGA/VGA rows - 1

// State #######################################################################

struct {
    bool errored;        // error flag
    unsigned int top;    // image pool fill level
    int l, t, r, b;      // edge stacking depth
} Scm;

// Fail the current command with message (NONE when already reported)
#define scm_fail(m) (Scm.errored = TRUE, (const char*) (m))

// BIOS ########################################################################

// Scan code of the next key without waiting
int _cdecl scm_poll(void) {
    unsigned int k = 0xFFFF;  // no key (not a real value)
    _asm {
        mov ah, 1
        int 16h               // ZF set: no key waiting
        jz done
        xor ah, ah
        int 16h               // AH: scan code, AL: character
        mov k, ax
        done:
    }
    if (!k || ((k & 0xFF) == 0x03)) {  // Ctrl+Break (0000h) or Ctrl+C (2E03h)
        sys_poll();
        Interrupted = TRUE;
        return -1;
    }
    return k >> 8;                     // scan code
}

// Set page zero and move the cursor on page 0 to row 24
void _cdecl scm_cursor(void) {
    _asm {
        mov ax, 0x0500
        int 10h  // set active page
        mov ah, 2
        xor bh, bh
        mov dx, 0x1800
        int 10h  // set cursor
    }
}

// Attribute bit 7 selects bright backgrounds instead of blinking (EGA/VGA)
void _cdecl scm_bright(void) {
    _asm {
        mov ax, 0x1003
        xor bx, bx
        int 10h
    }
}

// Wait for next hardware interrupt
void _cdecl scm_wait(void) {
    _asm {
        sti
        hlt
    }
}

// Keys ########################################################################

// Wait for special key
const char* scm_key(void) {
    while (!Interrupted) {
        switch (scm_poll()) {
            case 0xFF: scm_wait(); break;  // avoid busy-wait
            case 0x48: return "up";        // up arrow key
            case 0x50: return "dn";        // down arrow key
            case 0x4B: return "lt";        // left arrow key
            case 0x4D: return "rt";        // right arrow key
            case 0x01: return "es";        // escape key
            case 0x1C: return "en";        // enter key
            case 0x2C: return "b1";        // z key
            case 0x2D: return "b2";        // x key
        }
    }
    return scm_fail(NONE);
}

// Images ######################################################################

// Find image by name in image pool
byte _far* scm_find(const char* s) {
    byte _far* p = SCM_POOL;
    byte _far* q = SCM_POOL + Scm.top;
    unsigned int n;
    if (!*s) return NONE;
    n = s - Symtab;
    for (; p < q; p += SCM_HDR + (p[SCM_WID] * p[SCM_HGT]))
        if (*(unsigned int _far*) p == n) return p;
    return NONE;
}

// Load hex file with name into image pool
const char* scm_load(const char* s, const char* f) {
    static const char Imagesize[] = "image size";
    byte _far* p = SCM_POOL + Scm.top;           // header of the new image
    unsigned int i = SCM_HDR;                    // start of pixels
    const char* e = NONE;
    int c, k, w = 0, h = 0, x = 0;

    if (!*s) return scm_fail("missing name");
    if (!*f) return scm_fail("missing file");
    if (scm_find(s)) return scm_fail("name taken");
    if (!iopush(f)) return scm_fail(NONE);       // iopush reported the error
    do {
        c = read_char();
        if ((c == '\r') || (c == '\n') || (c == EOT)) {  // end of row (CRLF gives a blank row, skipped)
            if (x) {
                if (!w) w = x;
                h++;
                if ((x != w) || (h > SCM_H)) e = Imagesize;
                x = 0;
            }
        } else {
            k = ((c >= '0') && (c <= '9')) ? c - '0' :
            ((c >= 'A') && (c <= 'F')) ? c - 'A' + 10 :
            (c == '.') ? SCM_TRANS : -1;
            if (k < 0) e = "image char";
            else if (++x > SCM_W) e = Imagesize;
            else if (Scm.top + i >= SCM_SIZE) e = "vram full";
            else p[i++] = (byte) k;
        }
    } while ((c != EOT) && !e);
    iopop();
    if (Interrupted) return scm_fail(NONE);
    if (!e && !h) e = Imagesize;
    if (e) return scm_fail(e);

    *(unsigned int _far*) p = s - Symtab;  // write header
    p[SCM_WID] = (byte) w;
    p[SCM_HGT] = (byte) h;
    Scm.top += i;
    return NONE;
}

// Screen ######################################################################

// Reset Edges
const char* scm_reset(void) {
    Scm.l = Scm.t = 0;
    Scm.r = SCM_W;
    Scm.b = SCM_H;
    return NONE;
}

// Clear double buffer
void scm_clear(void) {
    unsigned int _far* d = (unsigned int _far*) SCM_DOUBLE;
    unsigned int i;
    for (i = SCM_W * SCM_H / 2; i; i--) *d++ = 0;
    scm_reset();
}

// Stack image centered (on edge t, b, l, r, or centered c) in off-screen buffer
const char* scm_draw(const char* s, const char* e, const char* m) {
    byte _far* p = scm_find(s);
    int w, h, x, y, c0, r0, n, r, c, k, pskip, iskip, dx = 1;
    unsigned int i;

    if (!p) return scm_fail("not loaded");
    w = p[SCM_WID];
    h = p[SCM_HGT];
    p += SCM_HDR;
    x = Scm.l + ((Scm.r - Scm.l - w) >> 1);
    y = Scm.t + ((Scm.b - Scm.t - h) >> 1);
    switch (*e) {                                 // first letter
        case 'c': x = (SCM_W - w) >> 1; y = (SCM_H - h) >> 1; break;  // draw centered
        case 't': y = Scm.t; Scm.t = (y + h < Scm.b) ? y + h : Scm.b; break;
        case 'b': y = Scm.b - h; Scm.b = (y > Scm.t) ? y : Scm.t; break;
        case 'l': x = Scm.l; Scm.l = (x + w < Scm.r) ? x + w : Scm.r; break;
        case 'r': x = Scm.r - w; Scm.r = (x > Scm.l) ? x : Scm.l; break;
        default:  return scm_fail("bad edge");
    }
    c0 = (x < 0) ? -x : 0;                        // horizontal clipping
    n  = ((x + w > SCM_W) ? SCM_W - x : w) - c0;  // visible pixels per row
    r0 = (y < 0) ? -y : 0;                        // vertical clipping
    r  = ((y + h > SCM_H) ? SCM_H - y : h) - r0;  // visible rows
    if ((n > 0) && (r > 0)) {
        p += r0 * w + c0;                         // first visible source pixel
        pskip = w - n;                            // next row offset
        if (*m) {                                 // flipped?: read backwards
            p += w - 1 - 2 * c0;
            pskip = w + n;
            dx = -1;
        }
        i = (y + r0) * SCM_W + x + c0;
        iskip = SCM_W - n;
        for (; r; r--, p += pskip, i += iskip)
            for (c = n; c; c--, p += dx, i++)
                if ((k = *p) != SCM_TRANS) SCM_DOUBLE[i] = (byte) k;
    }
    return NONE;
}

// Flush off-screen buffer to screen: two pixel rows per text row
const char* scm_show(void) {
    volatile unsigned int _far* v = (volatile unsigned int _far*) SCM_SCREEN;
    byte _far* p = SCM_DOUBLE;                 // top pixel of the current cell
    unsigned int r, c;
    for (r = SCM_H / 2; r; r--, p += SCM_W)    // skip the bottom pixel row
        for (c = SCM_W; c; c--, p++)           // foreground: top, background: bottom
            *v++ = ((unsigned int) p[SCM_W] << 12) | ((unsigned int) *p << 8) | SCM_PIXEL;
    for (c = SCM_W; c; c--) *v++ = SCM_BLANK;  // status row
    scm_cursor();
    Fresh = TRUE;
    scm_clear();
    return NONE;
}

// Dispatch ####################################################################

// Run command a[0] with arguments a[1], a[2], a[3]: error message or NONE
const char* scm_run(const char** a) {
    if (streq(a[0], "key"))   return scm_key();
    if (streq(a[0], "reset")) return scm_reset();
    if (Restarted) {
        if ((SCM_MODE != 3) || (SCM_COLS != 80) || (SCM_ROWS != 24))
            return scm_fail("no 80x25 color text mode");
        scm_bright();
        Restarted = FALSE;
        Scm.top = 0;
        scm_clear();
    }
    if (streq(a[0], "set?")) return "true";
    if (streq(a[0], "load")) return scm_load(a[1], a[2]);
    if (streq(a[0], "draw")) return scm_draw(a[1], a[2], a[3]);
    if (streq(a[0], "show")) return scm_show();
    return scm_fail("unknown");
}

// (scm sym [sym [sym [sym]]]): symbol, nil or ERR
node scm(node x) {
    const char* a[4];
    const char* r;
    node p, s;
    int i = 0;
    SYNTAX_IF(x, 1, 4);
    a[1] = a[2] = a[3] = "";
    for (p = cdr(x); p != NIL; p = cdr(p)) {
        s = car(p);
        ERROR_IF((s != NIL) && !isSymbol(s), Type, x);
        a[i++] = (s == NIL) ? "" : symstr(s);
    }
    Scm.errored = FALSE;
    r = scm_run(a);
    if (Scm.errored) return r ? error(r, x) : ERR;
    return r ? add_sym(r, UNDEF) : NIL;
}
