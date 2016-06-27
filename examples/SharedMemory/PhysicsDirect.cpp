#include "PhysicsDirect.h"

#include "PhysicsClientSharedMemory.h"
#include "../CommonInterfaces/CommonGUIHelperInterface.h"
#include "SharedMemoryCommands.h"
#include "PhysicsServerCommandProcessor.h"
#include "LinearMath/btHashMap.h"
#include "LinearMath/btAlignedObjectArray.h"
#include "../../Extras/Serialize/BulletFileLoader/btBulletFile.h"
#include "../../Extras/Serialize/BulletFileLoader/autogenerated/bullet.h"
#include "BodyJointInfoUtility.h"


struct BodyJointInfoCache2
{
	btAlignedObjectArray<b3JointInfo> m_jointInfo;
};

struct PhysicsDirectInternalData
{
	DummyGUIHelper m_noGfx;

	SharedMemoryCommand m_command;
	SharedMemoryStatus m_serverStatus;
	bool m_hasStatus;
	bool m_verboseOutput;
	
	btAlignedObjectArray<TmpFloat3> m_debugLinesFrom;
    btAlignedObjectArray<TmpFloat3> m_debugLinesTo;
    btAlignedObjectArray<TmpFloat3> m_debugLinesColor;

	btHashMap<btHashInt,BodyJointInfoCache2*> m_bodyJointMap;
	
	char    m_bulletStreamDataServerToClient[SHARED_MEMORY_MAX_STREAM_CHUNK_SIZE];

	int m_cachedCameraPixelsWidth;
	int m_cachedCameraPixelsHeight;
	btAlignedObjectArray<unsigned char> m_cachedCameraPixelsRGBA;
	btAlignedObjectArray<float> m_cachedCameraDepthBuffer;


	PhysicsServerCommandProcessor* m_commandProcessor;

	PhysicsDirectInternalData()
		:m_hasStatus(false),
		m_verboseOutput(false)
	{
	}
};

PhysicsDirect::PhysicsDirect()
{
	m_data = new PhysicsDirectInternalData;
	m_data->m_commandProcessor = new PhysicsServerCommandProcessor;
	
	
}

PhysicsDirect::~PhysicsDirect()
{
	delete m_data->m_commandProcessor;
	delete m_data;
}


// return true if connection succesfull, can also check 'isConnected'
bool PhysicsDirect::connect()
{
	m_data->m_commandProcessor->setGuiHelper(&m_data->m_noGfx);
	return true;
}

////todo: rename to 'disconnect'
void PhysicsDirect::disconnectSharedMemory()
{
	m_data->m_commandProcessor->setGuiHelper(0);
}

bool PhysicsDirect::isConnected() const
{
	return true;
}

// return non-null if there is a status, nullptr otherwise
const  SharedMemoryStatus* PhysicsDirect::processServerStatus()
{
	SharedMemoryStatus* stat = 0;
	if (m_data->m_hasStatus)
	{
		stat = &m_data->m_serverStatus;
		m_data->m_hasStatus = false;
	}
	return stat;
}

SharedMemoryCommand* PhysicsDirect::getAvailableSharedMemoryCommand()
{
	return &m_data->m_command;
}

bool PhysicsDirect::canSubmitCommand() const
{
	return true;
}

