//
// FPlayAndroid is distributed under the FreeBSD License
//
// Copyright (c) 2013-2014, Carlos Rafael Gimenes das Neves
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
// ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// The views and conclusions contained in the software and documentation are those
// of the authors and should not be interpreted as representing official policies,
// either expressed or implied, of the FreeBSD Project.
//
// https://github.com/carlosrafaelgn/FPlayAndroid
//

typedef void (*EFFECTPROC)(short* buffer, unsigned int sizeInFrames);

#define x_n1_L samples[0]
#define x_n1_R samples[1]
#define y_n1_L samples[2]
#define y_n1_R samples[3]
#define x_n2_L samples[4]
#define x_n2_R samples[5]
#define y_n2_L samples[6]
#define y_n2_R samples[7]

#define equalizerX86() \
	/* since this is a cascade filter, band0's output is band1's input and so on.... */ \
	for (int i = 0; i < equalizerMaxBandCount; i++, samples += 8) { \
		/* y(n) = b0.x(n) + b1.x(n-1) + b2.x(n-2) - a1.y(n-1) - a2.y(n-2) */ \
		/* but, since our a2 was negated and b1 = a1, the formula becomes: */ \
		/* y(n) = b0.x(n) + b1.(x(n-1) - y(n-1)) + b2.x(n-2) + a2.y(n-2) */ \
		\
		/* b0_b1 = { b0, b0, b1, b1 } */ \
		__m128 b0_b1 = _mm_load_ps(equalizerCoefs + (i << 3)); \
		/* b2_a2 = { b2, b2, -a2, -a2 } */ \
		__m128 b2_a2 = _mm_load_ps(equalizerCoefs + (i << 3) + 4); \
		\
		/* local = { x_n1_L, x_n1_R, y_n1_L, y_n1_R } */ \
		__m128 local = _mm_load_ps(samples); \
		\
		/* tmp = { x_n1_L, x_n1_R, y_n1_L, y_n1_R } */ \
		__m128 tmp = local; \
		\
		/* tmp2 = { y_n1_L, y_n1_R, xxx, xxx } */ \
		tmp2 = _mm_movehl_ps(tmp2, local); \
		\
		/* tmp = { x_n1_L - y_n1_L, x_n1_R - y_n1_R, xxx, xxx } */ \
		tmp = _mm_sub_ps(tmp, tmp2); \
		\
		/* inLR = { L, R, x_n1_L - y_n1_L, x_n1_R - y_n1_R } */ \
		inLR = _mm_movelh_ps(inLR, tmp); \
		\
		/* tmp2 = { x_n2_L, x_n2_R, y_n2_L, y_n2_R } */ \
		tmp2 = _mm_load_ps(samples + 4); \
		\
		/* b0_b1 = { b0 * L, b0 * R, b1 * (x_n1_L - y_n1_L), b1 * (x_n1_R - y_n1_R) } */ \
		b0_b1 = _mm_mul_ps(b0_b1, inLR); \
		\
		/* b2_a2 = { b2 * x_n2_L, b2 * x_n2_R, -a2 * y_n2_L, -a2 * y_n2_R } */ \
		b2_a2 = _mm_mul_ps(b2_a2, tmp2); \
		\
		/* b0_b1 = { (b0 * L) + (b2 * x_n2_L), (b0 * R) + (b2 * x_n2_R), (b1 * (x_n1_L - y_n1_L)) + (-a2 * y_n2_L), (b1 * (x_n1_R - y_n1_R)) + (-a2 * y_n2_R) } */ \
		b0_b1 = _mm_add_ps(b0_b1, b2_a2); \
		/* tmp = { (b1 * (x_n1_L - y_n1_L)) + (-a2 * y_n2_L), (b1 * (x_n1_R - y_n1_R)) + (-a2 * y_n2_R), xxx, xxx } */ \
		tmp = _mm_movehl_ps(tmp, b0_b1); \
		\
		/*
		x_n2_L = local_x_n1_L;
		x_n2_R = local_x_n1_R;
		y_n2_L = local_y_n1_L;
		y_n2_R = local_y_n1_R; */ \
		_mm_store_ps(samples + 4, local); \
		\
		/* tmp = { outL, outR, xxx, xxx } */ \
		tmp = _mm_add_ps(tmp, b0_b1); \
		\
		/* inLR = { L, R, outL, outR } */ \
		inLR = _mm_movelh_ps(inLR, tmp); \
		\
		/*
		x_n1_L = inL;
		x_n1_R = inR;
		y_n1_L = outL;
		y_n1_R = outR;*/ \
		_mm_store_ps(samples, inLR); \
		\
		/*
		inL = outL;
		inR = outR; */ \
		inLR = tmp; \
	}

