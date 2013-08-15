/*
-----------------------------------------------------------------------------
Filename:    GameApplication.cpp
-----------------------------------------------------------------------------
*/

#include "GameApplication.h"
#include "GameObject.h"
#include "MessageHandler.h"
#include "AI.h"
#include <math.h>
#include <OgreParticleSystem.h>
#include <stdlib.h>



//-------------------------------------------------------------------------------------
GameAPP::GameAPP(void)
{
    mState = STATE_MAINMENU;
	mMPState = MPSTATE_SP;
    mDifficulty = 1;
    mIsPlayerOneServing = true;
    mIsPlayerTwoServing = false;
    baseMinForwardSpeedForLevel = 0.0f;
	minForwardSpeedForLevel = 0.0f;
	minForwardSpeedIncreasePerHit = 0.0f;
	numHitsWithPaddle = 0;
    playerOneNumLives = 0;
    playerOneScore = 0;
    playerOneHighScore = 0;
    playerTwoNumLives = 0;
    playerTwoScore = 0;
    playerTwoHighScore = 0;
    levelNum = 0;
    mGameWon = false;
    mGameLoss = false;

    mPreviousBallVelocity = btVector3(0.0,0.0,0.0);

    // Other UI info
    int levelNum;
    isReadyForHitIncrementPlayerOne = false;
    isReadyForHitIncrementPlayerTwo = false;
    mScoreMult = 1;
	init_Audio();

    mLock = SDL_CreateMutex();

    mpThreadData.message_list = &mReceivedMessages;
    mpThreadData.lock = mLock;

    mMessageHandler = new MessageHandler();

    staggeredSendUpdateNum = 0;
 
    sendfile.open("net_send.txt");

}
//-------------------------------------------------------------------------------------
GameAPP::~GameAPP(void)
{
    SDL_DestroyMutex(mLock);
    sendfile.close();
    delete mMessageHandler;
	delete mDeepBlue;
}

//-------------------------------------------------------------------------------------

void
GameAPP::updateUserInput(const Ogre::FrameEvent &evt, GameObject* paddleObj, Ogre::Vector3& paddleDirection)
{
	if(paddleObj == NULL || mGameWon || mGameLoss)
        return;

    Ogre::Vector3 paddlePosition = paddleObj->getNode()->getPosition();

    if( paddlePosition.x > 42.0f/* && paddleDirection.x > 0.0f*/)
    {
        paddleDirection.x = 0.0f;
        paddleObj->getNode()->setPosition(42.0f, paddlePosition.y, paddlePosition.z);
    }
	 else if( paddlePosition.x < -42.0f/* && paddleDirection.x < 0.0f*/)
    {
        paddleDirection.x = 0.0f;
        paddleObj->getNode()->setPosition(-42.0f, paddlePosition.y, paddlePosition.z);
    }
	 else if( paddlePosition.y > 69.0f/* && paddleDirection.y > 0.0f*/)
    {
        paddleDirection.y = 0.0f;
        paddleObj->getNode()->setPosition(paddlePosition.x, 69.0f, paddlePosition.z);
    }
	 else if( paddlePosition.y < 6.0f/* && paddleDirection.y < 0.0f*/)
    {
  	    paddleDirection.y = 0.0f;
        paddleObj->getNode()->setPosition(paddlePosition.x, 6.0f, paddlePosition.z);
    }
	paddleObj->getNode()->translate(paddleDirection * evt.timeSinceLastFrame, Ogre::Node::TS_LOCAL);

    paddleDirection.x = 0.0f;
    paddleDirection.y = 0.0f;
}

void
GameAPP::updatePhysics(const Ogre::FrameEvent &evt)
{
    // this is the single player and server's resposibilty to update physics
    if(mMPState == MPSTATE_MPCLIENT)
        return;

    btTransform trans;

	mSimulator->stepSimulation(evt.timeSinceLastFrame, 2);

    if(mBallObject == NULL)
        goto ENDPHYS;
	  
	// loss of ball check.
    // player one misses case
	if(mMPState != MPSTATE_SP && mBallObject->getNode()->getPosition().z > 220.0f ||
        (mMPState == MPSTATE_SP && mBallObject->getNode()->getPosition().z > 190.0f || 
        mMPState == MPSTATE_SP && mBallObject->getNode()->getPosition().z < -500.0f))
	{
		if(mMPState != MPSTATE_SP)
		{			
			if(mMPState == MPSTATE_MPSERVER)
			{
				// alert client of player one miss
				std::string message = "Msg_P1Miss";
				server_send(message);
			}
			minForwardSpeedForLevel = baseMinForwardSpeedForLevel; // reset speed to prevent continuing speed up
			playerTwoScore += mScoreMult;
		}

        playSound(SOUND_BALLMISS);
		isReadyForHitIncrementPlayerOne = false;
        isReadyForHitIncrementPlayerTwo = false;
		mIsPlayerOneServing = true;
        mIsPlayerTwoServing = false;
        --playerOneNumLives;
        
		mBallObject->getBody()->setLinearVelocity(btVector3(0.0f, 0.0f, 0.0f)); // bug on going to main menu
	}
    // player two misses case
    else if(mMPState != MPSTATE_SP && mBallObject->getNode()->getPosition().z < -220.0f)
    {
        if(mMPState == MPSTATE_MPSERVER)
		{
				// alert client of player two miss
			std::string message = "Msg_P2Miss";
			server_send(message);
		}	

        minForwardSpeedForLevel = baseMinForwardSpeedForLevel;  // reset speed to prevent continuing speed up
        playSound(SOUND_BALLMISS);
		isReadyForHitIncrementPlayerOne = false;
        isReadyForHitIncrementPlayerTwo = false;
        mIsPlayerOneServing = false;
		mIsPlayerTwoServing = true;
        
        --playerTwoNumLives;
        playerOneScore += mScoreMult;

		mBallObject->getBody()->setLinearVelocity(btVector3(0.0f, 0.0f, 0.0f)); // bug on going to main menu
    }

	// translate paddles based on user input
	{
		trans.setIdentity();
		trans.setOrigin(btVector3(mPaddleObjectOne->getNode()->getPosition().x, mPaddleObjectOne->getNode()->getPosition().y, mPaddleObjectOne->getNode()->getPosition().z));
		btRigidBody* body = mPaddleObjectOne->getBody();
		body->getMotionState()->setWorldTransform(trans);
		body->setCenterOfMassTransform(trans);

		if(mPaddleObjectTwo != NULL)
		{
			trans.setIdentity();
			trans.setOrigin(btVector3(mPaddleObjectTwo->getNode()->getPosition().x, mPaddleObjectTwo->getNode()->getPosition().y, mPaddleObjectTwo->getNode()->getPosition().z));
			trans.setRotation(btQuaternion(btVector3(0.0f, 1.0f, 0.0f), btScalar(3.14159265358f)));
			body = mPaddleObjectTwo->getBody();
			body->getMotionState()->setWorldTransform(trans);
			body->setCenterOfMassTransform(trans);
		}
	}

    if(!mIsPlayerOneServing && !mIsPlayerTwoServing)
    {
        btVector3 velocity = mBallObject->getBody()->getLinearVelocity();
		btScalar zVelocity = velocity.getZ();
        bool isCollisionSoundPlayed = false;

        // check for paddle hit
		if(zVelocity > 0.0f && mBallObject->getNode()->getPosition().z > 115.0f)
			isReadyForHitIncrementPlayerOne = true;

        
		if(zVelocity < 0.0f && isReadyForHitIncrementPlayerOne)
		{
            if(mMPState == MPSTATE_MPSERVER)
            {
                // alert client of paddle hit
                std::string message = "Msg_PdlHit";
                server_send(message);
            }

			playSound(SOUND_PADDLECOLLISION);
            isCollisionSoundPlayed = true;
			++numHitsWithPaddle;
            if(mMPState == MPSTATE_SP)
                playerOneScore += mScoreMult; // increase score for hitting the ball with the paddle in normal sp mode
			minForwardSpeedForLevel += minForwardSpeedIncreasePerHit;
			isReadyForHitIncrementPlayerOne = false;
            isReadyForHitIncrementPlayerTwo = false;

            // modify x and y velocity with hit
            velocity = mBallObject->getBody()->getLinearVelocity();
	        mBallObject->getBody()->setLinearVelocity(btVector3(velocity.getX() + mPaddleVelocityDirectionOne.x * 2.0f, velocity.getY() + mPaddleVelocityDirectionOne.y * 2.0f, velocity.getZ()));
		}

        // ball hit check for player two
        if(mMPState != MPSTATE_SP)
        {
            if(zVelocity < 0.0f && mBallObject->getNode()->getPosition().z < -115.0f)
			    isReadyForHitIncrementPlayerTwo = true;
            
		    if(zVelocity > 0.0f && isReadyForHitIncrementPlayerTwo)
		    {
                if(mMPState == MPSTATE_MPSERVER)
                {
                    // alert client of paddle hit
                    std::string message = "Msg_PdlHit";
                    server_send(message);
                }

			    playSound(SOUND_PADDLECOLLISION);
                isCollisionSoundPlayed = true;
			    ++numHitsWithPaddle;
			    minForwardSpeedForLevel += minForwardSpeedIncreasePerHit;
			    isReadyForHitIncrementPlayerOne = false;
                isReadyForHitIncrementPlayerTwo = false;

                // modify x and y velocity with hit
                velocity = mBallObject->getBody()->getLinearVelocity();
	            mBallObject->getBody()->setLinearVelocity(btVector3(velocity.getX() - mPaddleVelocityDirectionTwo.x * 2.0f, velocity.getY() - mPaddleVelocityDirectionTwo.y * 2.0f, velocity.getZ()));
		    }
        }

        velocity = mBallObject->getBody()->getLinearVelocity();
        zVelocity = velocity.getZ();

        // ensure minimal z velocity of ball
		
        if(zVelocity <= 0.0f && zVelocity > -minForwardSpeedForLevel)
			zVelocity = -minForwardSpeedForLevel;
		else if(zVelocity > 0.0f && zVelocity < minForwardSpeedForLevel)
			zVelocity = minForwardSpeedForLevel;
        
        // ensure maximal velocities all axes
        btScalar xVelocity = velocity.getX();
        btScalar yVelocity = velocity.getY();
        if(xVelocity <= 0.0f && xVelocity < -minForwardSpeedForLevel * 0.9)
			xVelocity = -minForwardSpeedForLevel * 1.0;
		else if(xVelocity > 0.0f && xVelocity > minForwardSpeedForLevel * 0.9)
			xVelocity = minForwardSpeedForLevel * 1.0;

        if(yVelocity <= 0.0f && yVelocity < -minForwardSpeedForLevel * 0.9)
			yVelocity = -minForwardSpeedForLevel * 1.0;
		else if(yVelocity > 0.0f && yVelocity > minForwardSpeedForLevel * 0.9)
			yVelocity = minForwardSpeedForLevel * 1.0;

        // ensure maximal z velocity of ball
        if(zVelocity <= 0.0f && zVelocity < -mMaxBallSpeedLimit)
            zVelocity = -mMaxBallSpeedLimit;
        else if(zVelocity > 0.0f && zVelocity > mMaxBallSpeedLimit)
            zVelocity = mMaxBallSpeedLimit;

		mBallObject->getBody()->setLinearVelocity(btVector3(xVelocity, yVelocity, zVelocity));

        velocity = mBallObject->getBody()->getLinearVelocity();

        // check for wall collisions
        if(!isCollisionSoundPlayed && 
            ((mPreviousBallVelocity.x() < 0.0f && velocity.x() > 0.0f) ||
            (mPreviousBallVelocity.y() < 0.0f && velocity.y() > 0.0f && velocity.y() - mPreviousBallVelocity.y() > 0.2f) ||
            (mPreviousBallVelocity.z() < 0.0f && velocity.z() > 0.0f) ||
            (mPreviousBallVelocity.x() > 0.0f && velocity.x() < 0.0f) ||
            (mPreviousBallVelocity.y() > 0.0f && velocity.y() < 0.0f && mPreviousBallVelocity.y() - velocity.y() > 0.2f) ||
            (mPreviousBallVelocity.z() > 0.0f && velocity.z() < 0.0f)))
        {
            if(mMPState == MPSTATE_MPSERVER)
            {
                std::string message = "Msg_WalHit";
                server_send(message);
            }
            playSound(SOUND_WALLCOLLISION);
        }

        mPreviousBallVelocity = velocity;
    }
	else
	{
		//mBallObject->getNode()->translate(mPaddleDirectionOne* evt.timeSinceLastFrame * .5, Ogre::Node::TS_LOCAL);
		// set ball location to paddle location
		trans.setIdentity();

		if(mMPState != MPSTATE_SP)
		{
			// TODO: allow for appropriate switching of positions when on the client or server side
            if(mIsPlayerOneServing)
			    trans.setOrigin(btVector3(mPaddleObjectOne->getNode()->getPosition().x, mPaddleObjectOne->getNode()->getPosition().y, 148.0f));
            else if(mIsPlayerTwoServing)
                trans.setOrigin(btVector3(mPaddleObjectTwo->getNode()->getPosition().x, mPaddleObjectTwo->getNode()->getPosition().y, -148.0f));
		}
		else
		{
			trans.setOrigin(btVector3(mPaddleObjectOne->getNode()->getPosition().x, mPaddleObjectOne->getNode()->getPosition().y, 118.0f));
		}
		btRigidBody* body = mBallObject->getBody();
		body->getMotionState()->setWorldTransform(trans);
		body->setCenterOfMassTransform(trans);
	}
	
	// make sure ball speed does not exceed maximum
    if(minForwardSpeedForLevel > mMaxBallSpeedLimit * 0.9f)
        minForwardSpeedForLevel = mMaxBallSpeedLimit * 0.9f;
    
    // make eye object follow the boss paddle and look at the player paddle
    if(mFinalBossEyeNode != NULL)
    {
        mFinalBossEyeNode->lookAt(mPaddleObjectOne->getNode()->getPosition(), Ogre::Node::TS_WORLD, Ogre::Vector3(0.0f, 1.0f, 0.0f));
        mFinalBossEyeNode->setPosition(mPaddleObjectTwo->getNode()->getPosition());
    }
    ENDPHYS:
    updateFireballPhysics();
}

