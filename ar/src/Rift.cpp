#include "Rift.h"

// Needed by Ogre::Singleton:
template<> Rift* Ogre::Singleton<Rift>::msSingleton = 0;

bool Rift::isInitialized = false;

// TODO: Make these member variables of the Rift class:
cv::VideoCapture TheVideoCapturer_left;
cv::Mat TheInputImage_left, TheInputImageUnd_left;

Ogre::PixelBox mPixelBox_left;
Ogre::TexturePtr mTexture_left;
aruco::CameraParameters CameraParams_left, CameraParamsUnd_left;

// Right camera
cv::VideoCapture TheVideoCapturer_right;
cv::Mat TheInputImage_right, TheInputImageUnd_right;

Ogre::PixelBox mPixelBox_right;
Ogre::TexturePtr mTexture_right;
aruco::CameraParameters CameraParams_right, CameraParamsUnd_right;

int cameraCount = 1;

//////////////////////////////////////////
// Static members for handling the API:
//////////////////////////////////////////
void Rift::init() {
	if (!isInitialized) {
		ovr_Initialize();
		isInitialized = true;
	}
}
void Rift::shutdown() {
	if (isInitialized) {
		ovr_Shutdown();
		isInitialized = false;
	}
}

/////////////////////////////////////////
// Per-Device methods (non static):
/////////////////////////////////////////

