/*



*/


#pragma diag_suppress 368

#include "AudioInput.h"
#include <limits.h>
#include "apptypes.h"

#ifndef INC_FREERTOS_H
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#endif


/** Audio Class driver interface configuration and state information. This structure is
 *  passed to all Audio Class driver functions, so that multiple instances of the same class
 *  within a device can be differentiated from one another.
 */
USB_ClassInfo_Audio_Device_t USBAudioIF =
{
	.Config = 
	{
		.StreamingInterfaceNumber = 1,
		.DataINEndpointNumber     = AUDIO_STREAM_EPNUM,
		.DataINEndpointSize       = AUDIO_STREAM_EPSIZE,
		.PortNumber             = 0,
	},
};

// for runtime modification
extern USB_Descriptor_Configuration_t ConfigurationDescriptor;
//
#define TICKRATE_HZ 1

#define BLUELED 1
#define GREENLED 0

// SSP
#define LPC_SSP           LPC_SSP1
#define SSP_IRQ           SSP1_IRQn
#define LPC_GPDMA_SSP_TX  GPDMA_CONN_SSP1_Tx
#define LPC_GPDMA_SSP_RX  GPDMA_CONN_SSP1_Rx
#define BUFFER_SIZE                         (0x100)
#define SSP_DATA_BITS                       (SSP_BITS_8)
#define SSP_DATA_BIT_NUM(databits)          (databits+1)
#define SSP_DATA_BYTES(databits)            (((databits) > SSP_BITS_8) ? 2:1)
#define SSP_LO_BYTE_MSK(databits)           ((SSP_DATA_BYTES(databits) > 1) ? 0xFF:(0xFF>>(8-SSP_DATA_BIT_NUM(databits))))
#define SSP_HI_BYTE_MSK(databits)           ((SSP_DATA_BYTES(databits) > 1) ? (0xFF>>(16-SSP_DATA_BIT_NUM(databits))):0)

#define SSP_MODE_SEL                        (0x31)
#define SSP_TRANSFER_MODE_SEL               (0x32)
#define SSP_MASTER_MODE_SEL                 (0x31)
#define SSP_SLAVE_MODE_SEL                  (0x32)
#define SSP_POLLING_SEL                     (0x31)
#define SSP_INTERRUPT_SEL                   (0x32)
#define SSP_DMA_SEL                         (0x33)

//-----------------------------------------------------------------------------
// Defines SSP/SPI data format
static SSP_ConfigFormat ssp_format;

//-----------------------------------------------------------------------------
extern void UARTTask(void* pvParameters);

//-----------------------------------------------------------------------------
// Max Sample Frequency.
const uint32_t AUDIO_MAX_SAMPLE_FREQ = 48000;
/** Current audio sampling frequency of the streaming audio endpoint. */
uint32_t CurrentAudioSampleFrequency = AUDIO_MAX_SAMPLE_FREQ;
/* Sample Buffer */
// uint16_t* sample_buffer = NULL;
PRAGMA_ALIGN_4
//
int modulo = 6 * 500;

// some bitmasks to try and work out what is happening to us
#define MODE_ENABLED 8
#define MODE_MASK (0xFFFFFFFF & (1 << MODE_ENABLED))
#define FS_48K		7
#define FS_44K1		6
#define FS_BAD		5

//-----------------------------------------------------------------------------
// This callback function provides iso buffer address for HAL iso transfer processing.
// for ISO In EP, this function also returns the size of buffer, depend on SampleFrequency.

// LED state 
bool iso_state = false; 
// ISO packet callback counter
int iso_packets = 0; 
//
int iso_index = 0;
//-----------------------------------------------------------------------------
//
xQueueHandle logQueueHandle = 0;
xQueueHandle usbQueueHandle = 0;
//-----------------------------------------------------------------------------
// we assume that on SOF interrput we send N channels * sizeof sample to isoch EP
#ifdef FULL_SPEED
// 48 samples every microframe
const int SAMPLE_COUNT = 48;
#else
// 6 samples per 125ns microframe
const int SAMPLE_COUNT = 6;
#endif