void
GameAPP::updateFireballPhysics()
{  
    //btVector3 velocity = mBallObject->getBody()->getLinearVelocity();
    
    if(mFireballObjects.size() > 0)
    {
        list<GameObject*>::iterator listIterator = mFireballObjects.begin();
        for(int i = 0; i < mFireballObjects.size(); ++i)
        {
            GameObject*& fireball = *listIterator;
            
            if(fireball == NULL)
            {
                ++listIterator;
                continue;
            }
            
            Ogre::SceneNode* node = fireball->getNode();
            Ogre::Vector3 fireballPosition = node->getPosition();
            
            // remove fireball if out of bounds
            if(fireballPosition.x > 200.0f || fireballPosition.x < -200.0f ||
                fireballPosition.y > 200.0f || fireballPosition.y < -200.0f ||
                fireballPosition.z > 300.0f || fireballPosition.z < -300.0f)
            {
                // destroy node, entity, and light associated with fireball
                char fireballIdentity = node->getName()[12];
                
                string entityName = "FireballEntity";
                string nodeName = "FireballNode";
                string lightName = "FireballLight";
                string smokeName = "FireballSmoke";
               // string explodeName = "FireballExplode";
                entityName.push_back(fireballIdentity);
                nodeName.push_back(fireballIdentity);
                lightName.push_back(fireballIdentity);
                smokeName.push_back(fireballIdentity);
               // explodeName.push_back(fireballIdentity);
                
                node->detachAllObjects();
                mSceneMgr->destroyParticleSystem(smokeName);
                //mSceneMgr->destroyParticleSystem(explodeName);
                mSceneMgr->destroyEntity(entityName);
                mSceneMgr->destroyLight(lightName);
                mSceneMgr->destroySceneNode(nodeName);
                
                // destroy gameobject and remove from simulator
                mSimulator->removeObject(fireball);
                fireball = NULL;
            }
            else
            {
                // check for paddle collision with player's paddle
                Ogre::Vector3 paddlePosition = mPaddleObjectOne->getNode()->getPosition();
                if(std::abs(fireballPosition.x - paddlePosition.x) < 14.0f && std::abs(fireballPosition.y - paddlePosition.y) < 9.0f && std::abs(fireballPosition.z - paddlePosition.z) < 5.0f)
                {
                    //create fireball explosion on paddle hit only if it doesnt exist
                    if(mParticleSystem == NULL){
                        --playerOneNumLives;
                        playSound(SOUND_FIREHIT);
                        // create a particle system named explosions using the explosionTemplate
                        mParticleSystem = mSceneMgr->createParticleSystem("FireballExplode", "explosionTemplate");
                        // fast forward 1 second  to the point where the particle has been emitted
                        mParticleSystem->fastForward(1.0);
                       // attach the particle system to a scene node
                        mPaddleObjectOne->getNode()->attachObject(mParticleSystem);
                        mSceneMgr->destroyEntity("Ball");
                        mSceneMgr->destroySceneNode("BallNode");
                        mSimulator->removeObject(mBallObject);
                        mBallObject = NULL;

                    }
                       
                }
                //speed up fireball animation then destroy it and create new scene
                if(mParticleSystem != NULL){
                     mParticleSystem->fastForward(0.000001);
                     if(mParticleSystem->getNumParticles() <= 0){
                         mSceneMgr->destroyParticleSystem("FireballExplode");
                         mParticleSystem = NULL;
                         clearScene();
                         createAILevelScene(6, true);
                     }
                
                }
            }
            
            ++listIterator;
        }
    }
    
    // remove null elements from list
    mFireballObjects.remove(NULL);
}

void
GameAPP::updateClientServerSynchronization(bool doSendMessages)
{
    if(mMPState == MPSTATE_SP || mMPState == MPSTATE_SPAI)
        return;

    // message receiving code
    std::vector<std::string> newMessages;

    if(mReceivedMessages.size() > 0)
    {
        //sendfile << "num received messages: " << mReceivedMessages.size() << "\n";
        // move all received messages to the newMessages vector in a lock
        SDL_mutexP(mLock);
        while(mReceivedMessages.size() > 0)
        {
            //sendfile << "received message: " << mReceivedMessages.back() << "\n";
            newMessages.push_back(mReceivedMessages.back());
            mReceivedMessages.pop_back();
        }       
		SDL_mutexV(mLock);
    }

    // handle received messages
    if(mMPState == MPSTATE_MPCLIENT)
    {
        for(unsigned int i = 0; i < newMessages.size(); ++i)
        {
            //sendfile << "handle message num " << i <<"\n";
            std::string& message = newMessages[i];
            std::string messageName = mMessageHandler->GetMessageName(message);

            //sendfile << "message name: " << messageName << "\n";
            //sendfile << "whole message: " << message << "\n";

            if(messageName == "Msg_Pd1Pos")
            {
                Ogre::Vector3 pos = mMessageHandler->GetVector3(message);
                mPaddleObjectOne->getNode()->setPosition(pos);
                //sendfile << "updated mPaddleObjectOne\n";
            }
            else if(messageName == "Msg_BalPos")
            {
                Ogre::Vector3 pos = mMessageHandler->GetVector3(message);
                mBallObject->getNode()->setPosition(pos);
                //sendfile << "updated mBallObject\n";
            }
            else if(messageName == "Msg_P1Miss")
            {
                playSound(SOUND_BALLMISS);
                
                --playerOneNumLives;
                playerTwoScore += mScoreMult;
            }
            else if(messageName == "Msg_P2Miss")
            {
                playSound(SOUND_BALLMISS);
            
                --playerTwoNumLives;
                playerOneScore += mScoreMult;
            }
            else if(messageName == "Msg_PdlHit")
            {
                playSound(SOUND_PADDLECOLLISION);
            }
            else if(messageName == "Msg_PdlSer")
            {
                playSound(SOUND_PADDLESERVE);
            }
            else if(messageName == "Msg_WalHit")
            {
                playSound(SOUND_WALLCOLLISION);
            }   
        }
    }
    else if(mMPState == MPSTATE_MPSERVER)
    {
        for(unsigned int i = 0; i < newMessages.size(); ++i)
        {
            std::string& message = newMessages[i];
            std::string messageName = mMessageHandler->GetMessageName(message);

            if(messageName == "Msg_Pd2Pos")
            {
                Ogre::Vector3 pos = mMessageHandler->GetVector3(message);
                mPaddleObjectTwo->getNode()->setPosition(pos);
            }
            else if(messageName == "Msg_P2Clck")
            {
                if(mIsPlayerTwoServing)
                    launchBall();
            }
            else if(messageName == "Msg_Pd2VDi")
            {
                Ogre::Vector3 pos = mMessageHandler->GetVector3(message);
                mPaddleVelocityDirectionTwo = pos;
            }
        }
    }
    else
    {
        assert(0);
    }

    if(!doSendMessages)
        return;

    // send messages
    if(mMPState == MPSTATE_MPCLIENT)
    {
        // update paddle two position
        if(staggeredSendUpdateNum == 0)
        {
            std::string message = "Msg_Pd2Pos";
            message = mMessageHandler->EncodeVector3(message, mPaddleObjectTwo->getNode()->getPosition());
            client_send(message);
        }
        else if(staggeredSendUpdateNum == 1)
        {
            std::string message = "Msg_Pd2VDi";
            message = mMessageHandler->EncodeVector3(message, mPaddleVelocityDirectionTwo);
            client_send(message);
        }

        ++staggeredSendUpdateNum;
        if(staggeredSendUpdateNum > 1)
            staggeredSendUpdateNum = 0;
    }
    else if(mMPState == MPSTATE_MPSERVER)
    {
        // update position of paddles and balls for the client
        if(staggeredSendUpdateNum == 0)
        {
            // update ball position
            std::string message = "Msg_BalPos";
            btRigidBody* body = mBallObject->getBody();
            btVector3 pos = body->getCenterOfMassPosition();
            //message = mMessageHandler->EncodeVector3(message, mBallObject->getNode()->getPosition());
            message = mMessageHandler->EncodeVector3(message, Ogre::Vector3(pos.getX(), pos.getY(), pos.getZ()));
            server_send(message);
        }
        else if(staggeredSendUpdateNum == 1)
        {
            // update paddle one position
            std::string message = "Msg_Pd1Pos";
            message = mMessageHandler->EncodeVector3(message, mPaddleObjectOne->getNode()->getPosition());
            server_send(message);
           // sendfile << "SERVER PADDLE: " << message << endl;
        }
        ++staggeredSendUpdateNum;
        if(staggeredSendUpdateNum > 1)
            staggeredSendUpdateNum = 0;
    }
}


