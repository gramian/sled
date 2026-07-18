;;;; bench.scm

;; Returns last argument via Takeuchi function
(define tak (lambda (x y z)
  (if (shorter? y x)
      (self (self (tail x) y z)
            (self (tail y) z x)
            (self (tail z) x y))
      z)))

(println 'Running\ Tak)

;; Tak Benchmark (18,12,6)
(tak (list nil nil nil nil nil nil nil nil nil nil nil nil nil nil nil nil nil nil)
     (list nil nil nil nil nil nil nil nil nil nil nil nil)
     (list nil nil nil nil nil nil))