bool PhysicsDirect::processDebugLines(const struct SharedMemoryCommand& orgCommand)
{
	SharedMemoryCommand command = orgCommand;

	const SharedMemoryStatus& serverCmd = m_data->m_serverStatus;

	do
	{

		bool hasStatus = m_data->m_commandProcessor->processCommand(command,m_data->m_serverStatus,&m_data->m_bulletStreamDataServerToClient[0],SHARED_MEMORY_MAX_STREAM_CHUNK_SIZE);
		m_data->m_hasStatus = hasStatus;
		if (hasStatus)
		{
			btAssert(m_data->m_serverStatus.m_type == CMD_DEBUG_LINES_COMPLETED);

			if (m_data->m_verboseOutput) 
			{
				b3Printf("Success receiving %d debug lines",
							serverCmd.m_sendDebugLinesArgs.m_numDebugLines);
			}

			int numLines = serverCmd.m_sendDebugLinesArgs.m_numDebugLines;
			float* linesFrom =
				(float*)&m_data->m_bulletStreamDataServerToClient[0];
			float* linesTo =
				(float*)(&m_data->m_bulletStreamDataServerToClient[0] + 
						numLines * 3 * sizeof(float));
			float* linesColor =
				(float*)(&m_data->m_bulletStreamDataServerToClient[0] +
							2 * numLines * 3 * sizeof(float));

			m_data->m_debugLinesFrom.resize(serverCmd.m_sendDebugLinesArgs.m_startingLineIndex +
											numLines);
			m_data->m_debugLinesTo.resize(serverCmd.m_sendDebugLinesArgs.m_startingLineIndex +
											numLines);
			m_data->m_debugLinesColor.resize(
				serverCmd.m_sendDebugLinesArgs.m_startingLineIndex + numLines);

			for (int i = 0; i < numLines; i++) 
			{
				TmpFloat3 from = CreateTmpFloat3(linesFrom[i * 3], linesFrom[i * 3 + 1],
													linesFrom[i * 3 + 2]);
				TmpFloat3 to =
					CreateTmpFloat3(linesTo[i * 3], linesTo[i * 3 + 1], linesTo[i * 3 + 2]);
				TmpFloat3 color = CreateTmpFloat3(linesColor[i * 3], linesColor[i * 3 + 1],
													linesColor[i * 3 + 2]);

				m_data
					->m_debugLinesFrom[serverCmd.m_sendDebugLinesArgs.m_startingLineIndex + i] =
					from;
				m_data->m_debugLinesTo[serverCmd.m_sendDebugLinesArgs.m_startingLineIndex + i] =
					to;
				m_data->m_debugLinesColor[serverCmd.m_sendDebugLinesArgs.m_startingLineIndex +
											i] = color;
			}

			if (serverCmd.m_sendDebugLinesArgs.m_numRemainingDebugLines > 0)
			{
				command.m_type = CMD_REQUEST_DEBUG_LINES;
				command.m_requestDebugLinesArguments.m_startingLineIndex =
					serverCmd.m_sendDebugLinesArgs.m_numDebugLines +
					serverCmd.m_sendDebugLinesArgs.m_startingLineIndex;
			}
		}

	} while (serverCmd.m_sendDebugLinesArgs.m_numRemainingDebugLines > 0);
	
	return m_data->m_hasStatus;
}

bool PhysicsDirect::processCamera(const struct SharedMemoryCommand& orgCommand)
{
	SharedMemoryCommand command = orgCommand;

	const SharedMemoryStatus& serverCmd = m_data->m_serverStatus;

	do
	{

		bool hasStatus = m_data->m_commandProcessor->processCommand(command,m_data->m_serverStatus,&m_data->m_bulletStreamDataServerToClient[0],SHARED_MEMORY_MAX_STREAM_CHUNK_SIZE);
		m_data->m_hasStatus = hasStatus;
		if (hasStatus)
		{
			btAssert(m_data->m_serverStatus.m_type == CMD_CAMERA_IMAGE_COMPLETED);

			if (m_data->m_verboseOutput) 
			{
				b3Printf("Camera image OK\n");
			}

			int numBytesPerPixel = 4;//RGBA
			int numTotalPixels = serverCmd.m_sendPixelDataArguments.m_startingPixelIndex+
				serverCmd.m_sendPixelDataArguments.m_numPixelsCopied+
				serverCmd.m_sendPixelDataArguments.m_numRemainingPixels;

			m_data->m_cachedCameraPixelsWidth = 0;
			m_data->m_cachedCameraPixelsHeight = 0;

            int numPixels = serverCmd.m_sendPixelDataArguments.m_imageWidth*serverCmd.m_sendPixelDataArguments.m_imageHeight;

            m_data->m_cachedCameraPixelsRGBA.reserve(numPixels*numBytesPerPixel);
			m_data->m_cachedCameraDepthBuffer.resize(numTotalPixels);
			m_data->m_cachedCameraPixelsRGBA.resize(numTotalPixels*numBytesPerPixel);
                
                
			unsigned char* rgbaPixelsReceived =
                (unsigned char*)&m_data->m_bulletStreamDataServerToClient[0];
			
			float* depthBuffer = (float*)&(m_data->m_bulletStreamDataServerToClient[serverCmd.m_sendPixelDataArguments.m_numPixelsCopied*4]);
			
          //  printf("pixel = %d\n", rgbaPixelsReceived[0]);
                
			for (int i=0;i<serverCmd.m_sendPixelDataArguments.m_numPixelsCopied;i++)
			{
				m_data->m_cachedCameraDepthBuffer[i + serverCmd.m_sendPixelDataArguments.m_startingPixelIndex] = depthBuffer[i];
			}
			for (int i=0;i<serverCmd.m_sendPixelDataArguments.m_numPixelsCopied*numBytesPerPixel;i++)
			{
				m_data->m_cachedCameraPixelsRGBA[i + serverCmd.m_sendPixelDataArguments.m_startingPixelIndex*numBytesPerPixel] 
					= rgbaPixelsReceived[i];
			}

			if (serverCmd.m_sendPixelDataArguments.m_numRemainingPixels > 0)
			{
				

				// continue requesting remaining pixels
				command.m_type = CMD_REQUEST_CAMERA_IMAGE_DATA;
				command.m_requestPixelDataArguments.m_startPixelIndex = 
					serverCmd.m_sendPixelDataArguments.m_startingPixelIndex + 
					serverCmd.m_sendPixelDataArguments.m_numPixelsCopied;
				
			} else
			{
				m_data->m_cachedCameraPixelsWidth = serverCmd.m_sendPixelDataArguments.m_imageWidth;
				m_data->m_cachedCameraPixelsHeight = serverCmd.m_sendPixelDataArguments.m_imageHeight;
			}	
		}
	}  while (serverCmd.m_sendPixelDataArguments.m_numRemainingPixels > 0);
	
	return m_data->m_hasStatus;


}


