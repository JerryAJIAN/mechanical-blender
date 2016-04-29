
#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "DNA_mesh_types.h"

#include "ED_mesh.h"

#include "BKE_editmesh.h"

#include "UI_resources.h"

#include "bmesh.h"

#include "mesh_dimensions.h"
#include "mechanical_utils.h"


bool valid_constraint_setting(BMDim *edm, int constraint) {
	bool ret = false;
	switch (constraint) {
		case DIM_PLANE_CONSTRAINT:
			ret = ELEM(edm->dim_type,DIM_TYPE_LINEAR,DIM_TYPE_ANGLE_3P,DIM_TYPE_ANGLE_4P);
			break;
		case DIM_AXIS_CONSTRAINT:
			ret = ELEM(edm->dim_type,DIM_TYPE_RADIUS, DIM_TYPE_DIAMETER);
			break;
		case DIM_ALLOW_SLIDE_CONSTRAINT:
			ret = ELEM(edm->dim_type,DIM_TYPE_ANGLE_3P,DIM_TYPE_ANGLE_4P);
			break;
	}
	return ret;
}

//Return dimension between two vertex
float get_dimension_value(BMDim *edm){
	float v=0;
	switch (edm->dim_type) {
		case DIM_TYPE_LINEAR:
			v = len_v3v3(edm->v[0]->co, edm->v[1]->co);
			break;
		case DIM_TYPE_DIAMETER:
			v = len_v3v3(edm->v[0]->co, edm->center)*2;
			break;
		case DIM_TYPE_RADIUS:
			v = len_v3v3(edm->v[0]->co, edm->center);
			break;
		case DIM_TYPE_ANGLE_3P:
			v = RAD2DEG(angle_v3v3v3(edm->v[0]->co,edm->center,edm->v[2]->co));
			break;
		case DIM_TYPE_ANGLE_4P:
			v = RAD2DEG(angle_v3v3v3(edm->v[0]->co,edm->center, edm->v[3]->co));
			break;
		default:
			BLI_assert(0);
	}

	return v;
}

int get_necessary_dimension_verts (int dim_type) {
	static int min_vert[] = {0,
	                  2, // DIM_TYPE_LINEAR
	                  3, // DIM_TYPE_DIAMETER
	                  3,  // DIM_TYPE_RADIUS
	                  3, // DIM_TYPE_ANGLE_3P
	                  4, // DIM_TYPE_ANGLE_4P
	                 };
	return min_vert[dim_type];
}


static void untag_dimension_necessary_verts(BMDim *edm) {
	for (int i=0;i<get_necessary_dimension_verts(edm->dim_type);i++) {
		BM_elem_flag_disable(edm->v[i], BM_ELEM_TAG);
	}
}

static void apply_dimension_diameter_from_center(BMesh *bm, BMDim *edm, float value, int constraints) {

	BLI_assert (ELEM(edm->dim_type,DIM_TYPE_DIAMETER, DIM_TYPE_RADIUS));

	float axis[3],v[3], ncenter[3], p[3];
	float curv = get_dimension_value(edm); // Current Value

	BMIter iter;
	BMVert* eve;

	get_dimension_plane(axis, p, edm);

	tag_vertexs_affected_by_dimension (bm, edm);

	if (constraints & DIM_AXIS_CONSTRAINT) {

		BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
			if (BM_elem_flag_test(eve,BM_ELEM_TAG)) {
				continue;
			}
			sub_v3_v3v3(v,eve->co, edm->center);
			project_v3_v3v3(ncenter,v, axis);
			add_v3_v3(ncenter,edm->center);

			if (fabs((len_v3v3 (eve->co, ncenter)*2 - curv)) <  DIM_CONSTRAINT_PRECISION){
				BM_elem_flag_enable(eve, BM_ELEM_TAG);
			}
		}
	}

	// Update related Verts
	BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
		if (BM_elem_flag_test(eve, BM_ELEM_TAG)) {
			sub_v3_v3v3(v,eve->co, edm->center);

			project_v3_v3v3(ncenter,v, axis);
			add_v3_v3(ncenter,edm->center);

			sub_v3_v3v3(v,eve->co, ncenter);

			normalize_v3(v);
			mul_v3_fl(v,value);
			add_v3_v3v3(eve->co,ncenter,v);

			// Reset tag
			BM_elem_flag_disable(eve, BM_ELEM_TAG);
		}
	}
}

/**
 * @brief apply_dimension_linear_value_exec
 * @param va
 * @param vb  Vertex to be modified
 * @param value
 * @param r_res  The vector of translation
 */
