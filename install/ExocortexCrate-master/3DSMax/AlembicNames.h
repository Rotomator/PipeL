#ifndef __ALEMBIC_NAMES_H__
#define __ALEMBIC_NAMES_H__



#define EXOCORTEX_ALEMBIC_CATEGORY _T("Alembic")

#define EMPTY_SPLINE_OBJECT_CLASSID					Class_ID(0x4eb65d6, 0x7c454597)
#define EMPTY_POLYLINE_OBJECT_CLASSID				Class_ID(0x2cac433a, 0x424c0456)
#define ALEMBIC_EXPORTER_CLASSID					Class_ID(0x79d613a4, 0x4f21c3ad)

#define ALEMBIC_MESH_NORMALS_MODIFIER_CLASSID		Class_ID(0x4a9c3800, 0x7f1d67d8)
#define ALEMBIC_MESH_NORMALS_MODIFIER_NAME			"Alembic Mesh Normals"
#define ALEMBIC_MESH_NORMALS_MODIFIER_SCRIPTNAME	"AlembicMeshNormals"

#define ALEMBIC_MESH_GEOM_MODIFIER_CLASSID			Class_ID(0x753e2df1, 0x5a8241c8)
#define ALEMBIC_MESH_GEOM_MODIFIER_NAME				"Alembic Mesh Geometry"
#define ALEMBIC_MESH_GEOM_MODIFIER_SCRIPTNAME		"AlembicMeshGeometry"

#define ALEMBIC_MESH_TOPO_MODIFIER_CLASSID			Class_ID(0x76ad6538, 0x424c2061)
#define ALEMBIC_MESH_TOPO_MODIFIER_NAME				"Alembic Mesh Topology"
#define ALEMBIC_MESH_TOPO_MODIFIER_SCRIPTNAME		"AlembicMeshTopology"

#define ALEMBIC_MESH_UVW_MODIFIER_CLASSID			Class_ID(0x19103686, 0x46234a41)
#define ALEMBIC_MESH_UVW_MODIFIER_NAME				"Alembic Mesh UVW"
#define ALEMBIC_MESH_UVW_MODIFIER_SCRIPTNAME		"AlembicMeshUVW"

#define ALEMBIC_XFORM_CONTROLLER_CLASSID			Class_ID(0x9bf4cdb, 0xe29448e)
#define ALEMBIC_XFORM_CONTROLLER_NAME				"Alembic Xform"
#define ALEMBIC_XFORM_CONTROLLER_SCRIPTNAME			"AlembicXform"

#define ALEMBIC_VISIBILITY_CONTROLLER_CLASSID		Class_ID(0x27e512c0, 0x389b3971)
#define ALEMBIC_VISIBILITY_CONTROLLER_NAME			"Alembic Visibility"
#define ALEMBIC_VISIBILITY_CONTROLLER_SCRIPTNAME	"AlembicVisibility"

#define ALEMBIC_TIME_CONTROL_HELPER_CLASSID			Class_ID(0x4eb65d6, 0x7c454597)
#define ALEMBIC_TIME_CONTROL_HELPER_NAME			"Alembic Time Control"
#define ALEMBIC_TIME_CONTROL_HELPER_SCRIPTNAME		"AlembicTimeControl"

#define ALEMBIC_SIMPLE_PARTICLE_CLASSID				Class_ID(0x246b1c1e, 0x3b2f032f)
#define ALEMBIC_SIMPLE_PARTICLE_NAME			    "Alembic Particles"
#define ALEMBIC_SIMPLE_PARTICLE_SCRIPTNAME		    "AlembicParticless"

#define ALEMBIC_SPLINE_GEOM_MODIFIER_CLASSID			Class_ID(0x28233914, 0x18260c30)
#define ALEMBIC_SPLINE_GEOM_MODIFIER_NAME			    "Alembic Spline Geometry"
#define ALEMBIC_SPLINE_GEOM_MODIFIER_SCRIPTNAME		    "AlembicSplineGeometry"