void
GameAPP::createGUI()
{
    mRenderer = &CEGUI::OgreRenderer::bootstrapSystem();
    
    CEGUI::Imageset::setDefaultResourceGroup("Imagesets");
    CEGUI::Font::setDefaultResourceGroup("Fonts");
    CEGUI::Scheme::setDefaultResourceGroup("Schemes");
    CEGUI::WidgetLookManager::setDefaultResourceGroup("LookNFeel");
    CEGUI::WindowManager::setDefaultResourceGroup("Layouts");

    CEGUI::SchemeManager::getSingleton().create("TaharezLook.scheme");
    CEGUI::System::getSingleton().setDefaultMouseCursor("TaharezLook", "MouseArrow");

    CEGUI::WindowManager &wmgr = CEGUI::WindowManager::getSingleton();
    CEGUI::Window *sheet = wmgr.createWindow("DefaultWindow", "CEGUIDemo/Sheet");
    //CEGUI::Window *menu_node = wmgr.createWindow("DefaultWindow", "CEGUIDemo/Menu");
    //CEGUI::Window *score_node = wmgr.createWindow("DefaultWindow", "CEGUIDemo/Score");
    //sheet->addChildWindow(menu_node);
    //sheet->addChildWindow(score_node);

    //MENU GUI
    CEGUI::Window *quit = wmgr.createWindow("TaharezLook/Button", "CEGUIDemo/QuitButton");
    quit->setText("Quit");
    quit->setSize(CEGUI::UVector2(CEGUI::UDim(0.2, 0), CEGUI::UDim(0.05, 0)));
    quit->setPosition(CEGUI::UVector2(CEGUI::UDim(0.4f, 0), CEGUI::UDim(0.5f, 0)));
    sheet->addChildWindow(quit);

    CEGUI::Window *single = wmgr.createWindow("TaharezLook/Button", "CEGUIDemo/SingleButton");
    single->setText("SinglePlayer");
    single->setSize(CEGUI::UVector2(CEGUI::UDim(0.2, 0), CEGUI::UDim(0.05, 0)));
    single->setPosition(CEGUI::UVector2(CEGUI::UDim(0.4f, 0), CEGUI::UDim(0.35f, 0)));
    sheet->addChildWindow(single);

    //AI OPTION BUTTON
    CEGUI::Window *ai = wmgr.createWindow("TaharezLook/Button", "CEGUIDemo/AIButton");
    ai->setText("SinglePlayer vs AI");
    ai->setSize(CEGUI::UVector2(CEGUI::UDim(0.2, 0), CEGUI::UDim(0.05, 0)));
    ai->setPosition(CEGUI::UVector2(CEGUI::UDim(0.4f, 0), CEGUI::UDim(0.4f, 0)));
    sheet->addChildWindow(ai);
    

    CEGUI::Window *multi = wmgr.createWindow("TaharezLook/Button", "CEGUIDemo/MultiButton");
    multi->setText("MultiPlayer");
    multi->setSize(CEGUI::UVector2(CEGUI::UDim(0.2, 0), CEGUI::UDim(0.05, 0)));
    multi->setPosition(CEGUI::UVector2(CEGUI::UDim(0.4f, 0), CEGUI::UDim(0.45f, 0)));
    sheet->addChildWindow(multi);

    CEGUI::Window *server = wmgr.createWindow("TaharezLook/Button", "CEGUIDemo/ServerButton");
    server->setText("Server");
    server->setSize(CEGUI::UVector2(CEGUI::UDim(0.2, 0), CEGUI::UDim(0.05, 0)));
    server->setPosition(CEGUI::UVector2(CEGUI::UDim(0.4f, 0), CEGUI::UDim(0.4f, 0)));
    sheet->addChildWindow(server);
    server->hide();

    CEGUI::Window *client = wmgr.createWindow("TaharezLook/Button", "CEGUIDemo/ClientButton");
    client->setText("Client");
    client->setSize(CEGUI::UVector2(CEGUI::UDim(0.2, 0), CEGUI::UDim(0.05, 0)));
    client->setPosition(CEGUI::UVector2(CEGUI::UDim(0.4f, 0), CEGUI::UDim(0.45f, 0)));
    sheet->addChildWindow(client);
    client->hide();

    // Editbox  for client
	CEGUI::Window *editbox = wmgr.createWindow("TaharezLook/Editbox", "CEGUIDemo/Edit");
	editbox->setText("Enter Server Name"); 
    editbox->setSize(CEGUI::UVector2(CEGUI::UDim(0.2, 0), CEGUI::UDim(0.05, 0)));
    editbox->setPosition(CEGUI::UVector2(CEGUI::UDim(0.4f, 0), CEGUI::UDim(0.45f, 0)));
    editbox->setProperty("ReadOnly","False");
    sheet->addChildWindow(editbox);
    editbox->hide();

    CEGUI::Window *client_ok = wmgr.createWindow("TaharezLook/Button", "CEGUIDemo/ClientStart");
	client_ok->setText("Ok"); 
    client_ok->setSize(CEGUI::UVector2(CEGUI::UDim(0.05, 0), CEGUI::UDim(0.05, 0)));
    client_ok->setPosition(CEGUI::UVector2(CEGUI::UDim(0.6f, 0), CEGUI::UDim(0.45f, 0)));
    sheet->addChildWindow(client_ok);
    client_ok->hide();
    
    
    CEGUI::Window *cancel = wmgr.createWindow("TaharezLook/Button", "CEGUIDemo/CancelButton");
    cancel->setText("Cancel");
    cancel->setSize(CEGUI::UVector2(CEGUI::UDim(0.2, 0), CEGUI::UDim(0.05, 0)));
    cancel->setPosition(CEGUI::UVector2(CEGUI::UDim(0.4f, 0), CEGUI::UDim(0.5f, 0)));
    sheet->addChildWindow(cancel);
    cancel->hide();

    // SCORE GUI
    CEGUI::Window *score = wmgr.createWindow("TaharezLook/StaticText", "CEGUIDemo/ScoreText");
    score->setText("Score: ");
    score->setSize(CEGUI::UVector2(CEGUI::UDim(0.2, 0), CEGUI::UDim(0.05, 0)));
    score->setPosition(CEGUI::UVector2(CEGUI::UDim(0, 0), CEGUI::UDim(0, 0)));
    sheet->addChildWindow(score);
    score->hide();

    CEGUI::Window *lives = wmgr.createWindow("TaharezLook/StaticText", "CEGUIDemo/LivesText");
    lives->setText("Lives: ");
    lives->setSize(CEGUI::UVector2(CEGUI::UDim(0.2, 0), CEGUI::UDim(0.05, 0)));
    lives->setPosition(CEGUI::UVector2(CEGUI::UDim(0.80, 0), CEGUI::UDim(0, 0)));
    sheet->addChildWindow(lives);
    lives->hide();

    CEGUI::Window *score2 = wmgr.createWindow("TaharezLook/StaticText", "CEGUIDemo/ScoreText2");
    score2->setText("Player2 Score: ");
    score2->setSize(CEGUI::UVector2(CEGUI::UDim(0.2, 0), CEGUI::UDim(0.05, 0)));
    score2->setPosition(CEGUI::UVector2(CEGUI::UDim(0, 0), CEGUI::UDim(0.05, 0)));
    sheet->addChildWindow(score2);
    score2->hide();
    
    CEGUI::Window *lives2 = wmgr.createWindow("TaharezLook/StaticText", "CEGUIDemo/LivesText2");
    lives2->setText("Enemy Lives: ");
    lives2->setSize(CEGUI::UVector2(CEGUI::UDim(0.2, 0), CEGUI::UDim(0.05, 0)));
    lives2->setPosition(CEGUI::UVector2(CEGUI::UDim(0.80, 0), CEGUI::UDim(0.05, 0)));
    sheet->addChildWindow(lives2);
    lives2->hide();

    //WIN AND LOSE MENU
    CEGUI::Window *win = wmgr.createWindow("TaharezLook/StaticText", "CEGUIDemo/WinLoss");
    win->setText("           You Win!!!");
    win->setSize(CEGUI::UVector2(CEGUI::UDim(0.2, 0), CEGUI::UDim(0.05, 0)));
    win->setPosition(CEGUI::UVector2(CEGUI::UDim(0.4f, 0), CEGUI::UDim(0.35f, 0)));
    sheet->addChildWindow(win);
    win->hide();

    CEGUI::Window *retry = wmgr.createWindow("TaharezLook/Button", "CEGUIDemo/RetryButton");
    retry->setText("Retry");
    retry->setSize(CEGUI::UVector2(CEGUI::UDim(0.2, 0), CEGUI::UDim(0.05, 0)));
    retry->setPosition(CEGUI::UVector2(CEGUI::UDim(0.4f, 0), CEGUI::UDim(0.4f, 0)));
    sheet->addChildWindow(retry);
    retry->hide();
    
    CEGUI::Window *airetry = wmgr.createWindow("TaharezLook/Button", "CEGUIDemo/AIRetryButton");
    airetry->setText("Retry");
    airetry->setSize(CEGUI::UVector2(CEGUI::UDim(0.2, 0), CEGUI::UDim(0.05, 0)));
    airetry->setPosition(CEGUI::UVector2(CEGUI::UDim(0.4f, 0), CEGUI::UDim(0.4f, 0)));
    sheet->addChildWindow(airetry);
    airetry->hide();

    CEGUI::Window *mainmenu = wmgr.createWindow("TaharezLook/Button", "CEGUIDemo/MainMenuButton");
    mainmenu->setText("Main Menu");
    mainmenu->setSize(CEGUI::UVector2(CEGUI::UDim(0.2, 0), CEGUI::UDim(0.05, 0)));
    mainmenu->setPosition(CEGUI::UVector2(CEGUI::UDim(0.4f, 0), CEGUI::UDim(0.45f, 0)));
    sheet->addChildWindow(mainmenu);
    mainmenu->hide();

    CEGUI::Window *easy = wmgr.createWindow("TaharezLook/Button", "CEGUIDemo/EasyButton");
    easy->setText("Easy");
    easy->setSize(CEGUI::UVector2(CEGUI::UDim(0.2, 0), CEGUI::UDim(0.05, 0)));
    easy->setPosition(CEGUI::UVector2(CEGUI::UDim(0.4f, 0), CEGUI::UDim(0.4f, 0)));
    sheet->addChildWindow(easy);
    easy->hide();

    CEGUI::Window *medium = wmgr.createWindow("TaharezLook/Button", "CEGUIDemo/MediumButton");
    medium->setText("Medium");
    medium->setSize(CEGUI::UVector2(CEGUI::UDim(0.2, 0), CEGUI::UDim(0.05, 0)));
    medium->setPosition(CEGUI::UVector2(CEGUI::UDim(0.4f, 0), CEGUI::UDim(0.45f, 0)));
    sheet->addChildWindow(medium);
    medium->hide();
    
    CEGUI::Window *hard = wmgr.createWindow("TaharezLook/Button", "CEGUIDemo/HardButton");
    hard->setText("Hard");
    hard->setSize(CEGUI::UVector2(CEGUI::UDim(0.2, 0), CEGUI::UDim(0.05, 0)));
    hard->setPosition(CEGUI::UVector2(CEGUI::UDim(0.4f, 0), CEGUI::UDim(0.5f, 0)));
    sheet->addChildWindow(hard);
    hard->hide();

    CEGUI::Window *level = wmgr.createWindow("TaharezLook/StaticText", "CEGUIDemo/LevelText");
    level->setText("           Level 1");
    level->setSize(CEGUI::UVector2(CEGUI::UDim(0.2, 0), CEGUI::UDim(0.05, 0)));
    level->setPosition(CEGUI::UVector2(CEGUI::UDim(0.4f, 0), CEGUI::UDim(0, 0)));
    sheet->addChildWindow(level);
    level->hide();
    
    CEGUI::System::getSingleton().setGUISheet(sheet);
    quit->subscribeEvent(CEGUI::PushButton::EventClicked, CEGUI::Event::Subscriber(&GameAPP::quit, this));
    single->subscribeEvent(CEGUI::PushButton::EventClicked, CEGUI::Event::Subscriber(&GameAPP::single, this));
    multi->subscribeEvent(CEGUI::PushButton::EventClicked, CEGUI::Event::Subscriber(&GameAPP::multi, this));
    cancel->subscribeEvent(CEGUI::PushButton::EventClicked, CEGUI::Event::Subscriber(&GameAPP::cancel, this));
    server->subscribeEvent(CEGUI::PushButton::EventClicked, CEGUI::Event::Subscriber(&GameAPP::server, this));
    client->subscribeEvent(CEGUI::PushButton::EventClicked, CEGUI::Event::Subscriber(&GameAPP::client, this));
    client_ok->subscribeEvent(CEGUI::PushButton::EventClicked, CEGUI::Event::Subscriber(&GameAPP::client_ok, this));
    ai->subscribeEvent(CEGUI::PushButton::EventClicked, CEGUI::Event::Subscriber(&GameAPP::ai, this));
    retry->subscribeEvent(CEGUI::PushButton::EventClicked, CEGUI::Event::Subscriber(&GameAPP::single, this));
    airetry->subscribeEvent(CEGUI::PushButton::EventClicked, CEGUI::Event::Subscriber(&GameAPP::airetry, this));
    mainmenu->subscribeEvent(CEGUI::PushButton::EventClicked, CEGUI::Event::Subscriber(&GameAPP::cancel, this));
    easy->subscribeEvent(CEGUI::PushButton::EventClicked, CEGUI::Event::Subscriber(&GameAPP::easy, this));
    medium->subscribeEvent(CEGUI::PushButton::EventClicked, CEGUI::Event::Subscriber(&GameAPP::medium, this));
    hard->subscribeEvent(CEGUI::PushButton::EventClicked, CEGUI::Event::Subscriber(&GameAPP::hard, this));
    

}
bool GameAPP::quit(const CEGUI::EventArgs &e)
{
    mShutDown = true;
    return true;
}
bool GameAPP::ai(const CEGUI::EventArgs &e)
{
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/SingleButton")->hide();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/AIButton")->hide();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/MultiButton")->hide();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/QuitButton")->hide();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/EasyButton")->show();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/MediumButton")->show();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/HardButton")->show();
    return true;
}
bool GameAPP::easy(const CEGUI::EventArgs &e)
{
    CEGUI::MouseCursor::getSingleton().hide( );
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/EasyButton")->hide();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/MediumButton")->hide();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/HardButton")->hide();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/LivesText")->show();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/ScoreText")->show();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/LivesText2")->show();
    clearScene();
    mDifficulty = 1;
	mState = STATE_AILEVEL1;
    createScene();
    return true;
}
bool GameAPP::medium(const CEGUI::EventArgs &e)
{
    CEGUI::MouseCursor::getSingleton().hide( );
   CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/EasyButton")->hide();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/MediumButton")->hide();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/HardButton")->hide();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/LivesText")->show();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/ScoreText")->show();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/LivesText2")->show();
    clearScene();
    mDifficulty = 2;
	mState = STATE_AILEVEL1;
    createScene();
    return true;
}
bool GameAPP::hard(const CEGUI::EventArgs &e)
{
    CEGUI::MouseCursor::getSingleton().hide( );
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/EasyButton")->hide();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/MediumButton")->hide();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/HardButton")->hide();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/LivesText")->show();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/ScoreText")->show();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/LivesText2")->show();
    clearScene();
    mDifficulty = 3;
	mState = STATE_AILEVEL1;
    createScene();
    return true;
}
bool GameAPP::single(const CEGUI::EventArgs &e)
{
    CEGUI::MouseCursor::getSingleton().hide( );
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/SingleButton")->hide();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/AIButton")->hide();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/MultiButton")->hide();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/QuitButton")->hide();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/RetryButton")->hide();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/WinLoss")->hide();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/MainMenuButton")->hide();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/LivesText")->show();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/ScoreText")->show();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/ScoreText")->show();
    mGameWon = false;
    mGameLoss = false;
    clearScene();
	mState = STATE_LEVEL1;
    createScene();
    return true;
}
bool GameAPP::airetry(const CEGUI::EventArgs &e)
{
    CEGUI::MouseCursor::getSingleton().hide( );
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/QuitButton")->hide();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/AIRetryButton")->hide();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/WinLoss")->hide();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/MainMenuButton")->hide();
    mGameWon = false;
    mGameLoss = false;
    clearScene();
	mState = STATE_AILEVEL1;
    createScene();
    return true;
}
bool GameAPP::multi(const CEGUI::EventArgs &e)
{
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/SingleButton")->hide();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/AIButton")->hide();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/MultiButton")->hide();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/QuitButton")->hide();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/ServerButton")->show();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/ClientButton")->show();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/CancelButton")->show();
    return true;
}
bool GameAPP::server(const CEGUI::EventArgs &e)
{
    CEGUI::MouseCursor::getSingleton().hide( );
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/ServerButton")->hide();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/ClientButton")->hide();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/CancelButton")->hide();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/LivesText")->show();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/ScoreText")->show();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/LivesText2")->show();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/ScoreText2")->show();
    clearScene();
	mMPState = MPSTATE_MPSERVER;
	mState = STATE_MPLEVEL1;

	init_server();
	find_connection();
	server_receive(&mpThreadData);

	createScene();
    return true;
}
bool GameAPP::client(const CEGUI::EventArgs &e)
{
    //sendfile << "clicked client button" << endl;
    //CEGUI::MouseCursor::getSingleton().hide( );
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/SingleButton")->hide();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/AIButton")->hide();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/MultiButton")->hide();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/QuitButton")->hide();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/ServerButton")->hide();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/ClientButton")->hide();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/Edit")->show();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/ClientStart")->show();

    //std::string valueEditbox = CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/Edit")->getText().c_str(); // Retrieve the text
    //sendfile << "test???: "<< valueEditbox << endl;
    return true;
}
bool GameAPP::client_ok(const CEGUI::EventArgs &e)
{
    std::string valueEditbox = CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/Edit")->getText().c_str(); // Retrieve the text
    //sendfile << "server name: " << valueEditbox << endl;
    CEGUI::MouseCursor::getSingleton().hide( );
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/Edit")->hide();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/ClientStart")->hide();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/CancelButton")->hide();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/LivesText")->show();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/ScoreText")->show();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/LivesText2")->show();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/ScoreText2")->show();
    clearScene();
	mMPState = MPSTATE_MPCLIENT;
	mState = STATE_MPLEVEL1;
        
   	init_client(valueEditbox);
	client_receive(&mpThreadData);

   	createScene();
    return true;
}
bool GameAPP::cancel(const CEGUI::EventArgs &e)
{
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/SingleButton")->show();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/AIButton")->show();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/MultiButton")->show();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/QuitButton")->show();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/ServerButton")->hide();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/ClientButton")->hide();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/CancelButton")->hide();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/Edit")->hide();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/ClientStart")->hide();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/RetryButton")->hide();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/WinLoss")->hide();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/MainMenuButton")->hide();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/LivesText")->hide();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/ScoreText")->hide();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/LivesText2")->hide();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/ScoreText2")->hide();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/AIRetryButton")->hide();
    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/LevelText")->hide();
    mGameWon = false;
    mGameLoss = false;
    /*if(mMPState == MPSTATE_MPSERVER)
        quit_server();
    else if (mMPState == MPSTATE_MPCLIENT)
        quit_client();*/
    clearScene();
	mState = STATE_MAINMENU;
    createScene();
    return true;
}

