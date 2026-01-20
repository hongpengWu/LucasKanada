#ifndef LK_HLS_H
#define LK_HLS_H

#include "lk_define.h"

// 函数声明
void hls_LK(pack_t img1[HEIGHT * WIDTH/2], 
            pack_t img2[HEIGHT * WIDTH/2], 
            short vx[HEIGHT * WIDTH], 
            short vy[HEIGHT * WIDTH]);

void hls_isotropic_kernel(pack_t img[HEIGHT * WIDTH/2], 
                         pack_t result[HEIGHT * WIDTH/2]);

void hls_twoIsotropicFilters(pack_t img1[HEIGHT * WIDTH/2], 
                            pack_t img2[HEIGHT * WIDTH/2],
                            pack_t blur1[HEIGHT * WIDTH/2], 
                            pack_t blur2[HEIGHT * WIDTH/2]);

void hls_derivatives_kernel(pack_t img1[HEIGHT * WIDTH/2], 
                           pack_t img2[HEIGHT * WIDTH/2],
                           short Ix[HEIGHT * WIDTH], 
                           short Iy[HEIGHT * WIDTH], 
                           short It[HEIGHT * WIDTH]);

#endif // LK_HLS_H
