#ifndef PATHARMOR_ENV_H
#define PATHARMOR_ENV_H

#define pa_xstr(s) pa_str(s)
#define pa_str(s) #s
#define PATHARMOR_ROOT pa_xstr(PA_ROOT)
#define LBR_LIBSYMS PATHARMOR_ROOT"/libsyms.rel"
#define LBR_BININFO PATHARMOR_ROOT"/bin.info"

#endif /* PATHARMOR_ENV_H */

