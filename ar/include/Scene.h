#ifndef SCENE_H
#define SCENE_H

#include "OGRE/Ogre.h"
#include "OIS/OIS.h"

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/calib3d/calib3d.hpp>
#include <aruco/aruco.h>
#include <iostream>
#include <sstream>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <boost/thread.hpp>

#include "Ogre.h"
#include <OIS/OIS.h>

#include "Leap.h"
#include <pthread.h>
#include <errno.h>

class Rift;

#include "Rift.h"

class Scene
{
	public:
		Scene( Ogre::Root* root, OIS::Mouse* mouse, OIS::Keyboard* keyboard, Rift* rift );
		~Scene();

		Ogre::SceneManager* getSceneMgr() { return mSceneMgr; }

		// Initialising/Loading the scene
		void loadARObjects();
		void createCameras();

		void update( float dt );

		Ogre::Camera* getLeftCamera() { return mCamLeft; }
		Ogre::Camera* getRightCamera() { return mCamRight; }
		void setIPD( float IPD );

		void setRiftPose( Ogre::Quaternion orientation, Ogre::Vector3 pos );

		// Keyboard and mouse events:
		bool keyPressed(const OIS::KeyEvent&);
		bool keyReleased(const OIS::KeyEvent&);
		bool mouseMoved(const OIS::MouseEvent&);
		bool mousePressed(const OIS::MouseEvent&, OIS::MouseButtonID);
		bool mouseReleased(const OIS::MouseEvent&, OIS::MouseButtonID);

		bool loadLeap();
		void loadMarkersData(string path);

		bool startSocketListener();

	private:
		Ogre::Root* mRoot;
		OIS::Mouse* mMouse;
		OIS::Keyboard* mKeyboard;
		Ogre::SceneManager* mSceneMgr;

		Ogre::Camera* mCamLeft;
		Ogre::Camera* mCamRight;

		Ogre::SceneNode* mHeadNode;
		Ogre::SceneNode* mBodyNode;
		Ogre::SceneNode* mBodyTiltNode;

		Ogre::SceneNode* mRoomNode;
		Ogre::SceneNode* mARONodes[20];

		Rift* mRift;
};


void *leapThread(void *arg);
void *socketListenThread(void *arg);

#endif
