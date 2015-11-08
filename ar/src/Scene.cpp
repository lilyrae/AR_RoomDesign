#include "Scene.h"

#include <Private.h>	// contains private data
#include <Parser.h>

using namespace std;
using namespace Leap;

int SCALE_RATE = 80;
int ROTATION_RATE = 2;	// degrees

float TheMarkerSize = 0.200;
aruco::MarkerDetector TheMarkerDetector;
std::vector<aruco::Marker> TheMarkers;

map<string, int> ARMarkersList;			// {id + _ + room} - index
map<string, bool> ARMarkersState;			// {id + _ + room} - visible?
map<string, float> ARMarkersScale;			// {id + _ + room} - scale
map<string, float> ARMarkersRotation;		// {id + _ + room} - Xrotation
map<string, string> ARMarkersMesh;			// {id + _ + room} - mesh

int errorT;
void *status;

pthread_t tid_leap;	//thread id
pthread_t socketListener;

bool isRunning = false;
bool isLeapConnected = false;

int closest_marker = -1;
int selected_marker = -1;

bool USE_LEAP = true;

string SEPARATOR_ROOM = "_";
string SELECTED_ROOM = "living_room";

bool notifiedChange = true;

boost::mutex m;

namespace patch {
template<typename T> std::string to_string(const T& n) {
	std::ostringstream stm;
	stm << n;
	return stm.str();
}
}
class SampleListener: public Listener {
public:
	virtual void onInit(const Controller&);
	virtual void onConnect(const Controller&);
	virtual void onDisconnect(const Controller&);
	virtual void onExit(const Controller&);
	virtual void onFrame(const Controller&);
	virtual void onFocusGained(const Controller&);
	virtual void onFocusLost(const Controller&);
	virtual void onDeviceChange(const Controller&);
	virtual void onServiceConnect(const Controller&);
	virtual void onServiceDisconnect(const Controller&);

private:
};

void SampleListener::onInit(const Controller& controller) {
	std::cout << "Initialized" << std::endl;
}

void SampleListener::onConnect(const Controller& controller) {
	std::cout << "Connected" << std::endl;
	isLeapConnected = true;
	controller.enableGesture(Gesture::TYPE_CIRCLE);
	controller.enableGesture(Gesture::TYPE_KEY_TAP);
	controller.enableGesture(Gesture::TYPE_SWIPE);
}

void SampleListener::onDisconnect(const Controller& controller) {
	// Note: not dispatched when running in a debugger.
	isLeapConnected = false;
	std::cout << "Disconnected" << std::endl;
}

void SampleListener::onExit(const Controller& controller) {
	isLeapConnected = false;
	std::cout << "Exited" << std::endl;
}

