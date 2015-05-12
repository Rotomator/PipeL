#include "stdafx.h"
#include "AlembicPolyMsh.h"
#include "AlembicXform.h"

#include "CommonProfiler.h"
#include "CommonMeshUtilities.h"
#include "CommonSubtreeMerge.h"

//#include "Utility.h"

using namespace XSI;
using namespace MATH;

AlembicPolyMesh::AlembicPolyMesh(SceneNodePtr eNode, AlembicWriteJob * in_Job, Abc::OObject oParent)
: AlembicObject(eNode, in_Job, oParent)
{
   AbcG::OPolyMesh mesh(GetMyParent(), eNode->name, GetJob()->GetAnimatedTs());
   mMeshSchema = mesh.getSchema();

   Primitive prim(GetRef(REF_PRIMITIVE));
   Abc::OCompoundProperty argGeomParamsProp = mMeshSchema.getArbGeomParams();
   customAttributes.defineCustomAttributes(prim.GetGeometry(), argGeomParamsProp, mMeshSchema.getMetaData(), GetJob()->GetAnimatedTs());
}

AlembicPolyMesh::~AlembicPolyMesh()
{
}

Abc::OCompoundProperty AlembicPolyMesh::GetCompound()
{
   return mMeshSchema;
}

CVector3 V3fToVector3(const Abc::V3f& v){
   return CVector3(v.x, v.y, v.z);
}



//TODO: should probably just using stl-style options everywhere
void CopyOptions(std::map<XSI::CString,XSI::CValue>& options_in, CommonOptions& options_out)
{
   for( std::map<XSI::CString,XSI::CValue>::iterator it=options_in.begin(); it != options_in.end(); it++){
      options_out.SetOption(it->first.GetAsciiString(), (int)it->second);
   }
}


XSI::CStatus AlembicPolyMesh::Save(double time)
{
   const bool bEnableLogging = false;

   mMeshSample.reset();

   Primitive prim(GetRef(REF_PRIMITIVE));
   PolygonMesh mesh = prim.GetGeometry(time);

   // store the metadata
   SaveMetaData(GetRef(REF_NODE),this);

   // check if the mesh is animated
   //if(mNumSamples > 0) {
   //   if(!isRefAnimated(GetRef(REF_PRIMITIVE),false,globalSpace))
   //      return CStatus::OK;
   //}

   Imath::M44f transform44f;
   transform44f.makeIdentity();
   
   CommonOptions options;
   CopyOptions(GetJob()->mOptions, options);

   if(options.GetBoolOption("exportFaceSets") && mNumSamples != 0){//turn off faceset exports for all frames except the first
      options.SetOption("exportFaceSets", false);
   }
   if(options.GetBoolOption("exportBindPose") && mNumSamples != 0){
      options.SetOption("exportBindPose", false);
   }
   if(options.GetBoolOption("exportUVs") && mNumSamples == 0){
      options.SetOption("exportUVOptions", true);
   }

   //finalMesh.clear();
   IntermediatePolyMeshXSI finalMesh;

   if(mExoSceneNode->type == SceneNode::POLYMESH_SUBTREE){
      SceneNodePolyMeshSubtreePtr meshSubtreeNode = reinterpret<SceneNode, SceneNodePolyMeshSubtree>(mExoSceneNode);
      mergePolyMeshSubtreeNode<IntermediatePolyMeshXSI>(meshSubtreeNode, finalMesh, options, time);
   }
   else{
      //for now, custom attribute will ignore if meshes are being merged
      customAttributes.exportCustomAttributes(mesh);

      finalMesh.Save(mExoSceneNode, transform44f, options, time);
   }

   const bool globalSpace = GetJob()->GetOption(L"globalSpace");
   if(globalSpace){
      const Imath::M44f globalXfo = mExoSceneNode->getGlobalTransFloat(time);
   
      for(int i=0; i<finalMesh.posVec.size(); i++){
         finalMesh.posVec[i] *= globalXfo;
      }

      finalMesh.bbox.min *= globalXfo;
      finalMesh.bbox.max *= globalXfo;

      for(int i=0; i<finalMesh.mIndexedNormals.values.size(); i++){
         finalMesh.mIndexedNormals.values[i] *= globalXfo; 
      }

      for(int i=0; i<finalMesh.mVelocitiesVec.size(); i++){
         finalMesh.mVelocitiesVec[i] *= globalXfo;
      }
   }

   // store the positions && bbox
   mMeshSample.setPositions(Abc::P3fArraySample(finalMesh.posVec));
   mMeshSample.setSelfBounds(finalMesh.bbox);

   
   // abort here if we are just storing points
   bool purePointCache = (bool)GetJob()->GetOption(L"exportPurePointCache");
   if(purePointCache)
   {
      if(bEnableLogging) ESS_LOG_WARNING("Exporting pure point cache");
      if(mNumSamples == 0)
      {
         // store a dummy empty topology
         mMeshSample.setFaceCounts(Abc::Int32ArraySample(NULL, 0));
         mMeshSample.setFaceIndices(Abc::Int32ArraySample(NULL, 0));
      }

      mMeshSchema.set(mMeshSample);
      mNumSamples++;
      return CStatus::OK;
   }

   //if( !m_bDynamicTopologyMesh && mNumSamples > 0)
   //{
   //   if(mFaceCountVec.size() != faceCount || mFaceIndicesVec.size() != sampleCount){
   //      ESS_LOG_WARNING("Dynamic Topology Mesh detected");
   //      m_bDynamicTopologyMesh = true;
   //   }
   //}

   mMeshSample.setFaceCounts(Abc::Int32ArraySample(finalMesh.mFaceCountVec));
   mMeshSample.setFaceIndices(Abc::Int32ArraySample(finalMesh.mFaceIndicesVec));
   

   //these three variables must be scope when the schema set call occurs
   AbcG::ON3fGeomParam::Sample normalSample;
   std::vector<Abc::N3f> indexedNormals;
	std::vector<Abc::uint32_t> normalIndexVec;

   bool exportNormals = GetJob()->GetOption(L"exportNormals");
   if(exportNormals)
   {
      if(bEnableLogging) ESS_LOG_WARNING("Exporting normals");
      normalSample.setScope(AbcG::kFacevaryingScope);
      
      normalSample.setVals(Abc::N3fArraySample(finalMesh.mIndexedNormals.values));
      if(finalMesh.mIndexedNormals.indices.size() > 0){
         normalSample.setIndices(Abc::UInt32ArraySample(finalMesh.mIndexedNormals.indices));
      }
      mMeshSample.setNormals(normalSample);
   }

   if(finalMesh.mVelocitiesVec.size() > 0){
       if(bEnableLogging) ESS_LOG_WARNING("Exporting velocities");
       mMeshSample.setVelocities(Abc::V3fArraySample(finalMesh.mVelocitiesVec));
   }

	if((bool)GetJob()->GetOption(L"exportUVs"))
	{
      AbcG::OV2fGeomParam::Sample uvSample;
	   saveIndexedUVs( mMeshSchema, mMeshSample, uvSample, mUvParams, GetJob()->GetAnimatedTs(), mNumSamples, finalMesh.mIndexedUVSet );
      if(bEnableLogging) ESS_LOG_WARNING("Exporting UV data.");

      if(mNumSamples == 0 && finalMesh.mUvOptionsVec.size() > 0)
      {
         if(bEnableLogging) ESS_LOG_WARNING("Export UV options.");
         mUvOptionsProperty = Abc::OFloatArrayProperty(mMeshSchema, ".uvOptions", mMeshSchema.getMetaData(), GetJob()->GetAnimatedTs() );
         mUvOptionsProperty.set(Abc::FloatArraySample(finalMesh.mUvOptionsVec));
      }
	}

   // set the subd level
   if(!mFaceVaryingInterpolateBoundaryProperty){
      mFaceVaryingInterpolateBoundaryProperty =
         Abc::OInt32Property( mMeshSchema, ".faceVaryingInterpolateBoundary", mMeshSchema.getMetaData(), GetJob()->GetAnimatedTs() );
   }

   mFaceVaryingInterpolateBoundaryProperty.set( finalMesh.bGeomApprox );


   if(GetJob()->GetOption(L"exportFaceSets") && mNumSamples == 0)
   {
      if(bEnableLogging) ESS_LOG_WARNING("Exporting facesets");

      for(FaceSetMap::iterator it = finalMesh.mFaceSets.begin(); it != finalMesh.mFaceSets.end(); it++){
         std::string name = it->first;
         FaceSetStruct& faceSetStruct = it->second;
         if(faceSetStruct.faceIds.size() > 0)
         {
            if(bEnableLogging) ESS_LOG_WARNING("Exporting faceset "<<name);
            AbcG::OFaceSet faceSet = mMeshSchema.createFaceSet(name);
            AbcG::OFaceSetSchema::Sample faceSetSample(Abc::Int32ArraySample(faceSetStruct.faceIds));
            faceSet.getSchema().set(faceSetSample);
         }
      }
   }

   // check if we need to export the bindpose (also only for first frame)
   if(GetJob()->GetOption(L"exportBindPose") && finalMesh.mBindPoseVec.size() > 0 && mNumSamples == 0)
   {
      if(bEnableLogging) ESS_LOG_WARNING("Exporting BindPose");
      mBindPoseProperty = Abc::OV3fArrayProperty(mMeshSchema, ".bindpose", mMeshSchema.getMetaData(), GetJob()->GetAnimatedTs());
      Abc::V3fArraySample sample = Abc::V3fArraySample(finalMesh.mBindPoseVec);
      mBindPoseProperty.set(sample);
   }   
   

   mMeshSchema.set(mMeshSample);

   mNumSamples++;

   return CStatus::OK;
}

