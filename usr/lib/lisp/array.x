(File array.l)
(listarray lexpr return cpy1 eval cons arrayref lessp |1-| do memq getaux car arraydims cdr quote apply eq error cxr getd symbolp and arrayp cond arg setq prog)
(fillarrayarray lambda |1+| arrayref replace >& do getlength min |1-| setq prog)
(fillarray lambda |1+| cdr replace arrayref set >& do getaux car memq getlength |1-| arrayp fillarrayarray return cxr getd setq symbolp cond prog)
(arraydims lambda quote *break break arrayp cxr getd car getaux symbolp cond)
(small-segment lambda return remprop putdelta putlength fake putdata marray putprop greaterp + rplacd - rplaca segment maknum cons fixnum-BitAndNot boole lessp not and error null cond cdr car quote get desetq setq prog)
(storeintern lambda cons apply)
(store macro apply getdisc bcdp dtpr and or cxr getd setq arrayp cddr cadr append list cdr car quote cons return eq cond do caddr cdadr caadr)
(arrac-nD lexpr value-eval set cpy1 replace memq arrayref null cddr do error + |1+| eq cond length setq cdr arg car getaux let)
(arrac-twoD lexpr error set cxr getdata caddr offset-cxr simple-arrayref value-eval eq cond cdr arg car getaux let)
(arrac-oneD lexpr error set cxr getdata arg offset-cxr simple-arrayref cdr value-eval eq cond)
(ev-arraycall lambda apply)
(arraycall macro append quote cons cdddr caddr cadr)
(*array lexpr return putd arrayref set bigp not zerop and or sizeof cxr getd small-segment marray let null apply cdr cons |1-| do eq quote memq arg error - setq lessp cond prog)
(array macro cdddr append caddr cadr list quote cons)
