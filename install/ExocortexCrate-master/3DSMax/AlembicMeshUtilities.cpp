#include "stdafx.h"
#include "Alembic.h"
#include "AlembicDefinitions.h"
#include "AlembicArchiveStorage.h"
#include "utility.h"
#include "AlembicXForm.h"
#include "AlembicVisibilityController.h"
#include "AlembicNames.h"
#include "AlembicMeshUtilities.h"
#include "AlembicMAXScript.h" 
#include "AlembicMetadataUtils.h"

#include "CommonProfiler.h"
#include "CommonMeshUtilities.h"

#include "hashInstanceTable.h"

void AlembicImport_FillInPolyMesh_Internal(alembic_fillmesh_options &options);

void AlembicImport_FillInPolyMesh(alembic_fillmesh_options &options)
{ 
	ESS_STRUCTURED_EXCEPTION_REPORTING_START
		AlembicImport_FillInPolyMesh_Internal( options );
	ESS_STRUCTURED_EXCEPTION_REPORTING_END
}

void validateMeshes( alembic_fillmesh_options &options, char* szName ) {
	if (options.pMNMesh != NULL)
	{
		//ESS_LOG_WARNING( "options.pMNMesh->MNDebugPrint() ------------------------------------------------------------ section: " << szName );
		//options.pMNMesh->MNDebugPrint( FALSE );
		//if( ! options.pMNMesh->CheckAllData() ) {
		//	ESS_LOG_WARNING( "options.pMNMesh->CheckAllData() failed, section: " << szName );
		//	options.pMNMesh->MNDebugPrint();
		//}
	}
}