//-----------------------------------------------------------------------------
// JME fix sample size at 16 bits for the time being. Keeps init code simple
// in any other configuration simply send zeroes
int16_t data[CHANNEL_COUNT * SAMPLE_COUNT] = { 0 };

//-----------------------------------------------------------------------------

const char* states[MaxStates] = 
{
#ifdef _USE_4357
#ifdef _USE_AC2
	"\n---------NGX 4357 AC2 target---------\n",
	#else
	"\n---NGX 4357 target with SPI console--\n",
#endif	
#else
	"\n---------NGX 4330 target---------\n",
#endif	
	"enabled",
	"disabled",
	"get sample rate",
	"get sample rate no data",
	"set sample rate",
	"set sample rate no data",
	"GetSetInterfaceProperty",
	"Unknown Endpoint Property",
	"Unknown Interface Property",
	"Other Endpoint Property",
	"Endpoint configuration",
	"GetDescriptor",
	"GetStatus",
	"REQ_ClearFeature",
	"REQ_SetFeature",
	"REQ_SetAddress",
	"REQ_GetDescriptor",
	"REQ_GetConfiguration",
	"REQ_SetConfiguration",
	"Unknown Get/SetConfiguration",
	"eGet_DeviceQualifier_Descriptor",
	"eGet_Unknown_Descriptor"
};

//-----------------------------------------------------------------------------
void Log(int index,int v1,int v2, int v3)
{
	/* No audio interface entities in the device descriptor, thus no properties to get or set. */
	portBASE_TYPE xHigherPriorityTaskWoken;
	dbg_message dbm = { 0, 0 };
	dbm.psz = states[index];
	dbm.v1 = v1;
	dbm.v2 = v2;
	dbm.v3 = v3;
	xQueueSendFromISR(logQueueHandle,&dbm,&xHigherPriorityTaskWoken);
}

//-----------------------------------------------------------------------------
void LogReq(const char* psz,int v1,int v2, int v3,int v4,int v5)
{
	/* No audio interface entities in the device descriptor, thus no properties to get or set. */
	portBASE_TYPE xHigherPriorityTaskWoken;
	dbg_message dbm = { 0, 0 };
	dbm.psz = psz;
	dbm.v1 = v1;
	dbm.v2 = v2;
	dbm.v3 = v3;
	dbm.v4 = v4;
	dbm.v5 = v5;
	xQueueSendFromISR(logQueueHandle,&dbm,&xHigherPriorityTaskWoken);
}

//-----------------------------------------------------------------------------
void Log5F(const char* file,int line,const char* psz,int v1,int v2, int v3,int v4,int v5)
{
	/* No audio interface entities in the device descriptor, thus no properties to get or set. */
	portBASE_TYPE xHigherPriorityTaskWoken;
	dbg_message dbm = { 0, 0 };
	dbm.file = file;
	dbm.line = line;
	dbm.psz = psz;
	dbm.v1 = v1;
	dbm.v2 = v2;
	dbm.v3 = v3;
	dbm.v4 = v4;
	dbm.v5 = v5;
	xQueueSendFromISR(logQueueHandle,&dbm,&xHigherPriorityTaskWoken);
}
//-----------------------------------------------------------------------------
void Log5(const char* psz,int v1,int v2, int v3,int v4,int v5)
{
	/* No audio interface entities in the device descriptor, thus no properties to get or set. */
	portBASE_TYPE xHigherPriorityTaskWoken;
	dbg_message dbm = { 0, 0 };
	dbm.psz = psz;
	dbm.v1 = v1;
	dbm.v2 = v2;
	dbm.v3 = v3;
	dbm.v4 = v4;
	dbm.v5 = v5;
	xQueueSendFromISR(logQueueHandle,&dbm,&xHigherPriorityTaskWoken);
}