Rift::Rift(int ID, Ogre::Root* root, Ogre::RenderWindow* renderWindow,
		bool rotateView) {
	if (!isInitialized)
		throw("Need to initialize first. Call Rift::init()!");
	std::cout << "Creating Rift (ID: " << ID << ")" << std::endl;

	mSceneMgr = root->createSceneManager(Ogre::ST_GENERIC);
	mSceneMgr->setAmbientLight(Ogre::ColourValue(0.5, 0.5, 0.5));

	mRenderWindow = renderWindow;

	hmd = NULL;
	mUseDummyHMD = false;

	hmd = ovrHmd_Create(ID);
	if (!hmd) {
		hmd = NULL;
		mUseDummyHMD = true;
		std::cout << "[Rift] Could not conntect to Rift.\n\tUsing dummy."
				<< std::endl;
		//throw( "Could not connect to Rift." );
	}

	if (!mUseDummyHMD) {
		std::cout << "Oculus Rift found." << std::endl;
		std::cout << "\tProduct Name: " << hmd->ProductName << std::endl;
		std::cout << "\tProduct ID: " << hmd->ProductId << std::endl;
		std::cout << "\tFirmware: " << hmd->FirmwareMajor << "."
				<< hmd->FirmwareMinor << std::endl;
		std::cout << "\tResolution: " << hmd->Resolution.w << "x"
				<< hmd->Resolution.h << std::endl;

		if (!ovrHmd_ConfigureTracking(hmd,
				ovrTrackingCap_Orientation | ovrTrackingCap_MagYawCorrection
						| ovrTrackingCap_Position, 0)) {
			ovrHmd_Destroy(hmd);
			hmd = NULL;
			mUseDummyHMD = true;
			std::cout
					<< "[Rift] This Rift does not support the features needed by the application."
					<< "\n\tUsing dummy." << std::endl;
			//throw ("\t\tThis Rift does not support the features needed by the application.");
		}
	}

	Ogre::SceneNode* meshNode =
			mSceneMgr->getRootSceneNode()->createChildSceneNode();
	if (!mUseDummyHMD) {

		// Configure Render Textures:
		Sizei recommendedTex0Size = ovrHmd_GetFovTextureSize(hmd, ovrEye_Left,
				hmd->DefaultEyeFov[0], 1.0f);
		Sizei recommendedTex1Size = ovrHmd_GetFovTextureSize(hmd, ovrEye_Right,
				hmd->DefaultEyeFov[1], 1.0f);

		// Generate a texture for each eye, as a rendertarget:
		mLeftEyeRenderTexture =
				Ogre::TextureManager::getSingleton().createManual(
						"RiftRenderTextureLeft",
						Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
						Ogre::TEX_TYPE_2D, recommendedTex0Size.w,
						recommendedTex0Size.h, 0, Ogre::PF_A8R8G8B8,
						Ogre::TU_RENDERTARGET);
		mRightEyeRenderTexture =
				Ogre::TextureManager::getSingleton().createManual(
						"RiftRenderTextureRight",
						Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
						Ogre::TEX_TYPE_2D, recommendedTex1Size.w,
						recommendedTex1Size.h, 0, Ogre::PF_A8R8G8B8,
						Ogre::TU_RENDERTARGET);

		// Assign the textures to the eyes used later:
		mMatLeft = Ogre::MaterialManager::getSingleton().getByName(
				"Oculus/LeftEye");
		mMatLeft->getTechnique(0)->getPass(0)->getTextureUnitState(1)->setTexture(
				mLeftEyeRenderTexture);
		mMatRight = Ogre::MaterialManager::getSingleton().getByName(
				"Oculus/RightEye");
		mMatRight->getTechnique(0)->getPass(0)->getTextureUnitState(1)->setTexture(
				mRightEyeRenderTexture);

		ovrEyeRenderDesc eyeRenderDesc[2];

		eyeRenderDesc[0] = ovrHmd_GetRenderDesc(hmd, ovrEye_Left,
				hmd->DefaultEyeFov[0]);
		eyeRenderDesc[1] = ovrHmd_GetRenderDesc(hmd, ovrEye_Right,
				hmd->DefaultEyeFov[1]);

		ovrVector2f UVScaleOffset[2];
		ovrRecti viewports[2];
		viewports[0].Pos.x = 0;
		viewports[0].Pos.y = 0;
		viewports[0].Size.w = recommendedTex0Size.w;
		viewports[0].Size.h = recommendedTex0Size.h;
		viewports[1].Pos.x = recommendedTex0Size.w;
		viewports[1].Pos.y = 0;
		viewports[1].Size.w = recommendedTex1Size.w;
		viewports[1].Size.h = recommendedTex1Size.h;

		// Create the Distortion Meshes:
		for (int eyeNum = 0; eyeNum < 2; eyeNum++) {
			ovrDistortionMesh meshData;
			ovrHmd_CreateDistortionMesh(hmd, eyeRenderDesc[eyeNum].Eye,
					eyeRenderDesc[eyeNum].Fov, 0, &meshData);

			Ogre::GpuProgramParametersSharedPtr params;

			if (eyeNum == 0) {
				ovrHmd_GetRenderScaleAndOffset(eyeRenderDesc[eyeNum].Fov,
						recommendedTex0Size, viewports[eyeNum], UVScaleOffset);
				params =
						mMatLeft->getTechnique(0)->getPass(0)->getVertexProgramParameters();
			} else {
				ovrHmd_GetRenderScaleAndOffset(eyeRenderDesc[eyeNum].Fov,
						recommendedTex1Size, viewports[eyeNum], UVScaleOffset);
				params =
						mMatRight->getTechnique(0)->getPass(0)->getVertexProgramParameters();
			}

			params->setNamedConstant("eyeToSourceUVScale",
					Ogre::Vector2(UVScaleOffset[0].x, UVScaleOffset[0].y));
			params->setNamedConstant("eyeToSourceUVOffset",
					Ogre::Vector2(UVScaleOffset[1].x, UVScaleOffset[1].y));

			std::cout << "UVScaleOffset[0]: " << UVScaleOffset[0].x << ", "
					<< UVScaleOffset[0].y << std::endl;
			std::cout << "UVScaleOffset[1]: " << UVScaleOffset[1].x << ", "
					<< UVScaleOffset[1].y << std::endl;

			// create ManualObject
			// TODO: Destroy the manual objects!!
			Ogre::ManualObject* manual;
			if (eyeNum == 0) {
				manual = mSceneMgr->createManualObject("RiftRenderObjectLeft");
				manual->begin("Oculus/LeftEye",
						Ogre::RenderOperation::OT_TRIANGLE_LIST);
				//manual->begin("BaseWhiteNoLighting", Ogre::RenderOperation::OT_TRIANGLE_LIST);
			} else {
				manual = mSceneMgr->createManualObject("RiftRenderObjectRight");
				manual->begin("Oculus/RightEye",
						Ogre::RenderOperation::OT_TRIANGLE_LIST);
				//manual->begin("BaseWhiteNoLighting", Ogre::RenderOperation::OT_TRIANGLE_LIST);
			}

			for (unsigned int i = 0; i < meshData.VertexCount; i++) {
				ovrDistortionVertex v = meshData.pVertexData[i];
				manual->position(v.ScreenPosNDC.x, v.ScreenPosNDC.y, 0);
				manual->textureCoord(v.TanEyeAnglesR.x,	//*UVScaleOffset[0].x + UVScaleOffset[1].x,
						v.TanEyeAnglesR.y);	//*UVScaleOffset[0].y + UVScaleOffset[1].y);
				manual->textureCoord(v.TanEyeAnglesG.x,	//*UVScaleOffset[0].x + UVScaleOffset[1].x,
						v.TanEyeAnglesG.y);	//*UVScaleOffset[0].y + UVScaleOffset[1].y);
				manual->textureCoord(v.TanEyeAnglesB.x,	//*UVScaleOffset[0].x + UVScaleOffset[1].x,
						v.TanEyeAnglesB.y);	//*UVScaleOffset[0].y + UVScaleOffset[1].y);
				float vig = std::max(v.VignetteFactor, (float) 0.0);
				manual->colour(vig, vig, vig, vig);
			}
			for (unsigned int i = 0; i < meshData.IndexCount; i++) {
				manual->index(meshData.pIndexData[i]);
			}

			// tell Ogre, your definition has finished
			manual->end();

			ovrHmd_DestroyDistortionMesh(&meshData);

			meshNode->attachObject(manual);
		}
		// Set up IPD in meters:
		mIPD = ovrHmd_GetFloat(hmd, OVR_KEY_IPD, 0.064f);
	} else {

		int texWidth = mRenderWindow->getWidth() * 0.5;
		int texHeight = mRenderWindow->getHeight();

		// Generate a texture for each eye, as a rendertarget:
		mLeftEyeRenderTexture =
				Ogre::TextureManager::getSingleton().createManual(
						"RiftRenderTextureLeft",
						Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
						Ogre::TEX_TYPE_2D, texWidth, texHeight, 0,
						Ogre::PF_A8R8G8B8, Ogre::TU_RENDERTARGET);
		mRightEyeRenderTexture =
				Ogre::TextureManager::getSingleton().createManual(
						"RiftRenderTextureRight",
						Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
						Ogre::TEX_TYPE_2D, texWidth, texHeight, 0,
						Ogre::PF_A8R8G8B8, Ogre::TU_RENDERTARGET);

		// Assign the textures to the eyes used later:
		mMatLeft = Ogre::MaterialManager::getSingleton().getByName(
				"Oculus/LeftEye");
		mMatLeft->getTechnique(0)->getPass(0)->getTextureUnitState(1)->setTexture(
				mLeftEyeRenderTexture);
		mMatRight = Ogre::MaterialManager::getSingleton().getByName(
				"Oculus/RightEye");
		mMatRight->getTechnique(0)->getPass(0)->getTextureUnitState(1)->setTexture(
				mRightEyeRenderTexture);

		/* Default values:
		 UVScaleOffset[0]: 0.464894, 0.376142
		 UVScaleOffset[1]: 0.492164, 0.5
		 UVScaleOffset[0]: 0.464894, 0.376142
		 UVScaleOffset[1]: 1.50784, 0.5
		 */
		// Create Dummy Distortion Meshes (don't actually distort anything):
		for (int eyeNum = 0; eyeNum < 2; eyeNum++) {
			Ogre::ManualObject* manual;

			Ogre::GpuProgramParametersSharedPtr params;
			int minX, maxX, minY, maxY;
			if (eyeNum == 0) {
				manual = mSceneMgr->createManualObject("RiftRenderObjectLeft");
				manual->begin("Oculus/LeftEye",
						Ogre::RenderOperation::OT_TRIANGLE_LIST);
				minX = -1.25875;
				maxX = 0;
				minY = -1.25875;
				maxY = 1.25875;
				/*params = mMatLeft->getTechnique(0)->getPass(0)->getVertexProgramParameters();
				 params->setNamedConstant( "eyeToSourceUVScale",
				 Ogre::Vector2( 0.464894, 0.376142 ) );
				 params->setNamedConstant( "eyeToSourceUVOffset",
				 Ogre::Vector2( 0.492164, 0.5 ) );*/
			} else {
				manual = mSceneMgr->createManualObject("RiftRenderObjectRight");
				manual->begin("Oculus/RightEye",
						Ogre::RenderOperation::OT_TRIANGLE_LIST);
				minX = 0;
				maxX = 1.25875;
				minY = -1.25875;
				maxY = 1.25875;
				/*params = mMatRight->getTechnique(0)->getPass(0)->getVertexProgramParameters();
				 params->setNamedConstant( "eyeToSourceUVScale",
				 Ogre::Vector2( 0.464894, 0.376142 ) );
				 params->setNamedConstant( "eyeToSourceUVOffset",
				 Ogre::Vector2( 1.50784, 0.5 ) );*/
			}

			manual->position(minX, minY, 0);
			manual->textureCoord(0, 1);
			manual->textureCoord(0, 1);
			manual->textureCoord(0, 1);
			manual->colour(1.0, 1.0, 1.0, 1.0);	// no vignette, so vertex colour is white.
			manual->position(minX, maxY, 0);
			manual->textureCoord(0, 0);
			manual->textureCoord(0, 0);
			manual->textureCoord(0, 0);
			manual->colour(1.0, 1.0, 1.0, 1.0);	// no vignette, so vertex colour is white.
			manual->position(maxX, maxY, 0);
			manual->textureCoord(1, 0);
			manual->textureCoord(1, 0);
			manual->textureCoord(1, 0);
			manual->colour(1.0, 1.0, 1.0, 1.0);	// no vignette, so vertex colour is white.
			manual->position(maxX, minY, 0);
			manual->textureCoord(1, 1);
			manual->textureCoord(1, 1);
			manual->textureCoord(1, 1);
			manual->colour(1.0, 1.0, 1.0, 1.0);	// no vignette, so vertex colour is white.

			manual->index(2);
			manual->index(1);
			manual->index(0);
			manual->index(3);
			manual->index(2);
			manual->index(3);

			manual->index(0);
			manual->index(1);
			manual->index(2);
			manual->index(0);
			manual->index(2);
			manual->index(3);
			manual->end();

			meshNode->attachObject(manual);
		}

		// Set a default value for interpupillary distance:
		mIPD = 0.064;
	}

	// Create a camera in the (new, external) scene so the mesh can be rendered onto it:
	mCamera = mSceneMgr->createCamera("OculusRiftExternalCamera");
	mCamera->setFarClipDistance(50);
	mCamera->setNearClipDistance(0.001);
	mCamera->setProjectionType(Ogre::PT_ORTHOGRAPHIC);
	mCamera->setOrthoWindow(2, 2);
	mCamera->lookAt(0, 0, -1);

	if (rotateView) {
		mCamera->roll(Ogre::Degree(-90));
	}

	mSceneMgr->getRootSceneNode()->attachObject(mCamera);

	meshNode->setPosition(0, 0, -1);
	meshNode->setScale(1, 1, -1);

	mViewport = mRenderWindow->addViewport(mCamera);
	mViewport->setBackgroundColour(Ogre::ColourValue(0, 0, 0, 0));
	mViewport->setOverlaysEnabled(true);

	mPosition = Ogre::Vector3::ZERO;

	///////////////////////////////////////
	// Start streaming devices:
	createVideoStreams();
}

