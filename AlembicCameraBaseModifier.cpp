#include "Alembic.h"
#include "AlembicMax.h"
#include "AlembicDefinitions.h"
#include "AlembicCameraBaseModifier.h"
#include "AlembicArchiveStorage.h"
#include "utility.h"
#include "AlembicVisibilityController.h"
#include "AlembicNames.h"
#include "AlembicFloatController.h"
#include "AlembicMAXScript.h"

 
//////////////////////////////////////////////////////////////////////////////////////////
// Import options struct containing the information necessary to fill the camera object
typedef struct _alembic_fillcamera_options
{
public:
    _alembic_fillcamera_options();

    Alembic::AbcGeom::IObject  *pIObj;
    GenCamera				*pCameraObj;
    TimeValue                   dTicks;
}
alembic_fillcamera_options;

_alembic_fillcamera_options::_alembic_fillcamera_options()
{
    pIObj = NULL;
    pCameraObj = NULL;
    dTicks = 0;
}

 
//////////////////////////////////////////////////////////////////////////////////////////
// Function Prototypes
void AlembicImport_FillInCamera(alembic_fillcamera_options &options);


//--- ClassDescriptor and class vars ---------------------------------

IObjParam *AlembicCameraBaseModifier::ip                    = NULL;
AlembicCameraBaseModifier *AlembicCameraBaseModifier::editMod   = NULL;

static AlembicCameraBaseModifierClassDesc s_AlembicCameraBaseModifierDesc;
ClassDesc2* GetAlembicCameraBaseModifierClassDesc() { return &s_AlembicCameraBaseModifierDesc; }

//--- Properties block -------------------------------

static ParamBlockDesc2 AlembicCameraBaseModifierParams(
	0,
	_T("AlembicCameraBaseModifier"),
	IDS_PROPS, 
	GetAlembicCameraBaseModifierClassDesc(),
	P_AUTO_CONSTRUCT | P_AUTO_UI,
	0,

	// rollout description
	IDD_EMPTY, IDS_PARAMS, 0, 0, NULL,

    // params
	AlembicCameraBaseModifier::ID_PATH, _T("path"), TYPE_FILENAME, 0, IDS_PATH,
		p_end,
        
	AlembicCameraBaseModifier::ID_IDENTIFIER, _T("identifier"), TYPE_STRING, 0, IDS_IDENTIFIER,
		p_end,

	AlembicCameraBaseModifier::ID_CURRENTTIMEHIDDEN, _T("currentTimeHidden"), TYPE_FLOAT, 0, IDS_CURRENTTIMEHIDDEN,
		p_end,

	AlembicCameraBaseModifier::ID_TIMEOFFSET, _T("timeOffset"), TYPE_FLOAT, 0, IDS_TIMEOFFSET,
		p_end,

	AlembicCameraBaseModifier::ID_TIMESCALE, _T("timeScale"), TYPE_FLOAT, 0, IDS_TIMESCALE,
		p_end,

		/*
	AlembicCameraBaseModifier::ID_MUTED, _T("muted"), TYPE_BOOL, 0, IDS_MUTED,
		p_end,
		*/

	p_end
);

//--- Modifier methods -------------------------------

AlembicCameraBaseModifier::AlembicCameraBaseModifier()
{
	pblock = NULL;
	GetAlembicCameraBaseModifierClassDesc()->MakeAutoParamBlocks(this);
}

RefTargetHandle AlembicCameraBaseModifier::Clone(RemapDir& remap)
{
	AlembicCameraBaseModifier *mod = new AlembicCameraBaseModifier();

    mod->ReplaceReference (0, remap.CloneRef(pblock));
	
    BaseClone(this, mod, remap);
	return mod;
}

Interval AlembicCameraBaseModifier::GetValidity (TimeValue t)
{
	Interval ret = FOREVER;
	pblock->GetValidity (t, ret);
	return ret;
}

