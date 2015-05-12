#include "stdafx.h"
#include "AlembicPoints.h"
#include "AlembicXform.h"


using namespace XSI;
using namespace MATH;


AlembicPoints::AlembicPoints(SceneNodePtr eNode, AlembicWriteJob * in_Job, Abc::OObject oParent)
: AlembicObject(eNode, in_Job, oParent)
{
  AbcG::OPoints points(GetMyParent(), eNode->name, GetJob()->GetAnimatedTs());

   mPointsSchema = points.getSchema();

   Abc::OCompoundProperty argGeomParams = mPointsSchema.getArbGeomParams();

   // particle attributes
   mScaleProperty = Abc::OV3fArrayProperty(argGeomParams, ".scale", mPointsSchema.getMetaData(), GetJob()->GetAnimatedTs() );
   mOrientationProperty = Abc::OQuatfArrayProperty(argGeomParams, ".orientation", mPointsSchema.getMetaData(), GetJob()->GetAnimatedTs() );
   mAngularVelocityProperty = Abc::OQuatfArrayProperty(argGeomParams, ".angularvelocity", mPointsSchema.getMetaData(), GetJob()->GetAnimatedTs() );
   mAgeProperty = Abc::OFloatArrayProperty(argGeomParams, ".age", mPointsSchema.getMetaData(), GetJob()->GetAnimatedTs() );
   mMassProperty = Abc::OFloatArrayProperty(argGeomParams, ".mass", mPointsSchema.getMetaData(), GetJob()->GetAnimatedTs() );
   mShapeTypeProperty = Abc::OUInt16ArrayProperty(argGeomParams, ".shapetype", mPointsSchema.getMetaData(), GetJob()->GetAnimatedTs() );
   mColorProperty = Abc::OC4fArrayProperty(argGeomParams, ".color", mPointsSchema.getMetaData(), GetJob()->GetAnimatedTs() );

   Primitive prim(GetRef(REF_PRIMITIVE));
   Abc::OCompoundProperty argGeomParamsProp = mPointsSchema.getArbGeomParams();
   customAttributes.defineCustomAttributes(prim.GetGeometry(), argGeomParamsProp, mPointsSchema.getMetaData(), GetJob()->GetAnimatedTs());
}

AlembicPoints::~AlembicPoints()
{
}

Abc::OCompoundProperty AlembicPoints::GetCompound()
{
   return mPointsSchema;
}