Rift::~Rift() {
	if (hmd)
		ovrHmd_Destroy(hmd);
}

// Takes the two cameras created in the scene and creates Viewports in the correct render textures:
void Rift::setCameras(Ogre::Camera* camLeft, Ogre::Camera* camRight) {
	Ogre::RenderTexture* renderTexLeft =
			mLeftEyeRenderTexture->getBuffer()->getRenderTarget();
	std::cout << "[Rift] Adding viewport to left texture" << std::endl;
	renderTexLeft->addViewport(camLeft);
	renderTexLeft->getViewport(0)->setClearEveryFrame(true);
	//renderTexLeft->getViewport(0)->setBackgroundColour(Ogre::ColourValue::Black);
	renderTexLeft->getViewport(0)->setBackgroundColour(
			Ogre::ColourValue(0, 0, 0, 0));
	//renderTexLeft->getViewport(0)->setOverlaysEnabled(true);
	std::cout << "[Rift] Left texture size: " << renderTexLeft->getWidth()
			<< "x" << renderTexLeft->getHeight() << std::endl;

	std::cout << "[Rift] Adding viewport to right texture" << std::endl;
	Ogre::RenderTexture* renderTexRight =
			mRightEyeRenderTexture->getBuffer()->getRenderTarget();
	renderTexRight->addViewport(camRight);
	renderTexRight->getViewport(0)->setClearEveryFrame(true);
	//renderTexRight->getViewport(0)->setBackgroundColour(Ogre::ColourValue::Black);
	renderTexRight->getViewport(0)->setBackgroundColour(
			Ogre::ColourValue(0, 0, 0, 0));
	//renderTexRight->getViewport(0)->setOverlaysEnabled(true);
	std::cout << "[Rift] Right texture size: " << renderTexRight->getWidth()
			<< "x" << renderTexRight->getHeight() << std::endl;

	if (!mUseDummyHMD) {
		ovrFovPort fovLeft = hmd->DefaultEyeFov[ovrEye_Left];
		ovrFovPort fovRight = hmd->DefaultEyeFov[ovrEye_Right];

		float combinedTanHalfFovHorizontal = std::max(fovLeft.LeftTan,
				fovLeft.RightTan);
		float combinedTanHalfFovVertical = std::max(fovLeft.UpTan,
				fovLeft.DownTan);

		float aspectRatio = combinedTanHalfFovHorizontal
				/ combinedTanHalfFovVertical;

		camLeft->setAspectRatio(aspectRatio);
		camRight->setAspectRatio(aspectRatio);

		ovrMatrix4f projL = ovrMatrix4f_Projection(fovLeft, 0.001, 50.0, true);
		ovrMatrix4f projR = ovrMatrix4f_Projection(fovRight, 0.001, 50.0, true);

		camLeft->setCustomProjectionMatrix(true,
				Ogre::Matrix4(projL.M[0][0], projL.M[0][1], projL.M[0][2],
						projL.M[0][3], projL.M[1][0], projL.M[1][1],
						projL.M[1][2], projL.M[1][3], projL.M[2][0],
						projL.M[2][1], projL.M[2][2], projL.M[2][3],
						projL.M[3][0], projL.M[3][1], projL.M[3][2],
						projL.M[3][3]));
		camRight->setCustomProjectionMatrix(true,
				Ogre::Matrix4(projR.M[0][0], projR.M[0][1], projR.M[0][2],
						projR.M[0][3], projR.M[1][0], projR.M[1][1],
						projR.M[1][2], projR.M[1][3], projR.M[2][0],
						projR.M[2][1], projR.M[2][2], projR.M[2][3],
						projR.M[3][0], projR.M[3][1], projR.M[3][2],
						projR.M[3][3]));
	} else {

		// If using a dummy Rift, simply generate two viewports:

		std::cout << "[Rift] Set aspect ratio: " << camLeft->getAspectRatio()
				<< std::endl;

		std::cout << "[Rift] Set aspect ratio: " << camRight->getAspectRatio()
				<< std::endl;
	}
}