void SampleListener::onFrame(const Controller& controller) {
	// Get the most recent frame and report some basic information
	const Frame frame = controller.frame();

	HandList hands = frame.hands();

	// Get gestures
	const GestureList gestures = frame.gestures();
	for (int g = 0; g < gestures.count(); ++g) {
		Gesture gesture = gestures[g];

		switch (gesture.type()) {
		case Gesture::TYPE_CIRCLE: {
			CircleGesture circle = gesture;
			std::string clockwiseness;

			float sweptAngle = 0;
			if (circle.state() != Gesture::STATE_START) {
				CircleGesture previousUpdate = CircleGesture(
						controller.frame(1).gesture(circle.id()));
				sweptAngle = (circle.progress() - previousUpdate.progress()) * 2
						* PI;
			}

			cout << "Swept angle: " << sweptAngle * RAD_TO_DEG << endl;
			if (circle.pointable().direction().angleTo(circle.normal())
					<= PI / 2) {
				clockwiseness = "clockwise";

				if (selected_marker != -1 && abs(sweptAngle * RAD_TO_DEG) > 5)
					ARMarkersScale[patch::to_string(selected_marker)
							+ SEPARATOR_ROOM + SELECTED_ROOM] +=
							ARMarkersScale[patch::to_string(selected_marker)
									+ SEPARATOR_ROOM + SELECTED_ROOM]
									/ SCALE_RATE;

			} else {
				clockwiseness = "counterclockwise";

				if (selected_marker != -1
						&& ARMarkersScale[patch::to_string(selected_marker)
								+ SEPARATOR_ROOM + SELECTED_ROOM]
								- ARMarkersScale[patch::to_string(
										selected_marker) + SEPARATOR_ROOM
										+ SELECTED_ROOM] / 10 > 0
						&& abs(sweptAngle * RAD_TO_DEG) > 5)
					ARMarkersScale[patch::to_string(selected_marker)
							+ SEPARATOR_ROOM + SELECTED_ROOM] -=
							ARMarkersScale[patch::to_string(selected_marker)
									+ SEPARATOR_ROOM + SELECTED_ROOM]
									/ SCALE_RATE;

			}

			cout << "Changing scale ID #" << selected_marker << " to "
					<< ARMarkersScale[patch::to_string(selected_marker)
							+ SEPARATOR_ROOM + SELECTED_ROOM] << endl; // Calculate angle swept since last frame

			std::cout << "GESTURE DETECTED: CIRCLE.\n";
			break;
		}
		case Gesture::TYPE_SWIPE: {
			SwipeGesture swipe = gesture;

			if (hands[0].fingers().count() >= 4) {
				if (selected_marker != -1) {
					if (swipe.direction()[0] >= 0)				// To the left
						ARMarkersRotation[patch::to_string(selected_marker)
								+ SEPARATOR_ROOM + SELECTED_ROOM] +=
								ROTATION_RATE;
					else if (swipe.direction()[0] < 0)		// To the right
						ARMarkersRotation[patch::to_string(selected_marker)
								+ SEPARATOR_ROOM + SELECTED_ROOM] -=
								ROTATION_RATE;

					cout << "Current rotation: "
							<< ARMarkersRotation[patch::to_string(
									selected_marker) + SEPARATOR_ROOM
									+ SELECTED_ROOM] << endl;
				}
			}

			std::cout << "GESTURE DETECTED: SWIPE " << swipe.direction()
					<< ".\n";

			break;
		}
		case Gesture::TYPE_KEY_TAP: {
			KeyTapGesture tap = gesture;

			if (selected_marker == -1)
				selected_marker = closest_marker;
			else if (selected_marker != closest_marker) {
				int selected_mesh_id = ARMarkersList[patch::to_string(
						selected_marker) + SEPARATOR_ROOM + SELECTED_ROOM];
				ARMarkersList[patch::to_string(selected_marker) + SEPARATOR_ROOM
						+ SELECTED_ROOM] = ARMarkersList[patch::to_string(
						closest_marker) + SEPARATOR_ROOM + SELECTED_ROOM];
				ARMarkersList[patch::to_string(closest_marker) + SEPARATOR_ROOM
						+ SELECTED_ROOM] = selected_mesh_id;

				float selected_mesh_scale = ARMarkersScale[patch::to_string(
						selected_marker) + SEPARATOR_ROOM + SELECTED_ROOM];
				ARMarkersScale[patch::to_string(selected_marker)
						+ SEPARATOR_ROOM + SELECTED_ROOM] =
						ARMarkersScale[patch::to_string(closest_marker)
								+ SEPARATOR_ROOM + SELECTED_ROOM];
				ARMarkersScale[patch::to_string(closest_marker) + SEPARATOR_ROOM
						+ SELECTED_ROOM] = selected_mesh_scale;

				float selected_mesh_rotation =
						ARMarkersRotation[patch::to_string(selected_marker)
								+ SEPARATOR_ROOM + SELECTED_ROOM];
				ARMarkersRotation[patch::to_string(selected_marker)
						+ SEPARATOR_ROOM + SELECTED_ROOM] =
						ARMarkersRotation[patch::to_string(closest_marker)
								+ SEPARATOR_ROOM + SELECTED_ROOM];
				ARMarkersRotation[patch::to_string(closest_marker)
						+ SEPARATOR_ROOM + SELECTED_ROOM] =
						selected_mesh_rotation;
			} else
				selected_marker = -1;

			cout << "Key Selected: " << selected_marker << endl;

			break;
		}
		case Gesture::TYPE_SCREEN_TAP: {
			ScreenTapGesture screentap = gesture;

			if (selected_marker == -1)
				selected_marker = closest_marker;
			else if (selected_marker != closest_marker) {
				int selected_mesh_id = ARMarkersList[patch::to_string(
						selected_marker) + SEPARATOR_ROOM + SELECTED_ROOM];
				ARMarkersList[patch::to_string(selected_marker) + SEPARATOR_ROOM
						+ SELECTED_ROOM] = ARMarkersList[patch::to_string(
						closest_marker) + SEPARATOR_ROOM + SELECTED_ROOM];
				ARMarkersList[patch::to_string(closest_marker) + SEPARATOR_ROOM
						+ SELECTED_ROOM] = selected_mesh_id;

				float selected_mesh_scale = ARMarkersScale[patch::to_string(
						selected_marker) + SEPARATOR_ROOM + SELECTED_ROOM];
				ARMarkersScale[patch::to_string(selected_marker)
						+ SEPARATOR_ROOM + SELECTED_ROOM] =
						ARMarkersScale[patch::to_string(closest_marker)
								+ SEPARATOR_ROOM + SELECTED_ROOM];
				ARMarkersScale[patch::to_string(closest_marker) + SEPARATOR_ROOM
						+ SELECTED_ROOM] = selected_mesh_scale;

				float selected_mesh_rotation =
						ARMarkersRotation[patch::to_string(selected_marker)
								+ SEPARATOR_ROOM + SELECTED_ROOM];
				ARMarkersRotation[patch::to_string(selected_marker)
						+ SEPARATOR_ROOM + SELECTED_ROOM] =
						ARMarkersRotation[patch::to_string(closest_marker)
								+ SEPARATOR_ROOM + SELECTED_ROOM];
				ARMarkersRotation[patch::to_string(closest_marker)
						+ SEPARATOR_ROOM + SELECTED_ROOM] =
						selected_mesh_rotation;

			} else
				selected_marker = -1;
			break;
		}
		default:
			//std::cout << std::string(2, ' ')  << "Unknown gesture type." << std::endl;
			break;
		}
	}

}