void PhysicsDirect::processBodyJointInfo(int bodyUniqueId, const SharedMemoryStatus& serverCmd)
{
    bParse::btBulletFile bf(
        &m_data->m_bulletStreamDataServerToClient[0],
        serverCmd.m_dataStreamArguments.m_streamChunkLength);
    bf.setFileDNAisMemoryDNA();
    bf.parse(false);


    BodyJointInfoCache2* bodyJoints = new BodyJointInfoCache2;
    m_data->m_bodyJointMap.insert(bodyUniqueId,bodyJoints);

    for (int i = 0; i < bf.m_multiBodies.size(); i++) 
    {
        int flag = bf.getFlags();
        if ((flag & bParse::FD_DOUBLE_PRECISION) != 0) 
        {
            Bullet::btMultiBodyDoubleData* mb =
                (Bullet::btMultiBodyDoubleData*)bf.m_multiBodies[i];

            addJointInfoFromMultiBodyData(mb,bodyJoints, m_data->m_verboseOutput);
        } else 
        {
            Bullet::btMultiBodyFloatData* mb =
                (Bullet::btMultiBodyFloatData*)bf.m_multiBodies[i];

            addJointInfoFromMultiBodyData(mb,bodyJoints, m_data->m_verboseOutput);
        }
    }
    if (bf.ok()) {
        if (m_data->m_verboseOutput) 
        {
            b3Printf("Received robot description ok!\n");
        }
    } else 
    {
        b3Warning("Robot description not received");
    }
}

bool PhysicsDirect::submitClientCommand(const struct SharedMemoryCommand& command)
{
	if (command.m_type==CMD_REQUEST_DEBUG_LINES)
	{			
		return processDebugLines(command);
	}

	if (command.m_type==CMD_REQUEST_CAMERA_IMAGE_DATA)
	{
		return processCamera(command);
	}

	bool hasStatus = m_data->m_commandProcessor->processCommand(command,m_data->m_serverStatus,&m_data->m_bulletStreamDataServerToClient[0],SHARED_MEMORY_MAX_STREAM_CHUNK_SIZE);
	m_data->m_hasStatus = hasStatus;
	if (hasStatus)
	{
		const SharedMemoryStatus& serverCmd = m_data->m_serverStatus;

		switch (m_data->m_serverStatus.m_type)
		{
			case CMD_RESET_SIMULATION_COMPLETED:
			{
				m_data->m_debugLinesFrom.clear();
				m_data->m_debugLinesTo.clear();
				m_data->m_debugLinesColor.clear();
				for (int i=0;i<m_data->m_bodyJointMap.size();i++)
				{
					BodyJointInfoCache2** bodyJointsPtr = m_data->m_bodyJointMap.getAtIndex(i);
					if (bodyJointsPtr && *bodyJointsPtr)
					{
						BodyJointInfoCache2* bodyJoints = *bodyJointsPtr;
						for (int j=0;j<bodyJoints->m_jointInfo.size();j++) {
							if (bodyJoints->m_jointInfo[j].m_jointName)
							{
								free(bodyJoints->m_jointInfo[j].m_jointName);
							}
							if (bodyJoints->m_jointInfo[j].m_linkName)
							{
								free(bodyJoints->m_jointInfo[j].m_linkName);
							}
						}
						delete (*bodyJointsPtr);
					}
				}
				m_data->m_bodyJointMap.clear();
                
				break;
			}
			case CMD_SDF_LOADING_COMPLETED:
            {
                //we'll stream further info from the physics server
                //so serverCmd will be invalid, make a copy
                
                
                int numBodies = serverCmd.m_sdfLoadedArgs.m_numBodies;
                for (int i=0;i<numBodies;i++)
                {
                    int bodyUniqueId = serverCmd.m_sdfLoadedArgs.m_bodyUniqueIds[i];
                    SharedMemoryCommand infoRequestCommand;
                    infoRequestCommand.m_type = CMD_REQUEST_BODY_INFO;
                    infoRequestCommand.m_sdfRequestInfoArgs.m_bodyUniqueId = bodyUniqueId;
                    SharedMemoryStatus infoStatus;
                    bool hasStatus = m_data->m_commandProcessor->processCommand(infoRequestCommand,infoStatus,&m_data->m_bulletStreamDataServerToClient[0],SHARED_MEMORY_MAX_STREAM_CHUNK_SIZE);
                    if (hasStatus)
                    {
                        processBodyJointInfo(bodyUniqueId, infoStatus);
                    }
                }
                break;
            }
			case CMD_URDF_LOADING_COMPLETED:
			{
				
				if (serverCmd.m_dataStreamArguments.m_streamChunkLength > 0) 
				{
				    int bodyIndex = serverCmd.m_dataStreamArguments.m_bodyUniqueId;
                    processBodyJointInfo(bodyIndex,serverCmd);
				}
                break;
            }
			
		 default:
			 {
				// b3Error("Unknown server status type");
			 }
		};
										  
		
	}
	return hasStatus;
}

