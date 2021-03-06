#include "PPSession.h"
#include "PPGraphics.h"
#include "PPSessionManager.h"

PPSession::~PPSession()
{
	if (g_network != nullptr) delete g_network;
}


void PPSession::initSession()
{
	if (g_network != nullptr) return;
	g_network = new PPNetwork();
	g_network->g_session = this;
	//------------------------------------------------------
	// callback when request data is received
	g_network->SetOnReceivedRequest([=](PPNetwork *self, u8* buffer, u32 size, u32 tag) {
		//------------------------------------------------------
		// verify authentication
		//------------------------------------------------------
		if (!g_authenticated)
		{
			if (tag == PPREQUEST_AUTHEN)
			{
				// check for authentication
				PPMessage *authenMsg = new PPMessage();
				if (authenMsg->ParseHeader(buffer))
				{
					if (authenMsg->GetMessageCode() == MSG_CODE_RESULT_AUTHENTICATION_SUCCESS)
					{
						printf("#%d : Authentication successted.\n", sessionID);
						g_authenticated = true;
						if (g_onAuthenSuccessed != nullptr) 
							g_onAuthenSuccessed(self, nullptr, 0);
					}
					else
					{
						printf("#%d : Authenticaiton failed.\n", sessionID);
						return;
					}
				}
				else
				{
					printf("#%d : Authenticaiton failed.\n", sessionID);
					return;
				}
				delete authenMsg;
			}
			else
			{
				printf("Client was not authentication.\n");
				return;
			}
		}
		//------------------------------------------------------
		// process data by tag
		switch (tag)
		{
		case PPREQUEST_HEADER:
		{
			if (!g_tmpMessage) g_tmpMessage = new PPMessage();
			if (g_tmpMessage->ParseHeader(buffer))
			{
				//----------------------------------------------------
				// request body part of this message
				self->SetRequestData(g_tmpMessage->GetContentSize(), PPREQUEST_BODY);
			}
			else
			{
				delete g_tmpMessage;
				g_tmpMessage = nullptr;
			}
			break;
		}
		case PPREQUEST_BODY:
		{
			//------------------------------------------------------
			// if tmp message is null that mean this is useless data then we avoid it
			if (!g_tmpMessage) return;
			// verify buffer size with message estimate size
			if (size == g_tmpMessage->GetContentSize())
			{
				// process message
				switch (g_sessionType)
				{
				case PPSESSION_MOVIE:
				{
					processMovieSession(buffer, size);
					break;
				}
				case PPSESSION_SCREEN_CAPTURE:
				{
					processScreenCaptureSession(buffer, size);
					break;
				}
				case PPSESSION_INPUT_CAPTURE:
				{
					processInputSession(buffer, size);
					break;
				}
				}

				//----------------------------------------------------------
				// Request for next message
				self->SetRequestData(MSG_COMMAND_SIZE, PPREQUEST_HEADER);
			}
			//------------------------------------------------------
			// remove message after use
			delete g_tmpMessage;
			g_tmpMessage = nullptr;
			break;
		}
		default: break;
		}
	});
}


void PPSession::InitMovieSession()
{
	initSession();
	//--------------------------------------
	// init specific for movie session
	g_sessionType = PPSESSION_MOVIE;

	printf("#%d : Init new MOVIE session.\n", sessionID);
	gfxFlushBuffers();
}

void PPSession::InitScreenCaptureSession(PPSessionManager* manager)
{
	g_manager = manager;
	initSession();
	//--------------------------------------
	// init specific for movie session
	g_sessionType = PPSESSION_SCREEN_CAPTURE;
	//--------------------------------------
	printf("#%d : Init new SCREEN CAPTURE session.\n", sessionID);
	gfxFlushBuffers();
}

void PPSession::InitInputCaptureSession(PPSessionManager* manager)
{
	g_manager = manager;
	initSession();
	//--------------------------------------
	// init specific for movie session
	g_sessionType = PPSESSION_INPUT_CAPTURE;

	printf("#%d : Init new INPUT session.\n", sessionID);
	gfxFlushBuffers();
}


