/*
 * @brief Audio device class declarations, definitions for using in application
 *
 * @note
 * Copyright(C) NXP Semiconductors, 2012
 * Copyright(C) Dean Camera, 2011, 2012
 * All rights reserved.
 *
 * @par
 * Software that is described herein is for illustrative purposes only
 * which provides customers with programming information regarding the
 * LPC products.  This software is supplied "AS IS" without any warranties of
 * any kind, and NXP Semiconductors and its licensor disclaim any and
 * all warranties, express or implied, including all implied warranties of
 * merchantability, fitness for a particular purpose and non-infringement of
 * intellectual property rights.  NXP Semiconductors assumes no responsibility
 * or liability for the use of the software, conveys no license or rights under any
 * patent, copyright, mask work right, or any other intellectual property rights in
 * or to any products. NXP Semiconductors reserves the right to make changes
 * in the software without notification. NXP Semiconductors also makes no
 * representation or warranty that such application will be suitable for the
 * specified use without further testing or modification.
 *
 * @par
 * Permission to use, copy, modify, and distribute this software and its
 * documentation is hereby granted, under NXP Semiconductors' and its
 * licensor's relevant copyrights in the software, without fee, provided that it
 * is used in conjunction with NXP Semiconductors microcontrollers.  This
 * copyright, permission, and disclaimer notice must appear in all copies of
 * this code.
 */

#ifndef __DESCRIPTORS_H_
#define __DESCRIPTORS_H_

#include "USB.h"

#ifndef CONFIG_H
#include "config.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

//
#define FEATURE_VOLUME (1 << 1)
#define FORMAT_TYPE_1 0x01

/** @defgroup Audio_Input_Device_Descriptor Class descriptors
 * @ingroup USB_Audio_Input_Device_18xx43xx USB_Audio_Input_Device_17xx40xx
 * @{
 */

/**
 * @brief Endpoint number of the Audio isochronous streaming data endpoint
 */
#if defined(__LPC175X_6X__) || defined(__LPC177X_8X__) || defined(__LPC407X_8X__)
#define AUDIO_STREAM_EPNUM           3
#else
#define AUDIO_STREAM_EPNUM           1
#endif

/**
 * @brief Endpoint size in bytes of the Audio isochronous streaming data endpoint. The Windows audio stack requires
 *        at least 192 bytes for correct output, thus the smaller 128 byte maximum endpoint size on some of the smaller
 *        USB models will result in unavoidable distorted output.
 */
#define AUDIO_STREAM_EPSIZE          ENDPOINT_MAX_SIZE(AUDIO_STREAM_EPNUM)


		/** @brief Audio class-specific Feature Unit Descriptor (nxpUSBlib naming conventions).
		 *
		 *  Type define for an Audio class-specific Feature Unit descriptor. This indicates to the host what features
		 *  are present in the device's audio stream for basic control, such as per-channel volume. See the USB Audio
		 *  specification for more details.
		 *
		 *  @see @ref USB_Audio_StdDescriptor_FeatureUnit_t for the version of this type with standard element names.
		 *
		 *  @note Regardless of CPU architecture, these values should be stored as little endian.
		 */
		typedef ATTR_IAR_PACKED struct
		{
			USB_Descriptor_Header_t Header; /**< Regular descriptor header containing the descriptor's type and length. */
			uint8_t                 Subtype; /**< Sub type value used to distinguish between audio class-specific descriptors,
			                                  *   must be @ref AUDIO_DSUBTYPE_CSInterface_Feature.
			                                  */

			uint8_t                 UnitID; /**< ID value of this feature unit - must be a unique value within the device. */
			uint8_t                 SourceID; /**< Source ID value of the audio source input into this feature unit. */

			// Size of each element in the \c ChannelControls array.
			// JME audit: what does this mean ???
			uint8_t ControlSize; 
			
			// JME - this value is a bitmask - one for master, one per 'channel'
			// Feature masks for the control channel, and each separate audio channel.
			uint8_t ChannelControls[CHANNEL_COUNT+1]; 
			// Index of a string descriptor describing this descriptor within the device.
			uint8_t FeatureUnitStrIndex; 
		} ATTR_PACKED USB_Audio_Descriptor_FeatureUnit_t;


/**
 * @brief Type define for the device configuration descriptor structure. This must be defined in the
 *        application code, as the configuration descriptor contains several sub-descriptors which
 *        vary between devices, and which describe the device's usage to the host.
 */
typedef struct 
{
	USB_Descriptor_Configuration_Header_t	Config;
	/* Audio Control Interface */
	USB_Descriptor_Interface_t				Audio_ControlInterface;
	USB_Audio_Descriptor_Interface_AC_t		Audio_ControlInterface_SPC;
	USB_Audio_Descriptor_InputTerminal_t	Audio_InputTerminal;
	USB_Audio_Descriptor_OutputTerminal_t	Audio_OutputTerminal;

	/* Audio Streaming Interface */
	USB_Descriptor_Interface_t				Audio_StreamInterface_Alt0;
	USB_Descriptor_Interface_t				Audio_StreamInterface_Alt1;
	USB_Audio_Descriptor_Interface_AS_t		Audio_StreamInterface_SPC;
	USB_Audio_Descriptor_Format_t			Audio_AudioFormat;
	// no. you utter fucking knuckleheads. do not hardwire size!
	USB_Audio_SampleFreq_t                    Audio_AudioFormatSampleRates[SUPPORTED_SAMPLE_RATES];
	USB_Audio_Descriptor_StreamEndpoint_Std_t Audio_StreamEndpoint;
	USB_Audio_Descriptor_StreamEndpoint_Spc_t Audio_StreamEndpoint_SPC;
	// this is zero filled and must not be included in size calculations
	unsigned char                             Audio_Termination;
	
} USB_Descriptor_Configuration_t;

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* __DESCRIPTORS_H_ */