//-----------------------------------------------------------------------------
void Log3(const char* psz,int v1,int v2, int v3)
{
	/* No audio interface entities in the device descriptor, thus no properties to get or set. */
	portBASE_TYPE xHigherPriorityTaskWoken;
	dbg_message dbm = { 0, 0 };
	dbm.psz = psz;
	dbm.v1 = v1;
	dbm.v2 = v2;
	dbm.v3 = v3;
	xQueueSendFromISR(logQueueHandle,&dbm,&xHigherPriorityTaskWoken);
}

//-----------------------------------------------------------------------------
// USB packet logger
void LogUSB(const char* file,int line,void* packet,int packet_size)
{
	/* No audio interface entities in the device descriptor, thus no properties to get or set. */
	portBASE_TYPE xHigherPriorityTaskWoken;
	USBMessage um = { 0 };
	um.file = file;
	um.line = line;
	memcpy(&um.request,packet,packet_size);
	xQueueSendFromISR(usbQueueHandle,&um,&xHigherPriorityTaskWoken);
}

//-----------------------------------------------------------------------------
// Set up the pre-defined N channel waveform we currently stream to host
void FillAudioBuffers()
{
	int16_t value = 0;
	int16_t step = SHRT_MAX / (SAMPLE_COUNT / 2);
	int index = 0;
	// set up the sample pattern in the USB xfer buffer
	// depending on how many channels and bus speed
	// this is a really simple full range saw wave
	int sample = 0;
	int channel = 0;
	for (sample = 0; sample < SAMPLE_COUNT; sample++)
	{
		// apply same values to all channels
		for (channel = 0; channel < CHANNEL_COUNT; channel++)
		{
			data[index++] = value;
		}
		// next value
		value += step;
	}
}

//-----------------------------------------------------------------------------
// Set the channel mask flags - not used any more.
void SetChannelMask()
{
	int i = 0;
	// this can be played with in the debugger ...
	bool use_channel_mask = (USE_CHANNEL_MASK == 1);
	//bool use_channel_mask = (CHANNEL_COUNT <= 8);
	// the default mask according to the spec can be zero
	uint32_t mask = 0;
	// set the channel mask at run-time
	if (use_channel_mask)
	{
		// set a bit for each channel
		for (i = 0; i < CHANNEL_COUNT; i++)
		{
			mask |= (1 << i);
		}
	}

	// now set the mask.
	ConfigurationDescriptor.Audio_InputTerminal.ChannelConfig = mask;
}

//-----------------------------------------------------------------------------
// Called on every SOF interrupt. Thus 8KHz at high-speed and 1KHz at full-speed
// this is actually returning a pointer to the data buffer to be transferred
// JME WTF does this not pass instance pointer?
uint32_t CALLBACK_HAL_GetISOBufferAddress(const uint32_t EPNum, uint32_t* packet_size)
{
	*packet_size = CHANNEL_COUNT * SAMPLE_COUNT * BYTES_PER_SAMPLE;
	// Check if this is audio stream endpoint
	if ((EPNum & 0x7F) == AUDIO_STREAM_EPNUM)
	{
		iso_packets++;
		if (iso_packets % modulo == 0)
		{
			iso_state = !iso_state;
			// should flash at sub-multiple of blue SOF LED
			// Board_LED_Set(GREENLED, iso_state);
		}
		// return the audio data buffer
		return (uint32_t) &data[0];
	}
	else
	{
		return 0;
	}
}
//-----------------------------------------------------------------------------
// timer runs and ticks green LED so we know we are waiting for SW2
void TIMER1_IRQHandler(void)
{
	static bool On = false;

	if (Chip_TIMER_MatchPending(LPC_TIMER1, 1)) 
	{
		Chip_TIMER_ClearMatch(LPC_TIMER1, 1);
		On = (bool) !On;
		Board_LED_Set(GREENLED, On);
	}
}

//-----------------------------------------------------------------------------
const int spiBufferSize = 255;
//
typedef struct _SPITaskData
{	
	char ipBuffer[spiBufferSize];
	char opBuffer[spiBufferSize];
} SPITaskData;