static void apply_dimension_linear_value_exec( float *a, float *b, float value, float *r_res){
	float vect [3];
	float prev[3];
	copy_v3_v3(prev,a);
	sub_v3_v3v3(vect,a,b);
	normalize_v3(vect);
	mul_v3_fl(vect,value);
	add_v3_v3v3(a,b, vect);
	sub_v3_v3v3(r_res, a, prev);
}


static void apply_dimension_linear_value(BMesh *bm, BMDim *edm, float value, int constraints) {

	float v[3];

	BMIter iter;
	BMVert* eve;

	BLI_assert (edm->dim_type == DIM_TYPE_LINEAR);

	if (edm->dir == 0){
		// Both directions
		float len=get_dimension_value(edm);
		float len2=(value-len)/2;
		edm->dir = 1;
		apply_dimension_linear_value(bm, edm,(len+len2), constraints);
		edm->dir = -1;
		apply_dimension_linear_value(bm, edm,value,constraints);
		edm->dir = 0;
		return;
	}

	tag_vertexs_affected_by_dimension (bm, edm);

	if (constraints & DIM_PLANE_CONSTRAINT) {
		float p[3], d_dir[3];
		if (edm->dir == -1) {
			copy_v3_v3(p, edm->v[0]->co);
		} else if (edm->dir == 1) {
			copy_v3_v3(p, edm->v[1]->co);
		}
		sub_v3_v3v3(d_dir,edm->v[0]->co,edm->v[1]->co);
		normalize_v3(d_dir);
		tag_vertexs_on_coplanar_faces(bm, p, d_dir);
	}
	// Untag dimensions vertex
	untag_dimension_necessary_verts(edm);


	// Update Dimension Verts
	if(edm->dir==1){
		apply_dimension_linear_value_exec(edm->v[1]->co,edm->v[0]->co, value, v);
	}else if(edm->dir==-1){
		apply_dimension_linear_value_exec(edm->v[0]->co,edm->v[1]->co, value, v);
	}

	// Update related Verts
	BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
		if (BM_elem_flag_test(eve, BM_ELEM_TAG)) {
			add_v3_v3(eve->co, v);
			// Reset tag
			BM_elem_flag_disable(eve, BM_ELEM_TAG);
		}
	}

}

static void apply_dimension_angle_exec(BMesh *bm, BMVert *eve, float *center, float *axis, float value, int constraints) {
	float rot[3], delta[3], r[3];
	BMVert *v,*v2;
	BMFace *f;
	BMIter iterv;
	BMIter iterf, iterf2;
	float dir[3];

	sub_v3_v3v3(delta, eve->co, center);
	cross_v3_v3v3(dir,delta,axis);
	normalize_v3(dir);
	rotate_v3_v3v3fl(rot, delta, axis, DEG2RAD(value));
	add_v3_v3v3(eve->co, rot, center);

	if (constraints & DIM_ALLOW_SLIDE_CONSTRAINT) {
		BM_ITER_MESH (f, &iterv, bm, BM_FACES_OF_MESH) {
			BM_ITER_ELEM (v, &iterf, f, BM_VERTS_OF_FACE) {
				if (v == eve) {
					if (perpendicular_v3_v3(axis, f->no)) {
						if (!parallel_v3u_v3u(dir,f->no)) {
							if (!point_on_axis(center,axis,eve->co)) {
								// Fin a vertex on face not matching eve
								BM_ITER_ELEM (v2, &iterf2, f, BM_VERTS_OF_FACE) {
									if (v2 != eve) break;
								}
								if (isect_line_plane_v3(r, center, eve->co, v2->co, f->no)){
									copy_v3_v3(eve->co,r);
								}
							}
						}
					}
				}
			}
		}
	}

}