void
GameAPP::playSound(SoundType soundType)
{ 
	char* file1 = "blip.wav";
	char* file2 = "boing2.wav";
	char* file3 = "boing_x.wav";
	char *file4 = "bloop.wav";
	char *file5 = "phaser.wav";
	char *file6 = "ball_miss.wav";
    char *file7 = "firehit.wav";
    char *file8 = "firelaunch.wav";
    switch(soundType)
    {
    case SOUND_WALLCOLLISION:
        play_Sound(file1);
        break;
    case SOUND_PADDLECOLLISION:
        play_Sound(file3);
        break;
    case SOUND_PADDLESERVE:
      play_Sound(file5);
        break;
    case SOUND_BALLMISS:
		play_Sound(file6);
        break;
    case SOUND_FIREHIT:
		play_Sound(file7);
        break;
    case SOUND_FIRELAUNCH:
		play_Sound(file8);
        break;
    }
}

void
GameAPP::launchBall()
{
	if(mBallObject == NULL)
	  return;

    if(mMPState == MPSTATE_MPCLIENT || mState == STATE_PAUSE)
      return;

    if(!mIsPlayerOneServing && !mIsPlayerTwoServing)
      return;

    if(mGameWon || mGameLoss)
      return;

    // alert client of paddle serve
    if(mMPState == MPSTATE_MPSERVER)
    {
        std::string message = "Msg_PdlSer";
        server_send(message);
    }

	btRigidBody* body = mBallObject->getBody();

    // TODO: add case in for client too! also allow for client to launch the ball
	//btScalar xComponent = btScalar(rand () % 300) * 0.1f - btScalar(rand () % 300) * 0.1f;
	//btScalar yComponent = btScalar(rand () % 300) * 0.1f - btScalar(rand () % 300) * 0.1f;

    if(mIsPlayerOneServing)
    {
        btScalar xComponent = btScalar(mPaddleVelocityDirectionOne.x);
        btScalar yComponent = btScalar(mPaddleVelocityDirectionOne.y);
	    btScalar zComponent = -minForwardSpeedForLevel;
	    body->setLinearVelocity(btVector3(xComponent, yComponent, zComponent));
        //if(mMPState == MPSTATE_SPAI)
            //launchFireball(false); // DEBUG:
    }
    else if(mIsPlayerTwoServing)
    {
        btScalar xComponent = btScalar(-mPaddleVelocityDirectionTwo.x);
        btScalar yComponent = btScalar(-mPaddleVelocityDirectionTwo.y);
	    btScalar zComponent = minForwardSpeedForLevel;
	    body->setLinearVelocity(btVector3(xComponent, yComponent, zComponent));
    }
	
    playSound(SOUND_PADDLESERVE);

	mIsPlayerOneServing = false;
    mIsPlayerTwoServing = false;

    CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/LevelText")->hide();
}

