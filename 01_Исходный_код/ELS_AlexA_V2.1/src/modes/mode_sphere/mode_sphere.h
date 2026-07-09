#ifndef MODE_SPHERE_H
#define MODE_SPHERE_H





#ifdef __cplusplus
extern "C" {
#endif


void mode_sphere_enter(void);
void mode_sphere_exit(void);
void mode_sphere_start(float center_x, float center_z, float radius, float depth);
void mode_sphere_process(void);


#ifdef __cplusplus
}
#endif
#endif