void PPSession::StartSession(const char* ip, const char* port, s32 prio, PPNetworkCallback authenSuccessed)
{
	if (g_network == nullptr) return;
	g_onAuthenSuccessed = authenSuccessed;
	g_network->SetOnConnectionSuccessed([=](PPNetwork *self, u8* data, u32 size)
	{
		//NOTE: this not called on main thread !
		//--------------------------------------------------
		int8_t code = 0;
		if (g_sessionType == PPSESSION_MOVIE) code = MSG_CODE_REQUEST_AUTHENTICATION_MOVIE;
		else if (g_sessionType == PPSESSION_SCREEN_CAPTURE) code = MSG_CODE_REQUEST_AUTHENTICATION_SCREEN_CAPTURE;
		else if (g_sessionType == PPSESSION_INPUT_CAPTURE) code = MSG_CODE_REQUEST_AUTHENTICATION_INPUT;
		if (code == 0)
		{
			printf("#%d : Invalid session type\n", sessionID);
			gfxFlushBuffers();
			return;
		}
		//--------------------------------------------------
		// screen capture session authen
		PPMessage *authenMsg = new PPMessage();
		authenMsg->BuildMessageHeader(code);
		u8* msgBuffer = authenMsg->BuildMessageEmpty();
		//--------------------------------------------------
		// send authentication message
		self->SendMessageData(msgBuffer, authenMsg->GetMessageSize());
		//--------------------------------------------------
		// set request to get result message
		self->SetRequestData(MSG_COMMAND_SIZE, PPREQUEST_AUTHEN);
		delete authenMsg;
	});
	g_network->SetOnConnectionClosed([=](PPNetwork *self, u8* data, u32 size)
	{
		//NOTE: this not called on main thread !
		printf("#%d : Connection interupted.\n", sessionID);
		gfxFlushBuffers();
	});
	g_network->Start(ip, port, prio);
}

void PPSession::CloseSession()
{
	if (g_network == nullptr) return;
	g_network->Stop();
	g_authenticated = false;
	SS_Reset();
}


void PPSession::processMovieSession(u8* buffer, size_t size)
{
	//NOTE: this not called on main thread !
	//------------------------------------------------------
	// process message data	by message type
	//------------------------------------------------------
	switch (g_tmpMessage->GetMessageCode())
	{
	default: break;
	}
}