void
GameAPP::launchFireball(bool isPlayerOneLaunching)
{
    static char fireballIdentity = 0;
    ++fireballIdentity;
    
    Ogre::Vector3 paddlePosition;
    
    if(isPlayerOneLaunching)
        paddlePosition = mPaddleObjectOne->getNode()->getPosition();
    else
        paddlePosition = mPaddleObjectTwo->getNode()->getPosition();
    
    Ogre::Entity* entity;
    Ogre::SceneNode* node;
    
    string entityName = "FireballEntity";
    string nodeName = "FireballNode";
    string objectName = "Fireball";
    string lightName = "FireballLight";
    string smokeName = "FireballSmoke";
    entityName.push_back(fireballIdentity);
    nodeName.push_back(fireballIdentity);
    objectName.push_back(fireballIdentity);
    lightName.push_back(fireballIdentity);
    smokeName.push_back(fireballIdentity);
    
    entity = mSceneMgr->createEntity(entityName, "fireball.mesh");
    node = mSceneMgr->getRootSceneNode()->createChildSceneNode(nodeName, paddlePosition);
    node->attachObject(entity);
    node->attachObject(mSceneMgr->createParticleSystem(smokeName, "Examples/Smoke"));

    GameObject* fireballObject = new GameObject(objectName, mSceneMgr, mSimulator, node, entity, entity, 2.0f);
    fireballObject->addToSimulator();
    
    Ogre::Light* l1 = mSceneMgr->createLight(lightName);
    l1->setType(Ogre::Light::LT_POINT);
    node->attachObject(l1);
    
    mFireballObjects.push_back(fireballObject);
    
    btRigidBody* body = fireballObject->getBody();
    
    if(isPlayerOneLaunching)
    {
        btScalar xComponent = btScalar(mPaddleVelocityDirectionOne.x);
        btScalar yComponent = btScalar(mPaddleVelocityDirectionOne.y);
        btScalar zComponent = -minForwardSpeedForLevel * 1.1f;
        body->setLinearVelocity(btVector3(xComponent, yComponent, zComponent));
    }
    else
    {
        btScalar xComponent = btScalar(-mPaddleVelocityDirectionTwo.x);
        btScalar yComponent = btScalar(-mPaddleVelocityDirectionTwo.y);
        btScalar zComponent = minForwardSpeedForLevel * 1.1f;
        body->setLinearVelocity(btVector3(xComponent, yComponent, zComponent));
    }
    
    playSound(SOUND_FIRELAUNCH);
}

void GameAPP::createScene()
{
    mTimePassedOnLevel = 0.0;
    mTimePassedSinceLastNetUpdate = 0.0;
	numHitsWithPaddle = 0;
    mPreviousBallVelocity = btVector3(0.0,0.0,0.0);
    mFinalBossEyeNode = NULL;
    
    switch(mState)
    {
    case STATE_MAINMENU:
        createMainMenuScene();
		play_Music(SONG_MENU);
        break;
    case STATE_LEVEL1:
        createSPLevelScene(1);
        CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/LevelText")->setText("           Level 1");
        CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/LevelText")->show();
	    play_Music(SONG_LEVEL1);
        break;
    case STATE_LEVEL2:
        createSPLevelScene(2);
        CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/LevelText")->setText("           Level 2");
        CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/LevelText")->show();
		play_Music(SONG_LEVEL2);
        break;
    case STATE_LEVEL3:
        createSPLevelScene(3);
        CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/LevelText")->setText("           Level 3");
        CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/LevelText")->show();
        if(!mGameWon && !mGameLoss)
		    play_Music(SONG_LEVEL3);
        else if (mGameWon)
            play_Music(SONG_WIN);
        break;
	case STATE_MPLEVEL1:
		createMPLevelScene();
		play_Music(SONG_MULTI);
		break;
	case STATE_AILEVEL1:
	    createAILevelScene(1);
        CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/LevelText")->setText("           Stage 1");
        CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/LevelText")->show();
	    play_Music(SONG_AI2);
        break;
    case STATE_AILEVEL2:
        createAILevelScene(2);
        CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/LevelText")->setText("           Stage 2");
        CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/LevelText")->show();
        play_Music(SONG_AI1);
        break;
    case STATE_AILEVEL3:
        createAILevelScene(3);
        CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/LevelText")->setText("           Stage 3");
        CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/LevelText")->show();
        play_Music(SONG_AI3);
        break;
    case STATE_AIBOSS1:
        createAILevelScene(4);
        CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/LevelText")->setText("       Final Stage");
        CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/LevelText")->show();
        play_Music(SONG_BOSS);
        break;
    case STATE_AIBOSS2:
        createAILevelScene(5);
        play_Music(SONG_BOSS);
        break;
    case STATE_AIBOSS3:
        createAILevelScene(6);
        if(!mGameWon && !mGameLoss)
            play_Music(SONG_BOSS);
        else if (mGameWon)
            play_Music(SONG_WIN);
        break;
    }
    
    minForwardSpeedForLevel = baseMinForwardSpeedForLevel;
}

bool GameAPP::createMainMenuScene()
{   
	mMPState = MPSTATE_SP;
    baseMinForwardSpeedForLevel = 70.0f;
	minForwardSpeedIncreasePerHit = 13.0f;
    mScoreMult = 1;
    playerOneNumLives = 0;
    playerTwoNumLives = 0;
    levelNum = 0;

	// Set up Physics System
    mSimulator = new PhysicsSystem(0.0);

	Ogre::Entity* entity;
	Ogre::SceneNode* node;

	entity = mSceneMgr->createEntity("Menu", "menu.mesh");

    node = mSceneMgr->getRootSceneNode()->createChildSceneNode("MenuNode", Ogre::Vector3(0.0f, 0.0f, 0.0f));
    node->attachObject(entity);
	mLevelObject = new GameObject("Menu", mSceneMgr, mSimulator, node, entity, entity, 0.0f); 
    mLevelObject->addToSimulator();

    mPaddleObjectOne = NULL;
	mPaddleObjectTwo = NULL;
    mBallObject = NULL;

    // add balls to bounce around
    GameObject* tmpGameObj;
    entity = mSceneMgr->createEntity("Ball1", "ball.mesh");
    node = mSceneMgr->getRootSceneNode()->createChildSceneNode("BallNode1", Ogre::Vector3(-10.0f, 70.0f, 20.0f));
    node->attachObject(entity);
    tmpGameObj = new GameObject("Ball1", mSceneMgr, mSimulator, node, entity, entity, 1.0f); 
    tmpGameObj->addToSimulator();
    tmpGameObj->getBody()->setLinearVelocity(btVector3(float(rand() % 100 - 50), float(rand() % 100 - 50), float(rand() % 100 - 50)));

    entity = mSceneMgr->createEntity("Ball2", "ball.mesh");
    node = mSceneMgr->getRootSceneNode()->createChildSceneNode("BallNode2", Ogre::Vector3(60.0f, 10.0f, 10.0f));
    node->attachObject(entity);
    tmpGameObj = new GameObject("Ball2", mSceneMgr, mSimulator, node, entity, entity, 1.0f); 
    tmpGameObj->addToSimulator();
    tmpGameObj->getBody()->setLinearVelocity(btVector3(float(rand() % 100 - 50), float(rand() % 100 - 50), float(rand() % 100 - 50)));

    entity = mSceneMgr->createEntity("Ball3", "ball.mesh");
    node = mSceneMgr->getRootSceneNode()->createChildSceneNode("BallNode3", Ogre::Vector3(-20.0f, 50.0f, 50.0f));
    node->attachObject(entity);
    tmpGameObj = new GameObject("Ball3", mSceneMgr, mSimulator, node, entity, entity, 1.0f); 
    tmpGameObj->addToSimulator();
    tmpGameObj->getBody()->setLinearVelocity(btVector3(float(rand() % 100 - 50), float(rand() % 100 - 50), float(rand() % 100 - 50)));

    entity = mSceneMgr->createEntity("Ball4", "ball.mesh");
    node = mSceneMgr->getRootSceneNode()->createChildSceneNode("BallNode4", Ogre::Vector3(30.0f, -50.0f, 30.0f));
    node->attachObject(entity);
    tmpGameObj = new GameObject("Ball4", mSceneMgr, mSimulator, node, entity, entity, 1.0f); 
    tmpGameObj->addToSimulator();
    tmpGameObj->getBody()->setLinearVelocity(btVector3(float(rand() % 100 - 50), float(rand() % 100 - 50), float(rand() % 100 - 50)));

    // Set ambient light
    mSceneMgr->setAmbientLight(Ogre::ColourValue(0.5, 0.5, 0.5));
 
    // Create a light
    Ogre::Light* l = mSceneMgr->createLight("MainLight");
    l->setPosition(45.0f,45.0f,45.0f);

    // Set Camera
    mCamera->setPosition(0.0f, 37.5f, 200.0f);
    mCamera->pitch(Ogre::Degree(0.0f));
    mCamera->yaw(Ogre::Degree(0.0f));

	return true;
}

bool GameAPP::createSPLevelScene(int level)
{
	mMPState = MPSTATE_SP;
    
    string meshName;
    
    switch(level)
    {
      case 1:
        meshName = "level1.mesh";
        playerOneNumLives = 3;
        baseMinForwardSpeedForLevel = 70.0f;
        minForwardSpeedIncreasePerHit = 13.0f;
        break;
      case 2:
        meshName = "level2.mesh";
        ++playerOneNumLives;
        baseMinForwardSpeedForLevel = 80.0f;
        minForwardSpeedIncreasePerHit = 16.0f;
        break;
      case 3:
        meshName = "level3.mesh";
        ++playerOneNumLives;
        baseMinForwardSpeedForLevel = 90.0f;
        minForwardSpeedIncreasePerHit = 10.0f;
        break;
    }
    
    mScoreMult = level;
    playerTwoNumLives = 0;
    levelNum = level;
    mIsPlayerOneServing = true;

	// Set up Physics System
    mSimulator = new PhysicsSystem(-9.81);

    // Set ambient light
    mSceneMgr->setAmbientLight(Ogre::ColourValue(0.5, 0.5, 0.5));

    mSceneMgr->setShadowTechnique(Ogre::SHADOWTYPE_STENCIL_ADDITIVE);
    Ogre::MeshManager::getSingleton().setPrepareAllMeshesForShadowVolumes(true);
    
	Ogre::Entity* entity;
    Ogre::Entity* physicalEntity;
	Ogre::SceneNode* node;

    entity = mSceneMgr->createEntity("Level", meshName);
    entity->getMesh()->buildEdgeList();
    entity->setCastShadows(false);
    node = mSceneMgr->getRootSceneNode()->createChildSceneNode("LevelNode", Ogre::Vector3(0.0f, 0.0f, 0.0f));
    node->attachObject(entity);
	mLevelObject = new GameObject("Level", mSceneMgr, mSimulator, node, entity, entity, 0.0f); 
    mLevelObject->addToSimulator();
    
    entity = mSceneMgr->createEntity("Paddle", "paddle.mesh");
    entity->getMesh()->buildEdgeList();
    entity->setCastShadows(false);
    node = mSceneMgr->getRootSceneNode()->createChildSceneNode("PaddleNode", Ogre::Vector3(0.0f, 37.5f, 125.0f));
    node->attachObject(entity);
    physicalEntity = mSceneMgr->createEntity("PhysicalPaddle", "paddlephysics.mesh");
    mPaddleObjectOne = new GameObject("Paddle", mSceneMgr, mSimulator, node, entity, physicalEntity, 0.0f); 
    mPaddleObjectOne->addToSimulator();

	mPaddleObjectTwo = NULL;

    entity = mSceneMgr->createEntity("Ball", "ball.mesh");
    entity->getMesh()->buildEdgeList();
    entity->setCastShadows(true);
    node = mSceneMgr->getRootSceneNode()->createChildSceneNode("BallNode", Ogre::Vector3(0.0f, 37.5f, 118.0f));
    node->attachObject(entity);
    mBallObject = new GameObject("Ball", mSceneMgr, mSimulator, node, entity, entity, 1.0f); 
    mBallObject->addToSimulator();
 
    // Create a light
    Ogre::Light* l = mSceneMgr->createLight("MainLight");
    l->setType(Ogre::Light::LT_POINT);
    l->setPosition(0.0f, 50.0f, 100.0f);

    // Set Camera
    mCamera->setPosition(0.0f, 37.5f, 215.0f);
    mCamera->pitch(Ogre::Degree(0.0f));
    mCamera->yaw(Ogre::Degree(0.0f));

	return true;
}

