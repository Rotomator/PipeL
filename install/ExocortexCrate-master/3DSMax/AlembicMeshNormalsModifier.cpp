#include "stdafx.h"
#include "Alembic.h"
#include "AlembicDefinitions.h"
#include "AlembicMeshNormalsModifier.h" 
#include "AlembicArchiveStorage.h"
#include "utility.h"
#include "AlembicXForm.h"
#include "AlembicVisibilityController.h"

using namespace MaxSDK::AssetManagement;

IObjParam *AlembicMeshNormalsModifier::ip              = NULL;
AlembicMeshNormalsModifier *AlembicMeshNormalsModifier::editMod         = NULL;

static AlembicMeshNormalsModifierClassDesc AlembicMeshNormalsModifierDesc;
ClassDesc2* GetAlembicMeshNormalsModifierClassDesc() {return &AlembicMeshNormalsModifierDesc;}

//--- Properties block -------------------------------

static ParamBlockDesc2 AlembicMeshNormalsModifierParams(
	0,
	_T(ALEMBIC_MESH_NORMALS_MODIFIER_SCRIPTNAME),
	0,
	GetAlembicMeshNormalsModifierClassDesc(),
	P_AUTO_CONSTRUCT | P_AUTO_UI,
	0,

	// rollout description 
	IDD_ALEMBIC_MESH_NORMALS_PARAMS, IDS_ALEMBIC, 0, 0, NULL,

    // params
	AlembicMeshNormalsModifier::ID_PATH, _T("path"), TYPE_FILENAME, P_RESET_DEFAULT, IDS_PATH,
	    p_default, "",
	    p_ui,        TYPE_EDITBOX,		IDC_PATH_EDIT,
		p_assetTypeID,	kExternalLink,
		p_end,
        
	AlembicMeshNormalsModifier::ID_IDENTIFIER, _T("identifier"), TYPE_STRING, P_RESET_DEFAULT, IDS_IDENTIFIER,
	    p_default, "",
	    p_ui,        TYPE_EDITBOX,		IDC_IDENTIFIER_EDIT,
		p_end,

	AlembicMeshNormalsModifier::ID_TIME, _T("time"), TYPE_FLOAT, P_ANIMATABLE, IDS_TIME,
		p_default,       0.0f,
		p_range,         0.0f, 1000.0f,
		p_ui,            TYPE_SPINNER,       EDITTYPE_FLOAT, IDC_TIME_EDIT,    IDC_TIME_SPIN,  0.01f,
		p_end,

	/*AlembicMeshNormalsModifier::ID_GEOALPHA, _T("geoAlpha"), TYPE_FLOAT, P_ANIMATABLE, IDS_GEOALPHA,
		p_default,       1.0f,
		p_range,         0.0f, 1.0f,
		p_ui,            TYPE_SPINNER,       EDITTYPE_FLOAT, IDC_GEOALPHA_EDIT,    IDC_GEOALPHA_SPIN, 0.1f,
		p_end,*/

	AlembicMeshNormalsModifier::ID_MUTED, _T("muted"), TYPE_BOOL, P_ANIMATABLE, IDS_MUTED,
		p_default,       TRUE,
		p_ui,            TYPE_SINGLECHEKBOX,  IDC_MUTED_CHECKBOX,
		p_end,

	p_end
);

//--- Modifier methods -------------------------------

AlembicMeshNormalsModifier::AlembicMeshNormalsModifier() 
{
    pblock = NULL;
    m_CachedAbcFile = "";

	GetAlembicMeshNormalsModifierClassDesc()->MakeAutoParamBlocks(this);
}

AlembicMeshNormalsModifier::~AlembicMeshNormalsModifier()
{
    delRefArchive(m_CachedAbcFile);
}

RefTargetHandle AlembicMeshNormalsModifier::Clone(RemapDir& remap) 
{
	AlembicMeshNormalsModifier *mod = new AlembicMeshNormalsModifier();

    mod->ReplaceReference (0, remap.CloneRef(pblock));
	
    BaseClone(this, mod, remap);
	return mod;
}