static void apply_dimension_angle(BMesh *bm, BMDim *edm, float value, int constraints) {
	float axis[3], ncenter[3], v[3], pp[3];
	float d; //Difential to move

	float p[3], r[3];
	float d_dir[3]; // Dimension dir

	float constraint_axis[3];
	float ccenter1[3],ccenter2[3];
	float r_constraint, r_constraint_c;

	bool do_axis = false;

	BMIter iter;
	BMVert* eve;

	BLI_assert (ELEM(edm->dim_type,DIM_TYPE_ANGLE_3P,DIM_TYPE_ANGLE_4P));

	get_dimension_plane(axis, pp, edm);
	d = value - get_dimension_value(edm);

	if (edm->dir == 0) {
		// Both sides
		edm->dir = 1;
		apply_dimension_angle(bm, edm,value - d/2.0f, constraints);
		edm->dir = -1;
		apply_dimension_angle(bm, edm,value,constraints);
		edm->dir = 0;
		return;
	}

	// Get Dimension dir
	if (edm->dir == 1) {
		copy_v3_v3(p, edm->v[2]->co);
		sub_v3_v3v3(r, edm->v[2]->co, edm->center);
	} else if (edm->dir == -1) {
		copy_v3_v3(p, edm->v[0]->co);
		sub_v3_v3v3(r, edm->v[0]->co, edm->center);
	}
	cross_v3_v3v3(d_dir,axis,r);
	normalize_v3(d_dir);

	tag_vertexs_affected_by_dimension (bm, edm);

	if (constraints & DIM_PLANE_CONSTRAINT) {
		tag_vertexs_on_coplanar_faces(bm, p, d_dir);
	}

	int i = (edm->dim_type == DIM_TYPE_ANGLE_3P) ? 3 : 4;
	float r1[3],r2[3];
	if (
		(edm->totverts >= i+2) &&
		(center_of_3_points (ccenter1,edm->v[0]->co, edm->v[i]->co, edm->v[i+1]->co)))
	{
		/* We have information to get an axis */
		sub_v3_v3v3(r1, edm->v[0]->co, ccenter1);
		sub_v3_v3v3(r2, edm->v[i]->co, ccenter1);
		cross_v3_v3v3(constraint_axis,r1,r2);
		normalize_v3(constraint_axis);
		r_constraint = len_v3v3(ccenter1,edm->v[0]->co);


		v_perpendicular_to_axis (ccenter2, ccenter1, edm->center, constraint_axis);
		sub_v3_v3v3(ccenter2, edm->center, ccenter2);
		r_constraint_c = len_v3v3(ccenter2,edm->center);

		do_axis = true;

	}

	if (constraints & DIM_AXIS_CONSTRAINT && do_axis) {
		BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
			float delta = fabs(point_dist_to_axis (ccenter1, constraint_axis,eve->co));
			if ((delta - r_constraint)< DIM_CONSTRAINT_PRECISION)
			{
				BM_elem_flag_enable(eve, BM_ELEM_TAG);
			}
		}

	}


	// Untag dimensions vertex
	untag_dimension_necessary_verts(edm);

	if (!do_axis) {
		// Untag vertex not in plane
		BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
			if (BM_elem_flag_test(eve, BM_ELEM_TAG)) {
				if (!point_on_plane (edm->center,d_dir,eve->co)) {
					BM_elem_flag_disable(eve, BM_ELEM_TAG);
				}
			}
		}
	}

	if (edm->dir == 1) {
		apply_dimension_angle_exec(bm,edm->v[2],edm->center, axis,d,constraints);
		if (edm->dim_type == DIM_TYPE_ANGLE_4P){
			apply_dimension_angle_exec(bm, edm->v[3],edm->center, axis,d,constraints);
		}
	}
	if (edm->dir == -1) {
		d = d*(-1);
		apply_dimension_angle_exec(bm, edm->v[0],edm->center, axis, d,constraints);
		if (edm->dim_type == DIM_TYPE_ANGLE_4P){
			apply_dimension_angle_exec(bm, edm->v[1],edm->center, axis,d, constraints);
		}
	}

	//Rotate tagged points against axis
	BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
		if (BM_elem_flag_test(eve, BM_ELEM_TAG)) {

			if (do_axis && (fabs(point_dist_to_axis (ccenter1, constraint_axis,eve->co) - r_constraint))< DIM_CONSTRAINT_OVERRIDE)
			{
				float naxis[3];

				sub_v3_v3v3(v,eve->co, ccenter2);
				v_perpendicular_to_axis(ncenter,ccenter2,v,constraint_axis);
				normalize_v3(ncenter);
				mul_v3_fl(ncenter,r_constraint_c);
				add_v3_v3(ncenter,ccenter2);
				normal_tri_v3(naxis,ccenter1,ccenter2,ncenter);

				apply_dimension_angle_exec(bm, eve,ncenter, naxis,d,constraints);

			} else {

				sub_v3_v3v3(v,eve->co, edm->center);
	
				project_v3_v3v3(ncenter,v, axis);
				add_v3_v3(ncenter,edm->center);
	
				apply_dimension_angle_exec(bm, eve,ncenter, axis,d,constraints);
			}
	
			// Reset tag
			BM_elem_flag_disable(eve, BM_ELEM_TAG);
		}
	}
}

