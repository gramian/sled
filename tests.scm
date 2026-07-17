;;;; tests.scm

;;; Special Symbols ############################################################

(println 'special _ 'symbols)

(newline)

;; ans

'a

(check 'ans 'a (equiv? ans 'a))

(newline)

;; err

;; nil

(check 'nil 'a (not nil))

(check 'nil 'b (equiv? nil '()))

(check 'nil 'c (list? (cons 'a nil)))

(newline)

;; self

;; space

(check 'space 'a (equiv? _ '\ ))

(newline)

;; true

(check 'true 'a true)

(check 'true 'b (not (not true)))

(newline)

;; ver

(check 'ver 'a (equiv? ver 'sled-0.1))

(newline)

;;; Special Forms ##############################################################

(println 'special _ 'forms)

(newline)

;; apply

(check 'apply 'a (apply id '(true)))

(newline)

;; begin

(check 'begin 'a (equiv? (begin 'x 'y) 'y))

(newline)

;; comment

(check 'comment 'a (comment nil) true)

(newline)

;; define

(check 'define 'a (equiv? (define x 'y) 'y))

(newline)

;; if

(check 'if 'a (equiv? (if true 'x 'y) 'x))

(check 'if 'b (equiv? (if 'z 'x 'y) 'x))

(check 'if 'c (equiv? (if nil 'x 'y) 'y))

(check 'if 'd (equiv? (if '() 'x 'y) 'y))

(check 'if 'e (equiv? (if nil 'x (if nil 'x 'y)) 'y))

(newline)

;; ifnil

(check 'ifnil 'a (equiv? (ifnil true 'y) true))

(check 'ifnil 'b (equiv? (ifnil 'x 'y) 'x))

(check 'ifnil 'c (equiv? (ifnil nil 'y) 'y))

(check 'ifnil 'd (equiv? (ifnil '() 'y) 'y))

(newline)

;; lambda

(check 'lambda 'a (equiv? ((lambda (x) x) 'hi) 'hi))

(check 'lambda 'b (equiv? ((lambda () 'hi)) 'hi))

(check 'lambda 'c (equiv? ((lambda (x y) x) 'a 'b) 'a))

(check 'lambda 'd (equiv? ((lambda (x y) y) 'a 'b) 'b))

(check 'lambda 'e (equiv? ((lambda (x) 'ignored x) 'a) 'a))

(check 'lambda 'f (equiv? ((lambda (x) ((lambda (y) y) x)) 'hi) 'hi))

(check 'lambda 'g (equal? ((lambda (x . r) r) 'a 'b 'c) '(b c)))

(check 'lambda 'h (equal? ((lambda x x) 'a 'b 'c) '(a b c)))

(newline)

;; let

(check 'let 'a (equiv? (let (x 'y) x) 'y))

(newline)

;; quote

(check 'quote 'a (equiv? (quote x) 'x))

(newline)

;;; Builtin Functions ##########################################################

(println 'builtin _ 'functions)

(newline)

;; atom?

(check 'atom? 'a (atom? nil))

(check 'atom? 'b (atom? '()))

(check 'atom? 'c (atom? true))

(check 'atom? 'd (atom? 'x))

(check 'atom? 'e (not (atom? '(x . y))))

(check 'atom? 'f (not (atom? '(x))))

(check 'atom? 'g (atom? (lambda (x) x)))

(newline)

;; cons

(check 'cons 'a  (equal? (cons 'a 'b) '(a . b)))

(check 'cons 'b  (equal? (cons 'a nil) '(a)))

(check 'cons 'c  (equal? (cons 'a (cons 'b nil)) '(a b)))

(newline)

;; defined?

(check 'defined? 'a (defined? 'defined?))

(check 'defined? 'b (defined? 'define))

(check 'defined? 'c (defined? 'ver))

(check 'defined? 'd (not (defined? 'this-does-exist)))

(newline)

;; empty?

(check 'empty? 'a (empty? nil))

(check 'empty? 'b (empty? '()))

(check 'empty? 'c (not (empty? true)))

(check 'empty? 'd (not (empty? 'x)))

(check 'empty? 'e (not (empty? '(x . y))))

(check 'empty? 'f (not (empty? '(x))))

(check 'empty? 'g (not (empty? (lambda (x) x))))

(newline)

;; env

; test manually

;; eof?

(check 'eof? 'a (not (eof? nil)))

(check 'eof? 'b (not (eof? '())))

(check 'eof? 'c (not (eof? true)))

(check 'eof? 'd (not (eof? 'x)))

(check 'eof? 'e (not (eof? '(x . y))))

(check 'eof? 'f (not (eof? '(x))))

(check 'eof? 'g (not (eof? (lambda (x) x))))

(newline)

;; equiv?

(check 'equiv? 'a (equiv? nil nil))

(check 'equiv? 'b (equiv? '() '()))

(check 'equiv? 'c (equiv? nil '()))

(check 'equiv? 'd (equiv? '() nil))

(check 'equiv? 'e (equiv? true true))

(check 'equiv? 'f (not (equiv? nil true)))

(check 'equiv? 'g (equiv? 'x 'x))

(check 'equiv? 'h (not (equiv? 'x 'y)))

(check 'equiv? 'i (equiv? 'X 'x))

(check 'equiv? 'j (not (equiv? 'x '(x))))

(check 'equiv? 'k (not (equiv? '(x . y) '(x . y))))

(check 'equiv? 'l (not (equiv? '(x) '(x))))

(newline)

;; error

; test manually

;; exit

; test manually

;; gc

; test manually

;; head

(check 'head 'a (equiv? (head '(x . y)) 'x))

(check 'head 'b (equiv? (head '(x)) 'x))

(check 'head 'c (equiv? (head '(x y)) 'x))

(newline)

;; load

;; newline

; test manually

;; print

; test manually

;; proc?

(check 'proc? 'a (not (proc? nil)))

(check 'proc? 'b (not (proc? '())))

(check 'proc? 'c (not (proc? true)))

(check 'proc? 'd (not (proc? 'x)))

(check 'proc? 'e (not (proc? '(x . y))))

(check 'proc? 'f (not (proc? '(x))))

(check 'proc? 'g (proc? (lambda (x) x)))

(newline)

;; read

; test manually

;; symbol?

(check 'symbol? 'a (not (symbol? nil)))

(check 'symbol? 'b (not (symbol? '())))

(check 'symbol? 'c (symbol? true))

(check 'symbol? 'd (symbol? 'x))

(check 'symbol? 'e (not (symbol? '(x . y))))

(check 'symbol? 'f (not (symbol? '(x))))

(check 'symbol? 'g (not (symbol? (lambda (x) x))))

(newline)

;; tail

(check 'tail 'a (equiv? (tail '(x . y)) 'y))

(check 'tail 'b (equiv? (tail '(x)) nil))

(check 'tail 'c (equal? (tail '(x y)) '(y)))

(newline)

;;; Standard aliases ###########################################################

;; nil?

(check 'nil? 'a (equiv? nil? 'empty?))

(newline)

;; not

(check 'not 'a (equiv? not 'empty?))

(newline)

;; zero?

(check 'not 'a (equiv? zero? 'empty?))

(newline)

;;; Standard library ###########################################################

(println 'standard _ 'library)

(newline)

;; all?

(check 'all? 'a (all? id '(true true true)))

(check 'all? 'b (not (all? id '(nil true true))))

(check 'all? 'c (not (all? id '(true nil true))))

(check 'all? 'd (not (all? id '(true true nil))))

(check 'all? 'e (all? id nil))

(newline)

;; and?

(check 'and? 'a (not (and? nil nil)))

(check 'and? 'b (not (and? nil true)))

(check 'and? 'c (not (and? true nil)))

(check 'and? 'd (and? true true))

(newline)

;; any?

(check 'any? 'a (any? id '(true true true)))

(check 'any? 'b (any? id '(true nil nil)))

(check 'any? 'c (any? id '(nil true nil)))

(check 'any? 'd (any? id '(nil nil true)))

(check 'any? 'e (not (any? id '(nil nil nil))))

(check 'any? 'f (not (any? id nil)))

(newline)

;; append

(check 'append 'a (equal? (append nil nil) nil))

(check 'append 'b (equal? (append '(a) nil) '(a)))

(check 'append 'c (equal? (append nil '(a)) '(a)))

(check 'append 'd (equal? (append '(a) '(b)) '(a b)))

(check 'append 'e (equal? (append '(a b) '(c)) '(a b c)))

(check 'append 'f (equal? (append '(a) '(b c)) '(a b c)))

(newline)

;; assert

(assert nil 'test)

(check 'assert 'a (error?))

(assert true 'test)

(check 'assert 'b (not (error?)))

(newline)

;; compose

(check 'compose 'a (equal? (compose 'a) 'a))

(check 'compose 'b (equal? (compose 'a id) 'a))

(check 'compose 'c (equal? (compose nil inc) '(nil)))

(check 'compose 'd (equal? (compose nil inc inc) '(nil nil)))

(check 'compose 'e (equal? (compose nil inc dec) nil))

(newline)

;; dec

(check 'dec 'a (equal? (dec nil) nil))

(check 'dec 'b (equal? (dec '(nil)) nil))

(newline)

;; equal?

(check 'equal? 'a (equal? nil nil))

(check 'equal? 'b (not (equal? nil true)))

(check 'equal? 'c (not (equal? true nil)))

(check 'equal? 'd (equal? true true))

(check 'equal? 'e (equal? '(a) '(a)))

(check 'equal? 'f (not (equal? '(a) '(b))))

(check 'equal? 'g (equal? '(a (b (c))) '(a (b (c)))))

(check 'equal? 'h (not (equal? '(a (b (c))) '(a (b c)))))

(check 'equal? 'i (not (equal? '(a (b c)) '(a (b (c))))))

(newline)

;; error?

(error 'test)

(check 'error? 'a (error?))

(check 'error? 'b (not (error?)))

(newline)

;; get

(check 'get 'a (equiv? (get 'a '((a . x) (b . y) (c . z))) 'x))

(check 'get 'b (equiv? (get 'b '((a . x) (b . y) (c . z))) 'y))

(check 'get 'c (equiv? (get 'c '((a . x) (b . y) (c . z))) 'z))

(check 'get 'd (equiv? (get 'd '((a . x) (b . y) (c . z))) nil))

(check 'get 'e (equiv? (get nil '((a . x) (nil . y))) 'y))

(check 'get 'f (equiv? (get 'a nil) nil))

(check 'get 'g (equiv? (get nil nil) nil))

(newline)

;; id

(check 'id 'a (equiv? (id 'a) 'a))

(check 'id 'b (equiv? (id nil) nil))

(newline)

;; inc

(check 'inc 'a (equal? (inc nil) '(nil)))

(check 'inc 'b (equal? (inc '(nil)) '(nil nil)))

(newline)

;; list

(check 'list 'a (equal? (list) nil))

(check 'list 'b (equal? (list 'a) '(a)))

(check 'list 'c (equal? (list 'a 'b) '(a b)))

(newline)

;; list?

(check 'list? 'a (list? nil))

(check 'list? 'b (list? '()))

(check 'list? 'c (not (list? true)))

(check 'list? 'd (not (list? 'x)))

(check 'list? 'e (not (list? '(x . y))))

(check 'list? 'f (list? '(x)))

(check 'list? 'g (not (list? (lambda (x) x))))

(newline)

;; map

(check 'map 'a (equal? (map (lambda (x) 'b) nil) nil))

(check 'map 'b (equal? (map (lambda (x) 'b) '(a)) '(b)))

(check 'map 'c (equal? (map (lambda (x) 'b) '(a a)) '(b b)))

(newline)

;; member

(check 'member 'a (equal? (member 'b nil) nil))

(check 'member 'b (equal? (member 'b '(b)) '(b)))

(check 'member 'c (equal? (member 'b '(b c)) '(b c)))

(check 'member 'd (equal? (member 'b '(c)) nil))

(newline)

;; or?

(check 'or? 'a (not (or? nil nil)))

(check 'or? 'b (or? nil true))

(check 'or? 'c (or? true nil))

(check 'or? 'd (or? true true))

(newline)

;; pair?

(check 'pair? 'a (not (pair? nil)))

(check 'pair? 'b (not (pair? '())))

(check 'pair? 'c (not (pair? true)))

(check 'pair? 'd (not (pair? 'x)))

(check 'pair? 'e (pair? '(x . y)))

(check 'pair? 'f (pair? '(x)))

(check 'pair? 'g (not (pair? (lambda (x) x))))

(newline)

;; printid

(check 'printid 'a (equiv? (printid 'a) 'a))

(check 'printid 'b (equiv? (printid 'a 'val) 'a))

(newline)

;; println

; test manually

;; put

(check 'put 'a (equal? (put nil 'a 'b) '((a . b))))

(check 'put 'b (equal? (put '((a . b)) 'c 'd) '((c . d) (a . b))))

(check 'put 'c (equal? (put '((a . b)) 'a 'z) '((a . z))))

(check 'put 'd (equal? (put '((c . d) (a . b)) 'a 'z) '((a . z) (c . d))))

(newline)

;; reverse

(check 'reverse 'a (equal? (reverse nil) nil))

(check 'reverse 'b (equal? (reverse '(x)) '(x)))

(check 'reverse 'c (equal? (reverse '(x y)) '(y x)))

(check 'reverse 'd (equal? (reverse '(x y z)) '(z y x)))

(newline)

;; shorter?

(check 'shorter? 'a (not (shorter? nil nil)))

(check 'shorter? 'b (shorter? nil '(nil)))

(check 'shorter? 'c (not (shorter? '(nil) nil)))

(check 'shorter? 'd (not (shorter? '(nil) '(nil))))

(newline)

;; tak

; test manually