SPITaskData sptid = { 0, 0 };

//-----------------------------------------------------------------------------
//
int UARTReadString()
{
	int ret = -1;
	int index = 0;
	while (index < spiBufferSize - 1)
	{
		ret = Board_UARTGetChar();
		if (ret != EOF)
		{
			if (ret == '\r' || ret == '\n')
			{
				break;
			}
			else
			{
				sptid.ipBuffer[index++] = (char) ret;
			}
		}
	}
	// ensure always terminated
	sptid.ipBuffer[index] = '\0';
	//
	return index;
}

//-----------------------------------------------------------------------------
uint8_t fromHex(const char ch)
{
	uint8_t ret = 0;
	if (ch >= '0' && ch <= '9')
	{
		ret = ch - '0';
	}
	else if (ch >= 'A' && ch <= 'F')
	{
		ret = ch - 'A' + 10;
	}
	else if (ch >= 'a' && ch <= 'f')
	{
		ret = ch - 'a' + 10;
	}
	return ret;
}

//-----------------------------------------------------------------------------
// i.e 'F''F' -> 0xFF
uint8_t fromHex2(const char hb,const char lb)
{
	uint8_t ret = fromHex(hb);
	ret <<= 4;
	ret = ret + fromHex(lb);
	return ret;
}

//-----------------------------------------------------------------------------
// Read from UART and dispatch to the SPI ...
void SPITask(void* pvParameters)
{
	// SSP initialization
	Board_SSP_Init(LPC_SSP);

	Chip_SSP_Init(LPC_SSP);

	// make sure we have the correct format and clock(s)
	ssp_format.frameFormat = CHIP_SSP_FRAME_FORMAT_TI;
	ssp_format.bits = SSP_BITS_16;
	ssp_format.clockMode = SSP_CLOCK_MODE0;
	Chip_SSP_SetFormat(LPC_SSP, &ssp_format);
	
	// enable the IP
	Chip_SSP_Enable(LPC_SSP);

	// we are the SSP/SPI master
	Chip_SSP_SetMaster(LPC_SSP, true);

	// this is the write/wait/read idiom
	//  loop forever
	uint16_t rxw = 0;
	int index = 0;
	for (;;)
	{
		//int ipch = Board_UARTGetChar();
		//if (ipch != EOF)
		int chars = UARTReadString();
		if (chars >= 4)
		{
			// get address and data from string
			uint8_t addr = fromHex2(sptid.ipBuffer[0],sptid.ipBuffer[1]);
			uint8_t data = fromHex2(sptid.ipBuffer[2],sptid.ipBuffer[3]);
			// create the 16bit frame
			uint16_t frame = addr << 8;
			frame |= data;
			// send
			//IP_SSP_SendFrame(LPC_SSP,0x0D00);
			IP_SSP_SendFrame(LPC_SSP,frame);
			// check status
			while (IP_SSP_GetStatus(LPC_SSP,SSP_STAT_RNE) != SET) 
			{
			}
			// read
			rxw = IP_SSP_ReceiveFrame(LPC_SSP);
			// format the message
			sprintf(sptid.opBuffer,"[%03d] SPI: %s (0x%04X) => 0x%04X\n",index,sptid.ipBuffer,frame,rxw);
			// write down the pipe
			Board_UARTPutSTR(sptid.opBuffer);
			//
			index++;
		}
	}
}

//-----------------------------------------------------------------------------
volatile bool connected = true;