void AlembicImport_FillInPolyMesh_Internal(alembic_fillmesh_options &options)
{
   ESS_PROFILE_FUNC();
   AbcG::IPolyMesh objMesh;
   AbcG::ISubD objSubD;

   {

   ESS_PROFILE_SCOPE("AlembicImport_FillInPolyMesh_Internal - objMesh and objSubD");

   if(AbcG::IPolyMesh::matches((*options.pIObj).getMetaData()))
       objMesh = AbcG::IPolyMesh(*options.pIObj,Abc::kWrapExisting);
   else
       objSubD = AbcG::ISubD(*options.pIObj,Abc::kWrapExisting);

   }

   if(!objMesh.valid() && !objSubD.valid())
       return;


   int nTicks = options.dTicks;
   float fRoundedTimeAlpha = 0.0f;
   if(options.nDataFillFlags & ALEMBIC_DATAFILL_IGNORE_SUBFRAME_SAMPLES){
      RoundTicksToNearestFrame(nTicks, fRoundedTimeAlpha);
   }
   double sampleTime = GetSecondsFromTimeValue(nTicks);

   SampleInfo sampleInfo;
   if(objMesh.valid()) {
	  ESS_PROFILE_SCOPE("getSampleInfo");
      sampleInfo = getSampleInfo(
         sampleTime,
         objMesh.getSchema().getTimeSampling(),
         objMesh.getSchema().getNumSamples()
      );
   }
   else{
      sampleInfo = getSampleInfo(
         sampleTime,
         objSubD.getSchema().getTimeSampling(),
         objSubD.getSchema().getNumSamples()
      );
   }
   AbcG::IPolyMeshSchema::Sample polyMeshSample;
   AbcG::ISubDSchema::Sample subDSample;

   if(objMesh.valid()) objMesh.getSchema().get(polyMeshSample,sampleInfo.floorIndex);
   else objSubD.getSchema().get(subDSample,sampleInfo.floorIndex);

   int currentNumVerts = options.pMNMesh->numv;

   Abc::P3fArraySamplePtr meshPos;
   Abc::V3fArraySamplePtr meshVel;

   bool hasDynamicTopo = options.pObjectCache->isMeshTopoDynamic;
   if(hasDynamicTopo){
      //do slower check to see if the topology did not change between this frame and the previous frame
      hasDynamicTopo = frameHasDynamicTopology(&polyMeshSample, &sampleInfo, &(objMesh.getSchema().getFaceIndicesProperty()));
   }

   //ESS_LOG_WARNING("dynamicTopology: "<<hasDynamicTopo<<" time: "<<sampleTime);

   if(objMesh.valid())
   {
   ESS_PROFILE_SCOPE("Mesh getPositions/getVelocities/faceCountProp");
     meshPos = polyMeshSample.getPositions();
     meshVel = polyMeshSample.getVelocities();
   }
   else
   {
   ESS_PROFILE_SCOPE("SubD getPositions/getVelocities/faceCountProp");
     meshPos = subDSample.getPositions();
     meshVel = subDSample.getVelocities();
   }

	//MH: What is this code for? //related to vertex blending
	//note that the fillInMesh call will crash if the points are not initilaized (tested max 2013)
   if(  ( options.nDataFillFlags & ALEMBIC_DATAFILL_FACELIST ) ||
	   ( options.nDataFillFlags & ALEMBIC_DATAFILL_VERTEX ) ) {
	   if (currentNumVerts != meshPos->size() && ! options.pMNMesh->GetFlag( MN_MESH_RATSNEST ) )
	   {
    		ESS_PROFILE_SCOPE("resize and clear vertices");
		   int numVerts = static_cast<int>(meshPos->size());
		   
		   options.pMNMesh->setNumVerts(numVerts);
		   MNVert* pMeshVerties = options.pMNMesh->V(0);
		   for(int i=0;i<numVerts;i++){
			   pMeshVerties[i].p = Point3(0,0,0);
		   }
	   }

		validateMeshes( options, "ALEMBIC_DATAFILL_FACELIST | ALEMBIC_DATAFILL_VERTEX" );
   }

   if ( options.nDataFillFlags & ALEMBIC_DATAFILL_VERTEX )
   {
       		ESS_PROFILE_SCOPE("ALEMBIC_DATAFILL_VERTEX");
	   Abc::V3f const* pPositionArray = ( meshPos.get() != NULL ) ? meshPos->get() : NULL;
	   Abc::V3f const* pVelocityArray = ( meshVel.get() != NULL ) ? meshVel->get() : NULL;

	   if( pPositionArray ){
	
		   std::vector<Abc::V3f> vArray;
		   vArray.reserve(meshPos->size());
		   //P3fArraySample* pPositionArray = meshPos->get();
		   for(size_t i=0;i<meshPos->size();i++) {
			  vArray.push_back(pPositionArray[i]);
		   }

           Abc::P3fArraySamplePtr meshPos2;

		   // blend - either between samples or using point velocities
		   if( ((options.nDataFillFlags & ~ALEMBIC_DATAFILL_IGNORE_SUBFRAME_SAMPLES) && sampleInfo.alpha != 0.0f ) || 
			   ((options.nDataFillFlags & ALEMBIC_DATAFILL_IGNORE_SUBFRAME_SAMPLES) && fRoundedTimeAlpha != 0.0f ) )
		   {
			   bool bSampleInterpolate = false;
			   bool bVelInterpolate = false;

			  if(objMesh.valid())
			  {
                 {
                     //ESS_PROFILE_SCOPE("AlembicImport_FillInPolyMesh_Internal - 2nd mesh sample read");
				     //AbcG::IPolyMeshSchema::Sample polyMeshSample2;
				     //objMesh.getSchema().get(polyMeshSample2,sampleInfo.ceilIndex);
				     //meshPos2 = polyMeshSample2.getPositions();

                     //ESS_PROFILE_SCOPE("AlembicImport_FillInPolyMesh_Internal - 2nd position sample read");

                     Abc::IP3fArrayProperty positionPropertyCeil = objMesh.getSchema().getPositionsProperty();
                     positionPropertyCeil.get(meshPos2, sampleInfo.ceilIndex);

				     const int posSize = meshPos2 ? (const int)meshPos2->size() : 0;
				     const int velSize = meshVel ? (const int)meshVel->size() : 0;
   	             
				     if(meshPos2->size() == vArray.size() && !hasDynamicTopo)
					     bSampleInterpolate = true;
				     else if(meshVel && meshVel->size() == vArray.size())
					     bVelInterpolate = true;
                  }
			  }
			  else
			  {
				  AbcG::ISubDSchema::Sample subDSample2;
				  objSubD.getSchema().get(subDSample2,sampleInfo.ceilIndex);
				  meshPos2 = subDSample2.getPositions();
				  bSampleInterpolate = true;
			  }

			  float sampleInfoAlpha = (float)sampleInfo.alpha; 
			  if (bSampleInterpolate)
			  {
				  pPositionArray = meshPos2->get();

				  for(size_t i=0;i<meshPos2->size();i++)
				  {	
					  vArray[i] += (pPositionArray[i] - vArray[i]) * sampleInfoAlpha; 
				  }
			  }
			  else if (bVelInterpolate)
			  {
				  assert( pVelocityArray != NULL );
				  pVelocityArray = meshVel->get();

				  float timeAlpha;
				  if(options.nDataFillFlags & ALEMBIC_DATAFILL_IGNORE_SUBFRAME_SAMPLES){
				      timeAlpha = fRoundedTimeAlpha;	
				  }
				  else {
					  timeAlpha = getTimeOffsetFromObject( *options.pIObj, sampleInfo );
				  }
				  for(size_t i=0;i<meshVel->size();i++)
				  {
					  vArray[i] += pVelocityArray[i] * timeAlpha;                  
				  }
			  }
		   }

		   if( options.fVertexAlpha != 1.0f ) {
			   for( int i = 0; i < vArray.size(); i ++ ) {
				   vArray[i] *= options.fVertexAlpha;
			   }
		   }
		 
		
		  // MNVert* pMeshVerties = options.pMNMesh->V(0);
		   ;
		   for(int i=0;i<vArray.size();i++)
		   {
			   if( options.bAdditive ) {
				   //pMeshVerties[i].p += 
				   options.pObject->SetPoint( i, options.pObject->GetPoint( i ) + ConvertAlembicPointToMaxPoint(vArray[i]) ); 
			   }
			   else {
				   //pMeshVerties[i].p = ConvertAlembicPointToMaxPoint(vArray[i]);
				   options.pObject->SetPoint( i, ConvertAlembicPointToMaxPoint(vArray[i]) ); 
			   }
		   }
		   validateMeshes( options, "ALEMBIC_DATAFILL_VERTEX" );
	   }
    }

  

   Abc::Int32ArraySamplePtr meshFaceCount;
   Abc::Int32ArraySamplePtr meshFaceIndices;

   if (objMesh.valid())
   {
       meshFaceCount = polyMeshSample.getFaceCounts();
       meshFaceIndices = polyMeshSample.getFaceIndices();
   }
   else
   {
       meshFaceCount = subDSample.getFaceCounts();
       meshFaceIndices = subDSample.getFaceIndices();
   }

   Abc::int32_t const* pMeshFaceCount = ( meshFaceCount.get() != NULL ) ? meshFaceCount->get() : NULL;
   Abc::int32_t const* pMeshFaceIndices = ( meshFaceIndices.get() != NULL ) ? meshFaceIndices->get() : NULL;

   int numFaces = static_cast<int>(meshFaceCount->size());
   int numIndices = static_cast<int>(meshFaceIndices->size());

   int sampleCount = 0;
   for(int i=0; i<numFaces; i++){
		int degree = pMeshFaceCount[i];
		sampleCount+=degree;
   }

   if ( options.nDataFillFlags & ALEMBIC_DATAFILL_FACELIST )
   {
       		ESS_PROFILE_SCOPE("ALEMBIC_DATAFILL_FACELIST");
	   if(sampleCount == numIndices)
	   {
	   
        // Set up the index buffer
        int offset = 0;
		
		bool onlyTriangles = true;

	    options.pMNMesh->setNumFaces(numFaces);
		MNFace *pFaces = options.pMNMesh->F(0);
   		for (int i = 0; i < numFaces; ++i)
		{
			int degree = pMeshFaceCount[i];

			if( degree > 3 ) {
				onlyTriangles = false;
			}
			MNFace *pFace = &(pFaces[i]);
			pFace->SetDeg(degree);
			pFace->material = 1;
			
			for (int j = degree - 1; j >= 0; --j)
			{
				pFace->vtx[j] = pMeshFaceIndices[offset];
				//pFace->edg[j] = -1;
				++offset;
			}
		}
		if( ! onlyTriangles ) {
			options.pMNMesh->SetFlag( MN_MESH_NONTRI, TRUE );
		}
		else {
			options.pMNMesh->SetFlag( MN_MESH_NONTRI, FALSE );
		}

		validateMeshes( options, "ALEMBIC_DATAFILL_FACELIST" );

	   }//if(sampleCount != numIndices)
	   else{
			ESS_LOG_WARNING("faceCount, index array mismatch. Not filling in indices (did you check 'dynamic topology' when exporting?).");
	   }
   }


   //3DS Max tends to crash if FillInMesh is not called. The call will build a winged-edge data structure, so good topology is required.
   //3DS Max duplicate points to fix bad topology. Thus, it is important for this call to happen last (after the last geometry modifier is
   //applied). Since this case is difficult to detect, we will throw an error some when multiple modifiers exist, a and there is bad topology.

   //if (( options.nDataFillFlags & ALEMBIC_DATAFILL_VERTEX )){//||
   //We seem to get crash if FillInPolyMesh is called in a modifier other that the topology one
   if( options.nDataFillFlags & ALEMBIC_DATAFILL_FACELIST ){
  	  //if( !options.pMNMesh->GetFlag( MN_MESH_FILLED_IN ) ){//This flag never seems to be set
      if(options.pMNMesh->ENum() == 0 ){
         ESS_PROFILE_SCOPE("FillInMesh");
         //ESS_LOG_WARNING("Filling in polymesh, fileName: " << options.fileName << " identifier: " << options.identifier );
         options.pMNMesh->FillInMesh();
     
 
         if( options.pMNMesh->GetFlag(MN_MESH_RATSNEST) && !(options.nDataFillFlags & ALEMBIC_DATAFILL_VERTEX) ){
            ESS_LOG_ERROR( "Mesh is a 'Rat's Nest' (more than 2 faces per edge),\n fileName: " << options.fileName << " identifier: " << options.identifier<<".\n Please import with the \"Load Geometry from Topology modifier\" option active" );
         }
      } 
      else if(options.pMNMesh->VNum() > meshPos->size()){
         ESS_LOG_WARNING("Mesh has bad topology. Multiple geometry modifiers not fully supported, fileName: " << options.fileName << " identifier: " << options.identifier );
      }
   }


	if( ( options.nDataFillFlags & ALEMBIC_DATAFILL_FACELIST ) &&
	   ( ! ( options.nDataFillFlags & ALEMBIC_DATAFILL_VERTEX ) ) ) {
		ESS_PROFILE_SCOPE("Reset mesh vertices to (0,0,0)");
		 
		   MNVert* pMeshVerties = options.pMNMesh->V(0);
		   for(int i=0;i<meshPos->size();i++)
		   {
			   pMeshVerties[i].p = Point3(0,0,0);
		   }
		
	}

   if ( objMesh.valid() && ( options.nDataFillFlags & ALEMBIC_DATAFILL_NORMALS ) )
   {
		ESS_PROFILE_SCOPE("ALEMBIC_DATAFILL_NORMALS");
       AbcG::IN3fGeomParam meshNormalsParam = objMesh.getSchema().getNormalsParam();
       if(meshNormalsParam.valid())
       {
		   std::vector<Abc::V3f> normalValuesFloor, normalValuesCeil;
		   std::vector<AbcA::uint32_t> normalIndicesFloor, normalIndicesCeil;

			bool normalsFloor = getIndexAndValues( meshFaceIndices, meshNormalsParam, sampleInfo.floorIndex,
					   normalValuesFloor, normalIndicesFloor );
			bool normalsCeil = false;
            if(!hasDynamicTopo){
               normalsCeil = getIndexAndValues( meshFaceIndices, meshNormalsParam, sampleInfo.ceilIndex,
					   normalValuesCeil, normalIndicesCeil );
            }

			if( ! normalsFloor ) {
			   ESS_LOG_ERROR( "Mesh normals are not set because they are not valid." );
			}
			else if( normalIndicesFloor.size() != sampleCount) {
			   //The last check ensures that we will not exceed the array bounds of PMeshNormalsFloor
			   ESS_LOG_ERROR( "Mesh normals are not set because their index count (" << normalIndicesFloor.size() << ") doesn't match the face index count (" << sampleCount << ")" );
		   }
		   else {

			   // Set up the specify normals
			   if( options.pMNMesh->GetSpecifiedNormals() == NULL ) {
					options.pMNMesh->SpecifyNormals();
			   }

			   //NOTE: options.pMNMesh->ClearSpecifiedNormals() will make getSpecifiedNormals return NULL again
  			   MNNormalSpec *normalSpec = options.pMNMesh->GetSpecifiedNormals();
			   normalSpec->SetParent( options.pMNMesh );
			   if( normalSpec->GetNumFaces() != numFaces || normalSpec->GetNumNormals() != (int)normalValuesFloor.size()) {
					//normalSpec->ClearAndFree();
				    normalSpec->SetNumFaces(numFaces);
					normalSpec->SetNumNormals((int)normalValuesFloor.size());
			   }
			   normalSpec->SetFlag(MESH_NORMAL_MODIFIER_SUPPORT, true);
			   normalSpec->SetAllExplicit(true); //this call is probably more efficient than the per vertex one since 3DS Max uses bit flags

			   // set normal values
			    if (sampleInfo.alpha != 0.0f && normalsCeil && normalValuesFloor.size() == normalValuesCeil.size() && !hasDynamicTopo )
			   {
				   for (int i = 0; i < normalValuesFloor.size(); i ++ ) {
					   Abc::V3f interpolatedNormal = normalValuesFloor[i] + (normalValuesCeil[i] - normalValuesFloor[i]) * float(sampleInfo.alpha);
			           interpolatedNormal *= options.fVertexAlpha;
					   normalSpec->Normal(i) = ConvertAlembicNormalToMaxNormal_Normalized( interpolatedNormal );
				   }
			   }
			   else
			   {
				   for (int i = 0; i < normalValuesFloor.size(); i ++ ) {
					   Abc::V3f interpolatedNormal = normalValuesFloor[i];
			           interpolatedNormal *= options.fVertexAlpha;
					   normalSpec->Normal(i) = ConvertAlembicNormalToMaxNormal_Normalized( interpolatedNormal );
				   }
			   }

			   // set normal indices
				{
				   int offset = 0;
				   for (int i = 0; i < numFaces; i++){
					   int degree = pMeshFaceCount[i];					   
					   MNNormalFace &normalFace = normalSpec->Face(i);
					   normalFace.SetDegree(degree);
					   for (int j = 0; j < degree; j ++ ) {
							normalFace.SetNormalID( degree - j - 1, (int) normalIndicesFloor[offset] );
						   offset ++;
					   }
				   }
			   }


			   //3DS Max documentation on MNMesh::checkNormals() - checks our flags and calls BuildNormals, ComputeNormals as needed. 
			   //MHahn: Probably not necessary since we explicility setting every normals
			   //normalSpec->CheckNormals();


			   // since we commented out check normals above, we need to set these explicitly.
			   normalSpec->SetFlag(MESH_NORMAL_NORMALS_BUILT, TRUE);
			   normalSpec->SetFlag(MESH_NORMAL_NORMALS_COMPUTED, TRUE);

			   //Also allocates space for the RVert array which we need for doing any normal vector queries
			   //options.pMNMesh->checkNormals(TRUE);
			   //MHahn: switched to build normals call, since all we need to is build the RVert array. Note: documentation says we only need
			   //to this if we query the MNMesh to ask about vertices. Do we actually need this capability?
			   options.pMNMesh->buildNormals();

		   }
       }
        validateMeshes( options, "ALEMBIC_DATAFILL_NORMALS" );
   }

   if ( options.nDataFillFlags & ALEMBIC_DATAFILL_UVS )
   {
      ESS_PROFILE_SCOPE("ALEMBIC_DATAFILL_UVS");
	   std::string strObjectIdentifier = options.identifier;
	   size_t found = strObjectIdentifier.find_last_of(":");
	   strObjectIdentifier = strObjectIdentifier.substr(found+1);
	   std::istringstream is(strObjectIdentifier);
	   int uvI = 0;
	   is >> uvI;

	   uvI--;

	   AbcG::IV2fGeomParam meshUvParam = getMeshUvParam(uvI, objMesh, objSubD);

	   //add 1 since channel 0 is reserved for colors
	   uvI++;

       if(meshUvParam.valid())
       {
           SampleInfo sampleInfo = getSampleInfo(
               sampleTime,
               meshUvParam.getTimeSampling(),
               meshUvParam.getNumSamples()
               );

		    std::vector<Abc::V2f> uvValuesFloor, uvValuesCeil;
		    std::vector<AbcA::uint32_t> uvIndicesFloor, uvIndicesCeil;

			bool uvFloor = getIndexAndValues( meshFaceIndices, meshUvParam, sampleInfo.floorIndex,
					   uvValuesFloor, uvIndicesFloor );
			bool uvCeil = false;
            if(!hasDynamicTopo){
               uvCeil = getIndexAndValues( meshFaceIndices, meshUvParam, sampleInfo.ceilIndex,
					   uvValuesCeil, uvIndicesCeil );
            }

            if( !uvFloor || uvValuesFloor.size() == 0) {
			   ESS_LOG_WARNING( "Mesh UVs are in an invalid state in Alembic file, ignoring." );
		   }
		   else{
            if(options.pMNMesh->MNum() <= uvI){
               options.pMNMesh->SetMapNum(uvI+1);
            }

				if(options.pMNMesh->MNum() > uvI)
				{
				   options.pMNMesh->InitMap(uvI);
				   MNMap *map = options.pMNMesh->M(uvI);
				   map->setNumVerts((int)uvValuesFloor.size());
				   map->setNumFaces(numFaces);
               //map->ClearFlag(MN_DEAD);
 
				   // set values
				   Point3* mapV = map->v;
				   if (sampleInfo.alpha != 0.0f && uvCeil && uvValuesFloor.size() == uvValuesCeil.size())
				   {
						for (int i = 0; i < uvValuesFloor.size(); i ++ )
					   {
						   Abc::V2f uv = uvValuesFloor[i] + ( uvValuesCeil[i] - uvValuesFloor[i] ) * float(sampleInfo.alpha);
						   mapV[i] = Point3( uv.x, uv.y, 0.0f );
					   }
				   }
				   else {
					   for (int i = 0; i < uvValuesFloor.size(); i ++ )
					   {
						   Abc::V2f uv =  uvValuesFloor[i];
						   mapV[i] = Point3( uv.x, uv.y, 0.0f );
					   }
				   }

                   bool bBadUVindices = false;
				   // set indices
				   int offset = 0;
				   MNMapFace* mapF = map->f;
				   for (int i = 0; i < numFaces; i += 1)
				   {
					   int degree = 0; 
					   if (i < options.pMNMesh->numf) {
						   degree = options.pMNMesh->F(i)->deg;
					   }
					   mapF[i].SetSize(degree);
					   for (int j = 0; j < degree; j ++)
                  {
                           int theIndex = 0;
                           if(offset < uvIndicesFloor.size()){
                              theIndex = (int)uvIndicesFloor[ offset ];
                           }
                           else{
                              ESS_LOG_WARNING("offset > uvIndicesFloor.size()");
                           }
                           if(theIndex < 0){
                              theIndex = 0;
                              bBadUVindices = true;
                           }
                           if(theIndex > uvValuesFloor.size()){
                              theIndex = (int)uvValuesFloor.size()-1;
                              bBadUVindices = true;
                           }

						   mapF[i].tv[degree - j - 1] = theIndex;//(int) uvIndicesFloor[ offset ];
						   ++offset;
					   }
				   }

                if(bBadUVindices){
                   ESS_LOG_WARNING("Corrected out of bounds UV indices to prevent crash.");
                }

			   }
			   else{
					ESS_LOG_WARNING( "UV channels have not been allocated. Cannot load uv channel "<<uvI );
			   }

		   }
       }
	   validateMeshes( options, "ALEMBIC_DATAFILL_UVS" );
   }

   if ( options.nDataFillFlags & ALEMBIC_DATAFILL_MATERIALIDS )
   {
 ESS_PROFILE_SCOPE("ALEMBIC_DATAFILL_MATERIALIDS");
	      Abc::IUInt32ArrayProperty materialIds;
       if(objMesh.valid() && objMesh.getSchema().getPropertyHeader( ".materialids" )) 
           materialIds = Abc::IUInt32ArrayProperty(objMesh.getSchema(), ".materialids");
       else if (objSubD.valid() && objSubD.getSchema().getPropertyHeader( ".materialids" ))
           materialIds = Abc::IUInt32ArrayProperty(objSubD.getSchema(), ".materialids");

       // If we don't detect a material id property then we try the facesets.  The order in the face set array is assumed
       // to be the material id
       if (materialIds.valid() && materialIds.getNumSamples() > 0)
       {
            Abc::UInt32ArraySamplePtr materialIdsPtr = materialIds.getValue(sampleInfo.floorIndex);

            if (materialIdsPtr->size() == numFaces)
            {
                for (int i = 0; i < materialIdsPtr->size(); i += 1)
                {
                    int nMatId = materialIdsPtr->get()[i];
                    if ( i < options.pMNMesh->numf)
                    {
                        options.pMNMesh->f[i].material = nMatId;
                    }
                }
            }
       }
       else
       {
           std::vector<std::string> faceSetNames;
           if(objMesh.valid())
               objMesh.getSchema().getFaceSetNames(faceSetNames);
           else
               objSubD.getSchema().getFaceSetNames(faceSetNames);


           std::vector<int> matIDs;
           for(size_t j=0;j<faceSetNames.size();j++)
           {
               std::string& name = faceSetNames[j];
               int a = 0;
               int b = (int)name.size()-1;
               bool bNumFound = false;
               for( int k=b; k>=0; k-- ){
                  if( '0' <= name[k] && name[k] <= '9' ){
                     if(!bNumFound){
                        bNumFound = true;
                        b = k;
                     }
                  }
                  else if(bNumFound){
                     a = k+1;
                     break;
                  }
               }
               
               std::string extractedNum = name.substr(a, b-a+1);

               try
               {
                  //subtract since 3DS Max is going to increment it for the UI
                  matIDs.push_back(boost::lexical_cast< int >(extractedNum)-1);
               }
               catch( const boost::bad_lexical_cast & )
               {
                  
               }
           }
           

           for(size_t j=0;j<faceSetNames.size();j++)
           {
               AbcG::IFaceSetSchema faceSet;
               if(objMesh.valid())
                   faceSet = objMesh.getSchema().getFaceSet(faceSetNames[j]).getSchema();
               else
                   faceSet = objSubD.getSchema().getFaceSet(faceSetNames[j]).getSchema();

               //ESS_LOG_WARNING("Reading faceset "<<sampleInfo.floorIndex);
 
               AbcG::IFaceSetSchema::Sample faceSetSample = faceSet.getValue(sampleInfo.floorIndex);
               Abc::Int32ArraySamplePtr faces = faceSetSample.getFaces();

               //ESS_LOG_WARNING("faceset name: "<<faceSetNames[j]);
               int nMatId = (int)j;
               if(matIDs.size() == faceSetNames.size()){
                  nMatId = matIDs[j];
                  //ESS_LOG_WARNING("parsed matID used: "<<nMatId);
               }
               else{
                  //ESS_LOG_WARNING("matID used: "<<nMatId<<" -- "<<(j < matIDs.size() ? matIDs[j] : -1));
               }

               for(size_t k=0;k<faces->size();k++)
               {
                   int faceId = faces->get()[k];
                   if ( faceId < options.pMNMesh->numf)
                   {
                       options.pMNMesh->f[faceId].material = nMatId;
                   }
               }
           }
       }
	   validateMeshes( options, "ALEMBIC_DATAFILL_MATERIALIDS" );
   }
 
 /*   if( options.pMNMesh->GetSpecifiedNormals() == NULL ) {
		 {
		 ESS_PROFILE_SCOPE("SpecifyNormals");

		options.pMNMesh->SpecifyNormals();
		 }
		 {
		 ESS_PROFILE_SCOPE("CheckNormals");
		options.pMNMesh->GetSpecifiedNormals()->CheckNormals();
		 }
		 {
		 ESS_PROFILE_SCOPE("CheckNormals");

	    options.pMNMesh->checkNormals(TRUE);
			  }
	}*/

  // This isn't required if we notify 3DS Max properly via the channel flags for vertex changes.
   //options.pMNMesh->MNDebugPrint();
   if ( (options.nDataFillFlags & ALEMBIC_DATAFILL_FACELIST) || (options.nDataFillFlags & ALEMBIC_DATAFILL_NORMALS) ) {
		 ESS_PROFILE_SCOPE("InvalidateTopoCache/InvalidateGeomCache");
	     options.pMNMesh->InvalidateTopoCache();
		 options.pMNMesh->InvalidateGeomCache();
	}
   else {
	  if( options.nDataFillFlags & ALEMBIC_DATAFILL_VERTEX ) {
		 ESS_PROFILE_SCOPE("InvalidateGeomCache");
		 // options.pMNMesh->InvalidateGeomCache();
		 options.pObject->PointsWereChanged();
	  }
   }
}