ESS_CALLBACK_START( alembic_polymesh_Define, CRef& )
   return alembicOp_Define(in_ctxt);
ESS_CALLBACK_END

ESS_CALLBACK_START( alembic_polymesh_DefineLayout, CRef& )
   return alembicOp_DefineLayout(in_ctxt); 
ESS_CALLBACK_END

ESS_CALLBACK_START( alembic_polymesh_Init, CRef& )
   return alembicOp_Init( in_ctxt );
ESS_CALLBACK_END


ESS_CALLBACK_START( alembic_polymesh_Update, CRef& )
   ESS_PROFILE_SCOPE("alembic_polymesh_Update");
   OperatorContext ctxt( in_ctxt );

   CString path = ctxt.GetParameterValue(L"path");

   alembicOp_Multifile( in_ctxt, ctxt.GetParameterValue(L"multifile"), ctxt.GetParameterValue(L"time"), path);
   CStatus pathEditStat = alembicOp_PathEdit( in_ctxt, path );

   if((bool)ctxt.GetParameterValue(L"muted") )
      return CStatus::OK;

   CString identifier = ctxt.GetParameterValue(L"identifier");

  AbcG::IObject iObj = getObjectFromArchive(path,identifier);
   if(!iObj.valid())
      return CStatus::OK;


  AbcG::IPolyMesh objMesh;
  AbcG::ISubD objSubD;
   if(AbcG::IPolyMesh::matches(iObj.getMetaData()))
      objMesh =AbcG::IPolyMesh(iObj,Abc::kWrapExisting);
   else
      objSubD =AbcG::ISubD(iObj,Abc::kWrapExisting);
   if(!objMesh.valid() && !objSubD.valid())
      return CStatus::OK;

   AbcA::TimeSamplingPtr timeSampling;
   int nSamples = 0;
   if(objMesh.valid())
   {
      timeSampling = objMesh.getSchema().getTimeSampling();
	  nSamples = (int) objMesh.getSchema().getNumSamples();
   }
   else
   {
      timeSampling = objSubD.getSchema().getTimeSampling();
	  nSamples = (int) objSubD.getSchema().getNumSamples();
   }


   //if(ctxt.GetParameterValue(L"multifile")){
   //   if( alembicOp_TimeSamplingInit(in_ctxt, timeSampling) == CStatus::Fail ){
   //      return CStatus::OK;
   //   }
   //}


   SampleInfo sampleInfo = getSampleInfo(
     ctxt.GetParameterValue(L"time"),
     timeSampling,
	 nSamples
   );

   Abc::P3fArraySamplePtr meshPos;
   if(objMesh.valid())
   {
     AbcG::IPolyMeshSchema::Sample sample;
      objMesh.getSchema().get(sample,sampleInfo.floorIndex);
      meshPos = sample.getPositions();
   }
   else
   {
     AbcG::ISubDSchema::Sample sample;
      objSubD.getSchema().get(sample,sampleInfo.floorIndex);
      meshPos = sample.getPositions();
   }

   PolygonMesh inMesh = Primitive((CRef)ctxt.GetInputValue(0)).GetGeometry();
   CVector3Array pos = inMesh.GetPoints().GetPositionArray();

   Operator op(ctxt.GetSource());
   updateOperatorInfo( op, sampleInfo, timeSampling, (int) pos.GetCount(), (int) meshPos->size());

   if(pos.GetCount() != meshPos->size())
      return CStatus::OK;

   for(size_t i=0;i<meshPos->size();i++)
      pos[(LONG)i].Set(meshPos->get()[i].x,meshPos->get()[i].y,meshPos->get()[i].z);

   // blend
   if(sampleInfo.alpha != 0.0)
   {
      if(objMesh.valid())
      {
        AbcG::IPolyMeshSchema::Sample sample;
         objMesh.getSchema().get(sample,sampleInfo.ceilIndex);
         meshPos = sample.getPositions();
      }
      else
      {
        AbcG::ISubDSchema::Sample sample;
         objSubD.getSchema().get(sample,sampleInfo.ceilIndex);
         meshPos = sample.getPositions();
      }
      for(size_t i=0;i<meshPos->size();i++)
         pos[(LONG)i].LinearlyInterpolate(pos[(LONG)i],CVector3(meshPos->get()[i].x,meshPos->get()[i].y,meshPos->get()[i].z),sampleInfo.alpha);
   }

   Primitive(ctxt.GetOutputTarget()).GetGeometry().GetPoints().PutPositionArray(pos);

   return CStatus::OK;