void PPSession::processScreenCaptureSession(u8* buffer, size_t size)
{
	//NOTE: this not called on main thread !
	//------------------------------------------------------
	// process message data	by message type
	//------------------------------------------------------
	switch (g_tmpMessage->GetMessageCode())
	{
	case MSG_CODE_REQUEST_NEW_SCREEN_FRAME:
	{
		try {
			// If using waiting for new frame then we need:
			//--------------------------------------------------
			// send request that client received frame
			{
				PPMessage *msgObj = new PPMessage();
				msgObj->BuildMessageHeader(MSG_CODE_REQUEST_SCREEN_RECEIVED_FRAME);
				u8* msgBuffer = msgObj->BuildMessageEmpty();
				//--------------------------------------------------
				// send authentication message
				g_network->SendMessageData(msgBuffer, msgObj->GetMessageSize());
				delete msgObj;
			}

			//------------------------------------------------------
			// process piece
			//------------------------------------------------------
			//TODO: process direct from here
			FramePiece* framePiece = new FramePiece();
			framePiece->frameIndex = READ_U32(buffer, 0);
			framePiece->pieceIndex = READ_U8(buffer, 4);
			framePiece->pieceSize = size - 5;
			framePiece->pieceAddr = buffer + 5;
			g_manager->SafeTrack(framePiece);
			
		}
		catch (...)
		{
			printf("Error when process frame.\n");
			gfxFlushBuffers();
		}
		break;
	}

	//======================================================================
	// AUDIO DECODE
	//======================================================================
	/*
	case MSG_CODE_REQUEST_NEW_AUDIO_FRAME:
	{
		int error;
		//check if opus file is init or not
		if (SS_opusFile == nullptr) {
			// check if it is opus stream
			OggOpusFile* opusTest = op_test_memory(buffer, size, &error);
			op_free(opusTest);
			if (error != 0) {
				printf("Stream is not OPUS format.\n");
				return;
			}
			// create new opus file from first block of memory
			SS_opusFile = op_open_memory(buffer, size, &error);

			// init audio
			//TODO: this should be call on outside
			ndspInit();

			// reset audio channel when start stream
			ndspChnReset(AUDIO_CHANNEL);
			ndspChnWaveBufClear(AUDIO_CHANNEL);
			ndspSetOutputMode(NDSP_OUTPUT_STEREO);
			ndspChnSetInterp(AUDIO_CHANNEL, NDSP_INTERP_LINEAR);
			ndspChnSetRate(AUDIO_CHANNEL, 48000);
			ndspChnSetFormat(AUDIO_CHANNEL, NDSP_FORMAT_STEREO_PCM16);

			float mix[12];
			memset(mix, 0, sizeof(mix));
			mix[0] = 1.0;
			mix[1] = 1.0;
			ndspChnSetMix(0, mix);


			ndspWaveBuf waveBuf[2];
			memset(waveBuf, 0, sizeof(waveBuf));
			waveBuf[0].data_vaddr = &audioBuffer[0];
			waveBuf[0].nsamples = SAMPLESPERBUF;
			waveBuf[1].data_vaddr = &audioBuffer[SAMPLESPERBUF];
			waveBuf[1].nsamples = SAMPLESPERBUF;

			ndspChnWaveBufAdd(AUDIO_CHANNEL, &waveBuf[0]);
			ndspChnWaveBufAdd(AUDIO_CHANNEL, &waveBuf[1]);
		}
		else
		{
			int16_t* bufferOut;
			u32 samplesToRead = 32 * 1024; // 32Kb buffer

										   // read more buffer
			u32 bufferToRead = 120 * 48 * 2;
			int samplesRead = op_read_stereo(SS_opusFile, bufferOut, samplesToRead > bufferToRead ? bufferToRead : samplesToRead);

			if (samplesRead == 0)
			{
				//finish read this
			}
			else if (samplesRead < 0)
			{
				// error ?
				return;
			}
		}

		break;
	}
	*/
	default: break;
	}
}


void PPSession::processInputSession(u8* buffer, size_t size)
{
	//NOTE: this not called on main thread !
	//------------------------------------------------------
	// process message data	by message type
	//------------------------------------------------------
	switch (g_tmpMessage->GetMessageCode())
	{
	default: break;
	}
}


//-----------------------------------------------------
// screen capture
//-----------------------------------------------------
void PPSession::SS_StartStream()
{
	PPMessage *msg = new PPMessage();
	msg->BuildMessageHeader(MSG_CODE_REQUEST_START_SCREEN_CAPTURE);
	u8* msgBuffer = msg->BuildMessageEmpty();
	g_network->SendMessageData(msgBuffer, msg->GetMessageSize());
	g_network->SetRequestData(MSG_COMMAND_SIZE, PPREQUEST_HEADER);
	SS_v_isStartStreaming = true;
	delete msg;
}

void PPSession::SS_StopStream()
{
	if (!SS_v_isStartStreaming) return;
	//--------------------------------------
	PPMessage *msg = new PPMessage();
	msg->BuildMessageHeader(MSG_CODE_REQUEST_STOP_SCREEN_CAPTURE);
	u8* msgBuffer = msg->BuildMessageEmpty();
	g_network->SendMessageData(msgBuffer, msg->GetMessageSize());
	g_network->SetRequestData(MSG_COMMAND_SIZE, PPREQUEST_HEADER);
	SS_v_isStartStreaming = false;
	delete msg;
	//--------------------------------------
	this->CloseSession();
}