bool Rift::update(float dt) {

	///////////////////////////////////////////////////////////
	// Fetching Left Eye
	TheVideoCapturer_left.grab();
	TheVideoCapturer_left.retrieve(TheInputImage_left);
	cv::undistort(TheInputImage_left, TheInputImageUnd_left,
			CameraParams_left.CameraMatrix, CameraParams_left.Distorsion);

	///////////////////////////////////////////////////////////
	// Fetching Right Eye
	if (cameraCount == 2) {
		TheVideoCapturer_right.grab();
		TheVideoCapturer_right.retrieve(TheInputImage_right);
		cv::undistort(TheInputImage_right, TheInputImageUnd_right,
				CameraParams_right.CameraMatrix, CameraParams_right.Distorsion);
	}

	if (!hmd)
		return true;

	frameTiming = ovrHmd_BeginFrameTiming(hmd, 0);
	ovrTrackingState ts = ovrHmd_GetTrackingState(hmd,
			frameTiming.ScanoutMidpointSeconds);

	//ovrPosef headPose;

	if (ts.StatusFlags
			& (ovrStatus_OrientationTracked | ovrStatus_PositionTracked)) {
		// The cpp compatibility layer is used to convert ovrPosef to Posef (see OVR_Math.h)
		Posef pose = ts.HeadPose.ThePose;
		//headPose = pose;
		//float yaw, pitch, roll;
		//pose.Rotation.GetEulerAngles<Axis_Y, Axis_X, Axis_Z>(&yaw, &pitch, &roll);

		// Optional: move cursor back to starting position and print values
		//std::cout << "yaw: "   << RadToDegree(yaw)   << std::endl;
		//std::cout << "pitch: " << RadToDegree(pitch) << std::endl;
		//std::cout << "roll: "  << RadToDegree(roll)  << std::endl;
		mOrientation = Ogre::Quaternion(pose.Rotation.w, pose.Rotation.x,
				pose.Rotation.y, pose.Rotation.z);

		mPosition = Ogre::Vector3(pose.Translation.x, pose.Translation.y,
				pose.Translation.z);
	}

	ovr_WaitTillTime(frameTiming.TimewarpPointSeconds);

	ovrHmd_EndFrameTiming(hmd);

	return true;
}

