;;;; engine.scm

;;; Globals

(define grid nil)   ; level map (set by level file), need to be framed by walls
(define notes nil)  ; sign texts ((label word ...) ...), set by level file
(define next nil)   ; next level script (set by level file)

(define pos nil)   ; player position (row . col)
(define dir 'n)    ; player direction symbol
(define msg nil)   ; upcoming status message
(define seen nil)  ; labels of signs read so far

(define cw  '((n . e) (e . s) (s . w) (w . n)))  ; clock-wise turn
(define ccw '((n . w) (w . s) (s . e) (e . n)))  ; counter clock-wise turn
(define rev '((n . s) (s . n) (e . w) (w . e)))  ; backwards

(define skies '((n . sky_n) (e . sky_e) (s . sky_s) (w . sky_w)))

(define views '(  ; view-depth-based texture groups (lw - left wall, fw - facing wall, lx - left exit, fx - facing exit, lu- left open exit, fu - facing open exit, lo - left open)
  ((w lw0 fw1) (q lq0 fq1) (x lx0 fx1) (u lu0 fu1) (_ lo0))    ; nearest
  ((w lw1 fw2) (q lq1 fq2) (x lx1 fx2) (u lu1 fu2) (_ lo1))
  ((w lw2 fw3) (q lq2 fq3) (x lx2 fx3) (u lu2 fu3) (_ lo2))
  ((w lw3 fw4) (q lq3 fq4) (x lx3 fx4) (u lu3 fu4) (_ lo3))))  ; farthest

;;; Helpers

;; print words separated by spaces
(define say (lambda (w)
  (if w (begin (print (head w))
               (map (lambda (x) (print _ x)) (tail w))))))

;; texture key of tile t: all signs look alike, the exit depends on the quest
(define kind (lambda (t f)
  (let (n (get t notes))
    (if n
      (if (equiv? (get f rev) (head n)) 'q 'w)
      (case t
        (x (if (drop seen notes) 'x 'u))
        (w 'w)
           '_)))))

;; read tile from map at position p
(define tile (lambda (p)
  (head (drop (tail p) (head (drop (head p) grid))))))

;; look one step from position p into direction d
(define step (lambda (p d)
  (case d
    (n (cons (dec (head p)) (tail p)))
    (s (cons (inc (head p)) (tail p)))
    (e (cons (head p) (inc (tail p))))
       (cons (head p) (dec (tail p))))))

;;; Renderer

;; Textures (left front) of tile t in depth table e
(define tex (lambda (p d e)
  (get (kind (tile (step p d)) d) e)))

;; draws textures based on position direction
(define ray (lambda (p d v)
  (if v
    (let (e (head v))                                ; get current view depth
      (scm 'draw (head (tex p (get d ccw) e)) 'l)
      (scm 'draw (head (tex p (get d cw) e)) 'r 'f)
      (let (f (tail (tex p d e)))
        (if f (scm 'draw (head f))                   ; draw facing wall
              (self (step p d) d (tail v))))))))

;; draw scene
(define draw (lambda ()
  (scm 'draw (get dir skies) 't)  ; sky depends on direction
  (scm 'draw 'floor 'b)           ; floor
  (scm 'reset)                    ; both edges are used up: restore them
  (ray pos dir views)
  (scm 'show)
  (print dir _ _)
  (say msg)
  (define msg nil)))

;;; Game Logic

;; quit game
(define bye (lambda ()
  (scm 'show)  ; the buffer is empty: blank screen
  (exit)))

;; next level
(define won (lambda ()
  (define msg '(level cleared))
  (draw)
  (scm 'key)
  (if next (restart next) (bye))))

;; try to enter new position
(define enter (lambda ()
  (let (q (step pos dir))
    (let (t (tile q))
      (case (kind t dir)
        (q (ifnil (member t seen) (define seen (cons t seen)))
           (define msg (tail (get t notes))))
        (w (define msg '(ouch)))
        (x (define msg '(locked - read all signs)))
        (u (won))
           (define pos q))))))

;; handle movement
(define move (lambda (k)
  (case k
    (es (bye))                       ; quit
    (up (enter))                     ; step forward
    (dn (define dir (get dir rev)))  ; turn around
    (lt (define dir (get dir ccw)))  ; turn left
    (rt (define dir (get dir cw)))   ; turn right
        nil)))

;; game loop
(define loop (lambda ()
  (draw)             ; draw scene
  (move (scm 'key))  ; wait for key and move
  (self)))

;;; Init Level

;; load floor, skies and all textures from table t
(define setup (lambda (d)
  (scm 'load 'floor 'hex d)                             ; load floor
  (map (lambda (s) (scm 'load (tail s) 'hex d)) skies)  ; load skies
  (map (lambda (dep)                                    ; for each depth level
    (map (lambda (grp)                                  ; for each texture group
      (map (lambda (img)                                ; for each texture
             (scm 'load img 'hex d))                    ; load texture
        (tail grp)))
    dep))
  views)))

;; find starting location in map g and return location as row-column pair
(define start (lambda (g r)
  (if g (let (m (member 's (head g)))         ; check if current row has start
          (if m (cons r (drop m (head g)))    ; determine current column tally
                (self (tail g) (inc r)))))))

;; start level
(define play (lambda (d)
  (setup d)                            ; load graphics
  (define pos (start grid))            ; locate starting position
  (if pos (loop) (error 'no-start))))  ; enter game loop