//-----------------------------------------------------------------------------
// This polls the USB registers for state change. Not ideal but works for now
void AudioTask(void* pvParameters)
{
	// switch off green LED
	Board_LED_Set(GREENLED, false);

	// 
	SetChannelMask();

	// set up our N channel waveform
	FillAudioBuffers();
	
	// 
	for (;;)
	{
		Board_UARTPutSTR("Press User switch for ~1s to connect\n");
		
		while (connected == false)
		{
			Board_LED_Set(0,true);
			// wait for ~10s
			vTaskDelay(configTICK_RATE_HZ / 10);
			//
			Board_LED_Set(0,false);
			//
			vTaskDelay(configTICK_RATE_HZ / 10);
		}

		// Initialize the USB audio driver
		USB_Init(USBAudioIF.Config.PortNumber, USB_MODE_Device,USE_FULL_SPEED);

		// enable SOF interrupt
		USB_Device_EnableSOFEvents();

		Board_UARTPutSTR("Device is connected to host\n");

		// JME audit:polled
		while (connected == true)
		{
			//
			Audio_Device_USBTask(&USBAudioIF);
			//
			USB_USBTask(USBAudioIF.Config.PortNumber, USB_MODE_Device);
		}
		//
		Board_UARTPutSTR("Device disconnecting from host\n");
		// should cause re-enumeration
		//USB_ResetInterface(USBAudioIF.Config.PortNumber,USB_MODE_Device);
		USB_Disable(USBAudioIF.Config.PortNumber,USB_MODE_Device);
		//
		Board_UARTPutSTR("Device disconnected from host\n");
		// wait for ~10s
		vTaskDelay(configTICK_RATE_HZ * 10);
		//
		Board_UARTPutSTR("Device waited ~10s\n");
	}

}

//-----------------------------------------------------------------------------
// Check User button. Flip mode as required. Acts as a very crude debouncer for SW2
void SwitchTask(void* pvParameters)
{
	for (;;)
	{
		//
		if (Buttons_GetStatus() & BUTTONS_BUTTON1)
		{
			Board_LED_Set(0,true);
			// wait ...
			vTaskDelay(configTICK_RATE_HZ/2);
			// still down ?
			if (Buttons_GetStatus() & BUTTONS_BUTTON1)
			{
				connected = !connected;
				//
				Board_LED_Set(0,false);
			}
			// wait ...
			vTaskDelay(configTICK_RATE_HZ);
		}
	}
}

//-----------------------------------------------------------------------------
// Set up all the initial timer IRQ stuff
void InitTimer()
{
	//
	uint32_t timerFreq = 0;
	/* Enable timer 1 clock and reset it */
	Chip_TIMER_Init(LPC_TIMER1);
	Chip_RGU_TriggerReset(RGU_TIMER1_RST);
	while (Chip_RGU_InReset(RGU_TIMER1_RST)) 
	{
		// NOP
	}

	/* Get timer 1 peripheral clock rate */
	timerFreq = Chip_Clock_GetRate(CLK_MX_TIMER1);

	/* Timer setup for match and interrupt at TICKRATE_HZ */
	Chip_TIMER_Reset(LPC_TIMER1);
	Chip_TIMER_MatchEnableInt(LPC_TIMER1, 1);
	Chip_TIMER_SetMatch(LPC_TIMER1, 1, (timerFreq / TICKRATE_HZ));
	Chip_TIMER_ResetOnMatchEnable(LPC_TIMER1, 1);
	Chip_TIMER_Enable(LPC_TIMER1);

	/* Enable timer interrupt */
//	NVIC_EnableIRQ(TIMER1_IRQn);
//	NVIC_ClearPendingIRQ(TIMER1_IRQn);
}

//-----------------------------------------------------------------------------
// Main program entry point. This routine contains the overall program flow, including initial
 // setup of all components and the main program loop.