void Rift::recenterPose() {
	if (hmd)
		ovrHmd_RecenterPose(hmd);
}

void Rift::setTextureLeft(Ogre::PixelBox px) {
	mTexture_left->getBuffer()->blitFromMemory(px);
}

void Rift::setTextureRight(Ogre::PixelBox px) {
	mTexture_right->getBuffer()->blitFromMemory(px);
}

///////////////////////////////////////////////////////////////////////////////
// Handle video streams:

void Rift::createVideoStreams() {

	// Left Camera
	TheVideoCapturer_left.open(1);
	CameraParams_left.readFromXMLFile("camera_left.yml");
	CameraParamsUnd_left = CameraParams_left;
	CameraParamsUnd_left.Distorsion = cv::Mat::zeros(4, 1, CV_32F);
	TheVideoCapturer_left.grab();

	TheVideoCapturer_left.retrieve(TheInputImage_left);

	cv::undistort(TheInputImage_left, TheInputImageUnd_left,
			CameraParams_left.CameraMatrix, CameraParams_left.Distorsion);

	double pMatrix_left[16];
	CameraParamsUnd_left.OgreGetProjectionMatrix(CameraParamsUnd_left.CamSize,
			CameraParamsUnd_left.CamSize, pMatrix_left, 0.05, 10, false);
	int width = CameraParamsUnd_left.CamSize.width;
	int height = CameraParamsUnd_left.CamSize.height;
	mPixelBox_left = Ogre::PixelBox(width, height, 1, Ogre::PF_R8G8B8,
			TheInputImageUnd_left.ptr<uchar>(0));
	mTexture_left = Ogre::TextureManager::getSingleton().createManual(
			"CameraTexture_left",
			Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
			Ogre::TEX_TYPE_2D, width, height, 0, Ogre::PF_R8G8B8,
			Ogre::TU_DYNAMIC);

	mMatLeft->getTechnique(0)->getPass(0)->getTextureUnitState(0)->setTexture(
			mTexture_left);

	// Right Camera
	if (cameraCount == 2) {
		cout << "OPENING RIGHT" << endl;

		TheVideoCapturer_right.open(2);

		CameraParams_right.readFromXMLFile("camera_right.yml");
		CameraParamsUnd_right = CameraParams_right;
		CameraParamsUnd_right.Distorsion = cv::Mat::zeros(4, 1, CV_32F);

		TheVideoCapturer_right.grab();
		TheVideoCapturer_right.retrieve(TheInputImage_right);
		cv::undistort(TheInputImage_right, TheInputImageUnd_right,
				CameraParams_right.CameraMatrix, CameraParams_right.Distorsion);

		double pMatrix_right[16];
		CameraParamsUnd_right.OgreGetProjectionMatrix(
				CameraParamsUnd_right.CamSize, CameraParamsUnd_right.CamSize,
				pMatrix_right, 0.05, 10, false);
		width = CameraParamsUnd_right.CamSize.width;
		height = CameraParamsUnd_right.CamSize.height;
		mPixelBox_right = Ogre::PixelBox(width, height, 1, Ogre::PF_R8G8B8,
				TheInputImageUnd_right.ptr<uchar>(0));
		mTexture_right = Ogre::TextureManager::getSingleton().createManual(
				"CameraTexture_right",
				Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
				Ogre::TEX_TYPE_2D, width, height, 0, Ogre::PF_R8G8B8,
				Ogre::TU_DYNAMIC);

		mMatRight->getTechnique(0)->getPass(0)->getTextureUnitState(0)->setTexture(
				mTexture_right);
	} else
		mMatRight->getTechnique(0)->getPass(0)->getTextureUnitState(0)->setTexture(
				mTexture_left);
}