bool AlembicImport_IsPolyObject(AbcG::IPolyMeshSchema::Sample &polyMeshSample)
{
    Abc::Int32ArraySamplePtr meshFaceCount = polyMeshSample.getFaceCounts();

    // Go through each face and check the number of vertices.  If a face does not have 3 vertices,
    // we consider it to be a polymesh, otherwise it is a triangle mesh
    for (size_t i = 0; i < meshFaceCount->size(); ++i)
    {
        int vertexCount = meshFaceCount->get()[i];
        if (vertexCount != 3)
        {
            return true;
        }
    }

    return false;
}

void addAlembicMaterialsModifier(INode *pNode, AbcG::IObject& iObj)
{
   ESS_PROFILE_FUNC();


	AbcG::IPolyMesh objMesh;
	AbcG::ISubD objSubD;
	int nLastSample = 0;

	if(AbcG::IPolyMesh::matches(iObj.getMetaData())){
	   objMesh = AbcG::IPolyMesh(iObj,Abc::kWrapExisting);
	   nLastSample = (int)objMesh.getSchema().getNumSamples()-1;
	}
	else{
	   objSubD = AbcG::ISubD(iObj,Abc::kWrapExisting);
	   nLastSample = (int)objSubD.getSchema().getNumSamples()-1;
	}

	if(!objMesh.valid() && !objSubD.valid()){
	   return;
	}

	SampleInfo sampleInfo;
	if(objMesh.valid()){
		sampleInfo = getSampleInfo(nLastSample, objMesh.getSchema().getTimeSampling(), objMesh.getSchema().getNumSamples());
	}
	else{
		sampleInfo = getSampleInfo(nLastSample, objSubD.getSchema().getTimeSampling(), objSubD.getSchema().getNumSamples());
	}

	Abc::IStringArrayProperty matNamesProperty;
	if(objMesh.valid() && objMesh.getSchema().getPropertyHeader(".materialnames")){
		matNamesProperty = Abc::IStringArrayProperty(objMesh.getSchema(), ".materialnames");
	}
	else if(objSubD.valid() && objSubD.getSchema().getPropertyHeader(".materialnames")){
		matNamesProperty = Abc::IStringArrayProperty(objSubD.getSchema(), ".materialnames");
	}

	std::vector<std::string> faceSetNames;

	if(!matNamesProperty.valid() || matNamesProperty.getNumSamples() == 0){//if we couldn't read the .materialnames property, look for the faceset names

		if(objMesh.valid()){
			sampleInfo = getSampleInfo(0, objMesh.getSchema().getTimeSampling(), objMesh.getSchema().getNumSamples());
		}
		else{
			sampleInfo = getSampleInfo(0, objSubD.getSchema().getTimeSampling(), objSubD.getSchema().getNumSamples());
		}

		if(objMesh.valid()){
			objMesh.getSchema().getFaceSetNames(faceSetNames);
		}
		else{
			objSubD.getSchema().getFaceSetNames(faceSetNames);
		}
	}
	else{

		Abc::StringArraySamplePtr matNamesSamplePtr = matNamesProperty.getValue(sampleInfo.floorIndex);
		size_t len = matNamesSamplePtr->size();

		for(size_t i=0; i<len; i++){

			faceSetNames.push_back(matNamesSamplePtr->get()[i]);
		}
	}

	if(faceSetNames.size() <= 0){
		return;
	}

	std::string names("");

	for(size_t j=0;j<faceSetNames.size();j++)
	{
		const char* name = faceSetNames[j].c_str();
		names+="\"";
		names+=name;
		names+="\"";
		if(j != faceSetNames.size()-1){
			names+=", ";
		}
	}

	//const size_t bufSize = names.size() + 500;

	//char* szBuffer = new char[bufSize];
	//sprintf_s(szBuffer, bufSize,
	//		"AlembicMaterialModifier = EmptyModifier()\n"
	//		"AlembicMaterialModifier.name = \"Alembic Materials\"\n"
	//		"addmodifier $ AlembicMaterialModifier\n"

	//		"AlembicMaterialCA = attributes AlembicMaterialModifier\n"
	//		"(\n"	
	//			"rollout AlembicMaterialModifierRLT \"Alembic Materials\"\n"
	//			"(\n"
	//				"listbox eTestList \"\" items:#(%s)\n"
	//			")\n"
	//		")\n"

	//		"custattributes.add $.modifiers[\"Alembic Materials\"] AlembicMaterialCA baseobject:false\n"
	//		"$.modifiers[\"Alembic Materials\"].enabled = false"
	//		//"$.modifiers[\"Alembic Materials\"].enabled = false\n"
	//		//"if $.modifiers[\"Alembic Mesh Normals\"] != undefined then (\n"
	//		//"$.modifiers[\"Alembic Mesh Normals\"].enabled = true\n"
	//		//")\n"
	//		//"if $.modifiers[\"Alembic Mesh Topology\"] != undefined then (\n"
	//		//"$.modifiers[\"Alembic Mesh Topology\"].enabled = true\n",
	//		//")\n"
	//		,
	//		names.c_str()
	//);


    std::stringstream exeBuffer;

    exeBuffer<<GET_MAXSCRIPT_NODE(pNode);
    exeBuffer<<"AlembicMaterialModifier = EmptyModifier()\n";
    exeBuffer<<"AlembicMaterialModifier.name = \"Alembic Materials\"\n";

	exeBuffer<<"addmodifier mynode2113 AlembicMaterialModifier\n";

	exeBuffer<<"AlembicMaterialCA = attributes AlembicMaterialModifier\n";
	exeBuffer<<"(\n";	
	  exeBuffer<<"parameters AlembicMaterialPRM rollout:AlembicMaterialModifierRLT\n";
		exeBuffer<<"(\n";
        exeBuffer<<"_materials type:#stringTab tabSize:"<<faceSetNames.size()<<"\n";
		exeBuffer<<")\n";
	  exeBuffer<<"rollout AlembicMaterialModifierRLT \"Alembic Materials\"\n";
      exeBuffer<<"(\n";
	     exeBuffer<<"listbox eTestList \"\" items:#("<<names<<") \n";
      exeBuffer<<")\n";
	exeBuffer<<")\n";

	exeBuffer<<"custattributes.add mynode2113.modifiers[\"Alembic Materials\"] AlembicMaterialCA baseobject:false\n";

	for(size_t j=0;j<faceSetNames.size();j++)
	{
       const char* name = faceSetNames[j].c_str();
       exeBuffer<<"mynode2113.modifiers[#Alembic_Materials]._materials["<<(j+1)<<"] = \""<<name<<"\" \n";
	}

	exeBuffer<<"mynode2113.modifiers[\"Alembic Materials\"].enabled = false";


	ExecuteMAXScriptScript( EC_UTF8_to_TCHAR( (char*)exeBuffer.str().c_str() ) );

	//delete[] szBuffer;
}