void SampleListener::onFocusGained(const Controller& controller) {
	std::cout << "Focus Gained" << std::endl;
}

void SampleListener::onFocusLost(const Controller& controller) {
	std::cout << "Focus Lost" << std::endl;
}

void SampleListener::onDeviceChange(const Controller& controller) {
	std::cout << "Device Changed" << std::endl;
	const DeviceList devices = controller.devices();

	for (int i = 0; i < devices.count(); ++i) {
		std::cout << "id: " << devices[i].toString() << std::endl;
		std::cout << "  isStreaming: "
				<< (devices[i].isStreaming() ? "true" : "false") << std::endl;
	}
}

void SampleListener::onServiceConnect(const Controller& controller) {
	std::cout << "Service Connected" << std::endl;
}

void SampleListener::onServiceDisconnect(const Controller& controller) {
	std::cout << "Service Disconnected" << std::endl;
}

Scene::Scene(Ogre::Root* root, OIS::Mouse* mouse, OIS::Keyboard* keyboard,
		Rift* rift) {
	mRift = rift;
	mRoot = root;
	mMouse = mouse;
	mKeyboard = keyboard;
	mSceneMgr = mRoot->createSceneManager(Ogre::ST_GENERIC);

	loadMarkersData("../data/objects");
	loadARObjects();
	createCameras();

	if (USE_LEAP)
		loadLeap();

	startSocketListener();

}

Scene::~Scene() {
	if (mSceneMgr)
		delete mSceneMgr;
	isRunning = false;
}

bool Scene::startSocketListener() {
	int errorT = pthread_create(&socketListener, NULL, socketListenThread,
	NULL);
	if (errorT) {
		printf("socketListener is not created...\n");
		return false;
	}

	return true;
}