int PhysicsDirect::getNumJoints(int bodyIndex) const
{
	BodyJointInfoCache2** bodyJointsPtr = m_data->m_bodyJointMap[bodyIndex];
	if (bodyJointsPtr && *bodyJointsPtr)
	{
		BodyJointInfoCache2* bodyJoints = *bodyJointsPtr;
		return bodyJoints->m_jointInfo.size(); 
	}
	btAssert(0);
	return 0;
}

bool PhysicsDirect::getJointInfo(int bodyIndex, int jointIndex, struct b3JointInfo& info) const
{
	BodyJointInfoCache2** bodyJointsPtr = m_data->m_bodyJointMap[bodyIndex];
	if (bodyJointsPtr && *bodyJointsPtr)
	{
		BodyJointInfoCache2* bodyJoints = *bodyJointsPtr;
        if (jointIndex < bodyJoints->m_jointInfo.size())
        {
            info = bodyJoints->m_jointInfo[jointIndex];
            return true;
        }
	}
    return false;
}

///todo: move this out of the
void PhysicsDirect::setSharedMemoryKey(int key)
{
	//m_data->m_physicsServer->setSharedMemoryKey(key);
	//m_data->m_physicsClient->setSharedMemoryKey(key);
}

void PhysicsDirect::uploadBulletFileToSharedMemory(const char* data, int len)
{
	//m_data->m_physicsClient->uploadBulletFileToSharedMemory(data,len);
}

int PhysicsDirect::getNumDebugLines() const
{
	return m_data->m_debugLinesFrom.size();
}

const float* PhysicsDirect::getDebugLinesFrom() const
{
	if (getNumDebugLines())
	{
		return &m_data->m_debugLinesFrom[0].m_x;
	}
	return 0;
}
const float* PhysicsDirect::getDebugLinesTo() const
{
	if (getNumDebugLines())
	{
		return &m_data->m_debugLinesTo[0].m_x;
	}
	return 0;
}
const float* PhysicsDirect::getDebugLinesColor() const
{
	if (getNumDebugLines())
	{
		return &m_data->m_debugLinesColor[0].m_x;
	}
	return 0;
}

void PhysicsDirect::getCachedCameraImage(b3CameraImageData* cameraData)
{
	if (cameraData)
	{
		cameraData->m_pixelWidth = m_data->m_cachedCameraPixelsWidth;
		cameraData->m_pixelHeight = m_data->m_cachedCameraPixelsHeight;
		cameraData->m_depthValues = m_data->m_cachedCameraDepthBuffer.size() ? &m_data->m_cachedCameraDepthBuffer[0] : 0;
		cameraData->m_rgbColorData = m_data->m_cachedCameraPixelsRGBA.size() ? &m_data->m_cachedCameraPixelsRGBA[0] : 0;
	}
}
