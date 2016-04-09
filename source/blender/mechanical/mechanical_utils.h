#ifndef MECHANICAL_UTILS_H
#define MECHANICAL_UTILS_H
/*
 *
 *
 *
*/

bool parallel_v3_v3(float *v1, float *v2);
bool parallel_v3u_v3u(float *v1, float *v2);
bool perpendicular_v3_v3(float *v1, float *v2);


void tag_vertexs_on_coplanar_faces(BMesh *bm, float* point, float *dir);
void tag_vertexs_affected_by_dimension (BMesh *bm, BMDim *edm);




#endif // MECHANICAL_UTILS_H
