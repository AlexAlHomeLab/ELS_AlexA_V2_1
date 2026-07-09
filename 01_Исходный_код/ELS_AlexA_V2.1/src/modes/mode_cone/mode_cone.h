#ifndef MODE_CONE_H
#define MODE_CONE_H





#ifdef __cplusplus
extern "C" {
#endif


void mode_cone_enter(void);
void mode_cone_exit(void);
void mode_cone_left_start(float start_x, float start_z, float angle, float length);
void mode_cone_right_start(float start_x, float start_z, float angle, float length);
void mode_cone_process(void);


#ifdef __cplusplus
}
#endif
#endif
