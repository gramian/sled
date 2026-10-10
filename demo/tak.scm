;;;; bench.scm

;; Returns last argument via Takeuchi function
(define tak (lambda (x y z)
  (if (shorter? y x)
      (self (self (tail x) y z)
            (self (tail y) z x)
            (self (tail z) x y))
      z)))

(println 'running _ 'tak)

;; Tak Benchmark (18,12,6) = 7
(assert (equal? '(nil nil nil nil nil nil nil)
                (tak '(nil nil nil nil nil nil nil nil nil nil nil nil nil nil nil nil nil nil)
                     '(nil nil nil nil nil nil nil nil nil nil nil nil)
                     '(nil nil nil nil nil nil)))
        '(wrong result))