void AlembicCameraBaseModifier::ModifyObject(TimeValue t, ModContext &mc, ObjectState *os, INode *node) 
{
	ESS_CPP_EXCEPTION_REPORTING_START

	Interval interval = os->obj->ObjectValidity(t);

	TimeValue now = GET_MAX_INTERFACE()->GetTime();

	MCHAR const* strPath = NULL;
	this->pblock->GetValue( AlembicCameraBaseModifier::ID_PATH, t, strPath, interval);

	MCHAR const* strIdentifier = NULL;
	this->pblock->GetValue( AlembicCameraBaseModifier::ID_IDENTIFIER, t, strIdentifier, interval);
 
	float fCurrentTimeHidden;
	this->pblock->GetValue( AlembicCameraBaseModifier::ID_CURRENTTIMEHIDDEN, t, fCurrentTimeHidden, interval);

	float fTimeOffset;
	this->pblock->GetValue( AlembicCameraBaseModifier::ID_TIMEOFFSET, t, fTimeOffset, interval);

	float fTimeScale;
	this->pblock->GetValue( AlembicCameraBaseModifier::ID_TIMESCALE, t, fTimeScale, interval); 

	float dataTime = fTimeOffset + fCurrentTimeHidden * fTimeScale;
	
	ESS_LOG_INFO( "AlembicCameraBaseModifier::ModifyObject strPath: " << strPath << " strIdentifier: " << strIdentifier << " fCurrentTimeHidden: " << fCurrentTimeHidden << " fTimeOffset: " << fTimeOffset << " fTimeScale: " << fTimeScale );

	if( strlen( strPath ) == 0 ) {
	   ESS_LOG_ERROR( "No filename specified." );
	   return;
	}
	if( strlen( strIdentifier ) == 0 ) {
	   ESS_LOG_ERROR( "No path specified." );
	   return;
	}

	if( ! fs::exists( strPath ) ) {
		ESS_LOG_ERROR( "Can't file Alembic file.  Path: " << strPath );
		return;
	}

	Alembic::AbcGeom::IObject iObj;
	try {
		iObj = getObjectFromArchive(strPath, strIdentifier);
	} catch( std::exception exp ) {
		ESS_LOG_ERROR( "Can not open Alembic data stream.  Path: " << strPath << " identifier: " << strIdentifier << " reason: " << exp.what() );
		return;
	}

	if(!iObj.valid()) {
		ESS_LOG_ERROR( "Not a valid Alembic data stream.  Path: " << strPath << " identifier: " << strIdentifier );
		return;
	}

    alembic_fillcamera_options options;
    options.pIObj = &iObj;
    options.pCameraObj = NULL;
    options.dTicks = t;

	// Find out if we are modifying a poly object or a tri object
	ESS_LOG_INFO( "SuperclassID: " << os->obj->SuperClassID() );
	MSTR pClassName;
	os->obj->GetClassName( pClassName );
	ESS_LOG_INFO( "GetClassName: " << pClassName  );
	ESS_LOG_INFO( "ClassID: " << os->obj->ClassID().PartA() << " " <<  os->obj->ClassID().PartB() );
	ESS_LOG_INFO( "GetObjectName: " << os->obj->GetObjectName() );
	MSTR szName;
	
	ESS_LOG_INFO( "os->obj->CanConvertToType(Class_ID(SIMPLE_CAM_CLASS_ID,0)): " << os->obj->CanConvertToType(Class_ID(SIMPLE_CAM_CLASS_ID,0)) );
	ESS_LOG_INFO( "os->obj->CanConvertToType(Class_ID(CAMERA_CLASS_ID,0)): " << os->obj->CanConvertToType(Class_ID(CAMERA_CLASS_ID,0)) );
	ESS_LOG_INFO( "os->obj->CanConvertToType(Class_ID(LOOKAT_CAM_CLASS_ID,0)): " << os->obj->CanConvertToType(Class_ID(LOOKAT_CAM_CLASS_ID,0)) );
	

	if( os->obj->CanConvertToType(Class_ID(SIMPLE_CAM_CLASS_ID,0)) ) {
	   GenCamera *pCameraObj = reinterpret_cast<GenCamera *>(os->obj->ConvertToType(now, Class_ID (SIMPLE_CAM_CLASS_ID, 0)));
	   options.pCameraObj = pCameraObj;
	}
    
   if( options.pCameraObj == NULL ) {
		ESS_LOG_ERROR( "Error converting target into a CameraObject." );
		return;
   }

   try {
		AlembicImport_FillInCamera(options);
	}
   catch(std::exception exp ) {
		ESS_LOG_ERROR( "Error reading camera from Alembic data stream.  Path: " << strPath << " identifier: " << strIdentifier << " reason: " << exp.what() );
		return;
   }
    // Determine the validity interval
    os->obj->UpdateValidity(DISP_ATTRIB_CHAN_NUM, interval);

	ESS_CPP_EXCEPTION_REPORTING_END
}