ESS_CALLBACK_END


ESS_CALLBACK_START( alembic_polymesh_Term, CRef& )
   return alembicOp_Term(in_ctxt);
ESS_CALLBACK_END

ESS_CALLBACK_START( alembic_normals_Define, CRef& )
   return alembicOp_Define(in_ctxt);
ESS_CALLBACK_END

ESS_CALLBACK_START( alembic_normals_DefineLayout, CRef& )
   return alembicOp_DefineLayout(in_ctxt);
ESS_CALLBACK_END

ESS_CALLBACK_START( alembic_normals_Init, CRef& )
   return alembicOp_Init( in_ctxt );
ESS_CALLBACK_END

ESS_CALLBACK_START( alembic_normals_Update, CRef& )
   ESS_PROFILE_SCOPE("alembic_normals_Update");
   OperatorContext ctxt( in_ctxt );

   CString path = ctxt.GetParameterValue(L"path");

   alembicOp_Multifile( in_ctxt, ctxt.GetParameterValue(L"multifile"), ctxt.GetParameterValue(L"time"), path);
   CStatus pathEditStat = alembicOp_PathEdit( in_ctxt, path );

   if((bool)ctxt.GetParameterValue(L"muted"))
      return CStatus::OK;

   CString identifier = ctxt.GetParameterValue(L"identifier");

  AbcG::IObject iObj = getObjectFromArchive(path,identifier);
   if(!iObj.valid())
      return CStatus::OK;
  AbcG::IPolyMesh obj(iObj,Abc::kWrapExisting);
   if(!obj.valid())
      return CStatus::OK;

   SampleInfo sampleInfo = getSampleInfo(
      ctxt.GetParameterValue(L"time"),
      obj.getSchema().getTimeSampling(),
      obj.getSchema().getNumSamples()
   );

   CDoubleArray normalValues = ClusterProperty((CRef)ctxt.GetInputValue(0)).GetElements().GetArray();
   PolygonMesh mesh = Primitive((CRef)ctxt.GetInputValue(1)).GetGeometry();
   CGeometryAccessor accessor = mesh.GetGeometryAccessor(siConstructionModeModeling);
   CLongArray counts;
   accessor.GetPolygonVerticesCount(counts);

  AbcG::IN3fGeomParam meshNormalsParam = obj.getSchema().getNormalsParam();
   if(meshNormalsParam.valid())
   {
      Abc::N3fArraySamplePtr meshNormals = meshNormalsParam.getExpandedValue(sampleInfo.floorIndex).getVals();

	  Operator op(ctxt.GetSource());
	  updateOperatorInfo( op, sampleInfo, obj.getSchema().getTimeSampling(),
						  (int) normalValues.GetCount(), (int) meshNormals->size()*3);

      if(meshNormals->size() * 3 == normalValues.GetCount())
      {
         // let's apply it!
         LONG offsetIn = 0;
         LONG offsetOut = 0;
         for(LONG i=0;i<counts.GetCount();i++)
         {
            for(LONG j=counts[i]-1;j>=0;j--)
            {
               normalValues[offsetOut++] = meshNormals->get()[offsetIn+j].x;
               normalValues[offsetOut++] = meshNormals->get()[offsetIn+j].y;
               normalValues[offsetOut++] = meshNormals->get()[offsetIn+j].z;
            }
            offsetIn += counts[i];
         }

         // blend
         if(sampleInfo.alpha != 0.0 /*&& !isAlembicMeshTopology(&iObj)*/)
         {
            meshNormals = meshNormalsParam.getExpandedValue(sampleInfo.ceilIndex).getVals();
            if(meshNormals->size() == normalValues.GetCount() / 3)
            {
               offsetIn = 0;
               offsetOut = 0;

               for(LONG i=0;i<counts.GetCount();i++)
               {
                  for(LONG j=counts[i]-1;j>=0;j--)
                  {
                     CVector3 normal(normalValues[offsetOut],normalValues[offsetOut+1],normalValues[offsetOut+2]);
                     normal.LinearlyInterpolate(normal,CVector3(
                        meshNormals->get()[offsetIn+j].x,
                        meshNormals->get()[offsetIn+j].y,
                        meshNormals->get()[offsetIn+j].z),sampleInfo.alpha);
                     normal.NormalizeInPlace();
                     normalValues[offsetOut++] = normal.GetX();
                     normalValues[offsetOut++] = normal.GetY();
                     normalValues[offsetOut++] = normal.GetZ();
                  }
                  offsetIn += counts[i];
               }
            }
         }
      }
   }

   ClusterProperty(ctxt.GetOutputTarget()).GetElements().PutArray(normalValues);

   return CStatus::OK;
ESS_CALLBACK_END

ESS_CALLBACK_START( alembic_normals_Term, CRef& )
   return alembicOp_Term(in_ctxt);
ESS_CALLBACK_END

ESS_CALLBACK_START( alembic_uvs_Define, CRef& )
   return alembicOp_Define(in_ctxt);
