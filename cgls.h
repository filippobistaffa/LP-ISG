#ifndef CGLS_H_
#define CGLS_H_

// Headers

#include "types.h"

#define TOLERANCE 1e-14
#define MAXITERATIONS 200
#define CGLSDEBUG true

unsigned cudacgls(const value *val, const unsigned *ptr, const unsigned *ind,
		  unsigned m, unsigned n, unsigned nnz, value *b, value *x, float *rt);

#endif /* CGLS_H_ */