void *socketListenThread(void *arg) {

	int sockfd;

	struct sockaddr_in serv_addr, cli_addr;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	if (sockfd < 0)
		cout << "ERROR opening socket" << endl;

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(9999);

	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
		cout << "ERROR on binding" << endl;
		exit(1);
	}

	listen(sockfd, 5);

	while (isRunning) {
		int newsockfd;
		socklen_t clilen;
		char buffer[256];

		clilen = sizeof(cli_addr);

		newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
		if (newsockfd < 0)
			cout << "ERROR on accept" << endl;

		memset(buffer, 0, 256);
		int n = read(newsockfd, buffer, 255);
		if (n < 0)
			cout << "ERROR reading from socket" << endl;
		else {
			m.lock();
			SELECTED_ROOM = buffer;
			SELECTED_ROOM.erase(
					std::remove(SELECTED_ROOM.begin(), SELECTED_ROOM.end(),
							'\n'), SELECTED_ROOM.end());
			printf("Selected room: %s\n", SELECTED_ROOM.c_str());
			notifiedChange = false;
			m.unlock();
		}

		close(newsockfd);
	}

	close(sockfd);

	if (!isRunning)
		printf("Dead because not running\n");
	else
		printf("Dead because WUT?\n");

	return (void *) 0;
}

void Scene::loadMarkersData(string path) {

	readTeamsFromFile(path, ARMarkersList, ARMarkersState, ARMarkersScale,
			ARMarkersRotation, ARMarkersMesh);

}

bool Scene::loadLeap() {

	isRunning = true;

	errorT = pthread_create(&tid_leap, NULL, leapThread, NULL);
	if (errorT) {
		printf("pthread Leap is not created...\n");
		return false;
	}

	return true;
}

void *leapThread(void *arg) {

	SampleListener listener;
	Controller controller;

	controller.addListener(listener);
	controller.setPolicy(Leap::Controller::POLICY_BACKGROUND_FRAMES);

	while (isRunning)
		;

// Remove the sample listener when done
	controller.removeListener(listener);

	return (void *) 0;

}
void Scene::loadARObjects() {
	mRoomNode = mSceneMgr->getRootSceneNode()->createChildSceneNode("RoomNode");

	Ogre::ResourceGroupManager::getSingleton().addResourceLocation("media",
			"FileSystem", "Popular");
	Ogre::ResourceGroupManager::getSingleton().initialiseAllResourceGroups();

	for (map<string, int>::iterator it = ARMarkersList.begin();
			it != ARMarkersList.end(); ++it) {

		string id = it->first;
		int i = it->second;

		string mesh_name = ARMarkersMesh[id];

		cout << "Adding id: " << id << " - mesh: " << mesh_name << " at index: "
				<< i << endl;

		Ogre::String entityName = "ARO_" + Ogre::StringConverter::toString(i);

		Ogre::Entity* ogreEntity = mSceneMgr->createEntity(entityName,
				mesh_name);
		Ogre::Real offset = ogreEntity->getBoundingBox().getHalfSize().y;
		mARONodes[i] = mSceneMgr->getRootSceneNode()->createChildSceneNode();
		Ogre::SceneNode *ogreNodeChild = mARONodes[i]->createChildSceneNode();
		ogreNodeChild->attachObject(ogreEntity);
		ogreNodeChild->rotate(Ogre::Vector3(1, 0, 0),
				Ogre::Radian(Ogre::Degree(90)));
		if(mesh_name == "sofa_tango.mesh")
			ogreNodeChild->translate(1, -2.2, offset, Ogre::Node::TS_PARENT);
		else
			ogreNodeChild->translate(0, 0, offset, Ogre::Node::TS_PARENT);

		ARMarkersList[id] = i;

		mARONodes[i]->setScale(ARMarkersScale[id], ARMarkersScale[id],
				ARMarkersScale[id]);
		mARONodes[i]->setVisible(false);

	}

	Ogre::Light* roomLight = mSceneMgr->createLight();
	roomLight->setType(Ogre::Light::LT_POINT);
	roomLight->setCastShadows(true);
	roomLight->setShadowFarDistance(30);
	roomLight->setAttenuation(1000, 1.0, 0.07, 0.017);
	roomLight->setSpecularColour(.25, .25, .25);
	roomLight->setDiffuseColour(0.85, 0.76, 0.7);
	roomLight->setPowerScale(100);

	roomLight->setPosition(5, 5, 5);
	mSceneMgr->setAmbientLight(Ogre::ColourValue(0.5, 0.5, 0.5));

	mRoomNode->attachObject(roomLight);
}