ESS_CALLBACK_END

ESS_CALLBACK_START( alembic_uvs_DefineLayout, CRef& )
   return alembicOp_DefineLayout(in_ctxt);
ESS_CALLBACK_END

ESS_CALLBACK_START( alembic_uvs_Init, CRef& )
   return alembicOp_Init( in_ctxt );
ESS_CALLBACK_END

ESS_CALLBACK_START( alembic_uvs_Update, CRef& )
   ESS_PROFILE_SCOPE("alembic_uvs_Update");
   OperatorContext ctxt( in_ctxt );

   CString path = ctxt.GetParameterValue(L"path");

   alembicOp_Multifile( in_ctxt, ctxt.GetParameterValue(L"multifile"), ctxt.GetParameterValue(L"time"), path);
   CStatus pathEditStat = alembicOp_PathEdit( in_ctxt, path );

   if((bool)ctxt.GetParameterValue(L"muted"))
      return CStatus::OK;

   CString identifierAndIndex = ctxt.GetParameterValue(L"identifier");

   ULONG colonOffset = identifierAndIndex.ReverseFindString(L":");

   CString identifier = identifierAndIndex.GetSubString(0, colonOffset);
 
   LONG uvI = 0;
   if( colonOffset+1 < identifierAndIndex.Length() ){
      uvI = (LONG)CValue(identifierAndIndex.GetSubString(colonOffset+1));
   }

   //ESS_LOG_WARNING("identifierAndIndex: "<<identifierAndIndex.GetAsciiString());
   //ESS_LOG_WARNING("identifier: "<<identifier.GetAsciiString());
   //ESS_LOG_WARNING("uvI: "<<uvI);

  AbcG::IObject iObj = getObjectFromArchive(path,identifier);
   if(!iObj.valid())
      return CStatus::OK;
  AbcG::IPolyMesh objMesh;
  AbcG::ISubD objSubD;
   if(AbcG::IPolyMesh::matches(iObj.getMetaData()))
      objMesh =AbcG::IPolyMesh(iObj,Abc::kWrapExisting);
   else
      objSubD =AbcG::ISubD(iObj,Abc::kWrapExisting);
   if(!objMesh.valid() && !objSubD.valid())
      return CStatus::OK;

   CDoubleArray uvValues = ClusterProperty((CRef)ctxt.GetInputValue(0)).GetElements().GetArray();
   PolygonMesh mesh = Primitive((CRef)ctxt.GetInputValue(1)).GetGeometry();
   CPolygonFaceRefArray faces = mesh.GetPolygons();
   CGeometryAccessor accessor = mesh.GetGeometryAccessor(siConstructionModeModeling);
   CLongArray counts;
   accessor.GetPolygonVerticesCount(counts);

  AbcG::IV2fGeomParam meshUvParam;
   if(objMesh.valid())
   {
      if(uvI == 0)
         meshUvParam = objMesh.getSchema().getUVsParam();
      else
      {
         CString storedUVName = CString(L"uv")+CString(uvI);
         if(objMesh.getSchema().getPropertyHeader( storedUVName.GetAsciiString() ) == NULL)
            return CStatus::OK;
         meshUvParam =AbcG::IV2fGeomParam( objMesh.getSchema(), storedUVName.GetAsciiString());
      }
   }
   else
   {
      if(uvI == 0)
         meshUvParam = objSubD.getSchema().getUVsParam();
      else
      {
         CString storedUVName = CString(L"uv")+CString(uvI);
         if(objSubD.getSchema().getPropertyHeader( storedUVName.GetAsciiString() ) == NULL)
            return CStatus::OK;
         meshUvParam =AbcG::IV2fGeomParam( objSubD.getSchema(), storedUVName.GetAsciiString());
      }
   }

   if(meshUvParam.valid())
   {
      SampleInfo sampleInfo = getSampleInfo(
         ctxt.GetParameterValue(L"time"),
         meshUvParam.getTimeSampling(),
         meshUvParam.getNumSamples()
      );

      Abc::V2fArraySamplePtr meshUVs = meshUvParam.getExpandedValue(sampleInfo.floorIndex).getVals();

	  Operator op(ctxt.GetSource());
	  updateOperatorInfo( op, sampleInfo, meshUvParam.getTimeSampling(),
						  (int) meshUVs->size() * 3, (int) uvValues.GetCount());

      if(meshUVs->size() * 3 == uvValues.GetCount())
      {
         // create a sample look table
         LONG offset = 0;
         CLongArray sampleLookup(accessor.GetNodeCount());
         for(LONG i=0;i<faces.GetCount();i++)
         {
            PolygonFace face(faces[i]);
            CLongArray samples = face.GetSamples().GetIndexArray();
            for(LONG j=samples.GetCount()-1;j>=0;j--){
               //ESS_LOG_WARNING("sampleLookup["<<samples[j]<<"]="<<offset);
               sampleLookup[samples[j]] = offset++;
               //sampleLookup[offset++] = samples[j];
            }
         }

         // let's apply it!
         offset = 0;
         for(LONG i=0;i<sampleLookup.GetCount();i++)
         {
            uvValues[offset++] = meshUVs->get()[sampleLookup[i]].x;
            uvValues[offset++] = meshUVs->get()[sampleLookup[i]].y;
            uvValues[offset++] = 0.0;
         }

         if(sampleInfo.alpha != 0.0)
         {
            meshUVs = meshUvParam.getExpandedValue(sampleInfo.ceilIndex).getVals();
            double ialpha = 1.0 - sampleInfo.alpha;

            offset = 0;
            for(LONG i=0;i<sampleLookup.GetCount();i++)
            {
               uvValues[offset] = uvValues[offset] * ialpha + meshUVs->get()[sampleLookup[i]].x * sampleInfo.alpha;
               offset++;
               uvValues[offset] = uvValues[offset] * ialpha + meshUVs->get()[sampleLookup[i]].y * sampleInfo.alpha;
               offset++;
               uvValues[offset] = 0.0;
               offset++;
            }
         }
      }
   }

   ClusterProperty(ctxt.GetOutputTarget()).GetElements().PutArray(uvValues);

   return CStatus::OK;
ESS_CALLBACK_END


ESS_CALLBACK_START( alembic_uvs_Term, CRef& )
   return alembicOp_Term(in_ctxt);
ESS_CALLBACK_END