RefResult AlembicCameraBaseModifier::NotifyRefChanged(Interval changeInt, RefTargetHandle hTarget,
										   PartID& partID, RefMessage message)
{
	switch (message)
    {
	case REFMSG_CHANGE:
		if (editMod != this)
        {
            break;
        }
        AlembicCameraBaseModifierParams.InvalidateUI(pblock->LastNotifyParamID());
		break;
	}

	return REF_SUCCEED;
}



void AlembicImport_FillInCamera(alembic_fillcamera_options &options)
{
   	ESS_CPP_EXCEPTION_REPORTING_START

	ESS_LOG_INFO( "AlembicImport_FillInCamera" );

	if (options.pCameraObj == NULL ||
        !Alembic::AbcGeom::ICamera::matches((*options.pIObj).getMetaData()))
    {
        return;
    }

    Alembic::AbcGeom::ICamera objCamera = Alembic::AbcGeom::ICamera(*options.pIObj, Alembic::Abc::kWrapExisting);
    if (!objCamera.valid())
    {
        return;
    }

    double sampleTime = GetSecondsFromTimeValue(options.dTicks);
    SampleInfo sampleInfo = getSampleInfo(sampleTime,
                                          objCamera.getSchema().getTimeSampling(),
                                          objCamera.getSchema().getNumSamples());
    Alembic::AbcGeom::CameraSample sample;
    objCamera.getSchema().get(sample, sampleInfo.floorIndex);

    // Extract the camera values from the sample
    //double focalLength = sample.getFocalLength();
    //double fov = sample.getHorizontalAperture();
    //double nearClipping = sample.getNearClippingPlane();
    //double farClipping = sample.getFarClippingPlane();

    // Blend the camera values, if necessary
    //if (sampleInfo.alpha != 0.0)
    //{
    //    objCamera.getSchema().get(sample, sampleInfo.ceilIndex);
    //    focalLength = (1.0 - sampleInfo.alpha) * focalLength + sampleInfo.alpha * sample.getFocalLength();
    //    fov = (1.0 - sampleInfo.alpha) * fov + sampleInfo.alpha * sample.getHorizontalAperture();
    //    nearClipping = (1.0 - sampleInfo.alpha) * nearClipping + sampleInfo.alpha * sample.getNearClippingPlane();
    //    farClipping = (1.0 - sampleInfo.alpha) * farClipping + sampleInfo.alpha * sample.getFarClippingPlane();
    //}

    //options.pCameraObj->SetTDist(options.dTicks, static_cast<float>(focalLength));
   // options.pCameraObj->SetFOVType(0);  // Width FoV = 0
    //options.pCameraObj->SetFOV(options.dTicks, static_cast<float>(fov));
    //options.pCameraObj->SetClipDist(options.dTicks, CAM_HITHER_CLIP, static_cast<float>(nearClipping));
    //options.pCameraObj->SetClipDist(options.dTicks, CAM_YON_CLIP, static_cast<float>(farClipping));

    options.pCameraObj->SetManualClip(TRUE);

	ESS_CPP_EXCEPTION_REPORTING_END
}



AlembicFloatController* createFloatController(const std::string &path, const std::string &identifier, const std::string& prop)
{
    // Create the xform modifier
    AlembicFloatController *pControl = static_cast<AlembicFloatController*>
        (GetCOREInterface()->CreateInstance(CTRL_FLOAT_CLASS_ID, ALEMBIC_FLOAT_CONTROLLER_CLASSID));

	if(!pControl){
		return NULL;
	}

	TimeValue zero( 0 );

	// Set the alembic id
    pControl->GetParamBlockByID( 0 )->SetValue( GetParamIdByName( pControl, 0, "path" ), zero, path.c_str());
	pControl->GetParamBlockByID( 0 )->SetValue( GetParamIdByName( pControl, 0, "identifier" ), zero, identifier.c_str() );
	pControl->GetParamBlockByID( 0 )->SetValue( GetParamIdByName( pControl, 0, "property" ), zero, prop.c_str() );
	pControl->GetParamBlockByID( 0 )->SetValue( GetParamIdByName( pControl, 0, "time" ), zero, 0.0f );
    pControl->GetParamBlockByID( 0 )->SetValue( GetParamIdByName( pControl, 0, "muted" ), zero, FALSE );

	return pControl;
}