// TODO: Not working for some reason...
int Rift::countCameras()
{
   /*cv::VideoCapture temp_camera;
   int maxTested = 2;
   for (int i = 0; i < maxTested; i++){
	   try{
		   cv::VideoCapture temp_camera(i);
		   temp_camera.grab();
		   temp_camera.release();
		   temp_camera.~VideoCapture();
		   std::cout << "Found camera #" << i << "." << std::endl;
	   }catch(...){
		   return i;
	   }

   }


   return maxTested;
   */

	return cameraCount;
}

cv::Mat Rift::getImageUndLeft() {
	return TheInputImageUnd_left;
}

cv::Mat Rift::getImageUndRight() {
	return TheInputImageUnd_right;
}

aruco::CameraParameters Rift::getCameraParamsUndLeft() {
	return CameraParamsUnd_left;
}

aruco::CameraParameters Rift::getCameraParamsUndRight() {
	return CameraParamsUnd_right;
}

aruco::CameraParameters Rift::getCameraParamsLeft() {
	return CameraParams_left;
}

aruco::CameraParameters Rift::getCameraParamsRight() {
	return CameraParams_right;
}

Ogre::PixelBox Rift::getPixelBoxLeft() {
	return mPixelBox_left;
}

Ogre::PixelBox Rift::getPixelBoxRight() {
	return mPixelBox_right;
}