ESS_CALLBACK_START( alembic_polymesh_topo_Define, CRef& )
   return alembicOp_Define(in_ctxt);
ESS_CALLBACK_END

ESS_CALLBACK_START( alembic_polymesh_topo_DefineLayout, CRef& )
   return alembicOp_DefineLayout(in_ctxt);
ESS_CALLBACK_END

ESS_CALLBACK_START( alembic_polymesh_topo_Init, CRef& )
   return alembicOp_Init( in_ctxt );
ESS_CALLBACK_END

ESS_CALLBACK_START( alembic_polymesh_topo_Update, CRef& )
   ESS_PROFILE_SCOPE("alembic_polymesh_topo_Update");
   OperatorContext ctxt( in_ctxt );

   CString path = ctxt.GetParameterValue(L"path");

   alembicOp_Multifile( in_ctxt, ctxt.GetParameterValue(L"multifile"), ctxt.GetParameterValue(L"time"), path);
   CStatus pathEditStat = alembicOp_PathEdit( in_ctxt, path );

   if((bool)ctxt.GetParameterValue(L"muted"))
      return CStatus::OK;

   CString identifier = ctxt.GetParameterValue(L"identifier");

  AbcG::IObject iObj = getObjectFromArchive(path,identifier);
   if(!iObj.valid())
      return CStatus::OK;
  AbcG::IPolyMesh objMesh;
  AbcG::ISubD objSubD;
   {
	ESS_PROFILE_SCOPE("alembic_polymesh_topo_Update type matching");
	if(AbcG::IPolyMesh::matches(iObj.getMetaData()))
      objMesh =AbcG::IPolyMesh(iObj,Abc::kWrapExisting);
   else
      objSubD =AbcG::ISubD(iObj,Abc::kWrapExisting);
   }

   {
		ESS_PROFILE_SCOPE("alembic_polymesh_topo_Update checking validity");
	   if(!objMesh.valid() && !objSubD.valid())
		  return CStatus::OK;
   }

   if( ! isAlembicMeshTopology( & iObj ) ) {
	   return CStatus::OK;
   }

   AbcA::TimeSamplingPtr timeSampling;
   int nSamples = 0;
   if(objMesh.valid())
   {
       timeSampling = objMesh.getSchema().getTimeSampling();
	  nSamples = (int) objMesh.getSchema().getNumSamples();
  }
   else
   {
        timeSampling = objSubD.getSchema().getTimeSampling();
	  nSamples = (int) objSubD.getSchema().getNumSamples();
   }

   SampleInfo sampleInfo;
   
   {
		ESS_PROFILE_SCOPE("alembic_polymesh_topo_Update getSampleInfo");
  
	   sampleInfo = getSampleInfo(
		 ctxt.GetParameterValue(L"time"),
		 timeSampling,
		 nSamples
	   );
   }

   Abc::P3fArraySamplePtr meshPos;
   Abc::V3fArraySamplePtr meshVel;
   Abc::Int32ArraySamplePtr meshFaceCount;
   Abc::Int32ArraySamplePtr meshFaceIndices;

   bool hasDynamicTopo = isAlembicMeshTopoDynamic( & objMesh );
   if(objMesh.valid())
   {
      ESS_PROFILE_SCOPE("alembic_polymesh_topo_Update load abc data arrays");

     AbcG::IPolyMeshSchema::Sample sample;
      objMesh.getSchema().get(sample,sampleInfo.floorIndex);
      meshPos = sample.getPositions();
      meshVel = sample.getVelocities();
      meshFaceCount = sample.getFaceCounts();
      meshFaceIndices = sample.getFaceIndices();
   }
   else
   {
      ESS_PROFILE_SCOPE("alembic_polymesh_topo_Update load abc data arrays");
     AbcG::ISubDSchema::Sample sample;
      objSubD.getSchema().get(sample,sampleInfo.floorIndex);
      meshPos = sample.getPositions();
      meshVel = sample.getVelocities();
      meshFaceCount = sample.getFaceCounts();
      meshFaceIndices = sample.getFaceIndices();
   }

   Operator op(ctxt.GetSource());
   updateOperatorInfo( op, sampleInfo, timeSampling, (int) meshPos->size(), (int) meshPos->size());

   CVector3Array pos((LONG)meshPos->size());
   CLongArray polies((LONG)(meshFaceCount->size() + meshFaceIndices->size()));

   {
       ESS_PROFILE_SCOPE("alembic_polymesh_topo_Update set positions");
	   for(size_t j=0;j<meshPos->size();j++) {
		  pos[(LONG)j].Set(meshPos->get()[j].x,meshPos->get()[j].y,meshPos->get()[j].z);
	   }
   }

   // check if this is an empty topo object
   if(meshFaceCount->size() > 0)
   {
        ESS_PROFILE_SCOPE("alembic_polymesh_topo_Update setup topology");
		if(meshFaceCount->get()[0] == 0)
      {
         if(!meshVel)
            return CStatus::OK;
         if(meshVel->size() != meshPos->size())
            return CStatus::OK;

         // dummy topo
         polies.Resize(4);
         polies[0] = 3;
         polies[1] = 0;
         polies[2] = 0;
         polies[3] = 0;
      }
      else
      {

         LONG offset1 = 0;
         Abc::int32_t offset2 = 0;

         //ESS_LOG_INFO("face count: " << (unsigned int)meshFaceCount->size());

         for(size_t j=0;j<meshFaceCount->size();j++)
         {
            Abc::int32_t singleFaceCount = meshFaceCount->get()[j];
            polies[offset1++] = singleFaceCount;
            offset2 += singleFaceCount;

            //ESS_LOG_INFO("singleFaceCount: " << (unsigned int)singleFaceCount);
            //ESS_LOG_INFO("offset2: " << (unsigned int)offset2);
            //ESS_LOG_INFO("meshFaceIndices->size(): " << (unsigned int)meshFaceIndices->size());

            unsigned int meshFIndxSz = (unsigned int)meshFaceIndices->size();

            for(size_t k=0;k<singleFaceCount;k++)
            {
               //ESS_LOG_INFO("index: " << (unsigned int)((size_t)offset2 - 1 - k));
               polies[offset1++] = meshFaceIndices->get()[(size_t)offset2 - 1 - k];
            }
         }
      }
   }

   // do the positional interpolation if necessary
   if(sampleInfo.alpha != 0.0)
   {
	  ESS_PROFILE_SCOPE("alembic_polymesh_topo_Update positional interpolation");
      double alpha = sampleInfo.alpha;
      double ialpha = 1.0 - alpha;

      // first check if the next frame has the same point count
      if(objMesh.valid())
      {
        AbcG::IPolyMeshSchema::Sample sample;
         objMesh.getSchema().get(sample,sampleInfo.ceilIndex);
         meshPos = sample.getPositions();
      }
      else
      {
        AbcG::ISubDSchema::Sample sample;
         objSubD.getSchema().get(sample,sampleInfo.floorIndex);
         meshPos = sample.getPositions();
      }

      if( !hasDynamicTopo)
      {
		  assert( meshPos->size() == (size_t)pos.GetCount() );

         for(LONG i=0;i<(LONG)meshPos->size();i++)
         {
            pos[i].PutX(ialpha * pos[i].GetX() + alpha * meshPos->get()[i].x);
            pos[i].PutY(ialpha * pos[i].GetY() + alpha * meshPos->get()[i].y);
            pos[i].PutZ(ialpha * pos[i].GetZ() + alpha * meshPos->get()[i].z);
         }
      }
      else if(meshVel)
      {
         float timeAlpha = getTimeOffsetFromObject( iObj, sampleInfo );
         if(meshVel->size() == (size_t)pos.GetCount())
         {
            for(LONG i=0;i<(LONG)meshVel->size();i++)
            {
               pos[i].PutX(pos[i].GetX() + timeAlpha * meshVel->get()[i].x);
               pos[i].PutY(pos[i].GetY() + timeAlpha * meshVel->get()[i].y);
               pos[i].PutZ(pos[i].GetZ() + timeAlpha * meshVel->get()[i].z);
            }
         }
      }
   }

   {
	PolygonMesh outMesh;
	{
	ESS_PROFILE_SCOPE("alembic_polymesh_topo_Update GetGeometry");
	outMesh = Primitive(ctxt.GetOutputTarget()).GetGeometry();
	}
	{
	ESS_PROFILE_SCOPE("alembic_polymesh_topo_Update PolygonMesh set");
	outMesh.Set(pos,polies);
	}
   }

   //ESS_LOG_INFO("EXIT alembic_polymesh_topo_Update");
   return CStatus::OK;