#define floatToShortX86() \
	/*
	inL *= gainClip;
	inR *= gainClip; */ \
	inLR = _mm_mul_ps(inLR, gainClip); \
	\
	if (effectsFramesBeforeRecoveringGain <= 0) { \
		/*
		gainClip *= effectsGainRecoveryPerFrame[0];
		if (gainClip > 1.0f)
			gainClip = 1.0f; */ \
		gainClip = _mm_mul_ps(gainClip, *((__m128*)effectsGainRecoveryPerFrame)); \
		gainClip = _mm_min_ps(gainClip, *((__m128*)effectsGainRecoveryOne)); \
	} \
	\
	/*
	instead of doing the classic a & 0x7FFFFFFF in order to achieve the abs value,
	I decided to do this (which does not require external memory loads)
	maxAbsSample = max(maxAbsSample, max(0 - inLR, inLR)) */ \
	__m128 zero; \
	zero = _mm_xor_ps(zero, zero); \
	maxAbsSample = _mm_max_ps(maxAbsSample, _mm_max_ps(_mm_sub_ps(zero, inLR), inLR)); \
	\
	/*
	the final output is the last band's output (or its next band's input)
	const int iL = (int)inL;
	const int iR = (int)inR; */ \
	__m128i iLR = _mm_cvtps_epi32(inLR); \
	\
	/*
	buffer[0] = (iL >= 32767 ? 32767 : (iL <= -32768 ? -32768 : (short)iL)); \
	buffer[1] = (iR >= 32767 ? 32767 : (iR <= -32768 ? -32768 : (short)iR)); */ \
	iLR = _mm_packs_epi32(iLR, iLR); \
	*((int*)buffer) = _mm_cvtsi128_si32(iLR); \
	\
	buffer += 2;

#define footerX86() \
	if (!effectsGainEnabled) { \
		effectsFramesBeforeRecoveringGain = 0x7FFFFFFF; \
		return; \
	} \
	\
	_mm_store_ps((float*)effectsTemp, maxAbsSample); \
	effectsTemp[2] = 0; \
	effectsTemp[3] = 0; \
	const float maxAbsSampleMono = ((((float*)effectsTemp)[0] > ((float*)effectsTemp)[1]) ? ((float*)effectsTemp)[0] : ((float*)effectsTemp)[1]); \
	float gainClipMono; \
	_mm_store_ss(&gainClipMono, gainClip); \
	if (maxAbsSampleMono > 32768.0f) { \
		const float newGainClip = 33000.0f / maxAbsSampleMono; \
		if (newGainClip < gainClipMono) { \
			effectsGainClip[0] = newGainClip; \
			effectsGainClip[1] = newGainClip; \
		} else { \
			effectsGainClip[0] = gainClipMono; \
			effectsGainClip[1] = gainClipMono; \
		} \
		effectsFramesBeforeRecoveringGain = sampleRate << 2; /* wait some time before starting to recover the gain */ \
	} else if (effectsFramesBeforeRecoveringGain <= 0) { \
		effectsGainClip[0] = gainClipMono; \
		effectsGainClip[1] = gainClipMono; \
		effectsFramesBeforeRecoveringGain = ((gainClipMono >= 1.0f) ? 0x7FFFFFFF : 0); \
	}
	
#define equalizerPlain() \
	/* since this is a cascade filter, band0's output is band1's input and so on.... */ \
	for (int i = 0; i < equalizerMaxBandCount; i++, samples += 8) { \
		/*
		y(n) = b0.x(n) + b1.x(n-1) + b2.x(n-2) - a1.y(n-1) - a2.y(n-2)
		but, since our a2 was negated and b1 = a1, the formula becomes:
		y(n) = b0.x(n) + b1.(x(n-1) - y(n-1)) + b2.x(n-2) + a2.y(n-2) */ \
		const float b0 = equalizerCoefs[(i << 3)]; \
		const float b1_a1 = equalizerCoefs[(i << 3) + 2]; \
		const float b2 = equalizerCoefs[(i << 3) + 4]; \
		const float a2 = equalizerCoefs[(i << 3) + 6]; \
		\
		const float local_x_n1_L = x_n1_L; \
		const float local_x_n1_R = x_n1_R; \
		\
		const float local_y_n1_L = y_n1_L; \
		const float local_y_n1_R = y_n1_R; \
		\
		const float outL = (b0 * inL) + (b1_a1 * (local_x_n1_L - local_y_n1_L)) + (b2 * x_n2_L) + (a2 * y_n2_L); \
		const float outR = (b0 * inR) + (b1_a1 * (local_x_n1_R - local_y_n1_R)) + (b2 * x_n2_R) + (a2 * y_n2_R); \
		\
		x_n2_L = local_x_n1_L; \
		x_n2_R = local_x_n1_R; \
		y_n2_L = local_y_n1_L; \
		y_n2_R = local_y_n1_R; \
		\
		x_n1_L = inL; \
		x_n1_R = inR; \
		y_n1_L = outL; \
		y_n1_R = outR; \
		\
		inL = outL; \
		inR = outR; \
	}

