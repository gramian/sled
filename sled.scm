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

;; Exit the interpreter
(define quit exit)

;; Answers if the von Neumann ordinal argument is zero
(define zero? empty?)

;;; Functions

;; Answers if both arguments are not NIL
(define and? (lambda (x y)
  (if x (if y true))))

;; Concatenate lists
(define append (lambda (l x)
  (if (empty? l) x
    (cons (head l) (self (tail l) x)))))

;; Raises error of second argument symbol if first argument is NIL
(define assert (lambda (x y)
  (ifnil x (error y))))

;; Returns first argument piped left-to-right through list of unary functions
(define compose (lambda (x . y)
  (if y (apply self (cons ((head y) x) (tail y)))
        x)))

;; Returns decremented unary ordinal argument list
(define dec (lambda (x)
  (if x (tail x))))

;; Answers if arguments are recursively equal
(define equal? (lambda (x y)
  (ifnil (equiv? x y)
    (if (atom? x) nil
      (if (atom? y) nil
        (if (self (head x) (head y))
            (self (tail x) (tail y))))))))

;; Answers if previous top-level evaluation raised an error
(define error? (lambda ()
  (equiv? ans err)))

;; Returns value to first argument key symbol in second argument association list
(define get (lambda (x l)
  (if (empty? l) nil
    (if (equiv? x (head (head l)))
      (tail (head l))
      (self x (tail l))))))

;; Returns argument
(define id (lambda (x)
  x))

;; Returns incremented unary ordinal argument list
(define inc (lambda (x)
  (cons nil x)))

;; Returns list of arguments
(define list (lambda x x))

;; Answers if argument is a proper list
(define list? (lambda (x)
  (if (atom? x) (empty? x)
    (self (tail x)))))

;; Returns list of first argument unary function applied to each second argument list element
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
  (if x true (if y true))))

;; Answers if argument is a pair
(define pair? (lambda (x)
  (if (atom? x) nil true)))

;; Returns first argument after printing (optional and) first argument
(define printid (lambda (x . y)
  (if y (print (head y) '\:\ ))
  (print x)
  (newline)
  x))

;; Print argument and break line
(define println (lambda x
  (apply print x)
  (newline)))

;; Returns third argument association list with pair of first and second argument upserted
(define put (lambda (x y l)
  (if l
    (if (equiv? (head (head l)) x)
        (cons (cons x y) (tail l))
        (cons (head l) (self x y (tail l))))
    (cons (cons x y) nil))))

;; Returns reverse of argument list
(define reverse (lambda (l)
  ((lambda (l a)
    (if (empty? l) a
      (self (tail l) (cons (head l) a))))
   l nil)))

;; Answers if first argument list is shorter than second argument list
(define shorter? (lambda (x y)
  (if y (if x (self (tail x) (tail y)) true))))

;;; Your start-up code below (all definitions in this file are not redefinable):

;; Print beep character
(define beep (lambda () (println '\)))