ESS_CALLBACK_END


ESS_CALLBACK_START( alembic_polymesh_topo_Term, CRef& )
   return alembicOp_Term(in_ctxt);
ESS_CALLBACK_END


ESS_CALLBACK_START( alembic_bbox_Define, CRef& )
   alembicOp_Define(in_ctxt);

   Context ctxt( in_ctxt );
   CustomOperator oCustomOperator;

   Parameter oParam;
   CRef oPDef;

   Factory oFactory = Application().GetFactory();
   oCustomOperator = ctxt.GetSource();

   oPDef = oFactory.CreateParamDef(L"extend",CValue::siFloat,siAnimatable| siPersistable,L"extend",L"extend",0.0f,-10000.0f,10000.0f,0.0f,10.0f);
   oCustomOperator.AddParameter(oPDef,oParam);
   return CStatus::OK;
ESS_CALLBACK_END


ESS_CALLBACK_START( alembic_bbox_DefineLayout, CRef& )
   alembicOp_DefineLayout(in_ctxt);

   Context ctxt( in_ctxt );
   PPGLayout oLayout;
   PPGItem oItem;
   oLayout = ctxt.GetSource();
   oLayout.AddItem(L"extend",L"Extend Box");
   return CStatus::OK;
ESS_CALLBACK_END

ESS_CALLBACK_START( alembic_bbox_Init, CRef& )
   return alembicOp_Init( in_ctxt );
ESS_CALLBACK_END