void Scene::createCameras() {
	mCamLeft = mSceneMgr->createCamera("LeftCamera");
	mCamRight = mSceneMgr->createCamera("RightCamera");

// Create a scene nodes which the cams will be attached to:
	mBodyNode = mSceneMgr->getRootSceneNode()->createChildSceneNode("BodyNode");
	mBodyTiltNode = mBodyNode->createChildSceneNode();
	mHeadNode = mBodyTiltNode->createChildSceneNode("HeadNode");
	mBodyNode->setFixedYawAxis(true);	// don't roll!

	mHeadNode->attachObject(mCamLeft);
	mHeadNode->attachObject(mCamRight);

// Position cameras according to interpupillary distance
	float dist = 0.05;
	/*if (mRift->isAttached())
	 {
	 dist = mRift->getStereoConfig().GetIPD();
	 }*/
	mCamLeft->setPosition(-dist / 2.0f, 0.0f, 0.0f);
	mCamRight->setPosition(dist / 2.0f, 0.0f, 0.0f);

// If possible, get the camera projection matricies given by the rift:
	/*if (mRift->isAttached())
	 {
	 mCamLeft->setCustomProjectionMatrix( true, mRift->getProjectionMatrix_Left() );
	 mCamRight->setCustomProjectionMatrix( true, mRift->getProjectionMatrix_Right() );
	 }*/
	mCamLeft->setFarClipDistance(50);
	mCamRight->setFarClipDistance(50);

	mCamLeft->setNearClipDistance(0.001);
	mCamRight->setNearClipDistance(0.001);

	aruco::CameraParameters camParams_left;
	camParams_left.readFromXMLFile("camera_left.yml");

	double pMatrix[16];
	camParams_left.OgreGetProjectionMatrix(camParams_left.CamSize,
			camParams_left.CamSize, pMatrix, 0.05, 10, false);

	Ogre::Matrix4 PM_left(pMatrix[0], pMatrix[1], pMatrix[2], pMatrix[3],
			pMatrix[4], pMatrix[5], pMatrix[6], pMatrix[7], pMatrix[8],
			pMatrix[9], pMatrix[10], pMatrix[11], pMatrix[12], pMatrix[13],
			pMatrix[14], pMatrix[15]);
	mCamLeft->setCustomProjectionMatrix(true, PM_left);
	mCamLeft->setCustomViewMatrix(true, Ogre::Matrix4::IDENTITY);

///

	aruco::CameraParameters camParams_right;
	camParams_right.readFromXMLFile("camera_right.yml");

	camParams_right.OgreGetProjectionMatrix(camParams_right.CamSize,
			camParams_right.CamSize, pMatrix, 0.05, 10, false);

	Ogre::Matrix4 PM_right(pMatrix[0], pMatrix[1], pMatrix[2], pMatrix[3],
			pMatrix[4], pMatrix[5], pMatrix[6], pMatrix[7], pMatrix[8],
			pMatrix[9], pMatrix[10], pMatrix[11], pMatrix[12], pMatrix[13],
			pMatrix[14], pMatrix[15]);
	mCamRight->setCustomProjectionMatrix(true, PM_right);
	mCamRight->setCustomViewMatrix(true, Ogre::Matrix4::IDENTITY);

	/*mHeadLight = mSceneMgr->createLight();
	 mHeadLight->setType(Ogre::Light::LT_POINT);
	 mHeadLight->setCastShadows( true );
	 mHeadLight->setShadowFarDistance( 30 );
	 mHeadLight->setAttenuation( 65, 1.0, 0.07, 0.017 );
	 mHeadLight->setSpecularColour( 1.0, 1.0, 1.0 );
	 mHeadLight->setDiffuseColour( 1.0, 1.0, 1.0 );
	 mHeadNode->attachObject( mHeadLight );*/

	mBodyNode->setPosition(4.0, 1.5, 4.0);
//mBodyNode->lookAt( Ogre::Vector3::ZERO, Ogre::SceneNode::TS_WORLD );

	Ogre::Light* light = mSceneMgr->createLight();
	light->setType(Ogre::Light::LT_POINT);
	light->setCastShadows(false);
	light->setAttenuation(65, 1.0, 0.07, 0.017);
	light->setSpecularColour(.25, .25, .25);
	light->setDiffuseColour(0.35, 0.27, 0.23);
	mBodyNode->attachObject(light);
}