bool assignControllerToLevel1SubAnim(Animatable* controller, Animatable* pObj, int i, int j)
{

	if( i >= pObj->NumSubs() ){
		ESS_LOG_WARNING("Level 0 NumAnimSubs exceeded. Controller not assigned.");
		return false;
	}

	Animatable* an = pObj->SubAnim(i);

	if( j >= an->NumSubs() ){
		ESS_LOG_WARNING("Level 1 NumAnimSubs exceeded. Controller not assigned.");
		return false;
	}

	an->AssignController(controller, j);

	return true;
}

int AlembicImport_Camera(const std::string &path, Alembic::AbcGeom::IObject& iObj, alembic_importoptions &options, INode** pMaxNode)
{
	const std::string &identifier = iObj.getFullName();

	if (!Alembic::AbcGeom::ICamera::matches(iObj.getMetaData())){
        return alembic_failure;
    }
    Alembic::AbcGeom::ICamera objCamera = Alembic::AbcGeom::ICamera(iObj, Alembic::Abc::kWrapExisting);
    if (!objCamera.valid()){
        return alembic_failure;
    }
	bool isConstant = objCamera.getSchema().isConstant();


	// Create the camera object and place it in the scene
    GenCamera *pCameraObj = GET_MAX_INTERFACE()->CreateCameraObject(FREE_CAMERA);
    if (pCameraObj == NULL)
    {
        return alembic_failure;
    }
    pCameraObj->Enable(TRUE);
    pCameraObj->SetConeState(TRUE);

    // Fill in the mesh
    alembic_fillcamera_options dataFillOptions;
    dataFillOptions.pIObj = &iObj;
    dataFillOptions.pCameraObj = pCameraObj;
    dataFillOptions.dTicks =  GET_MAX_INTERFACE()->GetTime();
	AlembicImport_FillInCamera(dataFillOptions);

    // Create the object node
	INode *pNode = GET_MAX_INTERFACE()->CreateObjectNode(pCameraObj, iObj.getName().c_str());
	if (pNode == NULL)
    {
		return alembic_failure;
    }
	*pMaxNode = pNode;


	GET_MAX_INTERFACE()->SelectNode( pNode );
	if(assignControllerToLevel1SubAnim(createFloatController(path, identifier, std::string("horizontalFOV")), pCameraObj, 0, 0) && !isConstant){
		AlembicImport_ConnectTimeControl( "$.FOV.controller.time", options );
	}
	if(assignControllerToLevel1SubAnim(createFloatController(path, identifier, std::string("FocusDistance")), pCameraObj, 0, 1) && !isConstant){
		AlembicImport_ConnectTimeControl( "$.targetDistance.controller.time", options );
	}	
	if(assignControllerToLevel1SubAnim(createFloatController(path, identifier, std::string("NearClippingPlane")), pCameraObj, 0, 2) && !isConstant){
		AlembicImport_ConnectTimeControl( "$.nearclip.controller.time", options );
	}	
	if(assignControllerToLevel1SubAnim(createFloatController(path, identifier, std::string("FarClippingPlane")), pCameraObj, 0, 3) && !isConstant){
		AlembicImport_ConnectTimeControl( "$.farclip.controller.time", options );
	}

	////Create the Camera modifier
	//Modifier *pModifier = static_cast<Modifier*>
	//	(GET_MAX_INTERFACE()->CreateInstance(OSM_CLASS_ID, ALEMBIC_CAMERA_MODIFIER_CLASSID));

	//TimeValue now =  GET_MAX_INTERFACE()->GetTime();

	////Set the alembic id
	//pModifier->GetParamBlockByID( 0 )->SetValue( GetParamIdByName( pModifier, 0, "path" ), now, path.c_str());
	//pModifier->GetParamBlockByID( 0 )->SetValue( GetParamIdByName( pModifier, 0, "identifier" ), now, identifier.c_str() );
	//
	////Set the alembic id
	////pModifier->SetCamera(pCameraObj);

	////Add the modifier to the node
	//GET_MAX_INTERFACE()->AddModifier(*pNode, *pModifier);

    // Add the new inode to our current scene list
    SceneEntry *pEntry = options.sceneEnumProc.Append(pNode, pCameraObj, OBTYPE_CAMERA, &std::string(iObj.getFullName())); 
    options.currentSceneList.Append(pEntry);

    // Set the visibility controller
    AlembicImport_SetupVisControl( path, identifier, iObj, pNode, options);

	return 0;


}