ESS_CALLBACK_START( alembic_bbox_Update, CRef& )
   OperatorContext ctxt( in_ctxt );

   CString path = ctxt.GetParameterValue(L"path");

   alembicOp_Multifile( in_ctxt, ctxt.GetParameterValue(L"multifile"), ctxt.GetParameterValue(L"time"), path);
   CStatus pathEditStat = alembicOp_PathEdit( in_ctxt, path );

   if((bool)ctxt.GetParameterValue(L"muted"))
      return CStatus::OK;

   CString identifier = ctxt.GetParameterValue(L"identifier");
   float extend = ctxt.GetParameterValue(L"extend");

  AbcG::IObject iObj = getObjectFromArchive(path,identifier);
   if(!iObj.valid())
      return CStatus::OK;

   Abc::Box3d box;
   SampleInfo sampleInfo;
   AbcA::TimeSamplingPtr timeSampling;
   int nSamples = 0;
   
   // check what kind of object we have
   const Abc::MetaData &md = iObj.getMetaData();
   if(AbcG::IPolyMesh::matches(md)) {
     AbcG::IPolyMesh obj(iObj,Abc::kWrapExisting);
      if(!obj.valid())
         return CStatus::OK;

      timeSampling = obj.getSchema().getTimeSampling();
	  nSamples = (int) obj.getSchema().getNumSamples();
      sampleInfo = getSampleInfo(
         ctxt.GetParameterValue(L"time"),
         timeSampling,
         nSamples
      );

     AbcG::IPolyMeshSchema::Sample sample;
      obj.getSchema().get(sample,sampleInfo.floorIndex);
      box = sample.getSelfBounds();

      if(sampleInfo.alpha > 0.0)
      {
         obj.getSchema().get(sample,sampleInfo.ceilIndex);
         Abc::Box3d box2 = sample.getSelfBounds();

         box.min = (1.0 - sampleInfo.alpha) * box.min + sampleInfo.alpha * box2.min;
         box.max = (1.0 - sampleInfo.alpha) * box.max + sampleInfo.alpha * box2.max;
      }
   } else if(AbcG::ICurves::matches(md)) {
     AbcG::ICurves obj(iObj,Abc::kWrapExisting);
      if(!obj.valid())
         return CStatus::OK;

      timeSampling = obj.getSchema().getTimeSampling();
	  nSamples = (int) obj.getSchema().getNumSamples();
      sampleInfo = getSampleInfo(
         ctxt.GetParameterValue(L"time"),
         timeSampling,
         nSamples
      );

     AbcG::ICurvesSchema::Sample sample;
      obj.getSchema().get(sample,sampleInfo.floorIndex);
      box = sample.getSelfBounds();

      if(sampleInfo.alpha > 0.0)
      {
         obj.getSchema().get(sample,sampleInfo.ceilIndex);
         Abc::Box3d box2 = sample.getSelfBounds();

         box.min = (1.0 - sampleInfo.alpha) * box.min + sampleInfo.alpha * box2.min;
         box.max = (1.0 - sampleInfo.alpha) * box.max + sampleInfo.alpha * box2.max;
      }
   } else if(AbcG::IPoints::matches(md)) {
     AbcG::IPoints obj(iObj,Abc::kWrapExisting);
      if(!obj.valid())
         return CStatus::OK;

      timeSampling = obj.getSchema().getTimeSampling();
	  nSamples = (int) obj.getSchema().getNumSamples();
      sampleInfo = getSampleInfo(
         ctxt.GetParameterValue(L"time"),
         timeSampling,
         nSamples
      );

     AbcG::IPointsSchema::Sample sample;
      obj.getSchema().get(sample,sampleInfo.floorIndex);
      box = sample.getSelfBounds();

      if(sampleInfo.alpha > 0.0)
      {
         obj.getSchema().get(sample,sampleInfo.ceilIndex);
         Abc::Box3d box2 = sample.getSelfBounds();

         box.min = (1.0 - sampleInfo.alpha) * box.min + sampleInfo.alpha * box2.min;
         box.max = (1.0 - sampleInfo.alpha) * box.max + sampleInfo.alpha * box2.max;
      }
   } else if(AbcG::ISubD::matches(md)) {
     AbcG::ISubD obj(iObj,Abc::kWrapExisting);
      if(!obj.valid())
         return CStatus::OK;

      timeSampling = obj.getSchema().getTimeSampling();
	  nSamples = (int) obj.getSchema().getNumSamples();
      sampleInfo = getSampleInfo(
         ctxt.GetParameterValue(L"time"),
         timeSampling,
         nSamples
      );

     AbcG::ISubDSchema::Sample sample;
      obj.getSchema().get(sample,sampleInfo.floorIndex);
      box = sample.getSelfBounds();

      if(sampleInfo.alpha > 0.0)
      {
         obj.getSchema().get(sample,sampleInfo.ceilIndex);
         Abc::Box3d box2 = sample.getSelfBounds();

         box.min = (1.0 - sampleInfo.alpha) * box.min + sampleInfo.alpha * box2.min;
         box.max = (1.0 - sampleInfo.alpha) * box.max + sampleInfo.alpha * box2.max;
      }
   }

   Primitive inPrim((CRef)ctxt.GetInputValue(0));
   CVector3Array pos = inPrim.GetGeometry().GetPoints().GetPositionArray();

   Operator op(ctxt.GetSource());
   updateOperatorInfo( op, sampleInfo, timeSampling, pos.GetCount(), pos.GetCount());

   box.min.x -= extend;
   box.min.y -= extend;
   box.min.z -= extend;
   box.max.x += extend;
   box.max.y += extend;
   box.max.z += extend;

   // apply the bbox
   for(LONG i=0;i<pos.GetCount();i++)
   {
      pos[i].PutX( pos[i].GetX() < 0 ? box.min.x : box.max.x );
      pos[i].PutY( pos[i].GetY() < 0 ? box.min.y : box.max.y );
      pos[i].PutZ( pos[i].GetZ() < 0 ? box.min.z : box.max.z );
   }

   Primitive outPrim(ctxt.GetOutputTarget());
   outPrim.GetGeometry().GetPoints().PutPositionArray(pos);

   return CStatus::OK;
ESS_CALLBACK_END


ESS_CALLBACK_START( alembic_bbox_Term, CRef& )
   return alembicOp_Term(in_ctxt);
ESS_CALLBACK_END


/// ICE NODE!
enum IDs
{
	ID_IN_path = 10,
	ID_IN_identifier = 11,
	ID_IN_renderpath = 12,
	ID_IN_renderidentifier = 13,
	ID_IN_time = 14,
	ID_IN_usevel = 15,
    ID_IN_multifile = 16,
	ID_G_100 = 1005,
	ID_OUT_position = 12771,
	ID_OUT_velocity = 12772,
	ID_OUT_faceCounts = 12773,
	ID_OUT_faceIndices = 12774,

	ID_TYPE_CNS = 1400,
	ID_STRUCT_CNS,
	ID_CTXT_CNS,
};

#define ID_UNDEF ((ULONG)-1)

using namespace XSI;

ESS_CALLBACK_START( alembic_polyMesh2_Init, CRef& )
   return alembicOp_Init( in_ctxt );
ESS_CALLBACK_END

