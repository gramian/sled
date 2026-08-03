;;;; sled.scm (Standard Library)

;;; Aliases

;; Space character
(define _ '\ )

;; Break line
(define br newline)

;; Answers if argument is NIL
(define nil? empty?)

;; Returns logical Not of argument
(define not empty?)

;; Exit the parser
(define quit exit)

;; Answers if the von Neumann ordinal argument is zero
(define zero? empty?)

;;; Functions

;; Answers if both arguments are not NIL
(define and? (lambda (x y)
  (if x (if y true nil) nil)))

;; Concatenate lists (non TC)
(define append (lambda (l x)
  (if (empty? l) x
    (cons (head l) (self (tail l) x)))))

;; Raises error of second argument symbol if first argument is NIL
(define assert (lambda (x y)
  (ifnil x (error y))))

;; Returns composition of innermost argument and list of unary functions
(define compose (lambda x
  (if (empty? (tail x)) (head x)
    (apply self
      (cons ((head (tail x)) (head x))
            (tail (tail x)))))))

;; Returns decremented von Neumann ordinal argument list
(define dec (lambda (x)
  (if (empty? x) nil (tail x))))

;; Answers if arguments are recursively equal (non TC)
(define equal? (lambda (x y)
  (ifnil (equiv? x y)
    (if (atom? x) nil
      (if (atom? y) nil
        (if (self (head x) (head y))
            (self (tail x) (tail y))
            nil))))))

;; Answers if previous top-level evaluation raised an error
(define error? (lambda ()
  (equiv? ans err)))

;; Returns value to second argument key symbol in first argument association list
(define get (lambda (l x)
  (if (empty? l) nil
    (if (equiv? x (head (head l)))
      (tail (head l))
      (self (tail l) x)))))

;; Returns argument
(define id (lambda (x)
  x))

;; Returns incremented von Neumann ordinal argument list
(define inc (lambda (x)
  (cons nil x)))

;; Returns list of arguments
(define list (lambda x x))

;; Answers if argument is a proper list
(define list? (lambda (x)
  (ifnil (empty? x)
    (if (atom? x) nil
      (self (tail x))))))

;; Returns list of first argument unary function applied to each second argument list element (non TC)
(define map (lambda (f l)
  (if (empty? l) nil
    (cons (f (head l)) (self f (tail l))))))

;; Returns pair of second argument list whose head is the first argument
(define member (lambda (x l)
  (if (empty? l) nil
    (if (equiv? x (head l)) l
      (self x (tail l))))))

;; Answers if either arguments are not NIL
(define or? (lambda (x y)
  (if x true (if y true nil))))

;; Answers if argument is a pair
(define pair? (lambda (x)
  (if (atom? x) nil true)))

;; Returns first argument after printing last and first argument
(define printid (lambda (x . y)
  (if y (print (head y) '\:\ ))
  (print x)
  (newline)
  x))

;; Print argument and break line
(define println (lambda x
  (apply print x)
  (newline)))

;; Returns first argument association list with pair of second and third argument upserted
(define put (lambda (l x y)
  ((lambda (m n)
    (if (empty? n) (cons (cons x y) m)
    (if (equiv? (head (head n)) x) (self m (tail n))
                                   (self (cons (head n) m) (tail n)))))
    nil l)))

;; Returns reverse of argument list
(define reverse (lambda (l)
  ((lambda (l a)
    (if (empty? l) a
      (self (tail l) (cons (head l) a))))
   l nil)))

;; Answers if first argument list is shorter than second argument list
(define shorter? (lambda (x y)
  (if y (if x (self (tail x) (tail y)) true) nil)))

;;; Your start-up code below:

;; Print beep character
(define beep (lambda () (print '\)))