void Rift::selectMarker(cv::Mat &Image, aruco::Marker &m,
		const aruco::CameraParameters &CP, bool setYperpendicular) {
	cv::Mat objectPoints(8, 3, CV_32FC1);
	double halfSize = m.ssize / 2;

	if (setYperpendicular) {
		objectPoints.at<float>(0, 0) = -halfSize;
		objectPoints.at<float>(0, 1) = 0;
		objectPoints.at<float>(0, 2) = -halfSize;
		objectPoints.at<float>(1, 0) = halfSize;
		objectPoints.at<float>(1, 1) = 0;
		objectPoints.at<float>(1, 2) = -halfSize;
		objectPoints.at<float>(2, 0) = halfSize;
		objectPoints.at<float>(2, 1) = 0;
		objectPoints.at<float>(2, 2) = halfSize;
		objectPoints.at<float>(3, 0) = -halfSize;
		objectPoints.at<float>(3, 1) = 0;
		objectPoints.at<float>(3, 2) = halfSize;

		objectPoints.at<float>(4, 0) = -halfSize;
		objectPoints.at<float>(4, 1) = m.ssize;
		objectPoints.at<float>(4, 2) = -halfSize;
		objectPoints.at<float>(5, 0) = halfSize;
		objectPoints.at<float>(5, 1) = m.ssize;
		objectPoints.at<float>(5, 2) = -halfSize;
		objectPoints.at<float>(6, 0) = halfSize;
		objectPoints.at<float>(6, 1) = m.ssize;
		objectPoints.at<float>(6, 2) = halfSize;
		objectPoints.at<float>(7, 0) = -halfSize;
		objectPoints.at<float>(7, 1) = m.ssize;
		objectPoints.at<float>(7, 2) = halfSize;
	} else {
		objectPoints.at<float>(0, 0) = -halfSize;
		objectPoints.at<float>(0, 1) = -halfSize;
		objectPoints.at<float>(0, 2) = 0;
		objectPoints.at<float>(1, 0) = halfSize;
		objectPoints.at<float>(1, 1) = -halfSize;
		objectPoints.at<float>(1, 2) = 0;
		objectPoints.at<float>(2, 0) = halfSize;
		objectPoints.at<float>(2, 1) = halfSize;
		objectPoints.at<float>(2, 2) = 0;
		objectPoints.at<float>(3, 0) = -halfSize;
		objectPoints.at<float>(3, 1) = halfSize;
		objectPoints.at<float>(3, 2) = 0;

		objectPoints.at<float>(4, 0) = -halfSize;
		objectPoints.at<float>(4, 1) = -halfSize;
		objectPoints.at<float>(4, 2) = m.ssize;
		objectPoints.at<float>(5, 0) = halfSize;
		objectPoints.at<float>(5, 1) = -halfSize;
		objectPoints.at<float>(5, 2) = m.ssize;
		objectPoints.at<float>(6, 0) = halfSize;
		objectPoints.at<float>(6, 1) = halfSize;
		objectPoints.at<float>(6, 2) = m.ssize;
		objectPoints.at<float>(7, 0) = -halfSize;
		objectPoints.at<float>(7, 1) = halfSize;
		objectPoints.at<float>(7, 2) = m.ssize;
	}

	vector<cv::Point2f> imagePoints;
	cv::projectPoints(objectPoints, m.Rvec, m.Tvec, CP.CameraMatrix,
			CP.Distorsion, imagePoints);

	cv::line(Image, imagePoints[0], imagePoints[1],
			cv::Scalar(0, 255, 255, 255), 20, CV_AA);
	cv::line(Image, imagePoints[1], imagePoints[2],
			cv::Scalar(0, 255, 255, 255), 20, CV_AA);
	cv::line(Image, imagePoints[2], imagePoints[3],
			cv::Scalar(0, 255, 255, 255), 20, CV_AA);
	cv::line(Image, imagePoints[3], imagePoints[0],
			cv::Scalar(0, 255, 255, 255), 20, CV_AA);

}
