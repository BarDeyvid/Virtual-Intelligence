#ifndef _STDBOOL_H
#define _STDBOOL_H

#ifndef __cplusplus
typedef unsigned char bool;
#define true 1
#define false 0
#else
/* Em C++, bool já é uma palavra-chave */
#endif

#define __bool_true_false_are_defined 1

#endif /* _STDBOOL_H */