void apply_dimension_value (BMesh *bm, BMDim *edm, float value, ToolSettings *ts) {

	int constraints = (edm->constraints & DIM_CONSTRAINT_OVERRIDE) ? edm->constraints : ts->dimension_constraints;

	switch (edm->dim_type) {
		case DIM_TYPE_LINEAR:
			apply_dimension_linear_value(bm,edm,value,constraints);
			break;
		case DIM_TYPE_DIAMETER:
			apply_dimension_diameter_from_center(bm,edm,value/2.0f,constraints);
			break;
		case DIM_TYPE_RADIUS:
			apply_dimension_diameter_from_center(bm,edm,value,constraints);
			break;
		case DIM_TYPE_ANGLE_3P:
		case DIM_TYPE_ANGLE_4P:
			apply_dimension_angle(bm,edm,value,constraints);
			break;
		default:
			BLI_assert(0);
	}

	BM_mesh_normals_update(bm);
}

BMDim* get_selected_dimension(BMesh *bm){
	BMDim *edm = NULL;
	BMIter iter;
	if (bm->totdimsel == 1) {
		BM_ITER_MESH (edm, &iter, bm, BM_DIMS_OF_MESH) {
			if ( BM_elem_flag_test(edm, BM_ELEM_SELECT)) {
				break;
			}
		}
	}
	return edm;
}


void get_dimension_mid(float mid[3],BMDim *edm){
	switch (edm->dim_type)
	{
		case DIM_TYPE_LINEAR:
			mid_of_2_points(mid, edm->v[0]->co, edm->v[1]->co);
			break;
		default:
			BLI_assert(0);
	}
}

void get_dimension_plane (float v[3], float p[3], BMDim *edm){
	float m[3]={0}, r[3]={0};
	switch (edm->dim_type) {
		case DIM_TYPE_LINEAR:
			BLI_assert(0); // Not possible
			break;
		case DIM_TYPE_DIAMETER:
		case DIM_TYPE_RADIUS:
			sub_v3_v3v3(m,edm->v[0]->co,edm->center);
			normalize_v3(m);
			sub_v3_v3v3(r,edm->v[1]->co,edm->center);
			normalize_v3(r);
			if (parallel_v3u_v3u(m,r)) {
				// Parallel vector, use next
				sub_v3_v3v3(r,edm->v[2]->co,edm->center);
				normalize_v3(r);
			}
			break;
		case DIM_TYPE_ANGLE_3P:
			sub_v3_v3v3(m,edm->v[0]->co,edm->center);
			sub_v3_v3v3(r,edm->v[2]->co,edm->center);
			break;
		case DIM_TYPE_ANGLE_4P:
			sub_v3_v3v3(m,edm->v[0]->co,edm->center);
			sub_v3_v3v3(r,edm->v[3]->co,edm->center);
			break;
		default:
			BLI_assert(0);
	}
	cross_v3_v3v3(v,m,r);
	normalize_v3(v);
	copy_v3_v3(p,edm->v[0]->co);
}


void mid_of_2_points (float *mid, float *p1, float *p2) {
	sub_v3_v3v3(mid, p2, p1);
	mul_v3_fl(mid,0.5);
	add_v3_v3(mid, p1);
}

static void set_dimension_angle_start_end(BMDim *edm, float *a, float *b) {
	float len;
	len = len_v3(edm->fpos);
	sub_v3_v3v3(edm->start,a,edm->center);
	normalize_v3(edm->start);
	mul_v3_fl(edm->start,len);
	add_v3_v3(edm->start,edm->center);
	sub_v3_v3v3(edm->end,b,edm->center);
	normalize_v3(edm->end);
	mul_v3_fl(edm->end,len);
	add_v3_v3(edm->end,edm->center);
}

static void set_dimension_circ_start_end(BMDim* edm, float value) {
	float v[3];
	sub_v3_v3v3(v,edm->fpos, edm->center);
	normalize_v3(v);
	mul_v3_fl(v,value);
	add_v3_v3v3(edm->start,edm->center,v);
	sub_v3_v3v3(edm->end, edm->center,v);
}

static void set_dimension_start_end(BMDim *edm) {
	switch (edm->dim_type) {
		case DIM_TYPE_LINEAR:
			add_v3_v3v3(edm->start,edm->fpos, edm->v[0]->co);
			add_v3_v3v3(edm->end, edm->fpos, edm->v[1]->co);
			break;
		case DIM_TYPE_DIAMETER:
			set_dimension_circ_start_end(edm,get_dimension_value(edm)/2.0f);
			break;
		case DIM_TYPE_RADIUS:
			set_dimension_circ_start_end(edm,get_dimension_value(edm));
			break;
		case DIM_TYPE_ANGLE_3P:
			set_dimension_angle_start_end(edm, edm->v[0]->co, edm->v[2]->co);
			break;
		case DIM_TYPE_ANGLE_4P:
			set_dimension_angle_start_end(edm, edm->v[0]->co, edm->v[3]->co);
			break;
		default:
			BLI_assert(0);
	}
}


