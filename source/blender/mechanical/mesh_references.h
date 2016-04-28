#ifndef MESH_REFERENCES_H
#define MESH_REFERENCES_H


#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"
#include "DNA_screen_types.h"
#include "DNA_object_types.h"

void reference_plane_matrix (BMReference *erf, float mat[3][3]);
bool reference_plane_project_input (Object *ob, BMReference *erf, ARegion *ar, View3D *v3d, const int mval[2], float r_co[3]);

#endif //MESH_REFERENCES_H