bool GameAPP::createAILevelScene(int level, bool isFireballReset)
{ 
	assert(level > 0);
	assert(level <= 6);
	
	//This is set for other methods in a different place.
	mMPState = MPSTATE_SPAI;
	
	string meshName;
    string aiPaddleName;
    Ogre::Real minAISpeed;
    Ogre::Real maxAISpeed;
    
    // TODO: add a max speed limit of ~320
	switch(level)
	{
    case 1:
		meshName = "mplevel1.mesh";
        aiPaddleName = "paddleblue.mesh";
		playerOneNumLives = 5;
		playerTwoNumLives = 3;
		baseMinForwardSpeedForLevel = 100.0f;
		minForwardSpeedIncreasePerHit = 8.0f;
        minAISpeed = 35.0f;
        maxAISpeed = 60.0f;
		break;
    case 2:
		meshName = "mplevel1.mesh";
        aiPaddleName = "paddleyellow.mesh";
		++playerOneNumLives;
        playerTwoNumLives = 3;
		baseMinForwardSpeedForLevel = 110.0f;
		minForwardSpeedIncreasePerHit = 9.0f;
		minAISpeed = 40.0f;
        maxAISpeed = 75.0f;
		break;
    case 3:
		meshName = "mplevel1.mesh";
        aiPaddleName = "paddlered.mesh";
		++playerOneNumLives;
        playerTwoNumLives = 3;
		baseMinForwardSpeedForLevel = 120.0f;
		minForwardSpeedIncreasePerHit = 10.0f;
        minAISpeed = 55.0f;
        maxAISpeed = 90.0f;
		break;
    case 4:
        meshName = "mplevel2.mesh";
        aiPaddleName = "paddle.mesh";
        ++playerOneNumLives;
        playerTwoNumLives = 3;
        baseMinForwardSpeedForLevel = 130.0f;
        minForwardSpeedIncreasePerHit = 11.0f;
        minAISpeed = 65.0f;
        maxAISpeed = 110.0f;
        break;
    case 5:
        meshName = "mplevel2.mesh";
        aiPaddleName = "bosspaddle.mesh";
        playerTwoNumLives = 2;
        baseMinForwardSpeedForLevel = 140.0f;
        minForwardSpeedIncreasePerHit = 13.0f;
        minAISpeed = 75.0f;
        maxAISpeed = 130.0f;
        break;
    case 6:
        meshName = "mplevel2.mesh";
        aiPaddleName = "finalbosspaddle.mesh";
        if(!isFireballReset)
        {
            ++playerOneNumLives;
            playerTwoNumLives = 3;
        }
        baseMinForwardSpeedForLevel = 150.0f;
        minForwardSpeedIncreasePerHit = 15.0f;
        minAISpeed = 90.0f;
        maxAISpeed = 135.0f;
        break;
	}

    //set difficulty
    Difficulty diff;
    switch(mDifficulty)
    {
    case 1:
        diff = DIFF_EASY;
        break;
    case 2:
        diff = DIFF_MEDIUM;
        break;
    case 3:
        diff = DIFF_HARD;
        break;
    }
    
    mScoreMult = level;
    levelNum = level;
    
	mIsPlayerOneServing = true;
    mIsPlayerTwoServing = false;
    
    mSimulator = new PhysicsSystem(-9.81);
    mSceneMgr->setAmbientLight(Ogre::ColourValue(0.6, 0.6, 0.6));

    mSceneMgr->setShadowTechnique(Ogre::SHADOWTYPE_STENCIL_ADDITIVE);
    Ogre::MeshManager::getSingleton().setPrepareAllMeshesForShadowVolumes(true);

	Ogre::Entity* entity;
    Ogre::Entity* physicalEntity;
	Ogre::SceneNode* node;

    entity = mSceneMgr->createEntity("AILevel", meshName);
    entity->getMesh()->buildEdgeList();
    entity->setCastShadows(false);
    node = mSceneMgr->getRootSceneNode()->createChildSceneNode("LevelNode", Ogre::Vector3(0.0f, 0.0f, 0.0f));
    node->attachObject(entity);
	mLevelObject = new GameObject("AILevel", mSceneMgr, mSimulator, node, entity, entity, 0.0f); 
    
    entity = mSceneMgr->createEntity("PaddleOne", "paddle.mesh");
    entity->getMesh()->buildEdgeList();
    entity->setCastShadows(false);
    if(level <= 4 || isFireballReset)
        node = mSceneMgr->getRootSceneNode()->createChildSceneNode("PaddleNodeOne", Ogre::Vector3(0.0f, 37.5f, 155.0f));
    else
        node = mSceneMgr->getRootSceneNode()->createChildSceneNode("PaddleNodeOne", mStoredPaddlePositionOne);
    node->attachObject(entity);
    physicalEntity = mSceneMgr->createEntity("PhysicalPaddle", "paddlephysics.mesh");
    mPaddleObjectOne = new GameObject("PaddleOne", mSceneMgr, mSimulator, node, entity, physicalEntity, 0.0f); 
    mPaddleObjectOne->addToSimulator();

	entity = mSceneMgr->createEntity("PaddleTwo", aiPaddleName);
    entity->getMesh()->buildEdgeList();
    entity->setCastShadows(false);
    if(level <= 4 || isFireballReset)
        node = mSceneMgr->getRootSceneNode()->createChildSceneNode("PaddleNodeTwo", Ogre::Vector3(0.0f, 37.5f, -155.0f));
    else
        node = mSceneMgr->getRootSceneNode()->createChildSceneNode("PaddleNodeTwo", mStoredPaddlePositionTwo);
	node->rotate(Ogre::Vector3(0.0f, 1.0f, 0.0f), Ogre::Radian(3.14159265358f));
    node->attachObject(entity);
    physicalEntity = mSceneMgr->createEntity("PhysicalPaddle2", "paddlephysics.mesh");
    mPaddleObjectTwo = new GameObject("PaddleTwo", mSceneMgr, mSimulator, node, entity, physicalEntity, 0.0f);
    mPaddleObjectTwo->addToSimulator();

    entity = mSceneMgr->createEntity("Ball", "ball.mesh");
    entity->getMesh()->buildEdgeList();
    entity->setCastShadows(true);
    if(level <= 4 || isFireballReset)
        node = mSceneMgr->getRootSceneNode()->createChildSceneNode("BallNode", Ogre::Vector3(0.0f, 37.5f, 148.0f));
    else
        node = mSceneMgr->getRootSceneNode()->createChildSceneNode("BallNode", mStoredPaddlePositionOne);
    node->attachObject(entity);
    mBallObject = new GameObject("Ball", mSceneMgr, mSimulator, node, entity, entity, 1.0f);

    mLevelObject->addToSimulator();
    
    mBallObject->addToSimulator();
    
    if(level == 6)
    {
        entity = mSceneMgr->createEntity("Eye", "finalbosseye.mesh");
        mFinalBossEyeNode = mSceneMgr->getRootSceneNode()->createChildSceneNode("EyeNode", mStoredPaddlePositionTwo);
        mFinalBossEyeNode->attachObject(entity);
    }

    // Create a light
    Ogre::Light* l1 = mSceneMgr->createLight("MainLight1");
    l1->setPosition(0.0f, 50.0f, 0.0f);
    l1 = mSceneMgr->createLight("MainLight2");
    l1->setPosition(75.0f, 50.0f, 1500.0f);
    l1 = mSceneMgr->createLight("MainLight3");
    l1->setPosition(-75.0f, 50.0f, -1500.0f);  
    
    mCamera->setPosition(0.0f, 37.5f, 245.0f);
    mCamera->pitch(Ogre::Degree(0.0f));
    mCamera->yaw(Ogre::Degree(0.0f));
	
	//true = random paddle movement, false = paddle follows player
	bool AIMode = true;
	if(level == 4 || level == 5)
	  AIMode = false;
	//Last level, AI launches fireballs
	bool fireballs = false;
	if (level == 6)
	  fireballs = true;
	mDeepBlue = new AI(*mPaddleObjectTwo, *mBallObject, &mPaddleVelocityDirectionTwo, &mPaddleDirectionTwo, minAISpeed, maxAISpeed, *this, AIMode, diff, fireballs);
    mParticleSystem = NULL;
    
    return true;
}