void set_dimension_center (BMDim *edm) {
	float a[3];
	switch (edm->dim_type) {
		case DIM_TYPE_LINEAR:
			zero_v3(edm->center);
			break;
		case DIM_TYPE_DIAMETER:
		case DIM_TYPE_RADIUS:
			// Recalc dimension center
			if (center_of_3_points (edm->center, edm->v[0]->co, edm->v[1]->co, edm->v[2]->co)) {
				// Ok
			} else if (center_of_3_points (edm->center, edm->v[1]->co, edm->v[0]->co, edm->v[2]->co)) {
				// Ok
			} else {
				// Somthing is going grong!
				BLI_assert (0);
			}
			break;
		case DIM_TYPE_ANGLE_3P:
			copy_v3_v3(edm->center, edm->v[1]->co);
			break;
		case DIM_TYPE_ANGLE_4P:
			isect_line_line_v3(
			            edm->v[0]->co,
						edm->v[1]->co,
						edm->v[2]->co,
						edm->v[3]->co,
						edm->center,
						a // Should match center if cooplanar
			        );
			break;
	}
}

static void ensure_fpos_on_plane (BMDim *edm){
	float n[3],p[3];
	// Set f pos always on dimension plane!
	// fpos is related to dimension center
	get_dimension_plane(n,p, edm);
	add_v3_v3(edm->fpos,edm->center);
	project_v3_plane(edm->fpos, n, p);
	sub_v3_v3(edm->fpos,edm->center);
}

void dimension_data_update (BMDim *edm) {
	float axis[3], v1[3],v2[3], l;

	switch (edm->dim_type) {
		case DIM_TYPE_LINEAR:
			// Set dimension always perpendicular to edge
			l = len_v3(edm->fpos);
			sub_v3_v3v3(axis, edm->v[0]->co, edm->v[1]->co);
			normalize_v3(axis);
			project_v3_v3v3(v1,edm->fpos,axis);
			sub_v3_v3(edm->fpos,v1);
			normalize_v3(edm->fpos);
			mul_v3_fl (edm->fpos, l);


			// Baseline
			set_dimension_start_end(edm);

			// Set txt pos acording pos factor
			sub_v3_v3v3(edm->dpos, edm->end, edm->start);
			mul_v3_fl(edm->dpos, edm->dpos_fact);
			add_v3_v3(edm->dpos, edm->start);

			break;
		case DIM_TYPE_DIAMETER:
			set_dimension_center(edm);
			ensure_fpos_on_plane(edm);
			set_dimension_start_end(edm);


			// Set txt pos acording pos factor
			sub_v3_v3v3(edm->dpos, edm->end, edm->start);
			mul_v3_fl(edm->dpos, edm->dpos_fact);
			add_v3_v3(edm->dpos, edm->start);
			break;
		case DIM_TYPE_RADIUS:
			set_dimension_center(edm);
			ensure_fpos_on_plane(edm);

			set_dimension_start_end(edm); // First

			// Set txt pos acording pos factor
			sub_v3_v3v3(edm->dpos, edm->end, edm->center);
			mul_v3_fl(edm->dpos, edm->dpos_fact);
			add_v3_v3(edm->dpos, edm->start);
			break;
		case DIM_TYPE_ANGLE_3P:
		case DIM_TYPE_ANGLE_4P:
			ensure_fpos_on_plane(edm);

			set_dimension_center(edm);  // First
			set_dimension_start_end(edm);

			// Set txt pos acording pos factor
			sub_v3_v3v3(v1,edm->start,edm->center);
			normalize_v3(v1);
			sub_v3_v3v3(v2,edm->end,edm->center);
			cross_v3_v3v3(axis,v1,v2);
			normalize_v3(v2);

			// Set txt pos acording pos factor
			mul_v3_fl(v1,len_v3(edm->fpos));
			rotate_v3_v3v3fl(edm->dpos,v1,axis, acos(dot_v3v3(v1,v2))*edm->dpos_fact);
			add_v3_v3(edm->dpos, edm->center);
			break;
		default:
			BLI_assert(0);
	}
}
