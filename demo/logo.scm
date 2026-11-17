;;;; logo.scm

(scm 'load 'logo 'hex 'demo)

((lambda ()
  (scm 'draw 'logo 'c)
  (scm 'show)
  (print 'press _ 'escape)
  (if (equiv? (scm 'key) 'es) ver (self))))