void AlembicMeshNormalsModifier::EnumAuxFiles(AssetEnumCallback& nameEnum, DWORD flags)  {
	if ((flags&FILE_ENUM_CHECK_AWORK1)&&TestAFlag(A_WORK1)) return; // LAM - 4/11/03

	if (!(flags&FILE_ENUM_INACTIVE)) return; // not needed by renderer

	if(flags & FILE_ENUM_ACCESSOR_INTERFACE)	{
		IEnumAuxAssetsCallback* callback = static_cast<IEnumAuxAssetsCallback*>(&nameEnum);
		callback->DeclareAsset(AlembicPathAccessor(this));		
	}
	//else {
	//	IPathConfigMgr::GetPathConfigMgr()->RecordInputAsset( this->GetParamBlockByID( 0 )->GetAssetUser( GetParamIdByName( this, 0, "path" ), 0 ), nameEnum, flags);
	//}

	ReferenceTarget::EnumAuxFiles(nameEnum, flags);
} 

void AlembicMeshNormalsModifier::ModifyObject (TimeValue t, ModContext &mc, ObjectState *os, INode *node) 
{
	ESS_CPP_EXCEPTION_REPORTING_START
ESS_PROFILE_FUNC();
	
	Interval interval = FOREVER;//os->obj->ObjectValidity(t);
	//ESS_LOG_INFO( "Interval Start: " << interval.Start() << " End: " << interval.End() );

    MCHAR const* strPath = NULL;
	this->pblock->GetValue( AlembicMeshNormalsModifier::ID_PATH, t, strPath, interval);

	MCHAR const* strIdentifier = NULL;
	this->pblock->GetValue( AlembicMeshNormalsModifier::ID_IDENTIFIER, t, strIdentifier, interval);
 
	float fTime;
	this->pblock->GetValue( AlembicMeshNormalsModifier::ID_TIME, t, fTime, interval);

	BOOL bTopology = false;
	BOOL bGeometry = false;

	float fGeoAlpha = 1.0f;
	//this->pblock->GetValue( AlembicMeshNormalsModifier::ID_GEOALPHA, t, fGeoAlpha, interval);

	BOOL bNormals = true;
	BOOL bUVs = false;

	BOOL bMuted;
	this->pblock->GetValue( AlembicMeshNormalsModifier::ID_MUTED, t, bMuted, interval);
	
	//ESS_LOG_INFO( "AlembicMeshNormalsModifier::ModifyObject strPath: " << strPath << " strIdentifier: " << strIdentifier << " fTime: " << fTime << 
	//	" bTopology: " << bTopology << " bGeometry: " << bGeometry << " bNormals: " << bNormals << " bUVs: " << bUVs << " bMuted: " << bMuted );

	if( !bMuted && strPath && strIdentifier) {

		std::string szPath = EC_MCHAR_to_UTF8( strPath );
		std::string szIdentifier = EC_MCHAR_to_UTF8( strIdentifier );

		AbcG::IObject iObj;
		try {
			iObj = getObjectFromArchive(szPath, szIdentifier);
		} catch( std::exception exp ) {
			extern bool g_hasModifierErrorOccurred;
			g_hasModifierErrorOccurred = true;
			ESS_LOG_ERROR( "Can not open Alembic data stream.  Path: " << szPath << " identifier: " << szIdentifier << " reason: " << exp.what() );
			return;
		}

		if(!iObj.valid()) {
			extern bool g_hasModifierErrorOccurred;
			g_hasModifierErrorOccurred = true;
			ESS_LOG_ERROR( "Not a valid Alembic data stream.  Path: " << szPath << " identifier: " << szIdentifier );
			return;
		}

	   alembic_fillmesh_options options;
	   options.fileName = szPath;
	   options.pObjectCache = getObjectCacheFromArchive(szPath, szIdentifier);
	   options.identifier = szIdentifier;
	   options.pIObj = &iObj;
	   options.dTicks = GetTimeValueFromSeconds( fTime );
	   options.nDataFillFlags = 0;
	   options.fVertexAlpha = fGeoAlpha;
		if( bTopology ) {
		   options.nDataFillFlags |= ALEMBIC_DATAFILL_FACELIST;
			options.nDataFillFlags |= ALEMBIC_DATAFILL_MATERIALIDS;
	   }
	   if( bGeometry ) {
		   options.nDataFillFlags |= ALEMBIC_DATAFILL_VERTEX;
	   }
	   if( bNormals ) {
		   options.nDataFillFlags |= ALEMBIC_DATAFILL_NORMALS;
	   }
	   if( bUVs ) {
			options.nDataFillFlags |= ALEMBIC_DATAFILL_UVS;
	   }
  
		options.pObject = os->obj;

	  // Find out if we are modifying a poly object or a tri object
	   if (os->obj->ClassID() == Class_ID(POLYOBJ_CLASS_ID, 0) )
	   {
		   PolyObject *pPolyObj = reinterpret_cast<PolyObject *>(os->obj );

		   options.pMNMesh = &( pPolyObj->GetMesh() );
	   }
	   else if (os->obj->CanConvertToType(Class_ID(POLYOBJ_CLASS_ID, 0)))
	   {
		   PolyObject *pPolyObj = reinterpret_cast<PolyObject *>(os->obj->ConvertToType(t, Class_ID(POLYOBJ_CLASS_ID, 0)));

		   options.pMNMesh = &( pPolyObj->GetMesh() );
    
		   if (os->obj != pPolyObj) {
			  os->obj = pPolyObj;
			  os->obj->UnlockObject();
		   }

	   }
	   else {
			extern bool g_hasModifierErrorOccurred;
			g_hasModifierErrorOccurred = true;
  			ESS_LOG_ERROR( "Can not convert internal mesh data into a PolyObject, confused." );
			return;
	   }

	   try {
		   AlembicImport_FillInPolyMesh(options);
	   }
	   catch(std::exception exp ) {
			extern bool g_hasModifierErrorOccurred;
			g_hasModifierErrorOccurred = true;
			ESS_LOG_ERROR( "Error reading mesh from Alembic data stream.  Path: " << strPath << " identifier: " << strIdentifier << " reason: " << exp.what() );
			return;
	   }
   }

   // update the validity channel
    if( bTopology || bNormals ) {
		os->obj->UpdateValidity(TOPO_CHAN_NUM, interval);
		os->obj->UpdateValidity(GEOM_CHAN_NUM, interval);
	}
    if( bGeometry ) {
		os->obj->UpdateValidity(GEOM_CHAN_NUM, interval);
	}
    if( bUVs ) {
		os->obj->UpdateValidity(TEXMAP_CHAN_NUM, interval);
   }

   	ESS_CPP_EXCEPTION_REPORTING_END
}
#if crate_Max_Version == 2015
RefResult AlembicMeshNormalsModifier::NotifyRefChanged (const Interval& changeInt, RefTargetHandle hTarget,
										   PartID& partID, RefMessage message, BOOL propagate) {
#else
RefResult AlembicMeshNormalsModifier::NotifyRefChanged (Interval changeInt, RefTargetHandle hTarget,
										   PartID& partID, RefMessage message) {
#endif
	ESS_CPP_EXCEPTION_REPORTING_START

	switch (message) 
    {
    case REFMSG_CHANGE:
        {
            if (hTarget == pblock) 
            {
                ParamID changing_param = pblock->LastNotifyParamID();
                switch(changing_param)
                {
                case ID_PATH:
                    {
                        delRefArchive(m_CachedAbcFile);
                        MCHAR const* strPath = NULL;
                        TimeValue t = GetCOREInterface()->GetTime();
						Interval v;
                        pblock->GetValue( AlembicMeshNormalsModifier::ID_PATH, t, strPath, v);
                        m_CachedAbcFile = EC_MCHAR_to_UTF8( strPath );
                        addRefArchive(m_CachedAbcFile);
                    }
                    break;
                default:
                    break;
                }

                AlembicMeshNormalsModifierParams.InvalidateUI(changing_param);
            }
        }
        break;
	 
    case REFMSG_WANT_SHOWPARAMLEVEL:

        break;
	}

	ESS_CPP_EXCEPTION_REPORTING_END

	return REF_SUCCEED;
}


void AlembicMeshNormalsModifier::BeginEditParams(IObjParam *ip,ULONG flags,Animatable *prev)
{
	this->ip = ip;
    editMod  = this;

	AlembicMeshNormalsModifierDesc.BeginEditParams(ip, this, flags, prev);
}

void AlembicMeshNormalsModifier::EndEditParams( IObjParam *ip, ULONG flags, Animatable *next )
{
	AlembicMeshNormalsModifierDesc.EndEditParams(ip, this, flags, next);

	this->ip = NULL;
    editMod  = NULL;
}