XSI::CStatus AlembicPoints::Save(double time)
{
   // store the transform
   Primitive prim(GetRef(REF_PRIMITIVE));
   bool globalSpace = GetJob()->GetOption(L"globalSpace");

   // query the global space
   CTransformation globalXfo;
   if(globalSpace)
      globalXfo = KinematicState(GetRef(REF_GLOBAL_TRANS)).GetTransform(time);
   CTransformation globalRotation;
   globalRotation.SetRotation(globalXfo.GetRotation());

   // store the metadata
   SaveMetaData(GetRef(REF_NODE),this);

   // check if the pointcloud is animated
   if(mNumSamples > 0) {
      if(!isRefAnimated(GetRef(REF_PRIMITIVE),false,globalSpace))
         return CStatus::OK;
   }

   // access the geometry
   Geometry geo = prim.GetGeometry(time);

   customAttributes.exportCustomAttributes(geo);

   // deal with each attribute. we scope here do free memory instantly
   {
      // position == PointPosition
      std::vector<Abc::V3f> positionVec;
      {
         // prepare the bounding box
         Abc::Box3d bbox;

         ICEAttribute attr = geo.GetICEAttributeFromName(L"PointPosition");
         if(attr.IsDefined() && attr.IsValid())
         {
            CICEAttributeDataArrayVector3f data;
            attr.GetDataArray(data);
            ULONG count = (data.IsConstant() || attr.IsConstant()) ? 1 : data.GetCount();
            positionVec.resize((size_t)count);
            for(ULONG i=0;i<count;i++)
            {
               positionVec[i].setValue(data[i].GetX(),data[i].GetY(),data[i].GetZ());
               bbox.extendBy(positionVec[i]);
            }
         }

         // store the bbox
         mPointsSample.setSelfBounds(bbox);
      }

      Abc::P3fArraySample positionSample = Abc::P3fArraySample(positionVec);

      // velocity == PointVelocity
      std::vector<Abc::V3f> velocityVec;
      {
         ICEAttribute attr = geo.GetICEAttributeFromName(L"PointVelocity");
         if(attr.IsDefined() && attr.IsValid())
         {
            CICEAttributeDataArrayVector3f data;
            attr.GetDataArray(data);
            ULONG count = (data.IsConstant() || attr.IsConstant()) ? 1 : data.GetCount();
            velocityVec.resize((size_t)count);
            if(count > 0)
            {
               bool isConstant = true;
               CVector3f firstVal = data[0];
               for(ULONG i=0;i<count;i++)
               {
                  velocityVec[i].setValue(data[i].GetX(),data[i].GetY(),data[i].GetZ());
                  if(isConstant)
                     isConstant = firstVal == data[i];
               }
               if(isConstant)
                  velocityVec.resize(1);
            }
         }
      }

      Abc::V3fArraySample velocitySample = Abc::V3fArraySample(velocityVec);

      // width == Size
      std::vector<float> widthVec;
      {
         ICEAttribute attr = geo.GetICEAttributeFromName(L"Size");
         if(attr.IsDefined() && attr.IsValid())
         {
            CICEAttributeDataArrayFloat data;
            attr.GetDataArray(data);
            ULONG count = (data.IsConstant() || attr.IsConstant()) ? 1 : data.GetCount();
            widthVec.resize((size_t)count);
            if(count > 0)
            {
               bool isConstant = true;
               float firstVal = data[0];
               for(ULONG i=0;i<count;i++)
               {
                  widthVec[i] = data[i];
                  if(isConstant)
                     isConstant = firstVal == data[i];
               }
               if(isConstant)
                  widthVec.resize(1);
            }
         }
      }

      Abc::FloatArraySample widthSample = Abc::FloatArraySample(widthVec);

      // id == ID
      std::vector<Abc::uint64_t> idVec;
      {
         ICEAttribute attr = geo.GetICEAttributeFromName(L"ID");
         if(attr.IsDefined() && attr.IsValid())
         {
            CICEAttributeDataArrayLong data;
            attr.GetDataArray(data);
            ULONG count = (data.IsConstant() || attr.IsConstant()) ? 1 : data.GetCount();
            idVec.resize((size_t)count);
            if(count > 0)
            {
               bool isConstant = true;
               LONG firstVal = data[0];
               for(ULONG i=0;i<count;i++)
               {
                  idVec[i] = (Abc::uint64_t)data[i];
                  if(isConstant)
                     isConstant = firstVal == data[i];
               }
               if(isConstant)
                  idVec.resize(1);
            }
         }
      }

      Abc::UInt64ArraySample idSample(idVec);

      // store the Points sample
      mPointsSample.setPositions(positionSample);
      mPointsSample.setVelocities(velocitySample);
      mPointsSample.setWidths(AbcG::OFloatGeomParam::Sample(widthSample,AbcG::kVertexScope));
      mPointsSample.setIds(idSample);
      mPointsSchema.set(mPointsSample);
   }

   // scale
   {
      ICEAttribute attr = geo.GetICEAttributeFromName(L"Scale");
      std::vector<Abc::V3f> vec;
      if(attr.IsDefined() && attr.IsValid() && attr.GetElementCount() > 0)
      {
         {
            CICEAttributeDataArrayVector3f data;
            attr.GetDataArray(data);
            ULONG count = (data.IsConstant() || attr.IsConstant()) ? 1 : data.GetCount();
            vec.resize((size_t)count);
            if(count > 0)
            {
               bool isConstant = true;
               CVector3f firstVal = data[0];
               for(ULONG i=0;i<count;i++)
               {
                  vec[i].setValue(data[i].GetX(),data[i].GetY(),data[i].GetZ());
                  if(isConstant)
                     isConstant = firstVal == data[i];
               }
               if(isConstant)
                  vec.resize(1);
            }
         }

      }
      Abc::V3fArraySample sample = Abc::V3fArraySample(vec);
      mScaleProperty.set(sample);
   }

   // orientation + angular vel
   for(int attrIndex = 0;attrIndex < 2; attrIndex++)
   {
      ICEAttribute attr = geo.GetICEAttributeFromName(attrIndex == 0 ? L"Orientation" : L"AngularVelocity");
      std::vector<Abc::Quatf> vec;
      if(attr.IsDefined() && attr.IsValid() && attr.GetElementCount() > 0)
      {
         {
            CICEAttributeDataArrayRotationf data;
            attr.GetDataArray(data);
            ULONG count = (data.IsConstant() || attr.IsConstant()) ? 1 : data.GetCount();
            vec.resize((size_t)count);
            if(count > 0)
            {
               bool isConstant = true;
               Abc::Quatf firstVal;
               for(ULONG i=0;i<count;i++)
               {
				   Imath::Quatf &vecVal = vec[i];
                  if(data[i].GetRepresentation() == CRotationf::siAxisAngleRot)
                  {
                     float angle;
                     CVector3f axis = data[i].GetAxisAngle(angle);
                     CRotation rot;
                     rot.SetFromAxisAngle(CVector3(axis.GetX(),axis.GetY(),axis.GetZ()),angle);
                     CQuaternion quat = rot.GetQuaternion();
                     vecVal.v.x = (float)quat.GetX();
                     vecVal.v.y = (float)quat.GetY();
                     vecVal.v.z = (float)quat.GetZ();
                     vecVal.r = (float)quat.GetW();
                  }
                  else if(data[i].GetRepresentation() == CRotationf::siEulerRot)
                  {
                     CVector3f euler = data[i].GetXYZAngles();
                     CRotation rot;
                     rot.SetFromXYZAngles(CVector3(euler.GetX(),euler.GetY(),euler.GetZ()));
                     CQuaternion quat = rot.GetQuaternion();
                     vecVal.v.x = (float)quat.GetX();
                     vecVal.v.y = (float)quat.GetY();
                     vecVal.v.z = (float)quat.GetZ();
                     vecVal.r = (float)quat.GetW();
                  }
                  else // quaternion
                  {
                     CQuaternionf quat = data[i].GetQuaternion();
                     vecVal.v.x = quat.GetX();
                     vecVal.v.y = quat.GetY();
                     vecVal.v.z = quat.GetZ();
                     vecVal.r = quat.GetW();
                  }
                  if(i==0)
                     firstVal = vecVal;
                  else if(isConstant)
                     isConstant = firstVal == vecVal;
               }
               if(isConstant)
                  vec.resize(1);
            }
         }

      }

      Abc::QuatfArraySample sample(vec);
      if(attrIndex == 0)
         mOrientationProperty.set(sample);
      else
         mAngularVelocityProperty.set(sample);
   }

   // age
   {
      ICEAttribute attr = geo.GetICEAttributeFromName(L"Age");
      std::vector<float> vec;
      if(attr.IsDefined() && attr.IsValid() && attr.GetElementCount() > 0)
      {
         {
            CICEAttributeDataArrayFloat data;
            attr.GetDataArray(data);
            ULONG count = (data.IsConstant() || attr.IsConstant()) ? 1 : data.GetCount();
            vec.resize((size_t)count);
            if(count > 0)
            {
               bool isConstant = true;
               float firstVal = data[0];
               for(ULONG i=0;i<count;i++)
               {
                  vec[i] = data[i];
                  if(isConstant)
                     isConstant = firstVal == data[i];
               }
               if(isConstant)
                  vec.resize(1);
            }
         }
      }
      Abc::FloatArraySample sample(vec);
      mAgeProperty.set(sample);
   }

   // mass
   {
      ICEAttribute attr = geo.GetICEAttributeFromName(L"Mass");
      std::vector<float> vec;
      if(attr.IsDefined() && attr.IsValid() && attr.GetElementCount() > 0)
      {
         {
            CICEAttributeDataArrayFloat data;
            attr.GetDataArray(data);
            ULONG count = (data.IsConstant() || attr.IsConstant()) ? 1 : data.GetCount();
            vec.resize((size_t)count);
            if(count > 0)
            {
               bool isConstant = true;
               float firstVal = data[0];
               for(ULONG i=0;i<count;i++)
               {
                  vec[i] = data[i];
                  if(isConstant)
                     isConstant = firstVal == data[i];
               }
               if(isConstant)
                  vec.resize(1);
            }
         }
      }
      Abc::FloatArraySample sample(vec);
      mMassProperty.set(sample);
   }

   // shapetype
   bool usesInstances = false;
   {
      ICEAttribute attr = geo.GetICEAttributeFromName(L"Shape");
      std::vector<Abc::uint16_t> vec;
      if(attr.IsDefined() && attr.IsValid() && attr.GetElementCount() > 0)
      {
         {
            CICEAttributeDataArrayShape data;
            attr.GetDataArray(data);
            ULONG count = (data.IsConstant() || attr.IsConstant()) ? 1 : data.GetCount();
            vec.resize((size_t)count);
            if(count > 0)
            {
               bool isConstant = true;
               CShape firstVal = data[0];
               for(ULONG i=0;i<count;i++)
               {
                  CShape shape = data[i];
                  switch(shape.GetType())
                  {
                     case siICEShapeBox:
                     {
                        vec[i] = ShapeType_Box;
                        break;
                     }
                     case siICEShapeCylinder:
                     {
                        vec[i] = ShapeType_Cylinder;
                        break;
                     }
                     case siICEShapeCone:
                     {
                        vec[i] = ShapeType_Cone;
                        break;
                     }
                     case siICEShapeDisc:
                     {
                        vec[i] = ShapeType_Disc;
                        break;
                     }
                     case siICEShapeRectangle:
                     {
                        vec[i] = ShapeType_Rectangle;
                        break;
                     }
                     case siICEShapeSphere:
                     {
                        vec[i] = ShapeType_Sphere;
                        break;
                     }
                     case siICEShapeInstance:
                     case siICEShapeReference:
                     {
                        vec[i] = ShapeType_Instance;
                        usesInstances = true;
                        break;
                     }
                     default:
                     {
                        vec[i] = ShapeType_Point;
                        break;
                     }
                  }
                  if(isConstant)
                  {
                     isConstant = firstVal.GetType() == shape.GetType();
                     if(isConstant && (firstVal.GetType() == siICEShapeInstance || firstVal.GetType() == siICEShapeReference))
                        isConstant = firstVal.GetReferenceID() == shape.GetReferenceID();
                  }
               }
               if(isConstant)
                  vec.resize(1);
            }
         }

      }
      Abc::UInt16ArraySample sample(vec);
      mShapeTypeProperty.set(sample);
   }

   // shapeInstanceID
   {
      ICEAttribute attr = geo.GetICEAttributeFromName(L"Shape");
      std::vector<Abc::uint16_t> vec;
      if(attr.IsDefined() && attr.IsValid())
      {
         if(attr.GetElementCount() > 0)
         {
            {
               std::map<unsigned long,size_t>::iterator it;

               CICEAttributeDataArrayShape data;
               attr.GetDataArray(data);
               ULONG count = (data.IsConstant() || attr.IsConstant()) ? 1 : data.GetCount();
               vec.resize((size_t)count);
               if(count > 0)
               {
                  bool isConstant = true;
                  Abc::uint16_t firstVal;
                  for(ULONG i=0;i<count;i++)
                  {
                     CShape shape = data[i];
                     switch(shape.GetType())
                     {
                        case siICEShapeInstance:
                        case siICEShapeReference:
                        {
                           // check if we know this instance
                           it = mInstanceMap.find(shape.GetReferenceID());
                           if(it == mInstanceMap.end())
                           {
                              // insert it
                              CRef ref = Application().GetObjectFromID(shape.GetReferenceID());
                              mInstanceMap.insert(std::pair<unsigned long,size_t>(shape.GetReferenceID(),mInstanceNames.size()));
                              vec[i] = (unsigned short)mInstanceNames.size();
                              mInstanceNames.push_back(getIdentifierFromRef(ref));
                           }
                           else
                              vec[i] = (unsigned short)it->second;
                           break;
                        }
                        default:
                        {
                           vec[i] = 0;
                           break;
                        }
                     }
                     if(i==0)
                        firstVal = vec[i];
                     else if(isConstant)
                        isConstant = firstVal == vec[i];
                  }
                  if(isConstant)
                     vec.resize(1);
               }
            }

            if(mInstanceNames.size() > 0)
            {
               if(!mInstancenamesProperty)
				   mInstancenamesProperty = Abc::OStringArrayProperty(mPointsSchema.getArbGeomParams(), "instancenames", mPointsSchema.getMetaData(), GetJob()->GetAnimatedTs() );

               std::vector<std::string> preRollVec(1,"");
               Abc::StringArraySample preRollSample(preRollVec);
               for(size_t i=mInstancenamesProperty.getNumSamples();i<mNumSamples;i++)
                  mInstancenamesProperty.set(preRollSample);


               Abc::StringArraySample sample(mInstanceNames);
               mInstancenamesProperty.set(sample);

               if(vec.size() > 0)
               {
                  if(!mShapeInstanceIDProperty)
					  mShapeInstanceIDProperty = Abc::OUInt16ArrayProperty(mPointsSchema.getArbGeomParams(), "shapeinstanceid", mPointsSchema.getMetaData(), GetJob()->GetAnimatedTs() );

                  std::vector<Abc::uint16_t> preRollVec(1,0);
                  Abc::UInt16ArraySample preRollSample(preRollVec);
                  for(size_t i=mShapeInstanceIDProperty.getNumSamples();i<mNumSamples;i++)
                     mShapeInstanceIDProperty.set(preRollSample);

                  Abc::UInt16ArraySample sample(vec);
                  mShapeInstanceIDProperty.set(sample);
               }
            }
         }
      }
   }

   // shapetime
   {
      ICEAttribute attr = geo.GetICEAttributeFromName(L"ShapeInstancetime");
      std::vector<float> vec;
      if(attr.IsDefined() && attr.IsValid())
      {
         if(mNumSamples == 0)
            mShapeTimeProperty = Abc::OFloatArrayProperty(mPointsSchema.getArbGeomParams(), "shapetime", mPointsSchema.getMetaData(), GetJob()->GetAnimatedTs() );

         if(attr.GetElementCount() > 0)
         {
            {
               CICEAttributeDataArrayFloat data;
               attr.GetDataArray(data);
               ULONG count = (data.IsConstant() || attr.IsConstant()) ? 1 : data.GetCount();
               vec.resize((size_t)count);
               if(count > 0)
               {
                  bool isConstant = true;
                  float firstVal = data[0];
                  for(ULONG i=0;i<count;i++)
                  {
                     vec[i] = data[i];
                     if(isConstant)
                        isConstant = firstVal == data[i];
                  }
                  if(isConstant)
                     vec.resize(1);
               }
            }
         }
         Abc::FloatArraySample sample(vec);
         mShapeTimeProperty.set(sample);
      }
   }

   // color
   {
      ICEAttribute attr = geo.GetICEAttributeFromName(L"Color");
      std::vector<Abc::C4f> vec;
      if(attr.IsDefined() && attr.IsValid() && attr.GetElementCount() > 0)
      {
         {
            CICEAttributeDataArrayColor4f data;
            attr.GetDataArray(data);
            ULONG count = (data.IsConstant() || attr.IsConstant()) ? 1 : data.GetCount();
            vec.resize((size_t)count);
            if(count > 0)
            {
               bool isConstant = true;
               Abc::C4f firstVal;
               for(ULONG i=0;i<count;i++)
               {
                  vec[i].r = data[i].GetR();
                  vec[i].g = data[i].GetG();
                  vec[i].b = data[i].GetB();
                  vec[i].a = data[i].GetA();
                  if(i==0)
                     firstVal = vec[i];
                  else if(isConstant)
                     isConstant = firstVal == vec[i];
               }
               if(isConstant)
                  vec.resize(1);
            }
         }
      }
      Abc::C4fArraySample sample(vec);
      mColorProperty.set(sample);
   }

   mNumSamples++;

   return CStatus::OK;
}

