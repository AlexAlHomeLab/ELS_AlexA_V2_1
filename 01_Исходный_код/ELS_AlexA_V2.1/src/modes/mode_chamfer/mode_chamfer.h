#ifndef MODE_CHAMFER_H
#define MODE_CHAMFER_H





#ifdef __cplusplus
extern "C" {
#endif


void mode_chamfer_enter(void);
void mode_chamfer_exit(void);
void mode_chamfer_ext_start(float start_x, float start_z, float length);
void mode_chamfer_int_start(float start_x, float start_z, float length);
void mode_chamfer_process(void);


#ifdef __cplusplus
}
#endif
#endif