#define ALEMBIC_SPLINE_TOPO_MODIFIER_CLASSID			Class_ID(0x4f5637f1, 0x3ffd61f3)
#define ALEMBIC_SPLINE_TOPO_MODIFIER_NAME			    "Alembic Spline Topology"
#define ALEMBIC_SPLINE_TOPO_MODIFIER_SCRIPTNAME		    "AlembicSplineTopology"

#define ALEMBIC_FLOAT_CONTROLLER_CLASSID				Class_ID(0x5fd044f3, 0xe030797)
#define ALEMBIC_FLOAT_CONTROLLER_NAME					"Alembic Float"
#define ALEMBIC_FLOAT_CONTROLLER_SCRIPTNAME				"AlembicFloat"

#define ALEMBIC_CAMERA_MODIFIER_CLASSID			Class_ID(0x3a5b6550, 0x5e14408f)
#define ALEMBIC_CAMERA_MODIFIER_NAME				"Alembic Camera Properties"
#define ALEMBIC_CAMERA_MODIFIER_SCRIPTNAME		"AlembicCameraProperties"

#define ALEMBIC_NURBS_MODIFIER_CLASSID			Class_ID(0x59980cef, 0x6e0077ff)
#define ALEMBIC_NURBS_MODIFIER_NAME			    "Alembic NURBS"
#define ALEMBIC_NURBS_MODIFIER_SCRIPTNAME		    "AlembicNURBS"

			

#define ALEMBIC_UNUSED_13_CLASSID					Class_ID(0x75d0291, 0x77876660)
#define ALEMBIC_UNUSED_14_CLASSID					Class_ID(0x27697f4e, 0x33b63fa5)
#define ALEMBIC_UNUSED_15_CLASSID					Class_ID(0x27de063a, 0x28fe18d9)
#define ALEMBIC_UNUSED_16_CLASSID					Class_ID(0x53da05d3, 0x90d1f9b)
#define ALEMBIC_UNUSED_17_CLASSID					Class_ID(0x7c0b4f98, 0x73e11929)
#define ALEMBIC_UNUSED_18_CLASSID					Class_ID(0x250b6e7a, 0xaf8348b)
#define ALEMBIC_UNUSED_19_CLASSID					Class_ID(0x42b015db, 0x8270650)
#define ALEMBIC_UNUSED_20_CLASSID					Class_ID(0x6ae5099b, 0x38c35ddd)
#define ALEMBIC_UNUSED_21_CLASSID					Class_ID(0x24cb562c, 0x6c4a5f9a)
#define ALEMBIC_UNUSED_22_CLASSID					Class_ID(0x4b3429e8, 0x2da67188)
#define ALEMBIC_UNUSED_23_CLASSID					Class_ID(0x7b2a5520, 0x19ab0773)
#define ALEMBIC_UNUSED_24_CLASSID					Class_ID(0x1b3227e1, 0x101202b8)
#define ALEMBIC_UNUSED_25_CLASSID					Class_ID(0x46c911e9, 0x7ce4278f)
#define ALEMBIC_UNUSED_26_CLASSID					Class_ID(0x6f941105, 0x377862fd)


ClassDesc2* GetAlembicXformControllerClassDesc();
ClassDesc2* GetAlembicVisibilityControllerClassDesc();

ClassDesc2* GetAlembicParticlesClassDesc();

ClassDesc2* GetAlembicMeshTopoModifierClassDesc();
ClassDesc2* GetAlembicMeshGeomModifierClassDesc();
ClassDesc2* GetAlembicMeshNormalsModifierClassDesc();
ClassDesc2* GetAlembicMeshUVWModifierClassDesc();

ClassDesc2 *GetEmptySplineObjectClassDesc();
ClassDesc2 *GetEmptyPolyLineObjectClassDesc();

ClassDesc2 *GetAlembicSplineGeomModifierClassDesc();
ClassDesc2 *GetAlembicSplineTopoModifierClassDesc();

ClassDesc2* GetAlembicFloatControllerClassDesc();

extern HINSTANCE hInstance;

#endif //__ALEMBIC_NAMES_H__

