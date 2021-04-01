/*
This file is a part of MonaSolutions Copyright 2017
mathieu.poux[a]gmail.com
jammetthomas[a]gmail.com

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License received along this program for more
details (or else see http://www.gnu.org/licenses/).

*/

#pragma once

#include "Mona/Mona.h"
#include "Mona/MediaWriter.h"
#include "Mona/Net.h"
#include "Mona/Util.h"
#include "Mona/Logs.h"


namespace Mona {


template<typename RTP_ProfileType>
struct RTPWriter : MediaWriter, virtual Object {
	// https://en.wikipedia.org/wiki/Real-time_Transport_Protocol
	// https://tools.ietf.org/html/rfc3550
	// playload dynamic types => http://www.iana.org/assignments/rtp-parameters/rtp-parameters.xhtml
	// profiles => https://en.wikipedia.org/wiki/RTP_audio_video_profile
	// /!\ Serialize bufferized packets with maximum size = MTU size configuration, ready to send on network
	
	template <typename ...ProfileArgs>
	RTPWriter(UInt32 mtuSize, ProfileArgs... args) : mtuSize(mtuSize), _profile(args ...) {
		if (mtuSize < 32) {
			ERROR("RTP MTU size must be superior to 32");
			mtuSize = 32;
		}
	}

	template <typename ...ProfileArgs>
	RTPWriter(ProfileArgs... args) : mtuSize(Net::MTU_RELIABLE_SIZE), _profile(args ...) {}
	
	const UInt32 mtuSize;

	void beginMedia(const OnWrite& onWrite) {
		_senderReportTime.update();
		_bytes = _count = 0;
		_ssrc = Util::Random<UInt32>();
	}
	void writeAudio(UInt8 track, const Media::Audio::Tag& tag, const Packet& packet, const OnWrite& onWrite) { write(track, tag, packet, onWrite); }
	void writeVideo(UInt8 track, const Media::Video::Tag& tag, const Packet& packet, const OnWrite& onWrite) { write(track, tag, packet, onWrite); }
	void endMedia(const OnWrite& onWrite) {
		if (!onWrite)
			return;
		// RTCP BYE packet
		BUFFER_RESET(_pBuffer, 0);
		BinaryWriter writer(*_pBuffer);
		writer.write(EXPAND("\x81\xCB\x00\x04"));		// Version (2), padding and Reception report count (1), packet type = 203 (Bye), lenght = 6
		writer.write32(_ssrc);
		onWrite(Packet(_pBuffer));
	}
	
private:


	bool writeMedia(const Media::Audio::Tag& tag, BinaryReader& reader, BinaryWriter& writer, UInt16 canWrite) { return _profile.writeAudio(tag, reader, writer, canWrite); }
	bool writeMedia(const Media::Video::Tag& tag, BinaryReader& reader, BinaryWriter& writer, UInt16 canWrite) { return _profile.writeVideo(tag, reader, writer, canWrite); }

	template<typename TagType>
	void write(UInt8 track, const TagType& tag, const Packet& packet, const OnWrite& onWrite) {
		if (!onWrite)
			return;
		bool isAudio(typeid(TagType) == typeid(Media::Audio::Tag));
		UInt8 playloadType(_profile.playloadType);
		if (!playloadType) {
			ERROR(TypeOf<RTP_ProfileType>(), " profile doesn't support ", Media::CodecToString(tag.codec), isAudio ? " audio" : " video");
			return;
		}

		if (track != _profile.track) {
			if (!_profile.supportTracks) {
				ERROR(TypeOf<RTP_ProfileType>()," profile doesn't support multiple tracks, ", isAudio ? "audio track " : "video track ", track, " ignored");
				return;
			}
			_profile.track = track;
		}

		UInt32 canWrite(0);
		BinaryReader reader(packet.data(), packet.size());
		bool flush(!reader.available());
	
		while (!flush) {
	
			if(!_pBuffer)
				_pBuffer.set();
			BinaryWriter writer(*_pBuffer);

			if (!writer) {
				// Write RTP header
				writer.write8(0x80);		// Version (2), padding and extension (0)
				writer.write8(0x80 | _profile.playloadType); // marker always set to 1
				writer.write16(++_count);	// Sequence number
				writer.write32(tag.time*90);	// Timestamp
				writer.write32(_ssrc);		// SSRC
			
				canWrite = mtuSize-12;
			}
	
			UInt32 written(writer.size());
			flush = writeMedia(tag, reader, writer, canWrite);
			written -= writer.size()>written ? written : writer.size();

			/// post condition checking
			if (!written) {
				// no write
				if (!flush)
					break; // no flush + no write, stop!
				if (writer.size() <= 12) { // just header, flush can't increase canWrite
					ERROR(TypeOf<RTP_ProfileType>()," requires size which superior to the MTU ", mtuSize, " size configured");
					break;
				}
			} else if(written>canWrite) {
				WARN(TypeOf<RTP_ProfileType>()," packet with ", writer.size()," size exceeds the MTU ", mtuSize, " size configured");
				canWrite = 0;
			} else
				canWrite -= written;
	
			if(flush) {
				// flush
				_bytes += _pBuffer->size()-12;
				onWrite(Packet(_pBuffer));
				if (!reader.available())
					break; // flushed, and no more data to read!
			}
		}

		if (_senderReportTime.isElapsed(5000)) {
			// RTCP Sender report
			BUFFER_RESET(_pBuffer, 0);
			BinaryWriter writer(*_pBuffer);
			writer.write(EXPAND("\x80\xC8\x00\x06"));		// Version (2), padding and Reception report count (0), packet type = 200 (Sender Report), lenght = 6
			writer.write32(_ssrc);							// SSRC
			writer.write64(0);								// NTP Timestamp (not needed)
			writer.write32(tag.time);						// RTP Timestamp
			writer.write32(_count);							// Packet count
			writer.write32(_bytes);							// Octet count
			_count = _bytes = 0;
			_senderReportTime.update();
		}
	}

	RTP_ProfileType			_profile;
	shared<Buffer>			_pBuffer;
	UInt32					_bytes;
	UInt16					_count;
	UInt32					_ssrc;
	Time					_senderReportTime;
	
};



} // namespace Mona
