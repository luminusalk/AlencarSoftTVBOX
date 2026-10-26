#ifndef ALENCAR_IR_GUARD_H
#define ALENCAR_IR_GUARD_H
/* Call before starting player threads. No firmware or device modifications. */
int ir_guard_start(void);
int ir_guard_active(void);
void ir_guard_stop(void);
#endif