bool GameAPP::createMPLevelScene()
{
	assert(mMPState == MPSTATE_MPSERVER || mMPState == MPSTATE_MPCLIENT);
	baseMinForwardSpeedForLevel = 90.0f;
	minForwardSpeedIncreasePerHit = 10.0f;
    mScoreMult = 1;
    playerOneNumLives = 5;
    playerTwoNumLives = 5;
    levelNum = 1;
    
    // TODO: make this random with message passing
    mIsPlayerOneServing = true;
    mIsPlayerTwoServing = false;

	// Set up Physics System
    if(mMPState == MPSTATE_MPSERVER)
        mSimulator = new PhysicsSystem(-9.81);
    else
        mSimulator = NULL;

     // Set ambient light
    mSceneMgr->setAmbientLight(Ogre::ColourValue(0.6, 0.6, 0.6));

    mSceneMgr->setShadowTechnique(Ogre::SHADOWTYPE_STENCIL_ADDITIVE);
    Ogre::MeshManager::getSingleton().setPrepareAllMeshesForShadowVolumes(true);

	Ogre::Entity* entity;
	Ogre::SceneNode* node;

    entity = mSceneMgr->createEntity("MPLevel1", "mplevel1.mesh");
    entity->getMesh()->buildEdgeList();
    entity->setCastShadows(false);
    node = mSceneMgr->getRootSceneNode()->createChildSceneNode("Level1Node", Ogre::Vector3(0.0f, 0.0f, 0.0f));
    node->attachObject(entity);
	mLevelObject = new GameObject("MPLevel1", mSceneMgr, mSimulator, node, entity, entity, 0.0f); 
    
    entity = mSceneMgr->createEntity("PaddleOne", "paddle.mesh");
    entity->getMesh()->buildEdgeList();
    entity->setCastShadows(false);
    node = mSceneMgr->getRootSceneNode()->createChildSceneNode("PaddleNodeOne", Ogre::Vector3(0.0f, 37.5f, 155.0f));
    node->attachObject(entity);
    mPaddleObjectOne = new GameObject("PaddleOne", mSceneMgr, mSimulator, node, entity, entity, 0.0f); 

	entity = mSceneMgr->createEntity("PaddleTwo", "paddleblue.mesh");
    entity->getMesh()->buildEdgeList();
    entity->setCastShadows(false);
    node = mSceneMgr->getRootSceneNode()->createChildSceneNode("PaddleNodeTwo", Ogre::Vector3(0.0f, 37.5f, -155.0f));
	node->rotate(Ogre::Vector3(0.0f, 1.0f, 0.0f), Ogre::Radian(3.14159265358f));
    node->attachObject(entity);
    mPaddleObjectTwo = new GameObject("PaddleTwo", mSceneMgr, mSimulator, node, entity, entity, 0.0f); 

    entity = mSceneMgr->createEntity("Ball", "ball.mesh");
    entity->getMesh()->buildEdgeList();
    entity->setCastShadows(true);
    node = mSceneMgr->getRootSceneNode()->createChildSceneNode("BallNode", Ogre::Vector3(0.0f, 37.5f, 148.0f));
    node->attachObject(entity);
    mBallObject = new GameObject("Ball", mSceneMgr, mSimulator, node, entity, entity, 1.0f); 

    if(mSimulator != NULL)
    {
        mLevelObject->addToSimulator();
        mPaddleObjectOne->addToSimulator();
        mPaddleObjectTwo->addToSimulator();
        mBallObject->addToSimulator();
    }

    // Create a light
    Ogre::Light* l1 = mSceneMgr->createLight("MainLight1");
    l1->setPosition(0.0f, 50.0f, 0.0f);
    l1 = mSceneMgr->createLight("MainLight2");
    l1->setPosition(75.0f, 50.0f, 1500.0f);
    l1 = mSceneMgr->createLight("MainLight3");
    l1->setPosition(-75.0f, 50.0f, -1500.0f);                         
                       

    // Set Camera
	if(mMPState == MPSTATE_MPSERVER)
	{
		mCamera->setPosition(0.0f, 37.5f, 245.0f);
		mCamera->pitch(Ogre::Degree(0.0f));
		mCamera->yaw(Ogre::Degree(0.0f));
	}
	else
	{
		mCamera->setPosition(0.0f, 37.5f, -245.0f);
		mCamera->pitch(Ogre::Degree(0.0f));
		mCamera->yaw(Ogre::Degree(180.0f));
	}
    //PROBLEM MAY BE HERE
    //mCamera->pitch(Ogre::Degree(0.0f));
    //mCamera->yaw(Ogre::Degree(0.0f));

	return true;
}

void GameAPP::clearScene()
{
    if(mMPState == MPSTATE_SPAI)
    {
        mStoredPaddlePositionOne = mPaddleObjectOne->getNode()->getPosition();
        mStoredPaddlePositionTwo = mPaddleObjectTwo->getNode()->getPosition();
    }
    
    mFireballObjects.clear();

    mSceneMgr->clearScene(); // NOTE: does not destroy camera
    
	mCamera->setPosition(Ogre::Vector3(0,0,80));
	mCamera->lookAt(Ogre::Vector3(0,0,-1000)); // look back along -Z to reset looking direction
    
    if(mSimulator != NULL)
        delete mSimulator;
}

void GameAPP::createFrameListener(void)
{
	BaseApplication::createFrameListener();
    // Set Default Values
    mPaddleDirectionOne = Ogre::Vector3::ZERO;
    mPaddleDirectionTwo = Ogre::Vector3::ZERO;

    createGUI();

    srand(time(NULL));
}
 
bool GameAPP::frameRenderingQueued(const Ogre::FrameEvent &evt)
{
    if(mState == STATE_PAUSE)
        goto END;
    // update level time
    mTimePassedOnLevel += evt.timeSinceLastFrame;
    mTimePassedSinceLastNetUpdate += evt.timeSinceLastFrame;

    updatePhysics(evt);
	
	if(mMPState == MPSTATE_SPAI && mBallObject != NULL)
        mDeepBlue->update(evt, mIsPlayerTwoServing, mIsPlayerOneServing, mTimePassedOnLevel, mPaddleObjectOne->getNode()->getPosition());
	
	if(mMPState == MPSTATE_MPCLIENT)
		updateUserInput(evt, mPaddleObjectTwo, mPaddleDirectionTwo);
	else
		updateUserInput(evt, mPaddleObjectOne, mPaddleDirectionOne);

    // currently sends one message every 1/100th of a second
    if(mTimePassedSinceLastNetUpdate > 0.010f)
    {
        mTimePassedSinceLastNetUpdate = 0.0f;
        updateClientServerSynchronization(true);
    }
    else
    {
        updateClientServerSynchronization(false);
    }

    updateScore();
    
     // single player level transition conditions
    if((mState == STATE_LEVEL1 || mState == STATE_LEVEL2 || mState == STATE_LEVEL3) && playerOneNumLives > 0 )
    {
        if(numHitsWithPaddle >= 10)
        {
            clearScene();
            if(mState == STATE_LEVEL1)
            {
                mState = STATE_LEVEL2;
            }
            else if(mState == STATE_LEVEL2)
            {
                mState = STATE_LEVEL3;
            }
            else
            {
                /*mState = STATE_MAINMENU; // TODO: make this the win screen
                CEGUI::MouseCursor::getSingleton().show( );
                CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/SingleButton")->show();
                CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/AIButton")->show();
                CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/MultiButton")->show();
                CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/QuitButton")->show();
                CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/LivesText")->hide();
                CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/ScoreText")->hide();
                */
                mGameWon = true;
                CEGUI::MouseCursor::getSingleton().show( );
                CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/WinLoss")->setText("           You Win!!!");
                CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/WinLoss")->show();
                CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/RetryButton")->show();
                CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/MainMenuButton")->show();
                CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/QuitButton")->show();
                resetScore();
            }
            createScene();
        }
    }
    
    // single player ai win conditions
    else if(mMPState == MPSTATE_SPAI && (playerTwoNumLives <= 0 || (mState == STATE_AIBOSS1 && playerTwoNumLives <= 2)))
    {
        clearScene();
        if(mState == STATE_AILEVEL1)
        {
            mState = STATE_AILEVEL2;
        }
        else if(mState == STATE_AILEVEL2)
        {
            mState = STATE_AILEVEL3;
        }
        else if(mState == STATE_AILEVEL3)
        {
            mState = STATE_AIBOSS1;
        }
        else if(mState == STATE_AIBOSS1)
        {
            mState = STATE_AIBOSS2;
        }
        else if(mState == STATE_AIBOSS2)
        {
            mState = STATE_AIBOSS3;
        }
        else if(mState == STATE_AIBOSS3)
        {
            mGameWon = true;
            CEGUI::MouseCursor::getSingleton().show( );
            CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/WinLoss")->setText("           You Win!!!");
            CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/WinLoss")->show();
            CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/AIRetryButton")->show();
            CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/MainMenuButton")->show();
            CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/QuitButton")->show();
            resetScore();
        }
        createScene();
    }
    // game ending conditions
    else if( (playerOneNumLives <= 0 || (mMPState != MPSTATE_SP && playerTwoNumLives <= 0) ) && !mGameWon) 
	{
		if(mState != STATE_MAINMENU)
		{
            clearScene();
            CEGUI::MouseCursor::getSingleton().show( );
            if(mMPState == MPSTATE_SP ){
                CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/RetryButton")->show();
            }
            else if(mMPState == MPSTATE_SPAI){
                CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/AIRetryButton")->show();
            }
            CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/MainMenuButton")->show();
            CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/QuitButton")->show();
            if((mMPState == MPSTATE_MPCLIENT && playerTwoNumLives != 0) || (mMPState == MPSTATE_MPSERVER && playerOneNumLives != 0)){
                CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/WinLoss")->setText("          You Win!!!");
                CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/WinLoss")->show();
                mGameWon = true;
            }
            else{
                CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/WinLoss")->setText("          You Lose!!!");
                CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/WinLoss")->show();
                mGameLoss = true;
            }
            playerOneNumLives = 0;
            playerTwoNumLives = 0;
			resetScore();
            createScene();
            if(mGameWon){
                play_Music(SONG_WIN);
            }
            else{
                play_Music(SONG_LOSE);
            }
		}
    }
    END:
    //Need to inject timestamps to CEGUI System.
    CEGUI::System::getSingleton().injectTimePulse(evt.timeSinceLastFrame);
	return BaseApplication::frameRenderingQueued(evt);
}

bool GameAPP::keyPressed( const OIS::KeyEvent &arg )
{
    CEGUI::System &sys = CEGUI::System::getSingleton();
    sys.injectKeyDown(arg.key);
    sys.injectChar(arg.text);
    if (mTrayMgr->isDialogVisible()) return true;   // don't process any more keys if dialog is up

	/*if(mState == STATE_MAINMENU && arg.key == OIS::KC_S)
	{
		clearScene();
		mState = STATE_LEVEL1;
        createScene();
	}
	else if(mState == STATE_MAINMENU && arg.key == OIS::KC_M)
	{
		clearScene();
		mState = STATE_MPMENU;
        createScene();
	}
	else if(mState == STATE_MAINMENU && arg.key == OIS::KC_ESCAPE)
	{
		mShutDown = true;
	}
	else if(mState == STATE_MPMENU && arg.key == OIS::KC_S)
	{
		clearScene();
		mMPState = MPSTATE_MPSERVER;
		mState = STATE_MPLEVEL1;

		init_server();
		find_connection();
		server_receive(&mpThreadData);

		createScene();
	}
	else if(mState == STATE_MPMENU && arg.key == OIS::KC_C)
	{
		clearScene();
		mMPState = MPSTATE_MPCLIENT;
		mState = STATE_MPLEVEL1;
        
   		init_client();
	    client_receive(&mpThreadData);

   		createScene();
    }
    else if (arg.key == OIS::KC_F)   // toggle visibility of advanced frame stats
    {
        mTrayMgr->toggleAdvancedFrameStats();
    }
    else if(arg.key == OIS::KC_F5)   // refresh all textures
    {
        Ogre::TextureManager::getSingleton().reloadAll();
    }
    else if (arg.key == OIS::KC_SYSRQ)   // take a screenshot
    {
        mWindow->writeContentsToTimestampedFile("screenshot", ".jpg");
    }*/
    if (arg.key == OIS::KC_ESCAPE)
    {
		// TODO: this code is repeated in touchCancelled
	    if(mMPState == MPSTATE_MPCLIENT){
		    quit_client();
        }
	    else if(mMPState == MPSTATE_MPSERVER){
		    quit_server();
        }
        resetScore();
        clearScene();
		mState = STATE_MAINMENU;
        CEGUI::MouseCursor::getSingleton().show( );
        CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/SingleButton")->show();
        CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/AIButton")->show();
        CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/MultiButton")->show();
        CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/QuitButton")->show();
        CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/LivesText")->hide();
        CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/ScoreText")->hide();
        CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/LivesText2")->hide();
        CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/ScoreText2")->hide();
        CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/ServerButton")->hide();
        CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/ClientButton")->hide();
        CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/CancelButton")->hide();
        CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/Edit")->hide();
        CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/ClientStart")->hide();
        CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/WinLoss")->hide();
        CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/RetryButton")->hide();
        CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/AIRetryButton")->hide();
        CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/MainMenuButton")->hide();
        CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/EasyButton")->hide();
        CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/MediumButton")->hide();
        CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/HardButton")->hide();
        CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/LevelText")->hide();
        createScene();
    }
    else if (arg.key == OIS::KC_SPACE){   // toggle pause
       if(mState != STATE_PAUSE && mState != STATE_MAINMENU && mMPState != MPSTATE_MPSERVER && mMPState != MPSTATE_MPCLIENT && !mGameLoss && !mGameWon ){
        CEGUI::MouseCursor::getSingleton().show( );
        CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/MainMenuButton")->show();
        CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/QuitButton")->show();
        mLastState = mState;
        mState = STATE_PAUSE;
       }
       else if(mState == STATE_PAUSE){
        mState = mLastState;
        CEGUI::MouseCursor::getSingleton().hide( );
        CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/MainMenuButton")->hide();
        CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/QuitButton")->hide();
       }
       
    }
    else if (arg.key == OIS::KC_K)   // kill enemy lives to advance level for demo
    {
         playerTwoNumLives--;
         numHitsWithPaddle++;
    }

    mCameraMan->injectKeyDown(arg);
    return true;
}
bool GameAPP::keyReleased( const OIS::KeyEvent &arg )
{
    CEGUI::System::getSingleton().injectKeyUp(arg.key);
    return true;
}
CEGUI::MouseButton convertButton(OIS::MouseButtonID buttonID)
{
    switch (buttonID)
    {
    case OIS::MB_Left:
        return CEGUI::LeftButton;
 
    case OIS::MB_Right:
        return CEGUI::RightButton;
 
    case OIS::MB_Middle:
        return CEGUI::MiddleButton;
 
    default:
        return CEGUI::LeftButton;
    }
}