enum IDs
{
	ID_IN_path = 0,
	ID_IN_identifier = 1,
	ID_IN_renderpath = 2,
	ID_IN_renderidentifier = 3,
	ID_IN_time = 4,
	ID_IN_usevel = 5,
    ID_IN_multifile = 6,
	ID_G_100 = 100,
	ID_OUT_position = 201,
	ID_OUT_velocity = 202,
	ID_OUT_id = 204,
	ID_OUT_size = 203,
	ID_OUT_scale = 205,
	ID_OUT_orientation = 206,
	ID_OUT_angularvelocity = 207,
	ID_OUT_age = 208,
	ID_OUT_mass = 209,
	ID_OUT_shape = 210,
	ID_OUT_shapeid = 211,
	ID_OUT_color = 212,
	ID_OUT_shapetime = 213,
	ID_TYPE_CNS = 400,
	ID_STRUCT_CNS,
	ID_CTXT_CNS,
};

#define ID_UNDEF ((ULONG)-1)

using namespace XSI;
using namespace MATH;

CStatus Register_alembic_points( PluginRegistrar& in_reg )
{
	ICENodeDef nodeDef;
	nodeDef = Application().GetFactory().CreateICENodeDef(L"alembic_points",L"alembic_points");

	CStatus st;
	st = nodeDef.PutColor(255,188,102);
	st.AssertSucceeded( ) ;

   st = nodeDef.PutThreadingModel(XSI::siICENodeSingleThreading);
	st.AssertSucceeded( ) ;

	// Add input ports and groups.
	st = nodeDef.AddPortGroup(ID_G_100);
	st.AssertSucceeded( ) ;

   st = nodeDef.AddInputPort(ID_IN_path,ID_G_100,siICENodeDataString,siICENodeStructureSingle,siICENodeContextSingleton,L"path",L"path",L"",ID_UNDEF,ID_UNDEF,ID_UNDEF);
	st.AssertSucceeded( ) ;
   st = nodeDef.AddInputPort(ID_IN_multifile,ID_G_100,siICENodeDataBool,siICENodeStructureSingle,siICENodeContextSingleton,L"multifile",L"multifile",L"",ID_UNDEF,ID_UNDEF,ID_UNDEF);
	st.AssertSucceeded( ) ;
    st = nodeDef.AddInputPort(ID_IN_identifier,ID_G_100,siICENodeDataString,siICENodeStructureSingle,siICENodeContextSingleton,L"identifier",L"identifier",L"",ID_UNDEF,ID_UNDEF,ID_UNDEF);
	st.AssertSucceeded( ) ;
   st = nodeDef.AddInputPort(ID_IN_renderpath,ID_G_100,siICENodeDataString,siICENodeStructureSingle,siICENodeContextSingleton,L"renderpath",L"renderpath",L"",ID_UNDEF,ID_UNDEF,ID_UNDEF);
	st.AssertSucceeded( ) ;
   st = nodeDef.AddInputPort(ID_IN_renderidentifier,ID_G_100,siICENodeDataString,siICENodeStructureSingle,siICENodeContextSingleton,L"renderidentifier",L"renderidentifier",L"",ID_UNDEF,ID_UNDEF,ID_UNDEF);
	st.AssertSucceeded( ) ;
   st = nodeDef.AddInputPort(ID_IN_time,ID_G_100,siICENodeDataFloat,siICENodeStructureSingle,siICENodeContextSingleton,L"time",L"time",0.0f,ID_UNDEF,ID_UNDEF,ID_UNDEF);
	st.AssertSucceeded( ) ;
   st = nodeDef.AddInputPort(ID_IN_usevel,ID_G_100,siICENodeDataBool,siICENodeStructureSingle,siICENodeContextSingleton,L"usevel",L"usevel",false,ID_UNDEF,ID_UNDEF,ID_UNDEF);
	st.AssertSucceeded( ) ;
	
   // Add output ports.
   st = nodeDef.AddOutputPort(ID_OUT_position,siICENodeDataVector3,siICENodeStructureArray,siICENodeContextSingleton,L"position",L"position",ID_UNDEF,ID_UNDEF,ID_UNDEF);
	st.AssertSucceeded( ) ;
   st = nodeDef.AddOutputPort(ID_OUT_velocity,siICENodeDataVector3,siICENodeStructureArray,siICENodeContextSingleton,L"velocity",L"velocity",ID_UNDEF,ID_UNDEF,ID_UNDEF);
	st.AssertSucceeded( ) ;
   st = nodeDef.AddOutputPort(ID_OUT_id,siICENodeDataLong,siICENodeStructureArray,siICENodeContextSingleton,L"id",L"id",ID_UNDEF,ID_UNDEF,ID_UNDEF);
	st.AssertSucceeded( ) ;
   st = nodeDef.AddOutputPort(ID_OUT_size,siICENodeDataFloat,siICENodeStructureArray,siICENodeContextSingleton,L"size",L"size",ID_UNDEF,ID_UNDEF,ID_UNDEF);
	st.AssertSucceeded( ) ;
   st = nodeDef.AddOutputPort(ID_OUT_scale,siICENodeDataVector3,siICENodeStructureArray,siICENodeContextSingleton,L"scale",L"scale",ID_UNDEF,ID_UNDEF,ID_UNDEF);
	st.AssertSucceeded( ) ;
   st = nodeDef.AddOutputPort(ID_OUT_orientation,siICENodeDataRotation,siICENodeStructureArray,siICENodeContextSingleton,L"orientation",L"orientation",ID_UNDEF,ID_UNDEF,ID_UNDEF);
	st.AssertSucceeded( ) ;
   st = nodeDef.AddOutputPort(ID_OUT_angularvelocity,siICENodeDataRotation,siICENodeStructureArray,siICENodeContextSingleton,L"angularvelocity",L"angularvelocity",ID_UNDEF,ID_UNDEF,ID_UNDEF);
	st.AssertSucceeded( ) ;
   st = nodeDef.AddOutputPort(ID_OUT_age,siICENodeDataFloat,siICENodeStructureArray,siICENodeContextSingleton,L"age",L"age",ID_UNDEF,ID_UNDEF,ID_UNDEF);
	st.AssertSucceeded( ) ;
   st = nodeDef.AddOutputPort(ID_OUT_mass,siICENodeDataFloat,siICENodeStructureArray,siICENodeContextSingleton,L"mass",L"mass",ID_UNDEF,ID_UNDEF,ID_UNDEF);
	st.AssertSucceeded( ) ;
   st = nodeDef.AddOutputPort(ID_OUT_shape,siICENodeDataShape,siICENodeStructureArray,siICENodeContextSingleton,L"shape",L"shape",ID_UNDEF,ID_UNDEF,ID_UNDEF);
	st.AssertSucceeded( ) ;
   st = nodeDef.AddOutputPort(ID_OUT_shapeid,siICENodeDataLong,siICENodeStructureArray,siICENodeContextSingleton,L"shapeid",L"shapeid",ID_UNDEF,ID_UNDEF,ID_UNDEF);
	st.AssertSucceeded( ) ;
   st = nodeDef.AddOutputPort(ID_OUT_shapetime,siICENodeDataFloat,siICENodeStructureArray,siICENodeContextSingleton,L"shapetime",L"shapetime",ID_UNDEF,ID_UNDEF,ID_UNDEF);
	st.AssertSucceeded( ) ;
   st = nodeDef.AddOutputPort(ID_OUT_color,siICENodeDataColor4,siICENodeStructureArray,siICENodeContextSingleton,L"color",L"color",ID_UNDEF,ID_UNDEF,ID_UNDEF);
	st.AssertSucceeded( ) ;

	PluginItem nodeItem = in_reg.RegisterICENode(nodeDef);
	nodeItem.PutCategories(L"Alembic");

	return CStatus::OK;
}


