/*
-----------------------------------------------------------------------------
Filename:    GameApplication.h
-----------------------------------------------------------------------------
*/
#ifndef __GameApplication_h_
#define __GameApplication_h_

#include "BaseApplication.h"
#include "Sound.h"
#include "PhysicsSystem.h"
#include "tcpserver.h"
#include "tcpclient.h"


#include <iostream>
#include <fstream>
using namespace std;

#include <deque>
#include <vector>
#include <list>

#include <stdlib.h>
#include <time.h>

#include <CEGUI/CEGUI.h>
#include <CEGUI/RendererModules/Ogre/CEGUIOgreRenderer.h>

class MessageHandler;
class AI;

enum GameState
{
    STATE_MAINMENU = 0,
    STATE_LEVEL1,
    STATE_LEVEL2,
    STATE_LEVEL3,
    STATE_AILEVEL1,
    STATE_AILEVEL2,
    STATE_AILEVEL3,
    STATE_AIBOSS1,
    STATE_AIBOSS2,
    STATE_AIBOSS3,
	STATE_MPLEVEL1,
    STATE_PAUSE
};

enum MPState
{
	MPSTATE_SP = 0,
	MPSTATE_SPAI,
	MPSTATE_MPSERVER,
	MPSTATE_MPCLIENT
};

enum SoundType
{
    SOUND_WALLCOLLISION = 0,
    SOUND_PADDLECOLLISION,
    SOUND_PADDLESERVE,
    SOUND_BALLMISS, 
    SOUND_FIREHIT,
    SOUND_FIRELAUNCH
};


class GameAPP : public BaseApplication
{
public:
    GameAPP(void);
    virtual ~GameAPP(void);
	void launchBall();
    void launchFireball(bool isPlayerOneLaunching);
 
protected:
    void updateUserInput(const Ogre::FrameEvent &evt, GameObject* paddleObj, Ogre::Vector3& paddleDirection);
    void updatePhysics(const Ogre::FrameEvent &evt);
    void updateFireballPhysics();
    void updateClientServerSynchronization(bool doSendMessages);
    void createGUI();

    void playSound(SoundType soundType);

    virtual void createScene();
    bool createMainMenuScene();
    bool createSPLevelScene(int level);
    bool createMPLevelScene();
    bool createAILevelScene(int level, bool isFireballReset = false);

    void clearScene();

    virtual void createFrameListener(void);
    virtual bool frameRenderingQueued(const Ogre::FrameEvent &evt);

	virtual bool keyPressed( const OIS::KeyEvent &arg );
    virtual bool keyReleased( const OIS::KeyEvent &arg );

#ifdef OGRE_IS_IOS
	virtual bool touchMoved(const OIS::MultiTouchEvent &evt);
	virtual bool touchPressed(const OIS::MultiTouchEvent &evt); 
	virtual bool touchReleased(const OIS::MultiTouchEvent &evt);
	virtual bool touchCancelled(const OIS::MultiTouchEvent &evt);
#else
    virtual bool mouseMoved( const OIS::MouseEvent& evt );
    virtual bool mousePressed( const OIS::MouseEvent& evt, OIS::MouseButtonID id );
    virtual bool mouseReleased( const OIS::MouseEvent& evt, OIS::MouseButtonID id );
#endif

    //GUI functions
    bool quit(const CEGUI::EventArgs &e);
    bool single(const CEGUI::EventArgs &e);
    bool multi(const CEGUI::EventArgs &e);
    bool server(const CEGUI::EventArgs &e);
    bool client(const CEGUI::EventArgs &e);
    bool client_ok(const CEGUI::EventArgs &e);
    bool cancel(const CEGUI::EventArgs &e);
    bool ai(const CEGUI::EventArgs &e);
    bool airetry(const CEGUI::EventArgs &e);
    bool easy(const CEGUI::EventArgs &e);
    bool medium(const CEGUI::EventArgs &e);
    bool hard(const CEGUI::EventArgs &e);

    void updateScore();
    void resetScore();

    GameState mState;
    GameState mLastState;
	MPState mMPState;
    int mDifficulty;
    bool mIsPlayerOneServing;
    bool mIsPlayerTwoServing;
    bool mGameWon;
    bool mGameLoss;

    PhysicsSystem* mSimulator;

	GameObject* mLevelObject;
	GameObject* mPaddleObjectOne; // used by single player or server player
	GameObject* mPaddleObjectTwo; // only usable by client player in mp
	GameObject* mBallObject;
    list<GameObject*> mFireballObjects; // used by final boss stage against player
    
    Ogre::ParticleSystem::ParticleSystem* mParticleSystem;

    Ogre::SceneNode* mFinalBossEyeNode;
	
	AI* mDeepBlue; // The most dangerous AI in the world

    Ogre::Vector3 mPaddleDirectionOne;    // The direction the paddle is moving
    Ogre::Vector3 mPaddleVelocityDirectionOne;
	Ogre::Vector3 mPaddleDirectionTwo;
    Ogre::Vector3 mPaddleVelocityDirectionTwo;
    
    // these two paddle position vectors are only used for keeping paddle position constant between boss stages
    Ogre::Vector3 mStoredPaddlePositionOne;
    Ogre::Vector3 mStoredPaddlePositionTwo;

    Ogre::Real mTimePassedOnLevel;
    Ogre::Real mTimePassedSinceLastNetUpdate;

    btVector3 mPreviousBallVelocity;

    static const btScalar mMaxBallSpeedLimit = 300.0f;
    btScalar baseMinForwardSpeedForLevel;
	btScalar minForwardSpeedForLevel;
	btScalar minForwardSpeedIncreasePerHit;

    // networking variables
    std::vector<std::string> mReceivedMessages;
    SDL_mutex* mLock;
    thread_data mpThreadData;
    int staggeredSendUpdateNum; // for use in staggering sending of data to prevent problems with receiving

    // message handler
    MessageHandler* mMessageHandler;
	
    
	int numHitsWithPaddle;
    
    // life and score tracking
   	int playerOneNumLives;
    int playerOneScore;
    int playerOneHighScore;
    int playerTwoNumLives;
    int playerTwoScore;
    int playerTwoHighScore;

    int mScoreMult;

    // Other UI info
    int levelNum;
    
	bool isReadyForHitIncrementPlayerOne;
    bool isReadyForHitIncrementPlayerTwo;
    
    
	ofstream sendfile;

    //CEGUI TESTING
    CEGUI::OgreRenderer* mRenderer;

};

#endif // #ifndef __GameApplication_h_