#ifdef OGRE_IS_IOS
bool touchMoved(const OIS::MultiTouchEvent &evt)
{
	CEGUI::System &sys = CEGUI::System::getSingleton();
    sys.injectMouseMove(evt.state.X.rel, evt.state.Y.rel);
    // Scroll wheel.
    if (evt.state.Z.rel)
        sys.injectMouseWheelChange(evt.state.Z.rel / 120.0f);

    if(mMPState == MPSTATE_SP || mMPState == MPSTATE_SPAI || mMPState == MPSTATE_MPSERVER)
    {
        mPaddleDirectionOne.x = evt.state.X.rel * 25.0f;
        mPaddleDirectionOne.y = -evt.state.Y.rel * 25.0f;

        if(mPaddleDirectionOne.x != 0.0f)
            mPaddleVelocityDirectionOne.x = mPaddleDirectionOne.x * 0.03f;
        if(mPaddleDirectionOne.x != 0.0f)
            mPaddleVelocityDirectionOne.y = mPaddleDirectionOne.y * 0.03f;
        mPaddleVelocityDirectionOne.z = 0.0f;
    }
    else
    {
        mPaddleDirectionTwo.x = evt.state.X.rel * 25.0f;
        mPaddleDirectionTwo.y = -evt.state.Y.rel * 25.0f;

        if(mPaddleDirectionTwo.x != 0.0f)
            mPaddleVelocityDirectionTwo.x = mPaddleDirectionTwo.x * 0.03f;
        if(mPaddleDirectionTwo.x != 0.0f)
            mPaddleVelocityDirectionTwo.y = mPaddleDirectionTwo.y * 0.03f;
        mPaddleDirectionTwo.z = 0.0f;
    }
	return true;
}

bool touchPressed(const OIS::MultiTouchEvent &evt)
{
	CEGUI::System::getSingleton().injectMouseButtonDown(convertButton(id));
}

bool touchReleased(const OIS::MultiTouchEvent &evt)
{
	// NOTE: for the iOS touch released shares the same functionality as both mouse pressed and mouse released
	CEGUI::System::getSingleton().injectMouseButtonUp(convertButton(id));;
    switch (id)
	{
	case OIS::MB_Left:
		if((mMPState == MPSTATE_SP || mMPState == MPSTATE_SPAI || mMPState == MPSTATE_MPSERVER) && mIsPlayerOneServing)
		{
			launchBall();
		}
        else if(mMPState == MPSTATE_MPCLIENT)
        {
            std::string message = "Msg_P2Clck";
            client_send(message);
        }  
	    break;
	default:
	    break;
	}
	
    if(mMPState == MPSTATE_SP || mMPState == MPSTATE_SPAI || mMPState == MPSTATE_MPSERVER)
    {
        mPaddleDirectionOne.x = 0;
	    mPaddleDirectionOne.y = 0;
    }
    else
    {
        mPaddleDirectionTwo.x = 0;
	    mPaddleDirectionTwo.y = 0;
    }
}

bool touchCancelled(const OIS::MultiTouchEvent &evt)
{
	// TODO: this code is repeated from the escape key code
	if(mMPState == MPSTATE_MPCLIENT){
		quit_client();
	}
	else if(mMPState == MPSTATE_MPSERVER){
		quit_server();
	}
	//resetScore();
	CEGUI::MouseCursor::getSingleton().show( );
	CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/Sheet")->show();
	clearScene();
	mState = STATE_MAINMENU;
	createScene();
}
#else
bool GameAPP::mouseMoved( const OIS::MouseEvent& evt )
{
    CEGUI::System &sys = CEGUI::System::getSingleton();
    sys.injectMouseMove(evt.state.X.rel, evt.state.Y.rel);
    // Scroll wheel.
    if (evt.state.Z.rel)
        sys.injectMouseWheelChange(evt.state.Z.rel / 120.0f);

    if(mMPState == MPSTATE_SP || mMPState == MPSTATE_SPAI || mMPState == MPSTATE_MPSERVER)
    {
        mPaddleDirectionOne.x = evt.state.X.rel * 25.0f;
        mPaddleDirectionOne.y = -evt.state.Y.rel * 25.0f;

        if(mPaddleDirectionOne.x != 0.0f)
            mPaddleVelocityDirectionOne.x = mPaddleDirectionOne.x * 0.05f;
        if(mPaddleDirectionOne.x != 0.0f)
            mPaddleVelocityDirectionOne.y = mPaddleDirectionOne.y * 0.05f;
        mPaddleVelocityDirectionOne.z = 0.0f;
    }
    else
    {
        mPaddleDirectionTwo.x = evt.state.X.rel * 25.0f;
        mPaddleDirectionTwo.y = -evt.state.Y.rel * 25.0f;

        if(mPaddleDirectionTwo.x != 0.0f)
            mPaddleVelocityDirectionTwo.x = mPaddleDirectionTwo.x * 0.05f;
        if(mPaddleDirectionTwo.x != 0.0f)
            mPaddleVelocityDirectionTwo.y = mPaddleDirectionTwo.y * 0.05f;
        mPaddleDirectionTwo.z = 0.0f;
    }
	return true;
}

bool GameAPP::mousePressed( const OIS::MouseEvent& evt, OIS::MouseButtonID id )
{
    CEGUI::System::getSingleton().injectMouseButtonDown(convertButton(id));
    switch (id)
	{
	case OIS::MB_Left:
		if((mMPState == MPSTATE_SP || mMPState == MPSTATE_SPAI || mMPState == MPSTATE_MPSERVER) && mIsPlayerOneServing)
		{
			launchBall();
		}
        else if(mMPState == MPSTATE_MPCLIENT)
        {
            std::string message = "Msg_P2Clck";
            client_send(message);
        }  
	    break;
	default:
	    break;
	}

	return true;
}

bool GameAPP::mouseReleased( const OIS::MouseEvent& evt, OIS::MouseButtonID id )
{
    CEGUI::System::getSingleton().injectMouseButtonUp(convertButton(id));
    if(mMPState == MPSTATE_SP || mMPState == MPSTATE_SPAI || mMPState == MPSTATE_MPSERVER)
    {
        mPaddleDirectionOne.x = 0;
	    mPaddleDirectionOne.y = 0;
    }
    else
    {
        mPaddleDirectionTwo.x = 0;
	    mPaddleDirectionTwo.y = 0;
    }
	
	return true;
}
#endif

void GameAPP::updateScore() {
     if( playerOneScore > playerOneHighScore) {
        playerOneHighScore = playerOneScore;
    }
    if( playerTwoScore > playerTwoHighScore) {
        playerTwoHighScore = playerTwoScore;
    }
    if(mMPState == MPSTATE_MPCLIENT){
        CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/LivesText")->setText("Lives:        " + Ogre::StringConverter::toString(playerTwoNumLives));
        CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/ScoreText")->setText("Score:        " + Ogre::StringConverter::toString(playerTwoScore));
        CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/LivesText2")->setText("Enemy Lives: " + Ogre::StringConverter::toString(playerOneNumLives));
        CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/ScoreText2")->setText("Enemy Score: " + Ogre::StringConverter::toString(playerOneScore));
    }
    else{
        CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/LivesText")->setText("Lives:        " + Ogre::StringConverter::toString(playerOneNumLives));
        CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/ScoreText")->setText("Score:        " + Ogre::StringConverter::toString(playerOneScore));
        CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/LivesText2")->setText("Enemy Lives: " + Ogre::StringConverter::toString(playerTwoNumLives));
        CEGUI::WindowManager::getSingleton().getWindow("CEGUIDemo/ScoreText2")->setText("Enemy Score: " + Ogre::StringConverter::toString(playerTwoScore));
    }
    
}

void GameAPP::resetScore() {
    if( playerOneScore > playerOneHighScore) {
        playerOneHighScore = playerOneScore;
    }
    if( playerTwoScore > playerTwoHighScore) {
        playerTwoHighScore = playerTwoScore;
    }
    playerOneNumLives = 0;
    playerOneScore = 0;
    playerTwoNumLives = 0;
    playerTwoScore = 0;
    levelNum = 0;
}

#if !defined(OGRE_IS_IOS)
#	if OGRE_PLATFORM == OGRE_PLATFORM_WIN32
#	define WIN32_LEAN_AND_MEAN
#	include "windows.h"
#	endif
 
#	ifdef __cplusplus
extern "C" {
#	endif
 
#	if OGRE_PLATFORM == OGRE_PLATFORM_WIN32
    INT WINAPI WinMain( HINSTANCE hInst, HINSTANCE, LPSTR strCmdLine, INT )
#	else
    int main(int argc, char *argv[])
#	endif
    {
        // Create application object
        GameAPP app;
 
        try {
            app.go();
        } catch( Ogre::Exception& e ) {
#	if OGRE_PLATFORM == OGRE_PLATFORM_WIN32
            MessageBox( NULL, e.getFullDescription().c_str(), "An exception has occured!", MB_OK | MB_ICONERROR | MB_TASKMODAL);
#	else
            std::cerr << "An exception has occured: " <<
                e.getFullDescription().c_str() << std::endl;
#	endif
        }
 
        return 0;
    }
 
#	ifdef __cplusplus
}
#	endif
#endif
