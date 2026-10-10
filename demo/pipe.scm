;;;; pipe.scm

;;; Pipe - zero-dimensional Brainfuck with two commands (+, f)

;; Return list of tally counts for each pipe program
(define pipe (lambda (p)
  ((lambda (p q r)
     (if (empty? p) (revpend r)
     (if (equiv? (head p) '\+) (self (tail p) (inc q) r)
     (if (equiv? (head p) 'f)  (self (tail p) nil (cons q r))
         (error '(bad _ command) (head p))))))
   p nil nil)))