int main(void)
{
	// set up hardware
	Board_Init();
	
	Board_Buttons_Init();
	
	// check LEDs
	Board_LED_Set(0,true);
	Board_LED_Set(1,true);
	Board_LED_Set(0,false);
	Board_LED_Set(1,false);
	
	// and ticker timer at 1hz
//	InitTimer();
	Board_Debug_Init();
	
	//
	Board_UARTPutSTR(states[eTarget]);

	// create the ISR safe qeueue	
	logQueueHandle = xQueueCreate(64,sizeof(DbgMessage));
	// another ISR safe queue for USB packets so we can decode completely
	usbQueueHandle = xQueueCreate(64,sizeof(USBMessage));
	//
	USBAudioIF.instance_data = logQueueHandle;
	
	// create the audio test. this currently polls, should eventually be
	// modified so it waits correctly (power)
	xTaskCreate(AudioTask, (signed char *) "AudioTask",
				configMINIMAL_STACK_SIZE, NULL, (tskIDLE_PRIORITY + 1UL),
				(xTaskHandle *) NULL);

	//  UART thread monitors queue(s) and spits out any debug/tracing
	xTaskCreate(UARTTask, (signed char *) "UARTTask",
				configMINIMAL_STACK_SIZE, NULL, (tskIDLE_PRIORITY + 1UL),
				(xTaskHandle *) NULL);

	//  switch thread checks user switch and does de-bouncing (!)
	xTaskCreate(SwitchTask, (signed char *) "SwitchTask",
		configMINIMAL_STACK_SIZE, NULL, (tskIDLE_PRIORITY + 1UL),
		(xTaskHandle *) NULL);

	//  check incoming for SPI commands (!)
	xTaskCreate(SPITask, (signed char *) "SPITask",
		configMINIMAL_STACK_SIZE, NULL, (tskIDLE_PRIORITY + 1UL),
		(xTaskHandle *) NULL);

	//
	Board_UARTPutSTR("vTaskStartScheduler() \n");

	/* Start the scheduler */
	vTaskStartScheduler();

	/* Should never arrive here */
	return 1;
}	


//-----------------------------------------------------------------------------
// Event handler for the library USB Connection event.
void EVENT_USB_Device_Connect(void)
{
	Board_LED_Set(GREENLED, 1);
}

//-----------------------------------------------------------------------------
// Event handler for the library USB Disconnection event.
void EVENT_USB_Device_Disconnect(void)
{
	Board_LED_Set(GREENLED, 0);
}

//-----------------------------------------------------------------------------
static uint32_t counter = 0;
static bool lit = false;

//-----------------------------------------------------------------------------
// ISR on each SOF
void EVENT_USB_Device_StartOfFrame(void)
{
	counter++;
	if (counter % modulo == 0)
	{
		lit = !lit;
		Board_LED_Set(BLUELED, lit);
	}
}

//-----------------------------------------------------------------------------
// Event handler for the library USB Configuration Changed event.
void EVENT_USB_Device_ConfigurationChanged(void)
{
	bool ConfigSuccess = true;
	ConfigSuccess &= Audio_Device_ConfigureEndpoints(&USBAudioIF);
	//
	portBASE_TYPE xHigherPriorityTaskWoken;
	dbg_message dbm = { 0 };
	xQueueHandle xq = (xQueueHandle)USBAudioIF.instance_data;
	dbm.psz = states[eConfigureEndpoints];
	dbm.v1 = USB_Device_ConfigurationNumber;
	xQueueSendFromISR(xq,&dbm,&xHigherPriorityTaskWoken);
}

//-----------------------------------------------------------------------------
// Event handler for the library USB Control Request reception event.
void EVENT_USB_Device_ControlRequest(void)
{
	Audio_Device_ProcessControlRequest(&USBAudioIF);
}

//-----------------------------------------------------------------------------
// Indicates if streaming has started ...
void EVENT_Audio_Device_StreamStartStop(USB_ClassInfo_Audio_Device_t* const AudioInterfaceInfo)
{
	portBASE_TYPE xHigherPriorityTaskWoken;
	dbg_message dbm = { 0 };
	xQueueHandle xq = (xQueueHandle)AudioInterfaceInfo->instance_data;
	if (AudioInterfaceInfo->State.InterfaceEnabled)
	{
		Board_LED_Set(GREENLED, 1);
		// enabled
		dbm.psz = states[eEnabled];
	}
	else
	{
		Board_LED_Set(GREENLED, 0);
		// disabled
		dbm.psz = states[eDisabled];
	}
	//
	xQueueSendFromISR(xq,&dbm,&xHigherPriorityTaskWoken);
}