void Scene::update(float dt) {

	if (!notifiedChange) {
		m.lock();
		cout << "New room selected! Switching to: " << SELECTED_ROOM << endl;
		notifiedChange = true;
		m.unlock();
	}

	cv::Mat TheInputImageUnd_left = mRift->getImageUndLeft();
	aruco::CameraParameters CameraParamsUnd_left =
			mRift->getCameraParamsUndLeft();

	// DETECTING MARKERS on Left Camera
	TheMarkerDetector.detect(TheInputImageUnd_left, TheMarkers,
			CameraParamsUnd_left, TheMarkerSize);

	// Leap status
	char leapStatus[400];
	cv::Point leapTextPos(20, 20);

	if (isLeapConnected)
		sprintf(leapStatus, "Leap is connected.");
	else
		sprintf(leapStatus, "Leap is not connected.");
	cv::putText(TheInputImageUnd_left, leapStatus, leapTextPos,
			cv::FONT_HERSHEY_PLAIN, 1, cv::Scalar::all(255), 1, CV_AA);

	// Room status
	char roomStatus[400];
	cv::Point roomTextPos(20, 40);
	sprintf(roomStatus, "Room selected: %s", SELECTED_ROOM.c_str());
	cv::putText(TheInputImageUnd_left, roomStatus, roomTextPos,
			cv::FONT_HERSHEY_PLAIN, 1, cv::Scalar::all(255), 1, CV_AA);

	// UPDATE SCENE
	uint i;
	double position[3], orientation[4];

	double markers_left[TheMarkers.size()];

	// Clear marker state
	for (map<string, bool>::iterator it = ARMarkersState.begin();
			it != ARMarkersState.end(); ++it) {
		it->second = false;

		mARONodes[ARMarkersList[it->first]]->setVisible(false);

	}

// show nodes for detected markers
	for (i = 0; i < TheMarkers.size(); i++) {

		cout << "SEARCHING.....: "
				<< patch::to_string(TheMarkers[i].id) + SEPARATOR_ROOM
						+ SELECTED_ROOM << endl;

		TheMarkers[i].OgreGetPoseParameters(position, orientation);

		if (ARMarkersList.find(
				patch::to_string(TheMarkers[i].id) + SEPARATOR_ROOM
						+ SELECTED_ROOM) != ARMarkersList.end()) {

			int index = ARMarkersList[patch::to_string(TheMarkers[i].id)
					+ SEPARATOR_ROOM + SELECTED_ROOM];

			std::cout << "FOUND MARKER - Left Camera!! " << TheMarkers[i].id
					<< " at index " << index << std::endl;

			mARONodes[index]->setPosition(position[0], position[1],
					position[2]);
			mARONodes[index]->setOrientation(orientation[0], orientation[1],
					orientation[2], orientation[3]);

			mARONodes[index]->rotate(Ogre::Vector3(0, 0, 1),
					Ogre::Radian(
							Ogre::Degree(
									ARMarkersRotation[patch::to_string(
											TheMarkers[i].id) + SEPARATOR_ROOM
											+ SELECTED_ROOM])));
			mARONodes[index]->setScale(
					ARMarkersScale[patch::to_string(TheMarkers[i].id)
							+ SEPARATOR_ROOM + SELECTED_ROOM],
					ARMarkersScale[patch::to_string(TheMarkers[i].id)
							+ SEPARATOR_ROOM + SELECTED_ROOM],
					ARMarkersScale[patch::to_string(TheMarkers[i].id)
							+ SEPARATOR_ROOM + SELECTED_ROOM]);

			ARMarkersState[patch::to_string(TheMarkers[i].id) + SEPARATOR_ROOM
					+ SELECTED_ROOM] = true;
			mARONodes[index]->setVisible(true);

			if (selected_marker != -1 && selected_marker == TheMarkers[i].id)
				mRift->selectMarker(TheInputImageUnd_left, TheMarkers[i],
						CameraParamsUnd_left);

		} else
			TheMarkers[i].draw(TheInputImageUnd_left, cv::Scalar(0, 0, 255), 1);

		// Store z coordinates
		markers_left[i] = position[2];

	}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Find closest ID
////////////////////////////////////////////////////////////////////////////////////////////////////
	float min = 1e5;
	int id_min = -1;
	for (uint j = 0; j < TheMarkers.size(); j++) {
		if (markers_left[j] < min) {
			min = markers_left[j];
			id_min = j;
		}
	}

// Select closest marker
	if (id_min > -1) {
		cout << "#" << TheMarkers[id_min].id << " - Min Distance: " << min
				<< "\n";
		closest_marker = TheMarkers[id_min].id;

	}

	mRift->setTextureLeft(mRift->getPixelBoxLeft());

////////////////

// Updating Right Eye
	if (mRift->countCameras() == 2) {

		cv::Mat TheInputImageUnd_right = mRift->getImageUndRight();
		aruco::CameraParameters CameraParamsUnd_right =
				mRift->getCameraParamsUndRight();

		// Leap status
		cv::putText(TheInputImageUnd_right, leapStatus, leapTextPos,
				cv::FONT_HERSHEY_PLAIN, 1, cv::Scalar::all(255), 1, CV_AA);

		// Room stastus
		sprintf(roomStatus, "Room Selected: %s", SELECTED_ROOM.c_str());
		cv::putText(TheInputImageUnd_right, roomStatus, roomTextPos,
				cv::FONT_HERSHEY_PLAIN, 1, cv::Scalar::all(255), 1, CV_AA);

		/// DETECTING MARKERS on Right Camera
		TheMarkerDetector.detect(TheInputImageUnd_right, TheMarkers,
				CameraParamsUnd_right, TheMarkerSize);

		double markers_right[TheMarkers.size()];

		// show nodes for detected markers
		for (i = 0; i < TheMarkers.size(); i++) {
			TheMarkers[i].OgreGetPoseParameters(position, orientation);

			if (ARMarkersList.find(
					patch::to_string(TheMarkers[i].id) + SEPARATOR_ROOM
							+ SELECTED_ROOM) != ARMarkersList.end()) {

				int index = ARMarkersList[patch::to_string(TheMarkers[i].id)
						+ SEPARATOR_ROOM + SELECTED_ROOM];

				std::cout << "FOUND MARKER - Right Camera!! "
						<< TheMarkers[i].id << " at index " << index
						<< std::endl;

				mARONodes[index]->setPosition(position[0], position[1],
						position[2]);
				mARONodes[index]->setOrientation(orientation[0], orientation[1],
						orientation[2], orientation[3]);

				mARONodes[index]->rotate(Ogre::Vector3(0, 0, 1),
						Ogre::Radian(
								Ogre::Degree(
										ARMarkersRotation[patch::to_string(
												TheMarkers[i].id)
												+ SEPARATOR_ROOM + SELECTED_ROOM])));

				mARONodes[index]->setScale(
						ARMarkersScale[patch::to_string(TheMarkers[i].id)
								+ SEPARATOR_ROOM + SELECTED_ROOM],
						ARMarkersScale[patch::to_string(TheMarkers[i].id)
								+ SEPARATOR_ROOM + SELECTED_ROOM],
						ARMarkersScale[patch::to_string(TheMarkers[i].id)
								+ SEPARATOR_ROOM + SELECTED_ROOM]);

				ARMarkersState[patch::to_string(TheMarkers[i].id)
						+ SEPARATOR_ROOM + SELECTED_ROOM] = true;
				mARONodes[index]->setVisible(true);

				if (selected_marker != -1
						&& selected_marker == TheMarkers[i].id)
					mRift->selectMarker(TheInputImageUnd_right, TheMarkers[i],
							CameraParamsUnd_right);

			} else
				TheMarkers[i].draw(TheInputImageUnd_right,
						cv::Scalar(0, 0, 255), 1);

			// Store z coordinates
			markers_right[i] = position[2];

		}

		////////////////////////////////////////////////////////////////////////////////////////////////////
		/// Find closest ID
		////////////////////////////////////////////////////////////////////////////////////////////////////
		float min = 1e5;
		int id_min = -1;
		for (uint j = 0; j < TheMarkers.size(); j++) {
			if (markers_right[j] < min) {
				min = markers_right[j];
				id_min = j;
			}
		}

		// Select closest marker
		if (id_min > -1) {
			cout << "#" << TheMarkers[id_min].id << " - Min Distance: " << min
					<< "\n";
			closest_marker = TheMarkers[id_min].id;

		}

		mRift->setTextureRight(mRift->getPixelBoxRight());
	}

// Hide the other markers
	for (map<string, bool>::iterator it = ARMarkersState.begin();
			it != ARMarkersState.end(); ++it) {
		if (!it->second)
			mARONodes[ARMarkersList[patch::to_string(it->first) + SEPARATOR_ROOM
					+ SELECTED_ROOM]]->setVisible(false);

	}

	float forward = (mKeyboard->isKeyDown(OIS::KC_W) ? 0 : 1)
			+ (mKeyboard->isKeyDown(OIS::KC_S) ? 0 : -1);
	float leftRight = (mKeyboard->isKeyDown(OIS::KC_A) ? 0 : 1)
			+ (mKeyboard->isKeyDown(OIS::KC_D) ? 0 : -1);

	if (mKeyboard->isKeyDown(OIS::KC_LSHIFT)) {
		forward *= 3;
		leftRight *= 3;
	}

	Ogre::Vector3 dirX = mBodyTiltNode->_getDerivedOrientation()
			* Ogre::Vector3::UNIT_X;
	Ogre::Vector3 dirZ = mBodyTiltNode->_getDerivedOrientation()
			* Ogre::Vector3::UNIT_Z;

	mBodyNode->setPosition(
			mBodyNode->getPosition() + dirZ * forward * dt
					+ dirX * leftRight * dt);
}

