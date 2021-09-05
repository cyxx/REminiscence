
/*
 * REminiscence - Flashback interpreter
 * Copyright (C) 2005-2019 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifdef USE_TREMOR
#include <tremor/ivorbisfile.h>
#endif
#ifdef USE_STB_VORBIS
#include "stb_vorbis.c"
#endif
#include "file.h"
#include "mixer.h"
#include "ogg_player.h"
#include "util.h"

#ifdef USE_TREMOR
struct VorbisFile: File {
	uint32_t offset;

	static size_t readHelper(void *ptr, size_t size, size_t nmemb, void *datasource) {
		VorbisFile *vf = (VorbisFile *)datasource;
		if (size != 0 && nmemb != 0) {
			const int n = vf->read(ptr, size * nmemb);
			if (n > 0) {
				vf->offset += n;
				return n / size;
			}
		}
		return 0;
	}
	static int seekHelper(void *datasource, ogg_int64_t offset, int whence) {
		VorbisFile *vf = (VorbisFile *)datasource;
		switch (whence) {
		case SEEK_SET:
			vf->offset = offset;
			break;
		case SEEK_CUR:
			vf->offset += offset;
			break;
		case SEEK_END:
			vf->offset = vf->size() + offset;
			break;
		}
		vf->seek(vf->offset);
		return 0;
	}
	static int closeHelper(void *datasource) {
		VorbisFile *vf = (VorbisFile *)datasource;
		vf->close();
		delete vf;
		return 0;
	}
	static long tellHelper(void *datasource) {
		VorbisFile *vf = (VorbisFile *)datasource;
		return vf->offset;
	}
};

struct OggDecoder_impl {
	OggDecoder_impl()
		: _open(false), _readBuf(0), _readBufSize(0) {
	}
	~OggDecoder_impl() {
		free(_readBuf);
		_readBuf = 0;
		if (_open) {
			ov_clear(&_ovf);
		}
	}

	bool load(const char *name, FileSystem *fs, int mixerSampleRate) {
		if (!_f.open(name, "rb", fs)) {
			return false;
		}
		_f.offset = 0;
		ov_callbacks ovcb;
		ovcb.read_func  = VorbisFile::readHelper;
		ovcb.seek_func  = VorbisFile::seekHelper;
		ovcb.close_func = VorbisFile::closeHelper;
		ovcb.tell_func  = VorbisFile::tellHelper;
		if (ov_open_callbacks(&_f, &_ovf, 0, 0, ovcb) < 0) {
			warning("Invalid .ogg file");
			return false;
		}
		_open = true;
		vorbis_info *vi = ov_info(&_ovf, -1);
		if ((vi->channels != 1 && vi->channels != 2) || vi->rate != mixerSampleRate) {
			warning("Unhandled ogg/pcm format ch %d rate %d", vi->channels, vi->rate);
			return false;
		}
		_channels = vi->channels;
		return true;
	}
	int read(int16_t *dst, int samples) {
		int size = samples * _channels * sizeof(int16_t);
		if (size > _readBufSize) {
			_readBufSize = size;
			free(_readBuf);
			_readBuf = (int16_t *)malloc(_readBufSize);
			if (!_readBuf) {
				return 0;
			}
		}
		int count = 0;
		while (size > 0) {
			const int len = ov_read(&_ovf, (char *)_readBuf, size, 0);
			if (len < 0) {
				// error in decoder
				return count;
			} else if (len == 0) {
				// loop
				ov_raw_seek(&_ovf, 0);
				continue;
			}
			assert((len & 1) == 0);
			switch (_channels) {
			case 2:
				assert((len & 3) == 0);
				for (int i = 0; i < len / 2; i += 2) {
					const int16_t s16 = (_readBuf[i] + _readBuf[i + 1]) / 2;
					*dst = ADDC_S16(*dst, s16);
					++dst;
				}
				break;
			case 1:
				for (int i = 0; i < len / 2; ++i) {
					*dst = ADDC_S16(*dst, _readBuf[i]);
					++dst;
				}
				break;
			}
			size -= len;
			count += len;
		}
		assert(size == 0);
		return count;
	}

	VorbisFile _f;
	OggVorbis_File _ovf;
	int _channels;
	bool _open;
	int16_t *_readBuf;
	int _readBufSize;
};
#endif

#ifdef USE_STB_VORBIS
static const int kMusicVolume = 192;

struct OggDecoder_impl {
	OggDecoder_impl()
		: _v(0) {
	}
	~OggDecoder_impl() {
		if (_v) {
			stb_vorbis_close(_v);
			_v = 0;
		}
	}
	bool load(const char *name, FileSystem *fs, int mixerSampleRate) {
		if (!_f.open(name, "rb", fs)) {
			return false;
		}
		_count = _f.read(_buffer, sizeof(_buffer));
		if (_count > 0) {
			int bytes = 0;
			int error = 0;
			_v = stb_vorbis_open_pushdata(_buffer, _count, &bytes, &error, 0);
			if (_v) {
				_offset = bytes;
				stb_vorbis_info info = stb_vorbis_get_info(_v);
				if (info.channels != 2 || (int)info.sample_rate != mixerSampleRate) {
					warning("Unhandled ogg/pcm format ch %d rate %d", info.channels, info.sample_rate);
					return false;
				}
				_decodedSamplesLen = 0;
				return true;
			}
		}
		return false;
	}
	int read(int16_t *dst, int samples) {
		int total = 0;
		if (_decodedSamplesLen != 0) {
			const int len = MIN(_decodedSamplesLen, samples);
			for (int i = 0; i < len; ++i) {
				const int sample = (_decodedSamples[0][i] + _decodedSamples[1][i]) / 2;
				*dst = ADDC_S16(*dst, ((sample * kMusicVolume) >> 8));
				++dst;
			}
			total += len;
			_decodedSamplesLen -= len;
		}
		while (total < samples) {
			int channels = 0;
			float **outputs;
			int count;
			int bytes = stb_vorbis_decode_frame_pushdata(_v, _buffer + _offset, _count - _offset, &channels, &outputs, &count);
			if (bytes == 0) {
				if (_offset != _count) {
					memmove(_buffer, _buffer + _offset, _count - _offset);
					_offset = _count - _offset;
				} else {
					_offset = 0;
				}
				_count = sizeof(_buffer) - _offset;
				bytes = _f.read(_buffer + _offset, _count);
				if (bytes < 0) {
					break;
				}
				if (bytes == 0) {
					// rewind
					_f.seek(0);
					_count = _f.read(_buffer, sizeof(_buffer));
					stb_vorbis_flush_pushdata(_v);
				} else {
					_count = _offset + bytes;
				}
				_offset = 0;
				continue;
			}
			_offset += bytes;
			if (channels == 2) {
				const int remain = samples - total;
				const int len = MIN(count, remain);
				for (int i = 0; i < len; ++i) {
					const int l = int(outputs[0][i] * 32768 + .5);
					const int r = int(outputs[1][i] * 32768 + .5);
					const int sample = (l + r) / 2;
					*dst = ADDC_S16(*dst, ((sample * kMusicVolume) >> 8));
					++dst;
				}
				if (count > remain) {
					_decodedSamplesLen = count - remain;
					assert(_decodedSamplesLen < 1024);
					for (int i = 0; i < _decodedSamplesLen; ++i) {
						_decodedSamples[0][i] = int(outputs[0][len + i] * 32768 + .5);
						_decodedSamples[1][i] = int(outputs[1][len + i] * 32768 + .5);
					}
					total = samples;
					break;
				}
			} else {
				warning("Invalid decoded data channels %d count %d", channels, count);
			}
			total += count;
		}
		return total;
	}

	uint8_t _buffer[8192];
	int16_t _decodedSamples[2][1024];
	int _decodedSamplesLen;
	uint32_t _offset, _count;
	stb_vorbis *_v;
	File _f;
};
#endif

OggPlayer::OggPlayer(Mixer *mixer, FileSystem *fs)
	: _mix(mixer), _fs(fs) {
	_impl = new OggDecoder_impl;
}

OggPlayer::~OggPlayer() {
	delete _impl;
	_impl = 0;
}

bool OggPlayer::playTrack(int num) {
	stopTrack();
	char buf[16];
	snprintf(buf, sizeof(buf), "track%02d.ogg", num);
	if (_impl->load(buf, _fs, _mix->getSampleRate())) {
		debug(DBG_INFO, "Playing '%s'", buf);
		_mix->setPremixHook(mixCallback, this);
		return true;
	}
	return false;
}

void OggPlayer::stopTrack() {
	if (_impl) {
		_mix->setPremixHook(0, 0);
	}
}

void OggPlayer::pauseTrack() {
	if (_impl) {
		_mix->setPremixHook(0, 0);
	}
}

void OggPlayer::resumeTrack() {
	if (_impl) {
		_mix->setPremixHook(mixCallback, this);
	}
}

bool OggPlayer::mix(int16_t *buf, int len) {
	if (_impl) {
		return _impl->read(buf, len) != 0;
	}
	return false;
}

bool OggPlayer::mixCallback(void *param, int16_t *buf, int len) {
	return ((OggPlayer *)param)->mix(buf, len);
}