XSIPLUGINCALLBACK CStatus alembic_points_Evaluate(ICENodeContext& in_ctxt)
{
	// The current output port being evaluated...
	ULONG out_portID = in_ctxt.GetEvaluatedOutputPortID( );

	CDataArrayString pathData( in_ctxt, ID_IN_path );
   CString path = pathData[0];
	CDataArrayString identifierData( in_ctxt, ID_IN_identifier );
   CString identifier = identifierData[0];
	CDataArrayFloat timeData( in_ctxt, ID_IN_time);
   double time = timeData[0];
   CDataArrayBool multifileData( in_ctxt, ID_IN_multifile);
   const bool bMultifile = multifileData[0];

   alembicOp_Init( in_ctxt );
   alembicOp_Multifile( in_ctxt, bMultifile, time, path);
   CStatus pathEditStat = alembicOp_PathEdit( in_ctxt, path );

  AbcG::IObject iObj = getObjectFromArchive(path,identifier);
   if(!iObj.valid())
      return CStatus::OK;
  AbcG::IPoints obj(iObj,Abc::kWrapExisting);
   if(!obj.valid())
      return CStatus::OK;

	CDataArrayBool usevelData( in_ctxt, ID_IN_usevel);
   double usevel = usevelData[0];

   AbcA::TimeSamplingPtr timeSampling = obj.getSchema().getTimeSampling();

   SampleInfo sampleInfo = getSampleInfo(
      time,
      obj.getSchema().getTimeSampling(),
      obj.getSchema().getNumSamples()
   );
  AbcG::IPointsSchema::Sample sample;
   obj.getSchema().get(sample,sampleInfo.floorIndex);

   int pointCount = 0;
   {
		Abc::P3fArraySamplePtr ptr = sample.getPositions();
		if(ptr != NULL) {
			pointCount = (int) ptr->size();
		}
       
   }

	switch( out_portID )
	{
      case ID_OUT_position:
		{
			// Get the output port array ...
         CDataArray2DVector3f outData( in_ctxt );
         CDataArray2DVector3f::Accessor acc;

         Abc::P3fArraySamplePtr ptr = sample.getPositions();
         if(ptr == NULL)
            acc = outData.Resize(0,0);
         else if(ptr->size() == 0)
            acc = outData.Resize(0,0);
         else if(ptr->size() == 1 && ptr->get()[0].x == FLT_MAX)
            acc = outData.Resize(0,0);
         else
         {
            acc = outData.Resize(0,(ULONG)ptr->size());
            bool done = false;
            if(sampleInfo.alpha != 0.0)
            {   
               
               if(usevel)
               {
                  Abc::V3fArraySamplePtr velPtr = sample.getVelocities();
                  if(velPtr == NULL)
                     done = false;
                  else if(velPtr->size() == 0)
                     done = false;
                  else
                  {
                     const float alpha = getTimeOffsetFromObject( iObj, sampleInfo );
                     for(ULONG i=0;i<acc.GetCount();i++)
                        acc[i].Set(
                           ptr->get()[i].x + alpha * velPtr->get()[i >= velPtr->size() ? 0 : i].x,
                           ptr->get()[i].y + alpha * velPtr->get()[i >= velPtr->size() ? 0 : i].y,
                           ptr->get()[i].z + alpha * velPtr->get()[i >= velPtr->size() ? 0 : i].z);
                     done = true;
                  }
               }
               else if(obj.getSchema().getIdsProperty().isConstant())
               {                  
                  Abc::IP3fArrayProperty posProp2 = obj.getSchema().getPositionsProperty();
                  Abc::P3fArraySamplePtr pointsCeilSamplePtr;
                  posProp2.get(pointsCeilSamplePtr, sampleInfo.ceilIndex);

                  if(ptr->size() == pointsCeilSamplePtr->size()){
                     const float alpha = (float) sampleInfo.alpha;
                     for(ULONG i=0;i<acc.GetCount();i++)
                        acc[i].Set(
                           (1.0f-alpha) * ptr->get()[i].x + alpha * pointsCeilSamplePtr->get()[i].x,
                           (1.0f-alpha) * ptr->get()[i].y + alpha * pointsCeilSamplePtr->get()[i].y,
                           (1.0f-alpha) * ptr->get()[i].z + alpha * pointsCeilSamplePtr->get()[i].z); 
                     done = true;
                  }
               }
            }

            if(!done)
            {
               for(ULONG i=0;i<acc.GetCount();i++)
                  acc[i].Set(ptr->get()[i].x,ptr->get()[i].y,ptr->get()[i].z);
            }
         }
   		break;
		}
      case ID_OUT_velocity:
		{
			// Get the output port array ...
         CDataArray2DVector3f outData( in_ctxt );
         CDataArray2DVector3f::Accessor acc;

         Abc::V3fArraySamplePtr ptr = sample.getVelocities();
		 if(ptr == NULL || ptr->size() == 0) {
           	acc = outData.Resize( 0, pointCount );
			for(ULONG i=0;i<acc.GetCount();i++)
            {
				acc[i].Set( 0, 0, 0 );
			}
		 }
         else
         {
            acc = outData.Resize(0,(ULONG)ptr->size());
            for(ULONG i=0;i<acc.GetCount();i++)
               acc[i].Set(ptr->get()[i].x,ptr->get()[i].y,ptr->get()[i].z);
         }
   		break;
		}
      case ID_OUT_id:
		{
			// Get the output port array ...
         CDataArray2DLong outData( in_ctxt );
         CDataArray2DLong::Accessor acc;

         Abc::UInt64ArraySamplePtr ptr = sample.getIds();
		 if(ptr == NULL || ptr->size() == 0) {
           	acc = outData.Resize( 0, pointCount );
			for(ULONG i=0;i<acc.GetCount();i++)
            {
				acc[i] = i;
			}
 		 }
         else
         {
            acc = outData.Resize(0,(ULONG)ptr->size());
            for(ULONG i=0;i<acc.GetCount();i++)
               acc[i] = (LONG)ptr->get()[i];
         }
   		break;
		}
      case ID_OUT_size:
		{
			// Get the output port array ...
         CDataArray2DFloat outData( in_ctxt );
         CDataArray2DFloat::Accessor acc;

        AbcG::IFloatGeomParam widthParam = obj.getSchema().getWidthsParam();
         if(!widthParam || widthParam.getNumSamples() == 0)
         {
            acc = outData.Resize( 0, pointCount );
			for(ULONG i=0;i<acc.GetCount();i++)
            {
				acc[i] = 1.0f;
			}
			return CStatus::OK;
         }         

         Abc::FloatArraySamplePtr ptr = widthParam.getExpandedValue(sampleInfo.floorIndex).getVals();
		 if(ptr == NULL || ptr->size() == 0) {
            acc = outData.Resize( 0, pointCount );
			for(ULONG i=0;i<acc.GetCount();i++)
            {
				acc[i] = 1.0f;
			}
		}
         else
         {
            acc = outData.Resize(0,(ULONG)ptr->size());
            for(ULONG i=0;i<acc.GetCount();i++)
               acc[i] = ptr->get()[i];
         }
   		break;
		}
      case ID_OUT_scale:
		{
			// Get the output port array ...
         CDataArray2DVector3f outData( in_ctxt );
         CDataArray2DVector3f::Accessor acc;

		 Abc::IV3fArrayProperty prop;
		 if( ! getArbGeomParamPropertyAlembic( obj, "scale", prop ) ) {

			acc = outData.Resize( 0, pointCount );
			for(ULONG i=0;i<acc.GetCount();i++)
            {
				acc[i].Set( 1.0f, 1.0f, 1.0f );
			}
            return CStatus::OK;
         }

		 /*
         if ( obj.getSchema().getPropertyHeader( ".scale" ) == NULL )
         {
            acc = outData.Resize(0,0);
            return CStatus::OK;
         }
         IV3fArrayProperty prop = Abc::IV3fArrayProperty( obj.getSchema(), ".scale" );
         if(!prop.valid())
         {
            acc = outData.Resize(0,0);
            return CStatus::OK;
         }
         if(prop.getNumSamples() == 0)
         {
            acc = outData.Resize(0,0);
            return CStatus::OK;
         }*/
         Abc::V3fArraySamplePtr ptr = prop.getValue(sampleInfo.floorIndex);
		 if(ptr == NULL || ptr->size() == 0) {
             acc = outData.Resize( 0, pointCount );
			for(ULONG i=0;i<acc.GetCount();i++)
            {
				acc[i].Set( 1.0f, 1.0f, 1.0f );
			}
		  }
         else
         {
            acc = outData.Resize(0,(ULONG)ptr->size());
            for(ULONG i=0;i<acc.GetCount();i++)
               acc[i].Set(ptr->get()[i].x,ptr->get()[i].y,ptr->get()[i].z);
         }
   		break;
		}
      case ID_OUT_orientation:
		{
			// Get the output port array ...
         CDataArray2DRotationf outData( in_ctxt );
         CDataArray2DRotationf::Accessor acc;

         Abc::IQuatfArrayProperty prop;
		 if( ! getArbGeomParamPropertyAlembic( obj, "orientation", prop ) ) {
             acc = outData.Resize( 0, pointCount );
			 CQuaternionf identityQuat;
			for(ULONG i=0;i<acc.GetCount();i++)
            {
				acc[i].Set( identityQuat );
			}
            return CStatus::OK;
         }
		 /*
		 if ( obj.getSchema().getPropertyHeader( ".orientation" ) == NULL )
         {
            acc = outData.Resize(0,0);
            return CStatus::OK;
         }
         IQuatfArrayProperty prop = Abc::IQuatfArrayProperty( obj.getSchema(), ".orientation" );
         if(!prop.valid())
         {
            acc = outData.Resize(0,0);
            return CStatus::OK;
         }
         if(prop.getNumSamples() == 0)
         {
            acc = outData.Resize(0,0);
            return CStatus::OK;
         }*/
         Abc::QuatfArraySamplePtr ptr = prop.getValue(sampleInfo.floorIndex);
		 if(ptr == NULL || ptr->size() == 0) {
		     acc = outData.Resize( 0, pointCount );
			CRotationf identityQuat = CQuaternionf();
			for(ULONG i=0;i<acc.GetCount();i++)
            {
				acc[i] = identityQuat;
			}
		}
         else
         {
            acc = outData.Resize(0,(ULONG)ptr->size());
            bool done = false;
            if(sampleInfo.alpha != 0.0 && usevel)
            {
               float alpha = (float)sampleInfo.alpha;

	            Abc::IQuatfArrayProperty velProp;
				 if( getArbGeomParamPropertyAlembic( obj, "angularvelocity", velProp ) ) {
					
				   Abc::QuatfArraySamplePtr velPtr = velProp.getValue(sampleInfo.floorIndex);
				   if( ! ( velPtr == NULL || velPtr->size() == 0) ) {
					  CQuaternionf quat,vel;
					  for(ULONG i=0;i<acc.GetCount();i++)
					  {
						 quat.Set(ptr->get()[i].r,ptr->get()[i].v.x,ptr->get()[i].v.y,ptr->get()[i].v.z);
						 vel.Set(
							velPtr->get()[i >= velPtr->size() ? 0 : i].r,
							velPtr->get()[i >= velPtr->size() ? 0 : i].v.x,
							velPtr->get()[i >= velPtr->size() ? 0 : i].v.y,
							velPtr->get()[i >= velPtr->size() ? 0 : i].v.z);
						 vel.PutW(vel.GetW() * alpha);
						 vel.PutX(vel.GetX() * alpha);
						 vel.PutY(vel.GetY() * alpha);
						 vel.PutZ(vel.GetZ() * alpha);
						 if(vel.GetW() != 0.0f)
							quat.Mul(vel,quat);
						 quat.NormalizeInPlace();
						 acc[i].Set(quat);
					  }
					  done = true;
				   }
				}
            }

            if(!done)
            {
               CQuaternionf quat;
               for(ULONG i=0;i<acc.GetCount();i++)
               {
                  quat.Set(ptr->get()[i].r,ptr->get()[i].v.x,ptr->get()[i].v.y,ptr->get()[i].v.z);
                  acc[i].Set(quat);
               }
            }
         }
   		break;
		}
      case ID_OUT_angularvelocity:
		{
			// Get the output port array ...
         CDataArray2DRotationf outData( in_ctxt );
         CDataArray2DRotationf::Accessor acc;

         Abc::IQuatfArrayProperty prop;
		 if( ! getArbGeomParamPropertyAlembic( obj, "angularvelocity", prop ) ) {
		    acc = outData.Resize( 0, pointCount );
			CRotationf identityQuat = CQuaternionf();
			for(ULONG i=0;i<acc.GetCount();i++)
            {
				acc[i] = identityQuat;
			}

			return CStatus::OK;
		 }
		 /*
		 if ( obj.getSchema().getPropertyHeader( ".angularvelocity" ) == NULL )
         {
            acc = outData.Resize(0,0);
            return CStatus::OK;
         }
         IQuatfArrayProperty prop = Abc::IQuatfArrayProperty( obj.getSchema(), ".angularvelocity" );
         if(!prop.valid())
         {
            acc = outData.Resize(0,0);
            return CStatus::OK;
         }
         if(prop.getNumSamples() == 0)
         {
            acc = outData.Resize(0,0);
            return CStatus::OK;
         }*/
         Abc::QuatfArraySamplePtr ptr = prop.getValue(sampleInfo.floorIndex);
		 if(ptr == NULL || ptr->size() == 0) {
             acc = outData.Resize( 0, pointCount );
			CRotationf identityQuat = CQuaternionf();
			for(ULONG i=0;i<acc.GetCount();i++)
            {
				acc[i] = identityQuat;
			}
		}
         else
         {
            acc = outData.Resize(0,(ULONG)ptr->size());
			CQuaternionf quat;
            for(ULONG i=0;i<acc.GetCount();i++)
            {
               quat.Set(ptr->get()[i].r,ptr->get()[i].v.z,ptr->get()[i].v.y,ptr->get()[i].v.z);
               acc[i].Set(quat);
            }
         }
   		break;
		}
      case ID_OUT_age:
		{
			// Get the output port array ...
         CDataArray2DFloat outData( in_ctxt );
         CDataArray2DFloat ::Accessor acc;

         Abc::IFloatArrayProperty prop;
		 if( ! getArbGeomParamPropertyAlembic( obj, "age", prop ) ) {
     	     acc = outData.Resize( 0, pointCount );
			for(ULONG i=0;i<acc.GetCount();i++)
            {
				acc[i] = 0;
			}
			return CStatus::OK;
		 }
         /*if ( obj.getSchema().getPropertyHeader( ".age" ) == NULL )
         {
            acc = outData.Resize(0,0);
            return CStatus::OK;
         }
         IFloatArrayProperty prop = Abc::IFloatArrayProperty( obj.getSchema(), ".age" );
         if(!prop.valid())
         {
            acc = outData.Resize(0,0);
            return CStatus::OK;
         }
         if(prop.getNumSamples() == 0)
         {
            acc = outData.Resize(0,0);
            return CStatus::OK;
         }*/
         Abc::FloatArraySamplePtr ptr = prop.getValue(sampleInfo.floorIndex);
		 if(ptr == NULL || ptr->size() == 0) {
             acc = outData.Resize( 0, pointCount );
			for(ULONG i=0;i<acc.GetCount();i++)
            {
				acc[i] = 0;
			}
		}
         else
         {
            acc = outData.Resize(0,(ULONG)ptr->size());
            for(ULONG i=0;i<acc.GetCount();i++)
            {
               acc[i] = ptr->get()[i];
            }
         }
   		break;
		}
      case ID_OUT_mass:
		{
			// Get the output port array ...
         CDataArray2DFloat outData( in_ctxt );
         CDataArray2DFloat ::Accessor acc;

         Abc::IFloatArrayProperty prop;
		 if( ! getArbGeomParamPropertyAlembic( obj, "mass", prop ) ) {
		    acc = outData.Resize(0,0);
			return CStatus::OK;
		 }
         /*if ( obj.getSchema().getPropertyHeader( ".mass" ) == NULL )
         {
            acc = outData.Resize(0,0);
            return CStatus::OK;
         }
         IFloatArrayProperty prop = Abc::IFloatArrayProperty( obj.getSchema(), ".mass" );
         if(!prop.valid())
         {
            acc = outData.Resize(0,0);
            return CStatus::OK;
         }
         if(prop.getNumSamples() == 0)
         {
            acc = outData.Resize(0,0);
            return CStatus::OK;
         }*/
         Abc::FloatArraySamplePtr ptr = prop.getValue(sampleInfo.floorIndex);
         if(ptr == NULL)
            acc = outData.Resize(0,0);
         else if(ptr->size() == 0)
            acc = outData.Resize(0,0);
         else
         {
            acc = outData.Resize(0,(ULONG)ptr->size());
            for(ULONG i=0;i<acc.GetCount();i++)
            {
               acc[i] = ptr->get()[i];
            }
         }
   		break;
		}
      case ID_OUT_shape:
		{
			// Get the output port array ...
         CDataArray2DShape outData( in_ctxt );
         CDataArray2DShape::Accessor acc;

         Abc::IUInt16ArrayProperty shapeTypeProp;
		 if( ! getArbGeomParamPropertyAlembic( obj, "shapetype", shapeTypeProp ) ) {
			acc = outData.Resize(0,(ULONG)sample.getPositions()->size());
			for(ULONG i=0;i<acc.GetCount();i++) {
				acc[i] = CShape(siICEShapePoint);
			}
		    //acc = outData.Resize(0,0);
			return CStatus::OK;
		 }
         /*if ( obj.getSchema().getPropertyHeader( ".shapetype" ) == NULL )
         {
            acc = outData.Resize(0,0);
            return CStatus::OK;
         }
         IUInt16ArrayProperty shapeTypeProp = Abc::IUInt16ArrayProperty( obj.getSchema(), ".shapetype" );
         /*
         IUInt16ArrayProperty shapeInstanceIDProp = Abc::IUInt16ArrayProperty( obj.getSchema(), ".shapeinstanceid" );
         IStringArrayProperty shapeInstanceNamesProp = Abc::IStringArrayProperty( obj.getSchema(), ".instancenames" );
         Abc::UInt16ArraySamplePtr shapeInstanceIDPtr = shapeInstanceIDProp.getValue(sampleInfo.floorIndex);
         Abc::StringArraySamplePtr shapeInstanceNamesPtr = shapeInstanceNamesProp.getValue(sampleInfo.floorIndex);
         * /
         if(!shapeTypeProp.valid())
         {
            acc = outData.Resize(0,0);
            return CStatus::OK;
         }
         if(shapeTypeProp.getNumSamples() == 0)
         {
            acc = outData.Resize(0,0);
            return CStatus::OK;
         }*/
         Abc::UInt16ArraySamplePtr shapeTypePtr = shapeTypeProp.getValue(sampleInfo.floorIndex);
         if(shapeTypePtr == NULL)
            acc = outData.Resize(0,0);
         else if(shapeTypePtr->size() == 0)
            acc = outData.Resize(0,0);
         else
         {
            acc = outData.Resize(0,(ULONG)shapeTypePtr->size());
            for(ULONG i=0;i<acc.GetCount();i++)
            {
               switch(shapeTypePtr->get()[i])
               {
                  case AlembicPoints::ShapeType_Point:
                  {
                     acc[i] = CShape(siICEShapePoint);
                     break;
                  }
                  case AlembicPoints::ShapeType_Box:
                  {
                     acc[i] = CShape(siICEShapeBox);
                     break;
                  }
                  case AlembicPoints::ShapeType_Sphere:
                  {
                     acc[i] = CShape(siICEShapeSphere);
                     break;
                  }
                  case AlembicPoints::ShapeType_Cylinder:
                  {
                     acc[i] = CShape(siICEShapeCylinder);
                     break;
                  }
                  case AlembicPoints::ShapeType_Cone:
                  {
                     acc[i] = CShape(siICEShapeCone);
                     break;
                  }
                  case AlembicPoints::ShapeType_Disc:
                  {
                     acc[i] = CShape(siICEShapeDisc);
                     break;
                  }
                  case AlembicPoints::ShapeType_Rectangle:
                  {
                     acc[i] = CShape(siICEShapeRectangle);
                     break;
                  }
                  case AlembicPoints::ShapeType_Instance:
                  {
                     acc[i] = CShape(siICEShapePoint);
                     break;
                  }
               }
            }
         }
   		break;
		}
      case ID_OUT_shapeid:
		{
			// Get the output port array ...
         CDataArray2DLong outData( in_ctxt );
         CDataArray2DLong::Accessor acc;

		 Abc::IUInt16ArrayProperty shapeTypeProp;
		 if( ! getArbGeomParamPropertyAlembic( obj, "shapetype", shapeTypeProp ) ) {
		    acc = outData.Resize(0,0);
			return CStatus::OK;
		 }
		 Abc::IUInt16ArrayProperty shapeInstanceIDProp;
		 if( ! getArbGeomParamPropertyAlembic( obj, "shapeinstanceid", shapeInstanceIDProp ) ) {
		    acc = outData.Resize(0,0);
			return CStatus::OK;
		 }

         /*if ( obj.getSchema().getPropertyHeader( ".shapetype" ) == NULL || obj.getSchema().getPropertyHeader( ".shapeinstanceid" ) == NULL )
         {
            acc = outData.Resize(0,0);
            return CStatus::OK;
         }
         IUInt16ArrayProperty shapeTypeProp = Abc::IUInt16ArrayProperty( obj.getSchema(), ".shapetype" );
         IUInt16ArrayProperty shapeInstanceIDProp = Abc::IUInt16ArrayProperty( obj.getSchema(), ".shapeinstanceid" );

         if(!shapeTypeProp.valid() || !shapeInstanceIDProp.valid())
         {
            acc = outData.Resize(0,0);
            return CStatus::OK;
         }
         if(shapeTypeProp.getNumSamples() == 0 || shapeInstanceIDProp.getNumSamples() == 0)
         {
            acc = outData.Resize(0,0);
            return CStatus::OK;
         }*/

         Abc::UInt16ArraySamplePtr shapeTypePtr = shapeTypeProp.getValue(sampleInfo.floorIndex);
         Abc::UInt16ArraySamplePtr shapeInstanceIDPtr = shapeInstanceIDProp.getValue(sampleInfo.floorIndex);
         if(shapeTypePtr == NULL || shapeInstanceIDPtr == NULL)
            acc = outData.Resize(0,0);
         else if(shapeTypePtr->size() == 0 || shapeInstanceIDPtr->size() == 0)
            acc = outData.Resize(0,0);
         else
         {
            acc = outData.Resize(0,(ULONG)shapeTypePtr->size());
            for(ULONG i=0;i<acc.GetCount();i++)
            {
               switch(shapeTypePtr->get()[i])
               {
                  case AlembicPoints::ShapeType_Instance:
                  {
                     acc[i] = (LONG)shapeInstanceIDPtr->get()[i];
                     break;
                  }
                  default:
                  {
                     // for all other shapes
                     acc[i] = -1l;
                     break;
                  }
               }
            }
         }
   		break;
		}
      case ID_OUT_shapetime:
		{
			// Get the output port array ...
         CDataArray2DFloat outData( in_ctxt );
         CDataArray2DFloat ::Accessor acc;

 		 Abc::IFloatArrayProperty prop;
		 if( ! getArbGeomParamPropertyAlembic( obj, "shapetime", prop ) ) {
		    acc = outData.Resize(0,0);
			return CStatus::OK;
		 }

		 /*if ( obj.getSchema().getPropertyHeader( ".shapetime" ) == NULL )
         {
            acc = outData.Resize(0,0);
            return CStatus::OK;
         }
         IFloatArrayProperty prop = Abc::IFloatArrayProperty( obj.getSchema(), ".shapetime" );
         if(!prop.valid())
         {
            acc = outData.Resize(0,0);
            return CStatus::OK;
         }
         if(prop.getNumSamples() == 0)
         {
            acc = outData.Resize(0,0);
            return CStatus::OK;
         }*/
         Abc::FloatArraySamplePtr ptr = prop.getValue(sampleInfo.floorIndex);
         if(ptr == NULL)
            acc = outData.Resize(0,0);
         else if(ptr->size() == 0)
            acc = outData.Resize(0,0);
         else
         {
            acc = outData.Resize(0,(ULONG)ptr->size());
            for(ULONG i=0;i<acc.GetCount();i++)
            {
               acc[i] = ptr->get()[i];
            }
         }
   		break;
		}
      case ID_OUT_color:
		{
			// Get the output port array ...
         CDataArray2DColor4f outData( in_ctxt );
         CDataArray2DColor4f::Accessor acc;

  		 Abc::IC4fArrayProperty prop;
		 if( ! getArbGeomParamPropertyAlembic( obj, "color", prop ) ) {
			acc = outData.Resize(0,(ULONG)sample.getPositions()->size());
			for(ULONG i=0;i<acc.GetCount();i++) {
				acc[i] = CColor4f(1.0f,1.0f,0.0f,1.0f);
			}
		    //acc = outData.Resize(0,0);
			return CStatus::OK;
		 }

		 /*if ( obj.getSchema().getPropertyHeader( ".color" ) == NULL )
         {
            acc = outData.Resize(0,0);
            return CStatus::OK;
         }
         IC4fArrayProperty prop = Abc::IC4fArrayProperty( obj.getSchema(), ".color" );
         if(!prop.valid())
         {
            acc = outData.Resize(0,0);
            return CStatus::OK;
         }
         if(prop.getNumSamples() == 0)
         {
            acc = outData.Resize(0,0);
            return CStatus::OK;
         }*/
         Abc::C4fArraySamplePtr ptr = prop.getValue(sampleInfo.floorIndex);
         if(ptr == NULL)
            acc = outData.Resize(0,0);
         else if(ptr->size() == 0)
            acc = outData.Resize(0,0);
         else
         {
            acc = outData.Resize(0,(ULONG)ptr->size());
            for(ULONG i=0;i<acc.GetCount();i++)
            {
               acc[i].Set(ptr->get()[i].r,ptr->get()[i].g,ptr->get()[i].b,ptr->get()[i].a);
            }
         }
   		break;
		}
	};

	return CStatus::OK;
}

XSIPLUGINCALLBACK CStatus alembic_points_Term(CRef& in_ctxt)
{
	return alembicOp_Term(in_ctxt);
}