//-----------------------------------------------------------------------------
// JME added to support UAC2
bool CALLBACK_Audio_Device_GetSetInterfaceProperty(USB_ClassInfo_Audio_Device_t* const AudioInterfaceInfo,
												   const uint8_t InterfaceIndex,
												   const uint8_t EntityIndex,
												   const uint8_t ControlIndex,
												   uint16_t* const DataLength,
												   uint8_t* Data)
{
	static const char* psz = "CALLBACK_Audio_Device_GetSetInterfaceProperty";
	Log5(psz,(uint8_t)InterfaceIndex,(uint8_t)ControlIndex,(uint8_t)EntityIndex,0,0);
	bool ret = false;
	/* Check the requested control to see if a supported control is being manipulated */
	if (ControlIndex == AUDIO_EPCONTROL_SamplingFreq)
	{
		ret = true;
	}
	return ret;
}

//-----------------------------------------------------------------------------
// Audio class driver callback for the setting and retrieval of streaming endpoint properties. 
// This callback must be implemented in the user application to 
// handle property manipulations on streaming audio endpoints.


bool 
CALLBACK_Audio_Device_GetSetEndpointProperty(
		USB_ClassInfo_Audio_Device_t* const AudioInterfaceInfo,
        const uint8_t EndpointProperty,
        const uint8_t EndpointAddress,
        const uint8_t EndpointControl,
        uint16_t* const DataLength,
        uint8_t* Data)
{
	portBASE_TYPE xHigherPriorityTaskWoken;
	bool ret = false;
	xQueueHandle xq = (xQueueHandle)AudioInterfaceInfo->instance_data;
	dbg_message dbm = { 0, 0 };
	/* Check the requested endpoint to see if a supported endpoint is being manipulated */
	if (EndpointAddress == (ENDPOINT_DIR_IN | USBAudioIF.Config.DataINEndpointNumber))
	{
		/* Check the requested control to see if a supported control is being manipulated */
		if (EndpointControl == AUDIO_EPCONTROL_SamplingFreq)
		{
			switch (EndpointProperty)
			{
			case AUDIO_REQ_SetCurrent:
				// Check if we are just testing for a valid property, 
				// or actually adjusting it
				if (DataLength != NULL)
				{
					dbm.psz = states[eSetSampleRate];
					dbm.v2 = *DataLength;
					// Set the new sampling frequency to the value given by the host
					CurrentAudioSampleFrequency =
					    (((uint32_t) Data[2] << 16) | ((uint32_t) Data[1] << 8) | (uint32_t) Data[0]);
					switch (CurrentAudioSampleFrequency)
					{
						case 48000:
							dbm.v1 = 48000;
						break;
						case 44100:
							dbm.v1 = 44100;
						break;
						default:
							dbm.v1 = 999;
						break;
					}
				}
				else
				{
					dbm.psz = states[eSetSampleRateNoData];
				}
				ret = true;
				break;
			case AUDIO_REQ_GetCurrent:
				// Check if we are just testing for a valid property, or actually reading it
				dbm.v1 = CurrentAudioSampleFrequency;
				//
				if (DataLength != NULL)
				{
					dbm.psz = states[eGetSampleRate];
					dbm.v2 = *DataLength;
					*DataLength = 3;
					Data[2] = (CurrentAudioSampleFrequency >> 16);
					Data[1] = (CurrentAudioSampleFrequency >> 8);
					Data[0] = (CurrentAudioSampleFrequency &  0xFF);
				}
				else
				{
					dbm.psz = states[eGetSampleRateNoData];
				}
				ret = true;
			break;
			default:
				dbm.psz = states[eUnknownEndpointProperty];
				dbm.v1 = EndpointProperty;
			}
		}
		else
		{
			// other end point control
			dbm.psz = states[eOtherEndpointProperty];
			dbm.v1 = EndpointControl;
		}
	}
	else
	{
		// other end point address
	}
	//
	xQueueSendFromISR(xq,&dbm,&xHigherPriorityTaskWoken);
	//
	return ret;
}