void PPSession::SS_ChangeSetting()
{
	PPMessage *authenMsg = new PPMessage();
	authenMsg->BuildMessageHeader(MSG_CODE_REQUEST_CHANGE_SETTING_SCREEN_CAPTURE);

	//-----------------------------------------------
	// alloc msg content block
	size_t contentSize = 13;
	u8* contentBuffer = (u8*)malloc(sizeof(u8) * contentSize);
	u8* pointer = contentBuffer;
	//----------------------------------------------
	// setting: wait for received frame
	u8 _setting_waitToReceivedFrame = SS_setting_waitToReceivedFrame ? 1 : 0;
	WRITE_U8(pointer, _setting_waitToReceivedFrame);
	// setting: smooth frame number ( only activate if waitForReceivedFrame = true)
	WRITE_U32(pointer, SS_setting_smoothStepFrames);
	// setting: frame quality [0 ... 100]
	WRITE_U32(pointer, SS_setting_sourceQuality);
	// setting: frame scale [0 ... 100]
	WRITE_U32(pointer, SS_setting_sourceScale);
	//-----------------------------------------------
	// build message
	u8* msgBuffer = authenMsg->BuildMessage(contentBuffer, contentSize);
	g_network->SendMessageData(msgBuffer, authenMsg->GetMessageSize());
	g_network->SetRequestData(MSG_COMMAND_SIZE, PPREQUEST_HEADER);
	free(contentBuffer);
	delete authenMsg;
}

void PPSession::SS_Reset()
{
	SS_v_isStartStreaming = false;
}

//-----------------------------------------------------
// common
//-----------------------------------------------------

void PPSession::RequestForheader()
{
	g_network->SetRequestData(MSG_COMMAND_SIZE, PPREQUEST_HEADER);
}

//-----------------------------------------------------
// Input
//-----------------------------------------------------
void PPSession::IN_Start()
{
	if (IN_isStart) return;
	PPMessage *msg = new PPMessage();
	msg->BuildMessageHeader(MSG_CODE_REQUEST_START_INPUT_CAPTURE);
	u8* msgBuffer = msg->BuildMessageEmpty();
	g_network->SendMessageData(msgBuffer, msg->GetMessageSize());
	g_network->SetRequestData(MSG_COMMAND_SIZE, PPREQUEST_HEADER);
	delete msg;

	IN_isStart = true;
}

bool PPSession::IN_SendInputData(u32 down, u32 up, short cx, short cy, short ctx, short cty)
{
	if (!IN_isStart) return false;
	PPMessage *msg = new PPMessage();
	msg->BuildMessageHeader(MSG_CODE_SEND_INPUT_CAPTURE);
	//-----------------------------------------------
	// alloc msg content block
	size_t contentSize = 16;
	u8* contentBuffer = (u8*)malloc(sizeof(u8) * contentSize);
	u8* pointer = contentBuffer;
	//----------------------------------------------
	WRITE_U32(pointer, down);
	WRITE_U32(pointer, up);
	WRITE_U16(pointer, cx);
	WRITE_U16(pointer, cy);
	WRITE_U16(pointer, ctx);
	WRITE_U16(pointer, cty);
	//-----------------------------------------------
	// build message and send
	u8* msgBuffer = msg->BuildMessage(contentBuffer, contentSize);
	g_network->SendMessageData(msgBuffer, msg->GetMessageSize());
	g_network->SetRequestData(MSG_COMMAND_SIZE, PPREQUEST_HEADER);
	delete msg;
	return true;
}

void PPSession::IN_SendIdleInput()
{
	PPMessage *msg = new PPMessage();
	msg->BuildMessageHeader(MSG_CODE_SEND_INPUT_CAPTURE_IDLE);
	u8* msgBuffer = msg->BuildMessageEmpty();
	g_network->SendMessageData(msgBuffer, msg->GetMessageSize());
	g_network->SetRequestData(MSG_COMMAND_SIZE, PPREQUEST_HEADER);
	delete msg;
}

void PPSession::IN_Stop()
{
	if (!IN_isStart) return;
	PPMessage *msg = new PPMessage();
	msg->BuildMessageHeader(MSG_CODE_REQUEST_STOP_INPUT_CAPTURE);
	u8* msgBuffer = msg->BuildMessageEmpty();
	g_network->SendMessageData(msgBuffer, msg->GetMessageSize());
	g_network->SetRequestData(MSG_COMMAND_SIZE, PPREQUEST_HEADER);
	delete msg;

	IN_isStart = false;

	this->CloseSession();
}
