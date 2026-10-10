;;;; fractal.scm

;; Pascal triangle for mod 2
(define pascal (lambda (left l)
  (if (empty? l) (cons left nil)
    (cons (if left (empty? (head l)) (head l))
          (self (head l) (tail l))))))

;; Draw one row
(define row (lambda (l)
  (if (empty? l) (newline)
    (begin (print (if (head l) '\* _)) (self (tail l))))))

;; Draw n rows of a right-angled Sierpinski triangle
(define fractal (lambda (n)
  ((lambda (r n)
     (if (empty? n) nil
         (begin (row r) (self (pascal nil r) (tail n)))))
   '(true) n)))

(fractal '(x x x x x x x x x x x x x x x x))