#define floatToShortPlain() \
	inL *= gainClip; \
	inR *= gainClip; \
	\
	if (effectsFramesBeforeRecoveringGain <= 0) { \
		gainClip *= effectsGainRecoveryPerFrame[0]; \
		if (gainClip > 1.0f) \
			gainClip = 1.0f; \
	} \
	\
	const float tmpAbsL = abs(inL); \
	if (maxAbsSample < tmpAbsL) \
		maxAbsSample = tmpAbsL; \
	const float tmpAbsR = abs(inR); \
	if (maxAbsSample < tmpAbsR) \
		maxAbsSample = tmpAbsR; \
	\
	/* the final output is the last band's output (or its next band's input) */ \
	const int iL = (int)inL; \
	const int iR = (int)inR; \
	buffer[0] = (iL >= 32767 ? 32767 : (iL <= -32768 ? -32768 : (short)iL)); \
	buffer[1] = (iR >= 32767 ? 32767 : (iR <= -32768 ? -32768 : (short)iR)); \
	\
	buffer += 2;

#define footerPlain() \
	if (!effectsGainEnabled) { \
		effectsFramesBeforeRecoveringGain = 0x7FFFFFFF; \
		return; \
	} \
	\
	if (maxAbsSample > 32768.0f) { \
		const float newGainClip = 33000.0f / maxAbsSample; \
		if (newGainClip < gainClip) { \
			effectsGainClip[0] = newGainClip; \
			effectsGainClip[1] = newGainClip; \
		} else { \
			effectsGainClip[0] = gainClip; \
			effectsGainClip[1] = gainClip; \
		} \
		effectsFramesBeforeRecoveringGain = sampleRate << 2; /* wait some time before starting to recover the gain */ \
	} else if (effectsFramesBeforeRecoveringGain <= 0) { \
		effectsGainClip[0] = gainClip; \
		effectsGainClip[1] = gainClip; \
		effectsFramesBeforeRecoveringGain = ((gainClip >= 1.0f) ? 0x7FFFFFFF : 0); \
	}

#define equalizerNeon() \
	/* since this is a cascade filter, band0's output is band1's input and so on.... */ \
	for (int i = 0; i < equalizerMaxBandCount; i++, samples += 8) { \
		/*
		y(n) = b0.x(n) + b1.x(n-1) + b2.x(n-2) - a1.y(n-1) - a2.y(n-2)
		but, since our a2 was negated and b1 = a1, the formula becomes:
		y(n) = b0.x(n) + b1.(x(n-1) - y(n-1)) + b2.x(n-2) + a2.y(n-2) */ \
		\
		/* b0_b1 = { b0, b0, b1, b1 } */ \
		float32x4_t b0_b1 = vld1q_f32(equalizerCoefs + (i << 3)); \
		/* b2_a2 = { b2, b2, -a2, -a2 } */ \
		float32x4_t b2_a2 = vld1q_f32(equalizerCoefs + (i << 3) + 4); \
		\
		/* local = { x_n1_L, x_n1_R, y_n1_L, y_n1_R } */ \
		float32x4_t local = vld1q_f32(samples); \
		\
		/* inLR4 = { L, R, x_n1_L - y_n1_L, x_n1_R - y_n1_R } */ \
		float32x4_t inLR4 = vcombine_f32(inLR, vsub_f32(vget_low_f32(local), vget_high_f32(local))); \
		\
		/* tmp2 = { x_n2_L, x_n2_R, y_n2_L, y_n2_R } */ \
		float32x4_t tmp2 = vld1q_f32(samples + 4); \
		\
		/* b0_b1 = { b0 * L, b0 * R, b1 * (x_n1_L - y_n1_L), b1 * (x_n1_R - y_n1_R) } */ \
		b0_b1 = vmulq_f32(b0_b1, inLR4); \
		\
		/* b2_a2 = { b2 * x_n2_L, b2 * x_n2_R, -a2 * y_n2_L, -a2 * y_n2_R } */ \
		b2_a2 = vmulq_f32(b2_a2, tmp2); \
		\
		/* b0_b1 = { (b0 * L) + (b2 * x_n2_L), (b0 * R) + (b2 * x_n2_R), (b1 * (x_n1_L - y_n1_L)) + (-a2 * y_n2_L), (b1 * (x_n1_R - y_n1_R)) + (-a2 * y_n2_R) } */ \
		b0_b1 = vaddq_f32(b0_b1, b2_a2); \
		\
		/*
		x_n2_L = local_x_n1_L;
		x_n2_R = local_x_n1_R;
		y_n2_L = local_y_n1_L;
		y_n2_R = local_y_n1_R; */ \
		vst1q_f32(samples + 4, local); \
		\
		/* outLR = { outL, outR } */ \
		float32x2_t outLR = vadd_f32(vget_low_f32(b0_b1), vget_high_f32(b0_b1)); \
		\
		/*
		x_n1_L = inL;
		x_n1_R = inR;
		y_n1_L = outL;
		y_n1_R = outR; */ \
		vst1q_f32(samples, vcombine_f32(inLR, outLR)); \
		\
		/*
		inL = outL;
		inR = outR; */ \
		inLR = outLR; \
	}

