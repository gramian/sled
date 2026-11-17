;;;; level0.scm

(load 'engine.scm)

(define grid '(
  (w w w w w w w w)
  (x s _ _ _ _ _ 1)
  (w w w w w w w w)))

(define notes '(
  (1 w This is a sign)))

(define next 'level1.scm)

(play 'hades)
