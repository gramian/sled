;;;; level1.scm

(load 'engine.scm)

(define grid '(
  (w w w w w w w w w w w 3 w)
  (w s 4 _ _ _ _ _ _ _ _ _ w)
  (w _ w _ w w w w w 7 w _ w)
  (w _ w _ 8 _ _ _ _ _ w _ w)
  (w _ w _ w _ w w w _ w _ w)
  (w _ w _ w _ w x w _ w _ w)
  (w _ w _ w _ w _ w _ w _ w)
  (w _ w _ w _ _ _ w _ w _ w)
  (w _ w _ w 9 w w w _ w _ w)
  (w _ w _ _ _ _ _ _ _ 6 _ w)
  (w _ w 5 w w w w w w w _ w)
  (w _ _ _ _ _ _ _ _ _ _ _ 2)
  (w 1 w w w w w w w w w w w)
  ))

(define notes '(
  (1 n nine)
  (2 w eight)
  (3 s seven)
  (4 e six)
  (5 n five)
  (6 w four)
  (7 s three)
  (8 e two)
  (9 n one)))

(define next 'level2.scm)

(play 'hades)
