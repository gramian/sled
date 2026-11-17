;;;; level2.scm

(load 'engine.scm)

(define grid '(
  (w w w w w w w w w w w w w w w w w w w w w)
  (w _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ w)
  (w _ w w w w w w w w _ w w w w w w w w _ w)
  (w _ w _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ w _ w)
  (w _ w _ w w w w w w _ w w w w w w _ w _ w)
  (w _ w _ w _ _ _ _ w _ w _ _ _ _ w _ w _ w)
  (w _ w _ w _ w w _ w 1 w _ w w _ w _ w _ w)
  (w _ w _ w _ w _ _ _ _ _ _ _ w _ w _ w _ w)
  (w _ w _ _ _ w _ w w _ w w _ w _ _ _ w _ w)
  (w _ w w w w w _ w _ _ _ w _ w w w w w _ w)
  (w _ _ _ _ _ 4 _ _ _ x _ _ _ 2 _ _ _ _ _ w)
  (w _ w w w w w _ w _ s _ w _ w w w w w _ w)
  (w _ w _ _ _ w _ w w _ w w _ w _ _ _ w _ w)
  (w _ w _ w _ w _ _ _ _ _ _ _ w _ w _ w _ w)
  (w _ w _ w _ w w _ w 3 w _ w w _ w _ w _ w)
  (w _ w _ w _ _ _ _ w _ w _ _ _ _ w _ w _ w)
  (w _ w _ w w w w w w _ w w w w w w _ w _ w)
  (w _ w _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ w _ w)
  (w _ w w w w w w w w _ w w w w w w w w _ w)
  (w _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ w)
  (w w w w w w w w w w w w w w w w w w w w w)
  ))

(define notes '(
  (1 n the four cardinal directions)
  (2 e so close but still far away)
  (3 s as if reflected by a mirror)
  (4 w thanks for playing doswayout)))

(define next nil)

(play 'hades)