//////////////////////////////////////////////////////////////
// Handle Rift Input:
//////////////////////////////////////////////////////////////

void Scene::setRiftPose(Ogre::Quaternion orientation, Ogre::Vector3 pos) {
	mHeadNode->setOrientation(orientation);
	mHeadNode->setPosition(pos);
}

void Scene::setIPD(float IPD) {
	mCamLeft->setPosition(-IPD / 2.0f, 0.0f, 0.0f);
	mCamRight->setPosition(IPD / 2.0f, 0.0f, 0.0f);
}

//////////////////////////////////////////////////////////////
// Handle Input:
//////////////////////////////////////////////////////////////

bool Scene::keyPressed(const OIS::KeyEvent& e) {
	return true;
}
bool Scene::keyReleased(const OIS::KeyEvent& e) {
	return true;
}
bool Scene::mouseMoved(const OIS::MouseEvent& e) {
	if (mMouse->getMouseState().buttonDown(OIS::MB_Left)) {
		mBodyNode->yaw(Ogre::Degree(-0.3 * e.state.X.rel));
		mBodyTiltNode->pitch(Ogre::Degree(-0.3 * e.state.Y.rel));
	}
	return true;
}
bool Scene::mousePressed(const OIS::MouseEvent& e, OIS::MouseButtonID id) {
	return true;
}
bool Scene::mouseReleased(const OIS::MouseEvent& e, OIS::MouseButtonID id) {
	return true;
}