#define floatToShortNeon() \
	/*
	inL *= gainClip;
	inR *= gainClip; */ \
	inLR = vmul_f32(inLR, gainClip); \
	\
	if (effectsFramesBeforeRecoveringGain <= 0) { \
		/*
		gainClip *= effectsGainRecoveryPerFrame[0];
		if (gainClip > 1.0f)
			gainClip = 1.0f; */ \
		gainClip = vmul_f32(gainClip, *((float32x2_t*)effectsGainRecoveryPerFrame)); \
		gainClip = vmin_f32(gainClip, *((float32x2_t*)effectsGainRecoveryOne)); \
	} \
	\
	maxAbsSample = vmax_f32(maxAbsSample, vabs_f32(inLR)); \
	\
	/*
	the final output is the last band's output (or its next band's input)
	const int iL = (int)inL;
	const int iR = (int)inR; */ \
	int32x2_t iLR = vcvt_s32_f32(inLR); \
	\
	/*
	buffer[0] = (iL >= 32767 ? 32767 : (iL <= -32768 ? -32768 : (short)iL));
	buffer[1] = (iR >= 32767 ? 32767 : (iR <= -32768 ? -32768 : (short)iR)); */ \
	int16x4_t iLRshort = vqmovn_s32(vcombine_s32(iLR, iLR)); \
	buffer[0] = vget_lane_s16(iLRshort, 0); \
	buffer[1] = vget_lane_s16(iLRshort, 1); \
	\
	buffer += 2;

#define footerNeon() \
	if (!effectsGainEnabled) { \
		effectsFramesBeforeRecoveringGain = 0x7FFFFFFF; \
		return; \
	} \
	\
	vst1_f32((float*)effectsTemp, maxAbsSample); \
	const float maxAbsSampleMono = ((((float*)effectsTemp)[0] > ((float*)effectsTemp)[1]) ? ((float*)effectsTemp)[0] : ((float*)effectsTemp)[1]); \
	float gainClipMono; \
	vst1_lane_f32(&gainClipMono, gainClip, 0); \
	if (maxAbsSampleMono > 32768.0f) { \
		const float newGainClip = 33000.0f / maxAbsSampleMono; \
		if (newGainClip < gainClipMono) { \
			effectsGainClip[0] = newGainClip; \
			effectsGainClip[1] = newGainClip; \
		} else { \
			effectsGainClip[0] = gainClipMono; \
			effectsGainClip[1] = gainClipMono; \
		} \
		effectsFramesBeforeRecoveringGain = sampleRate << 2; /* wait some time before starting to recover the gain */ \
	} else if (effectsFramesBeforeRecoveringGain <= 0) { \
		effectsGainClip[0] = gainClipMono; \
		effectsGainClip[1] = gainClipMono; \
		effectsFramesBeforeRecoveringGain = ((gainClipMono >= 1.0f) ? 0x7FFFFFFF : 0); \
	}