int AlembicImport_PolyMesh(const std::string &path, AbcG::IObject& iObj, alembic_importoptions &options, INode** pMaxNode)
{
  ESS_PROFILE_FUNC();
	const std::string& identifier = iObj.getFullName();

	// Fill in the mesh
    alembic_fillmesh_options dataFillOptions;
    dataFillOptions.pIObj = &iObj;
    dataFillOptions.pMNMesh = NULL;
    dataFillOptions.dTicks = GET_MAX_INTERFACE()->GetTime();

    dataFillOptions.nDataFillFlags = ALEMBIC_DATAFILL_VERTEX|ALEMBIC_DATAFILL_FACELIST;
    dataFillOptions.nDataFillFlags |= options.importNormals ? ALEMBIC_DATAFILL_NORMALS : 0;
    dataFillOptions.nDataFillFlags |= options.importUVs ? ALEMBIC_DATAFILL_UVS : 0;
    dataFillOptions.nDataFillFlags |= options.importBboxes ? ALEMBIC_DATAFILL_BOUNDINGBOX : 0;
    dataFillOptions.nDataFillFlags |= options.importMaterialIds ? ALEMBIC_DATAFILL_MATERIALIDS : 0;

    // Create the poly or tri object and place it in the scene
    // Need to use the attach to existing import flag here 
	Object *newObject = NULL;
	AbcG::IPolyMesh objMesh;
	AbcG::ISubD objSubD;

	if( AbcG::IPolyMesh::matches(iObj.getMetaData()) ) {

		objMesh = AbcG::IPolyMesh(iObj, Abc::kWrapExisting);
		if (!objMesh.valid())
		{
			return alembic_failure;
		}

		if( objMesh.getSchema().getNumSamples() == 0 ) {
			ESS_LOG_WARNING( "Alembic Mesh set has 0 samples, ignoring." );
			return alembic_failure;
		}

		//AbcG::IPolyMeshSchema::Sample polyMeshSample;
		//objMesh.getSchema().get(polyMeshSample, 0);

		//if (AlembicImport_IsPolyObject(polyMeshSample))
		//{
		//	
		//	PolyObject *pPolyObject = (PolyObject *) GetPolyObjDescriptor()->Create();
		//	dataFillOptions.pMNMesh = &(pPolyObject->GetMesh());
		//	newObject = pPolyObject;
		//}
		/*else
		{
			TriObject *pTriObj = (TriObject *) GetTriObjDescriptor()->Create();
			dataFillOptions.pMesh = &( pTriObj->GetMesh() );
			newObject = pTriObj;
		}*/
	}
	else if( AbcG::ISubD::matches(iObj.getMetaData()) )
	{
		objSubD = AbcG::ISubD(iObj, Abc::kWrapExisting);
		if (!objSubD.valid())
		{
			return alembic_failure;
		}

		if( objSubD.getSchema().getNumSamples() == 0 ) {
			ESS_LOG_WARNING( "Alembic SubD set has 0 samples, ignoring." );
			return alembic_failure;
		}

		//AbcG::ISubDSchema::Sample subDSample;
		//objSubD.getSchema().get(subDSample, 0);

		//PolyObject *pPolyObject = (PolyObject *) GetPolyObjDescriptor()->Create();
		//dataFillOptions.pMNMesh = &(pPolyObject->GetMesh());
		//newObject = pPolyObject;
	}
	else {
		return alembic_failure;
	}

   // Create the object pNode
	INode *pNode = *pMaxNode;
	bool bReplaceExistingModifiers = false;
	if(!pNode){
		Object* newObject = (PolyObject*)GetPolyObjDescriptor()->Create();
		if (newObject == NULL)
		{
			return alembic_failure;
		}
      Abc::IObject parent = iObj.getParent();
      std::string name = removeXfoSuffix(parent.getName().c_str());


      AbcU::Digest digest;
      bool bHashAvailable = false;
      if(options.enableInstances || options.enableReferences){
         bHashAvailable = iObj.getPropertiesHash(digest);
      }

      if(bHashAvailable){

         INode* cachedNode = InstanceMap_Get(digest);
         if(cachedNode)
         {
            INodeTab nodes;
            nodes.Append(1, &cachedNode);

            Point3 offset(0.0f, 0.0f, 0.0f);
            bool expandHierarchies = false;
            INodeTab resultSource;
            INodeTab resultTarget;
            
            if(options.enableInstances){
               GET_MAX_INTERFACE()->CloneNodes(nodes, offset, expandHierarchies, NODE_INSTANCE, &resultSource, &resultTarget);
            }
            else if(options.enableReferences){
               GET_MAX_INTERFACE()->CloneNodes(nodes, offset, expandHierarchies, NODE_REFERENCE, &resultSource, &resultTarget);
            }

            if(resultSource.Count() == resultTarget.Count() && resultTarget.Count() == 1){
                  
               *pMaxNode = resultTarget[0];

               return alembic_success;
            }
            else{
               return alembic_failure;
            }
         }
      }

      pNode = GET_MAX_INTERFACE()->CreateObjectNode(newObject, EC_UTF8_to_TCHAR(name.c_str()));

      if(bHashAvailable){
         InstanceMap_Add(digest, pNode);
      }
      
		if (pNode == NULL){
			return alembic_failure;
		}
		*pMaxNode = pNode;
	}
	else{
		bReplaceExistingModifiers = true;
	}

	TimeValue zero( 0 );

	std::vector<Modifier*> modifiersToEnable;

	bool isDynamicTopo = isAlembicMeshTopoDynamic( &iObj );
    if(options.loadGeometryInTopologyModifier){
      isDynamicTopo = true;
    }


	if( !FindModifier(pNode, "Alembic Metadata") ){
		importMetadata(pNode, iObj);
	}

	if( !FindModifier(pNode, "Alembic Materials") ){
		addAlembicMaterialsModifier(pNode, iObj);
	}


   if(!options.attachToExisting && options.loadUserProperties){
      setupPropertyModifiers(iObj, *pMaxNode, path, iObj.getFullName(), options, std::string("Shape"));

      AbcG::IObject parentXform = iObj.getParent();
      if(parentXform.valid()){
         setupPropertyModifiers(parentXform, *pMaxNode, path, iObj.getFullName(), options);
      }
   }

	//alembicMeshInfo meshInfo;
	//meshInfo.open(path, identifier);
	//meshInfo.setSample(0);

	//ESS_LOG_INFO( "Node: " << pNode->GetName() );
	//ESS_LOG_INFO( "isDynamicTopo: " << isDynamicTopo );
	if( isAlembicMeshTopology( &iObj ) )
	{
		// Create the polymesh modifier
		Modifier *pModifier = NULL;
		bool bCreatedModifier = false;
		if(bReplaceExistingModifiers){
			pModifier = FindModifier(pNode, ALEMBIC_MESH_TOPO_MODIFIER_CLASSID, identifier.c_str());
		}
		if(!pModifier){
			pModifier = static_cast<Modifier*>
				(GET_MAX_INTERFACE()->CreateInstance(OSM_CLASS_ID, ALEMBIC_MESH_TOPO_MODIFIER_CLASSID));
			bCreatedModifier = true;
		}
		pModifier->DisableMod();

		// Set the alembic id
		pModifier->GetParamBlockByID( 0 )->SetValue( GetParamIdByName( pModifier, 0, "path" ), zero, EC_UTF8_to_TCHAR( path.c_str() ) );
		
		if(bCreatedModifier){

			pModifier->GetParamBlockByID( 0 )->SetValue( GetParamIdByName( pModifier, 0, "identifier" ), zero, EC_UTF8_to_TCHAR( identifier.c_str() ) );
			pModifier->GetParamBlockByID( 0 )->SetValue( GetParamIdByName( pModifier, 0, "time" ), zero, 0.0f );
			if( isDynamicTopo ) {
				pModifier->GetParamBlockByID( 0 )->SetValue( GetParamIdByName( pModifier, 0, "geometry" ), zero, TRUE );
				pModifier->GetParamBlockByID( 0 )->SetValue( GetParamIdByName( pModifier, 0, "normals" ), zero, ( options.importNormals ? TRUE : FALSE ) );
				pModifier->GetParamBlockByID( 0 )->SetValue( GetParamIdByName( pModifier, 0, "uvs" ), zero, FALSE );
			}
			else {
				pModifier->GetParamBlockByID( 0 )->SetValue( GetParamIdByName( pModifier, 0, "geometry" ), zero, FALSE );
				pModifier->GetParamBlockByID( 0 )->SetValue( GetParamIdByName( pModifier, 0, "normals" ), zero, FALSE );
				pModifier->GetParamBlockByID( 0 )->SetValue( GetParamIdByName( pModifier, 0, "uvs" ), zero, FALSE );
			}

			pModifier->GetParamBlockByID( 0 )->SetValue( GetParamIdByName( pModifier, 0, "muted" ), zero, FALSE );
         pModifier->GetParamBlockByID( 0 )->SetValue( GetParamIdByName( pModifier, 0, "materialIDs" ), zero, options.importMaterialIds );
		
			// Add the modifier to the pNode
			GET_MAX_INTERFACE()->AddModifier(*pNode, *pModifier);
		}

		if( isDynamicTopo ) {
            std::stringstream controllerName;
            controllerName<<GET_MAXSCRIPT_NODE(pNode);
            controllerName<<"mynode2113.modifiers[#Alembic_Mesh_Topology].time";
			AlembicImport_ConnectTimeControl( controllerName.str().c_str(), options );
		}

		modifiersToEnable.push_back( pModifier );
	}
	//bool isUVWContant = true;
	if( /*!isDynamicTopo &&*/ options.importUVs /*&& isAlembicMeshUVWs( &iObj, isUVWContant )*/ ) {
		//ESS_LOG_INFO( "isUVWContant: " << isUVWContant );

		AbcG::IV2fGeomParam meshUVsParam;
		if(objMesh.valid()){
			meshUVsParam = objMesh.getSchema().getUVsParam();
		}
		else{ 
			meshUVsParam = objSubD.getSchema().getUVsParam();
		}

		if(meshUVsParam.valid())
		{
			size_t numUVSamples = meshUVsParam.getNumSamples();
			Abc::V2fArraySamplePtr meshUVs = meshUVsParam.getExpandedValue(0).getVals();
			if(meshUVs->size() > 0)
			{
				// check if we have a uv set names prop
				std::vector<std::string> uvSetNames;
				if(objMesh.valid() && objMesh.getSchema().getPropertyHeader( ".uvSetNames" ) != NULL ){
					Abc::IStringArrayProperty uvSetNamesProp = Abc::IStringArrayProperty( objMesh.getSchema(), ".uvSetNames" );
					Abc::StringArraySamplePtr ptr = uvSetNamesProp.getValue(0);
					for(size_t i=0;i<ptr->size();i++){
						uvSetNames.push_back(ptr->get()[i].c_str());
					}
				}
				else if ( objSubD.valid() && objSubD.getSchema().getPropertyHeader( ".uvSetNames" ) != NULL ){
					Abc::IStringArrayProperty uvSetNamesProp = Abc::IStringArrayProperty( objSubD.getSchema(), ".uvSetNames" );
					Abc::StringArraySamplePtr ptr = uvSetNamesProp.getValue(0);
					for(size_t i=0;i<ptr->size();i++){
						uvSetNames.push_back(ptr->get()[i].c_str());
					}
				}

				if(uvSetNames.size() == 0){
					uvSetNames.push_back("Default");
				}

				for(int i=0; i<uvSetNames.size(); i++){

					int channelNumber = 0;
					std::string uvName = uvSetNames[i];
					if( ! parseTrailingNumber( uvName, "map", channelNumber ) ) {
						if( ! parseTrailingNumber( uvName, "channel_", channelNumber ) ) {
							if( ! parseTrailingNumber( uvName, "Channel_", channelNumber ) ) {
								channelNumber = (i+1);
							}
						}
					}
					std::stringstream identifierStream;					
					identifierStream<<identifier<<":"<<channelNumber;

					// Create the polymesh modifier
					Modifier *pModifier = NULL;
					bool bCreatedModifier = false;
					if(bReplaceExistingModifiers){
						pModifier = FindModifier(pNode, ALEMBIC_MESH_UVW_MODIFIER_CLASSID, identifierStream.str().c_str());
					}
					if(!pModifier){
						pModifier = static_cast<Modifier*>
							(GET_MAX_INTERFACE()->CreateInstance(OSM_CLASS_ID, ALEMBIC_MESH_UVW_MODIFIER_CLASSID));
						bCreatedModifier = true;
					}
					pModifier->DisableMod();
					
					pModifier->GetParamBlockByID( 0 )->SetValue( GetParamIdByName( pModifier, 0, "path" ), zero, EC_UTF8_to_TCHAR( path.c_str() ) );
					
					if(bCreatedModifier){

						pModifier->GetParamBlockByID( 0 )->SetValue( GetParamIdByName( pModifier, 0, "identifier" ), zero, EC_UTF8_to_TCHAR( identifierStream.str().c_str() ) );
						pModifier->GetParamBlockByID( 0 )->SetValue( GetParamIdByName( pModifier, 0, "time" ), zero, 0.0f );
						pModifier->GetParamBlockByID( 0 )->SetValue( GetParamIdByName( pModifier, 0, "muted" ), zero, FALSE );

						// Add the modifier to the pNode
						GET_MAX_INTERFACE()->AddModifier(*pNode, *pModifier);
					}

                    AbcG::IV2fGeomParam meshUvParam = getMeshUvParam(i, objMesh, objSubD);

					if( !meshUvParam.isConstant() ) {
                        std::stringstream controllerName;
                        controllerName<<GET_MAXSCRIPT_NODE(pNode);
                        controllerName<<"mynode2113.modifiers[#Alembic_Mesh_UVW].time";
						AlembicImport_ConnectTimeControl( controllerName.str().c_str(), options );
					}

					modifiersToEnable.push_back( pModifier );
				}
			}
		}


	}
	bool isGeomContant = true;
	if( ( ! isDynamicTopo ) && isAlembicMeshPositions( &iObj, isGeomContant ) ) {
		//ESS_LOG_INFO( "isGeomContant: " << isGeomContant );
		
		Modifier *pModifier = NULL;
		bool bCreatedModifier = false;
		if(bReplaceExistingModifiers){
			pModifier = FindModifier(pNode, ALEMBIC_MESH_GEOM_MODIFIER_CLASSID, identifier.c_str());
		}
		if(!pModifier){
			pModifier = static_cast<Modifier*>
				(GET_MAX_INTERFACE()->CreateInstance(OSM_CLASS_ID, ALEMBIC_MESH_GEOM_MODIFIER_CLASSID));
			bCreatedModifier = true;
		}

		pModifier->DisableMod();

		pModifier->GetParamBlockByID( 0 )->SetValue( GetParamIdByName( pModifier, 0, "path" ), zero, EC_UTF8_to_TCHAR( path.c_str() ) );

		if(bCreatedModifier){

			pModifier->GetParamBlockByID( 0 )->SetValue( GetParamIdByName( pModifier, 0, "identifier" ), zero, EC_UTF8_to_TCHAR( identifier.c_str() ) );
			pModifier->GetParamBlockByID( 0 )->SetValue( GetParamIdByName( pModifier, 0, "geoAlpha" ), zero, 1.0f );
			pModifier->GetParamBlockByID( 0 )->SetValue( GetParamIdByName( pModifier, 0, "time" ), zero, 0.0f );
			pModifier->GetParamBlockByID( 0 )->SetValue( GetParamIdByName( pModifier, 0, "muted" ), zero, FALSE );
		
			// Add the modifier to the pNode
			GET_MAX_INTERFACE()->AddModifier(*pNode, *pModifier);
		}

		if( ! isGeomContant ) {
            std::stringstream controllerName;
            controllerName<<GET_MAXSCRIPT_NODE(pNode);
            controllerName<<"mynode2113.modifiers[#Alembic_Mesh_Geometry].time";
			AlembicImport_ConnectTimeControl( controllerName.str().c_str(), options );
		}

		modifiersToEnable.push_back( pModifier );
	}
	bool isNormalsContant = true;
	if( ( ! isDynamicTopo ) && isAlembicMeshNormals( &iObj, isNormalsContant ) ) {
		//ESS_LOG_INFO( "isNormalsContant: " << isNormalsContant );
		
		Modifier *pModifier = NULL;
		bool bCreatedModifier = false;
		if(bReplaceExistingModifiers){
			pModifier = FindModifier(pNode, ALEMBIC_MESH_NORMALS_MODIFIER_CLASSID, identifier.c_str());
		}
		if(!pModifier){
			pModifier = static_cast<Modifier*>
				(GET_MAX_INTERFACE()->CreateInstance(OSM_CLASS_ID, ALEMBIC_MESH_NORMALS_MODIFIER_CLASSID));
			bCreatedModifier = true;
		}

		pModifier->DisableMod();

		pModifier->GetParamBlockByID( 0 )->SetValue( GetParamIdByName( pModifier, 0, "path" ), zero, EC_UTF8_to_TCHAR( path.c_str() ) );

		if(bCreatedModifier){

			pModifier->GetParamBlockByID( 0 )->SetValue( GetParamIdByName( pModifier, 0, "identifier" ), zero, EC_UTF8_to_TCHAR( identifier.c_str() ) );
			pModifier->GetParamBlockByID( 0 )->SetValue( GetParamIdByName( pModifier, 0, "time" ), zero, 0.0f );
			pModifier->GetParamBlockByID( 0 )->SetValue( GetParamIdByName( pModifier, 0, "muted" ), zero, FALSE );
		
			// Add the modifier to the pNode
			GET_MAX_INTERFACE()->AddModifier(*pNode, *pModifier);
		}

		if( ! isNormalsContant ) {	
            std::stringstream controllerName;
            controllerName<<GET_MAXSCRIPT_NODE(pNode);
            controllerName<<"mynode2113.modifiers[#Alembic_Mesh_Normals].time";
            AlembicImport_ConnectTimeControl( controllerName.str().c_str(), options );
		}

		if( options.importNormals ) {
			modifiersToEnable.push_back( pModifier );
		}
	}

	if( AbcG::ISubD::matches(iObj.getMetaData()) && options.enableMeshSmooth )
	{
        //char* szBuffer =   "addmodifier $ (meshsmooth())\n"
        //                   "$.modifiers[#MeshSmooth].enabledInViews = false\n"
        //                   "$.modifiers[#MeshSmooth].iterations = 1\n";

        std::stringstream exeBuffer;
        exeBuffer<<GET_MAXSCRIPT_NODE(pNode);
        exeBuffer<<"addmodifier mynode2113 (meshsmooth())\n";
        exeBuffer<<"mynode2113.modifiers[#MeshSmooth].enabledInViews = false\n";
        exeBuffer<<"mynode2113.modifiers[#MeshSmooth].iterations = 1\n";     

		ExecuteMAXScriptScript( EC_UTF8_to_TCHAR( (char*)exeBuffer.str().c_str() ) );
	}

    // Add the new inode to our current scene list
   // SceneEntry *pEntry = options.sceneEnumProc.Append(pNode, newObject, OBTYPE_MESH, &std::string(iObj.getFullName())); 
    //options.currentSceneList.Append(pEntry);

    // Set the visibility controller
    AlembicImport_SetupVisControl( path.c_str(), identifier.c_str(), iObj, pNode, options);

	for( int i = 0; i < modifiersToEnable.size(); i ++ ) {
		modifiersToEnable[i]->EnableMod();
	}


	return 0;
}