XSIPLUGINCALLBACK CStatus alembic_polyMesh2_Evaluate(ICENodeContext& in_ctxt)
{
	//Application().LogMessage( "alembic_polyMesh2_Evaluate" );
	// The current output port being evaluated...
	ULONG out_portID = in_ctxt.GetEvaluatedOutputPortID( );

	CDataArrayString pathData( in_ctxt, ID_IN_path );
	CString path = pathData[0];
	CDataArrayString identifierData( in_ctxt, ID_IN_identifier );
	CString identifier = identifierData[0];
	CDataArrayFloat timeData( in_ctxt, ID_IN_time);
	const double time = timeData[0];

	CDataArrayBool multifile( in_ctxt, ID_IN_multifile );
	bool bMultifile = multifile[0];

    alembicOp_Multifile( in_ctxt, bMultifile, time, path);
    CStatus pathEditStat = alembicOp_PathEdit( in_ctxt, path );

	AbcG::IObject iObj = getObjectFromArchive(path,identifier);
	if(!iObj.valid())
		return CStatus::OK;

	CDataArrayBool usevelData( in_ctxt, ID_IN_usevel);
	const double usevel = usevelData[0];

	AbcG::IPolyMesh objMesh;
	AbcG::ISubD objSubD;
	const bool useMesh = AbcG::IPolyMesh::matches(iObj.getMetaData());
	if(useMesh)
		objMesh = AbcG::IPolyMesh(iObj,Abc::kWrapExisting);
	else
		objSubD = AbcG::ISubD(iObj,Abc::kWrapExisting);
	if(!objMesh.valid() && !objSubD.valid())
		return CStatus::OK;

	AbcA::TimeSamplingPtr timeSampling;
	int nSamples = 0;
	if(useMesh)
	{
		timeSampling = objMesh.getSchema().getTimeSampling();
		nSamples = (int) objMesh.getSchema().getNumSamples();
	}
	else
	{
		timeSampling = objSubD.getSchema().getTimeSampling();
		nSamples = (int) objSubD.getSchema().getNumSamples();
	}

	SampleInfo sampleInfo = getSampleInfo( time, timeSampling, nSamples );

	switch( out_portID )
	{
	case ID_OUT_position:
		{
			CDataArray2DVector3f outData( in_ctxt );
			CDataArray2DVector3f::Accessor acc;

			Abc::P3fArraySamplePtr ptr;
			if(useMesh)
			{
				AbcG::IPolyMeshSchema::Sample sample;
				objMesh.getSchema().get(sample,sampleInfo.floorIndex);
				ptr = sample.getPositions();
			}
			else
			{
				AbcG::ISubDSchema::Sample sample;
				objSubD.getSchema().get(sample,sampleInfo.floorIndex);
				ptr = sample.getPositions();
			}

			if(ptr == NULL || ptr->size() == 0 || (ptr->size() == 1 && ptr->get()[0].x == FLT_MAX) )
				acc = outData.Resize(0,0);
			else
			{
				acc = outData.Resize(0,(ULONG)ptr->size());
				bool done = false;
				if(sampleInfo.alpha != 0.0 && usevel)
				{
					const float alpha = (float)sampleInfo.alpha;//shouldn't we be using a time alpha here?

					Abc::V3fArraySamplePtr velPtr;
					if (useMesh)
					{
						AbcG::IPolyMeshSchema::Sample sample;
						objMesh.getSchema().get(sample, sampleInfo.floorIndex);
						velPtr = sample.getVelocities();
					}
					else
					{
						AbcG::ISubDSchema::Sample sample;
						objSubD.getSchema().get(sample, sampleInfo.floorIndex);
						velPtr = sample.getVelocities();
					}

					if(velPtr == NULL || velPtr->size() == 0)
						done = false;
					else
					{
						for(ULONG i=0; i<acc.GetCount(); ++i)
						{
							const Abc::V3f &pt = ptr->get()[i];
							const Abc::V3f &vl = velPtr->get()[i >= velPtr->size() ? 0 : i];
							acc[i].Set
							(
								pt.x + alpha * vl.x,
								pt.y + alpha * vl.y,
								pt.z + alpha * vl.z
							);
						}
						done = true;
					}
				}

				if(!done)
				{
					for(ULONG i=0;i<acc.GetCount();i++)
					{
						const Abc::V3f &pt = ptr->get()[i];
						acc[i].Set(pt.x, pt.y, pt.z);
					}
				}
			}
		}
		break;

	case ID_OUT_velocity:
		{
			CDataArray2DVector3f outData( in_ctxt );
			CDataArray2DVector3f::Accessor acc;

			Abc::V3fArraySamplePtr ptr;
			if(useMesh)
			{
				AbcG::IPolyMeshSchema::Sample sample;
				objMesh.getSchema().get(sample,sampleInfo.floorIndex);
				ptr = sample.getVelocities();
			}
			else
			{
				AbcG::ISubDSchema::Sample sample;
				objSubD.getSchema().get(sample,sampleInfo.floorIndex);
				ptr = sample.getVelocities();
			}

			if(ptr == NULL || ptr->size() == 0)
				acc = outData.Resize( 0, 0 );
			else
			{
				acc = outData.Resize(0, (ULONG)ptr->size());
				for(ULONG i=0; i<acc.GetCount(); ++i)
				{
					const Abc::V3f &vl = ptr->get()[i];
					acc[i].Set(vl.x, vl.y, vl.z);
				}
			}
		}
		break;

	case ID_OUT_faceCounts:
	case ID_OUT_faceIndices:
		{
			CDataArray2DLong outData( in_ctxt );
			CDataArray2DLong::Accessor acc;

			Abc::Int32ArraySamplePtr ptr;
			if (useMesh)
			{
				AbcG::IPolyMeshSchema::Sample sample;
				objMesh.getSchema().get(sample, sampleInfo.floorIndex);
				ptr = (out_portID == ID_OUT_faceCounts)?
								sample.getFaceCounts() :
								sample.getFaceIndices();
			}
			else
			{
				AbcG::ISubDSchema::Sample sample;
				objSubD.getSchema().get(sample, sampleInfo.floorIndex);
				ptr = (out_portID == ID_OUT_faceCounts)?
								sample.getFaceCounts() :
								sample.getFaceIndices();
			}

			if (ptr == NULL || ptr->size() == 0)
				acc = outData.Resize(0, 0);
			else
			{
				acc = outData.Resize(0, (ULONG)ptr->size() );
				for(ULONG i=0; i<acc.GetCount(); ++i)
					acc[i] = ptr->get()[i];
			}
		}
		break;
	}

	return CStatus::OK;
}


XSIPLUGINCALLBACK CStatus alembic_polyMesh2_Term(CRef& in_ctxt)
{
	return alembicOp_Term(in_ctxt);
}

XSI::CStatus Register_alembic_polyMesh( XSI::PluginRegistrar& in_reg )
{
	//Application().LogMessage( "Register_alembic_polyMesh" );
	ICENodeDef nodeDef = Application().GetFactory().CreateICENodeDef(L"alembic_polyMesh2", L"alembic_polyMesh2");

	CStatus st = nodeDef.PutColor(255, 188, 102);
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
	st = nodeDef.AddOutputPort(ID_OUT_position,		siICENodeDataVector3,	siICENodeStructureArray, siICENodeContextSingleton, L"position",	L"position");
	st.AssertSucceeded( ) ;
	st = nodeDef.AddOutputPort(ID_OUT_velocity,		siICENodeDataVector3,	siICENodeStructureArray, siICENodeContextSingleton, L"velocity",	L"velocity");
	st.AssertSucceeded( ) ;
	st = nodeDef.AddOutputPort(ID_OUT_faceCounts,	siICENodeDataLong,		siICENodeStructureArray, siICENodeContextSingleton, L"faceCounts",	L"faceCounts");
	st.AssertSucceeded( ) ;
	st = nodeDef.AddOutputPort(ID_OUT_faceIndices,	siICENodeDataLong,		siICENodeStructureArray, siICENodeContextSingleton, L"faceIndices",	L"faceIndices");
	st.AssertSucceeded( ) ;

	PluginItem nodeItem = in_reg.RegisterICENode(nodeDef);
	nodeItem.PutCategories(L"Alembic");

	return CStatus::OK;
}


