/* DirectSound EAX interface
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#define CONST_VTABLE
#include <stdarg.h>
#include <string.h>

#include "windows.h"
#include "dsound.h"

#include "dsound_private.h"
#include "eax-presets.h"


static inline float minF(float a, float b)
{ return (a <= b) ? a : b; }
static inline float maxF(float a, float b)
{ return (a >= b) ? a : b; }

#define APPLY_DRY_PARAMS 1
#define APPLY_WET_PARAMS 2
static void ApplyFilterParams(DSBuffer *buf, const EAXSOURCEPROPERTIES *props, int apply)
{
    /* The LFRatio properties determine how much the given level applies to low
     * frequencies as well as high frequencies. Technically, given that the
     * obstruction/occlusion/exclusion levels are the absolute level applied to
     * high frequencies (relative to full-scale, according to the EAX 2.0 spec)
     * while the HF filter gains are relative to the low, the HF gains should
     * increase as LFRatio increases.
     *
     * However it seems Creative was either wrong when writing out the spec,
     * or implemented it incorrectly, as the HF filter still applies in full
     * regardless of the LFRatio. So to replicate the hardware behavior, we do
     * the same here.
     */

    if((apply&APPLY_DRY_PARAMS) && buf->filter[0])
    {
        float mb   = props->lDirect   + props->lObstruction*props->flObstructionLFRatio;
        float mbhf = props->lDirectHF + props->lObstruction;
        /* The interaction of ratios is pretty wierd. The typical combination
         * of the two act as a minimal baseline, while the sum minus one is
         * used when larger. This creates a more linear change with the
         * individual ratios as DirectRatio goes beyond 1, but eases down as
         * the two ratios go toward 0.
         */
        mb += maxF(props->flOcclusionLFRatio+props->flOcclusionDirectRatio-1.0f,
                   props->flOcclusionLFRatio*props->flOcclusionDirectRatio) * props->lOcclusion;
        mbhf += props->lOcclusion * props->flOcclusionDirectRatio;

        alFilterf(buf->filter[0], AL_LOWPASS_GAIN, mB_to_gain(minF(mb, buf->filter_mBLimit)));
        alFilterf(buf->filter[0], AL_LOWPASS_GAINHF, mB_to_gain(mbhf));
    }
    if((apply&APPLY_WET_PARAMS) && buf->filter[1])
    {
        float mb   = props->lRoom   + props->lExclusion*props->flExclusionLFRatio;
        float mbhf = props->lRoomHF + props->lExclusion;
        mb += maxF(props->flOcclusionLFRatio+props->flOcclusionRoomRatio-1.0f,
                   props->flOcclusionLFRatio*props->flOcclusionRoomRatio) * props->lOcclusion;
        mbhf += props->lOcclusion * props->flOcclusionRoomRatio;

        alFilterf(buf->filter[1], AL_LOWPASS_GAIN, mB_to_gain(minF(mb, buf->filter_mBLimit)));
        alFilterf(buf->filter[1], AL_LOWPASS_GAINHF, mB_to_gain(mbhf));
    }
    checkALError();
}



/*******************
 * EAX 3 stuff
 ******************/

static EAXOBSTRUCTIONPROPERTIES EAXSourceObstruction(const EAXSOURCEPROPERTIES *props)
{
    EAXOBSTRUCTIONPROPERTIES ret;
    ret.lObstruction = props->lObstruction;
    ret.flObstructionLFRatio = props->flObstructionLFRatio;
    return ret;
}

static EAXOCCLUSIONPROPERTIES EAXSourceOcclusion(const EAXSOURCEPROPERTIES *props)
{
    EAXOCCLUSIONPROPERTIES ret;
    ret.lOcclusion = props->lOcclusion;
    ret.flOcclusionLFRatio = props->flOcclusionLFRatio;
    ret.flOcclusionRoomRatio = props->flOcclusionRoomRatio;
    ret.flOcclusionDirectRatio = props->flOcclusionDirectRatio;
    return ret;
}

static EAXEXCLUSIONPROPERTIES EAXSourceExclusion(const EAXSOURCEPROPERTIES *props)
{
    EAXEXCLUSIONPROPERTIES ret;
    ret.lExclusion = props->lExclusion;
    ret.flExclusionLFRatio = props->flExclusionLFRatio;
    return ret;
}

HRESULT EAX3_Query(DSPrimary *prim, DWORD propid, ULONG *pTypeSupport)
{
    if(prim->effect[0] == 0)
        return E_PROP_ID_UNSUPPORTED;

    switch((propid&~DSPROPERTY_EAX30LISTENER_DEFERRED))
    {
    case DSPROPERTY_EAX30LISTENER_NONE:
    case DSPROPERTY_EAX30LISTENER_ALLPARAMETERS:
    case DSPROPERTY_EAX30LISTENER_ENVIRONMENT:
    case DSPROPERTY_EAX30LISTENER_ENVIRONMENTSIZE:
    case DSPROPERTY_EAX30LISTENER_ENVIRONMENTDIFFUSION:
    case DSPROPERTY_EAX30LISTENER_ROOM:
    case DSPROPERTY_EAX30LISTENER_ROOMHF:
    case DSPROPERTY_EAX30LISTENER_ROOMLF:
    case DSPROPERTY_EAX30LISTENER_DECAYTIME:
    case DSPROPERTY_EAX30LISTENER_DECAYHFRATIO:
    case DSPROPERTY_EAX30LISTENER_DECAYLFRATIO:
    case DSPROPERTY_EAX30LISTENER_REFLECTIONS:
    case DSPROPERTY_EAX30LISTENER_REFLECTIONSDELAY:
    case DSPROPERTY_EAX30LISTENER_REFLECTIONSPAN:
    case DSPROPERTY_EAX30LISTENER_REVERB:
    case DSPROPERTY_EAX30LISTENER_REVERBDELAY:
    case DSPROPERTY_EAX30LISTENER_REVERBPAN:
    case DSPROPERTY_EAX30LISTENER_ECHOTIME:
    case DSPROPERTY_EAX30LISTENER_ECHODEPTH:
    case DSPROPERTY_EAX30LISTENER_MODULATIONTIME:
    case DSPROPERTY_EAX30LISTENER_MODULATIONDEPTH:
    case DSPROPERTY_EAX30LISTENER_AIRABSORPTIONHF:
    case DSPROPERTY_EAX30LISTENER_HFREFERENCE:
    case DSPROPERTY_EAX30LISTENER_LFREFERENCE:
    case DSPROPERTY_EAX30LISTENER_ROOMROLLOFFFACTOR:
    case DSPROPERTY_EAX30LISTENER_FLAGS:
        *pTypeSupport = KSPROPERTY_SUPPORT_GET | KSPROPERTY_SUPPORT_SET;
        return DS_OK;

    default:
        FIXME("Unhandled propid: 0x%08lx\n", propid);
    }
    return E_PROP_ID_UNSUPPORTED;
}

HRESULT EAX3_Set(DSPrimary *prim, DWORD propid, void *pPropData, ULONG cbPropData)
{
    HRESULT hr;

    if(prim->effect[0] == 0)
        return E_PROP_ID_UNSUPPORTED;
    /* Should this be using slot 0 or the primary slot? */
    if(prim->deferred.fxslot[0].effect_type != FXSLOT_EFFECT_REVERB)
    {
        ERR("Trying to set reverb parameters on non-reverb slot\n");
        return DSERR_INVALIDCALL;
    }

    hr = DSERR_INVALIDPARAM;
    switch(propid)
    {
    case DSPROPERTY_EAX30LISTENER_NONE: /* not setting any property, just applying */
        hr = DS_OK;
        break;

    /* TODO: Validate slot effect type. */
    case DSPROPERTY_EAX30LISTENER_ALLPARAMETERS:
        if(cbPropData >= sizeof(EAX30LISTENERPROPERTIES))
        {
            union {
                const void *v;
                const EAX30LISTENERPROPERTIES *props;
            } data = { pPropData };
            TRACE("Parameters:\n\tEnvironment: %lu\n\tEnvSize: %f\n\tEnvDiffusion: %f\n\t"
                "Room: %ld\n\tRoom HF: %ld\n\tRoom LF: %ld\n\tDecay Time: %f\n\t"
                "Decay HF Ratio: %f\n\tDecay LF Ratio: %f\n\tReflections: %ld\n\t"
                "Reflections Delay: %f\n\tReflections Pan: { %f, %f, %f }\n\tReverb: %ld\n\t"
                "Reverb Delay: %f\n\tReverb Pan: { %f, %f, %f }\n\tEcho Time: %f\n\t"
                "Echo Depth: %f\n\tMod Time: %f\n\tMod Depth: %f\n\tAir Absorption: %f\n\t"
                "HF Reference: %f\n\tLF Reference: %f\n\tRoom Rolloff: %f\n\tFlags: 0x%02lx\n",
                data.props->dwEnvironment, data.props->flEnvironmentSize,
                data.props->flEnvironmentDiffusion, data.props->lRoom, data.props->lRoomHF,
                data.props->lRoomLF, data.props->flDecayTime, data.props->flDecayHFRatio,
                data.props->flDecayLFRatio, data.props->lReflections,
                data.props->flReflectionsDelay, data.props->vReflectionsPan.x,
                data.props->vReflectionsPan.y, data.props->vReflectionsPan.z, data.props->lReverb,
                data.props->flReverbDelay, data.props->vReverbPan.x, data.props->vReverbPan.y,
                data.props->vReverbPan.z, data.props->flEchoTime, data.props->flEchoDepth,
                data.props->flModulationTime, data.props->flModulationDepth,
                data.props->flAirAbsorptionHF, data.props->flHFReference,
                data.props->flLFReference, data.props->flRoomRolloffFactor, data.props->dwFlags
            );

            ApplyReverbParams(prim->effect[0], data.props);
            FXSLOT_SET_DIRTY(prim->dirty.bit, 0, FXSLOT_EFFECT_BIT);
            hr = DS_OK;
        }
        break;

    case DSPROPERTY_EAX30LISTENER_ENVIRONMENT:
        if(cbPropData >= sizeof(DWORD))
        {
            union { const void *v; const DWORD *dw; } data = { pPropData };
            TRACE("Environment: %lu\n", *data.dw);
            if(*data.dw < EAX_ENVIRONMENT_UNDEFINED)
            {
                prim->deferred.fxslot[0].fx.reverb = EnvironmentDefaults[*data.dw];
                ApplyReverbParams(prim->effect[0], &EnvironmentDefaults[*data.dw]);
                FXSLOT_SET_DIRTY(prim->dirty.bit, 0, FXSLOT_EFFECT_BIT);
                hr = DS_OK;
            }
        }
        break;

    case DSPROPERTY_EAX30LISTENER_ENVIRONMENTSIZE:
        if(cbPropData >= sizeof(float))
        {
            union { const void *v; const float *fl; } data = { pPropData };
            TRACE("Environment Size: %f\n", *data.fl);

            RescaleEnvSize(&prim->deferred.fxslot[0].fx.reverb, clampF(*data.fl, 1.0f, 100.0f));
            ApplyReverbParams(prim->effect[0], &prim->deferred.fxslot[0].fx.reverb);

            FXSLOT_SET_DIRTY(prim->dirty.bit, 0, FXSLOT_EFFECT_BIT);
            hr = DS_OK;
        }
        break;
    case DSPROPERTY_EAX30LISTENER_ENVIRONMENTDIFFUSION:
        if(cbPropData >= sizeof(float))
        {
            union { const void *v; const float *fl; } data = { pPropData };
            TRACE("Environment Diffusion: %f\n", *data.fl);

            prim->deferred.fxslot[0].fx.reverb.flEnvironmentDiffusion = *data.fl;
            alEffectf(prim->effect[0], AL_EAXREVERB_DIFFUSION,
                      prim->deferred.fxslot[0].fx.reverb.flEnvironmentDiffusion);
            checkALError();

            FXSLOT_SET_DIRTY(prim->dirty.bit, 0, FXSLOT_EFFECT_BIT);
            hr = DS_OK;
        }
        break;

    case DSPROPERTY_EAX30LISTENER_ROOM:
        if(cbPropData >= sizeof(long))
        {
            union { const void *v; const long *l; } data = { pPropData };
            TRACE("Room: %ld\n", *data.l);

            prim->deferred.fxslot[0].fx.reverb.lRoom = *data.l;
            alEffectf(prim->effect[0], AL_EAXREVERB_GAIN,
                      mB_to_gain(prim->deferred.fxslot[0].fx.reverb.lRoom));
            checkALError();

            FXSLOT_SET_DIRTY(prim->dirty.bit, 0, FXSLOT_EFFECT_BIT);
            hr = DS_OK;
        }
        break;
    case DSPROPERTY_EAX30LISTENER_ROOMHF:
        if(cbPropData >= sizeof(long))
        {
            union { const void *v; const long *l; } data = { pPropData };
            TRACE("Room HF: %ld\n", *data.l);

            prim->deferred.fxslot[0].fx.reverb.lRoomHF = *data.l;
            alEffectf(prim->effect[0], AL_EAXREVERB_GAINHF,
                      mB_to_gain(prim->deferred.fxslot[0].fx.reverb.lRoomHF));
            checkALError();

            FXSLOT_SET_DIRTY(prim->dirty.bit, 0, FXSLOT_EFFECT_BIT);
            hr = DS_OK;
        }
        break;
    case DSPROPERTY_EAX30LISTENER_ROOMLF:
        if(cbPropData >= sizeof(long))
        {
            union { const void *v; const long *l; } data = { pPropData };
            TRACE("Room LF: %ld\n", *data.l);

            prim->deferred.fxslot[0].fx.reverb.lRoomLF = *data.l;
            alEffectf(prim->effect[0], AL_EAXREVERB_GAINLF,
                      mB_to_gain(prim->deferred.fxslot[0].fx.reverb.lRoomLF));
            checkALError();

            FXSLOT_SET_DIRTY(prim->dirty.bit, 0, FXSLOT_EFFECT_BIT);
            hr = DS_OK;
        }
        break;

    case DSPROPERTY_EAX30LISTENER_DECAYTIME:
        if(cbPropData >= sizeof(float))
        {
            union { const void *v; const float *fl; } data = { pPropData };
            TRACE("Decay Time: %f\n", *data.fl);

            prim->deferred.fxslot[0].fx.reverb.flDecayTime = *data.fl;
            alEffectf(prim->effect[0], AL_EAXREVERB_DECAY_TIME,
                      prim->deferred.fxslot[0].fx.reverb.flDecayTime);
            checkALError();

            FXSLOT_SET_DIRTY(prim->dirty.bit, 0, FXSLOT_EFFECT_BIT);
            hr = DS_OK;
        }
        break;
    case DSPROPERTY_EAX30LISTENER_DECAYHFRATIO:
        if(cbPropData >= sizeof(float))
        {
            union { const void *v; const float *fl; } data = { pPropData };
            TRACE("Decay HF Ratio: %f\n", *data.fl);

            prim->deferred.fxslot[0].fx.reverb.flDecayHFRatio = *data.fl;
            alEffectf(prim->effect[0], AL_EAXREVERB_DECAY_HFRATIO,
                      prim->deferred.fxslot[0].fx.reverb.flDecayHFRatio);
            checkALError();

            FXSLOT_SET_DIRTY(prim->dirty.bit, 0, FXSLOT_EFFECT_BIT);
            hr = DS_OK;
        }
        break;
    case DSPROPERTY_EAX30LISTENER_DECAYLFRATIO:
        if(cbPropData >= sizeof(float))
        {
            union { const void *v; const float *fl; } data = { pPropData };
            TRACE("Decay LF Ratio: %f\n", *data.fl);

            prim->deferred.fxslot[0].fx.reverb.flDecayLFRatio = *data.fl;
            alEffectf(prim->effect[0], AL_EAXREVERB_DECAY_LFRATIO,
                      prim->deferred.fxslot[0].fx.reverb.flDecayLFRatio);
            checkALError();

            FXSLOT_SET_DIRTY(prim->dirty.bit, 0, FXSLOT_EFFECT_BIT);
            hr = DS_OK;
        }
        break;

    case DSPROPERTY_EAX30LISTENER_REFLECTIONS:
        if(cbPropData >= sizeof(long))
        {
            union { const void *v; const long *l; } data = { pPropData };
            TRACE("Reflections: %ld\n", *data.l);

            prim->deferred.fxslot[0].fx.reverb.lReflections = *data.l;
            alEffectf(prim->effect[0], AL_EAXREVERB_REFLECTIONS_GAIN,
                      mB_to_gain(prim->deferred.fxslot[0].fx.reverb.lReflections));
            checkALError();

            FXSLOT_SET_DIRTY(prim->dirty.bit, 0, FXSLOT_EFFECT_BIT);
            hr = DS_OK;
        }
        break;
    case DSPROPERTY_EAX30LISTENER_REFLECTIONSDELAY:
        if(cbPropData >= sizeof(float))
        {
            union { const void *v; const float *fl; } data = { pPropData };
            TRACE("Reflections Delay: %f\n", *data.fl);

            prim->deferred.fxslot[0].fx.reverb.flReflectionsDelay = *data.fl;
            alEffectf(prim->effect[0], AL_EAXREVERB_REFLECTIONS_DELAY,
                      prim->deferred.fxslot[0].fx.reverb.flReflectionsDelay);
            checkALError();

            FXSLOT_SET_DIRTY(prim->dirty.bit, 0, FXSLOT_EFFECT_BIT);
            hr = DS_OK;
        }
        break;
    case DSPROPERTY_EAX30LISTENER_REFLECTIONSPAN:
        if(cbPropData >= sizeof(EAXVECTOR))
        {
            union { const void *v; const EAXVECTOR *vec; } data = { pPropData };
            TRACE("Reflections Pan: { %f, %f, %f }\n", data.vec->x, data.vec->y, data.vec->z);

            prim->deferred.fxslot[0].fx.reverb.vReflectionsPan = *data.vec;
            alEffectfv(prim->effect[0], AL_EAXREVERB_REFLECTIONS_PAN,
                       &prim->deferred.fxslot[0].fx.reverb.vReflectionsPan.x);
            checkALError();

            FXSLOT_SET_DIRTY(prim->dirty.bit, 0, FXSLOT_EFFECT_BIT);
            hr = DS_OK;
        }
        break;

    case DSPROPERTY_EAX30LISTENER_REVERB:
        if(cbPropData >= sizeof(long))
        {
            union { const void *v; const long *l; } data = { pPropData };
            TRACE("Reverb: %ld\n", *data.l);

            prim->deferred.fxslot[0].fx.reverb.lReverb = *data.l;
            alEffectf(prim->effect[0], AL_EAXREVERB_LATE_REVERB_GAIN,
                      mB_to_gain(prim->deferred.fxslot[0].fx.reverb.lReverb));
            checkALError();

            FXSLOT_SET_DIRTY(prim->dirty.bit, 0, FXSLOT_EFFECT_BIT);
            hr = DS_OK;
        }
        break;
    case DSPROPERTY_EAX30LISTENER_REVERBDELAY:
        if(cbPropData >= sizeof(float))
        {
            union { const void *v; const float *fl; } data = { pPropData };
            TRACE("Reverb Delay: %f\n", *data.fl);

            prim->deferred.fxslot[0].fx.reverb.flReverbDelay = *data.fl;
            alEffectf(prim->effect[0], AL_EAXREVERB_LATE_REVERB_DELAY,
                      prim->deferred.fxslot[0].fx.reverb.flReverbDelay);
            checkALError();

            FXSLOT_SET_DIRTY(prim->dirty.bit, 0, FXSLOT_EFFECT_BIT);
            hr = DS_OK;
        }
        break;
    case DSPROPERTY_EAX30LISTENER_REVERBPAN:
        if(cbPropData >= sizeof(EAXVECTOR))
        {
            union { const void *v; const EAXVECTOR *vec; } data = { pPropData };
            TRACE("Reverb Pan: { %f, %f, %f }\n", data.vec->x, data.vec->y, data.vec->z);

            prim->deferred.fxslot[0].fx.reverb.vReverbPan = *data.vec;
            alEffectfv(prim->effect[0], AL_EAXREVERB_LATE_REVERB_PAN,
                       &prim->deferred.fxslot[0].fx.reverb.vReverbPan.x);
            checkALError();

            FXSLOT_SET_DIRTY(prim->dirty.bit, 0, FXSLOT_EFFECT_BIT);
            hr = DS_OK;
        }
        break;

    case DSPROPERTY_EAX30LISTENER_ECHOTIME:
        if(cbPropData >= sizeof(float))
        {
            union { const void *v; const float *fl; } data = { pPropData };
            TRACE("Echo Time: %f\n", *data.fl);

            prim->deferred.fxslot[0].fx.reverb.flEchoTime = *data.fl;
            alEffectf(prim->effect[0], AL_EAXREVERB_ECHO_TIME,
                      prim->deferred.fxslot[0].fx.reverb.flEchoTime);
            checkALError();

            FXSLOT_SET_DIRTY(prim->dirty.bit, 0, FXSLOT_EFFECT_BIT);
            hr = DS_OK;
        }
        break;
    case DSPROPERTY_EAX30LISTENER_ECHODEPTH:
        if(cbPropData >= sizeof(float))
        {
            union { const void *v; const float *fl; } data = { pPropData };
            TRACE("Echo Depth: %f\n", *data.fl);

            prim->deferred.fxslot[0].fx.reverb.flEchoDepth = *data.fl;
            alEffectf(prim->effect[0], AL_EAXREVERB_ECHO_DEPTH,
                      prim->deferred.fxslot[0].fx.reverb.flEchoDepth);
            checkALError();

            FXSLOT_SET_DIRTY(prim->dirty.bit, 0, FXSLOT_EFFECT_BIT);
            hr = DS_OK;
        }
        break;

    case DSPROPERTY_EAX30LISTENER_MODULATIONTIME:
        if(cbPropData >= sizeof(float))
        {
            union { const void *v; const float *fl; } data = { pPropData };
            TRACE("Modulation Time: %f\n", *data.fl);

            prim->deferred.fxslot[0].fx.reverb.flModulationTime = *data.fl;
            alEffectf(prim->effect[0], AL_EAXREVERB_MODULATION_TIME,
                      prim->deferred.fxslot[0].fx.reverb.flModulationTime);
            checkALError();

            FXSLOT_SET_DIRTY(prim->dirty.bit, 0, FXSLOT_EFFECT_BIT);
            hr = DS_OK;
        }
        break;
    case DSPROPERTY_EAX30LISTENER_MODULATIONDEPTH:
        if(cbPropData >= sizeof(float))
        {
            union { const void *v; const float *fl; } data = { pPropData };
            TRACE("Modulation Depth: %f\n", *data.fl);

            prim->deferred.fxslot[0].fx.reverb.flModulationDepth = *data.fl;
            alEffectf(prim->effect[0], AL_EAXREVERB_MODULATION_DEPTH,
                      prim->deferred.fxslot[0].fx.reverb.flModulationDepth);
            checkALError();

            FXSLOT_SET_DIRTY(prim->dirty.bit, 0, FXSLOT_EFFECT_BIT);
            hr = DS_OK;
        }
        break;

    case DSPROPERTY_EAX30LISTENER_AIRABSORPTIONHF:
        if(cbPropData >= sizeof(float))
        {
            union { const void *v; const float *fl; } data = { pPropData };
            TRACE("Air Absorption HF: %f\n", *data.fl);

            prim->deferred.fxslot[0].fx.reverb.flAirAbsorptionHF = *data.fl;
            alEffectf(prim->effect[0], AL_EAXREVERB_AIR_ABSORPTION_GAINHF,
                      mB_to_gain(prim->deferred.fxslot[0].fx.reverb.flAirAbsorptionHF));
            checkALError();

            FXSLOT_SET_DIRTY(prim->dirty.bit, 0, FXSLOT_EFFECT_BIT);
            hr = DS_OK;
        }
        break;

    case DSPROPERTY_EAX30LISTENER_HFREFERENCE:
        if(cbPropData >= sizeof(float))
        {
            union { const void *v; const float *fl; } data = { pPropData };
            TRACE("HF Reference: %f\n", *data.fl);

            prim->deferred.fxslot[0].fx.reverb.flHFReference = *data.fl;
            alEffectf(prim->effect[0], AL_EAXREVERB_HFREFERENCE,
                      prim->deferred.fxslot[0].fx.reverb.flHFReference);
            checkALError();

            FXSLOT_SET_DIRTY(prim->dirty.bit, 0, FXSLOT_EFFECT_BIT);
            hr = DS_OK;
        }
        break;
    case DSPROPERTY_EAX30LISTENER_LFREFERENCE:
        if(cbPropData >= sizeof(float))
        {
            union { const void *v; const float *fl; } data = { pPropData };
            TRACE("LF Reference: %f\n", *data.fl);

            prim->deferred.fxslot[0].fx.reverb.flLFReference = *data.fl;
            alEffectf(prim->effect[0], AL_EAXREVERB_LFREFERENCE,
                      prim->deferred.fxslot[0].fx.reverb.flLFReference);
            checkALError();

            FXSLOT_SET_DIRTY(prim->dirty.bit, 0, FXSLOT_EFFECT_BIT);
            hr = DS_OK;
        }
        break;

    case DSPROPERTY_EAX30LISTENER_ROOMROLLOFFFACTOR:
        if(cbPropData >= sizeof(float))
        {
            union { const void *v; const float *fl; } data = { pPropData };
            TRACE("Room Rolloff Factor: %f\n", *data.fl);

            prim->deferred.fxslot[0].fx.reverb.flRoomRolloffFactor = *data.fl;
            alEffectf(prim->effect[0], AL_EAXREVERB_ROOM_ROLLOFF_FACTOR,
                      prim->deferred.fxslot[0].fx.reverb.flRoomRolloffFactor);
            checkALError();

            FXSLOT_SET_DIRTY(prim->dirty.bit, 0, FXSLOT_EFFECT_BIT);
            hr = DS_OK;
        }
        break;

    case DSPROPERTY_EAX30LISTENER_FLAGS:
        if(cbPropData >= sizeof(DWORD))
        {
            union { const void *v; const DWORD *dw; } data = { pPropData };
            TRACE("Flags: %lu\n", *data.dw);

            prim->deferred.fxslot[0].fx.reverb.dwFlags = *data.dw;
            alEffecti(prim->effect[0], AL_EAXREVERB_DECAY_HFLIMIT,
                      (prim->deferred.fxslot[0].fx.reverb.dwFlags&EAX30LISTENERFLAGS_DECAYHFLIMIT) ?
                      AL_TRUE : AL_FALSE);
            checkALError();

            FXSLOT_SET_DIRTY(prim->dirty.bit, 0, FXSLOT_EFFECT_BIT);
            hr = DS_OK;
        }
        break;

    default:
        hr = E_PROP_ID_UNSUPPORTED;
        FIXME("Unhandled propid: 0x%08lx\n", propid);
        break;
    }

    return hr;
}

HRESULT EAX3_Get(DSPrimary *prim, DWORD propid, void *pPropData, ULONG cbPropData, ULONG *pcbReturned)
{
    HRESULT hr;

    if(prim->effect[0] == 0)
        return E_PROP_ID_UNSUPPORTED;
    if(prim->deferred.fxslot[0].effect_type != FXSLOT_EFFECT_REVERB)
    {
        ERR("Trying to get reverb parameters on non-reverb slot\n");
        return DSERR_INVALIDCALL;
    }

#define GET_PROP(src, T) do {                              \
    if(cbPropData >= sizeof(T))                            \
    {                                                      \
        union { void *v; T *props; } data = { pPropData }; \
        *data.props = src;                                 \
        *pcbReturned = sizeof(T);                          \
        hr = DS_OK;                                        \
    }                                                      \
} while(0)
    hr = DSERR_INVALIDPARAM;
    switch(propid)
    {
    case DSPROPERTY_EAX30LISTENER_NONE:
        *pcbReturned = 0;
        hr = DS_OK;
        break;

    case DSPROPERTY_EAX30LISTENER_ALLPARAMETERS:
        GET_PROP(prim->deferred.fxslot[0].fx.reverb, EAX30LISTENERPROPERTIES);
        break;

    case DSPROPERTY_EAX30LISTENER_ENVIRONMENT:
        GET_PROP(prim->deferred.fxslot[0].fx.reverb.dwEnvironment, DWORD);
        break;

    case DSPROPERTY_EAX30LISTENER_ENVIRONMENTSIZE:
        GET_PROP(prim->deferred.fxslot[0].fx.reverb.flEnvironmentSize, float);
        break;
    case DSPROPERTY_EAX30LISTENER_ENVIRONMENTDIFFUSION:
        GET_PROP(prim->deferred.fxslot[0].fx.reverb.flEnvironmentDiffusion, float);
        break;

    case DSPROPERTY_EAX30LISTENER_ROOM:
        GET_PROP(prim->deferred.fxslot[0].fx.reverb.lRoom, long);
        break;
    case DSPROPERTY_EAX30LISTENER_ROOMHF:
        GET_PROP(prim->deferred.fxslot[0].fx.reverb.lRoomHF, long);
        break;
    case DSPROPERTY_EAX30LISTENER_ROOMLF:
        GET_PROP(prim->deferred.fxslot[0].fx.reverb.lRoomLF, long);
        break;

    case DSPROPERTY_EAX30LISTENER_DECAYTIME:
        GET_PROP(prim->deferred.fxslot[0].fx.reverb.flDecayTime, float);
        break;
    case DSPROPERTY_EAX30LISTENER_DECAYHFRATIO:
        GET_PROP(prim->deferred.fxslot[0].fx.reverb.flDecayHFRatio, float);
        break;
    case DSPROPERTY_EAX30LISTENER_DECAYLFRATIO:
        GET_PROP(prim->deferred.fxslot[0].fx.reverb.flDecayLFRatio, float);
        break;

    case DSPROPERTY_EAX30LISTENER_REFLECTIONS:
        GET_PROP(prim->deferred.fxslot[0].fx.reverb.lReflections, long);
        break;
    case DSPROPERTY_EAX30LISTENER_REFLECTIONSDELAY:
        GET_PROP(prim->deferred.fxslot[0].fx.reverb.flReflectionsDelay, float);
        break;
    case DSPROPERTY_EAX30LISTENER_REFLECTIONSPAN:
        GET_PROP(prim->deferred.fxslot[0].fx.reverb.vReflectionsPan, EAXVECTOR);
        break;

    case DSPROPERTY_EAX30LISTENER_REVERB:
        GET_PROP(prim->deferred.fxslot[0].fx.reverb.lReverb, long);
        break;
    case DSPROPERTY_EAX30LISTENER_REVERBDELAY:
        GET_PROP(prim->deferred.fxslot[0].fx.reverb.flReverbDelay, float);
        break;
    case DSPROPERTY_EAX30LISTENER_REVERBPAN:
        GET_PROP(prim->deferred.fxslot[0].fx.reverb.vReverbPan, EAXVECTOR);
        break;

    case DSPROPERTY_EAX30LISTENER_ECHOTIME:
        GET_PROP(prim->deferred.fxslot[0].fx.reverb.flEchoTime, float);
        break;
    case DSPROPERTY_EAX30LISTENER_ECHODEPTH:
        GET_PROP(prim->deferred.fxslot[0].fx.reverb.flEchoDepth, float);
        break;

    case DSPROPERTY_EAX30LISTENER_MODULATIONTIME:
        GET_PROP(prim->deferred.fxslot[0].fx.reverb.flModulationTime, float);
        break;
    case DSPROPERTY_EAX30LISTENER_MODULATIONDEPTH:
        GET_PROP(prim->deferred.fxslot[0].fx.reverb.flModulationDepth, float);
        break;

    case DSPROPERTY_EAX30LISTENER_AIRABSORPTIONHF:
        GET_PROP(prim->deferred.fxslot[0].fx.reverb.flAirAbsorptionHF, float);
        break;

    case DSPROPERTY_EAX30LISTENER_HFREFERENCE:
        GET_PROP(prim->deferred.fxslot[0].fx.reverb.flHFReference, float);
        break;
    case DSPROPERTY_EAX30LISTENER_LFREFERENCE:
        GET_PROP(prim->deferred.fxslot[0].fx.reverb.flLFReference, float);
        break;

    case DSPROPERTY_EAX30LISTENER_ROOMROLLOFFFACTOR:
        GET_PROP(prim->deferred.fxslot[0].fx.reverb.flRoomRolloffFactor, float);
        break;

    case DSPROPERTY_EAX30LISTENER_FLAGS:
        GET_PROP(prim->deferred.fxslot[0].fx.reverb.dwFlags, DWORD);
        break;

    default:
        hr = E_PROP_ID_UNSUPPORTED;
        FIXME("Unhandled propid: 0x%08lx\n", propid);
        break;
    }
#undef GET_PROP

    return hr;
}


HRESULT EAX3Buffer_Query(DSBuffer *buf, DWORD propid, ULONG *pTypeSupport)
{
    (void)buf;

    switch((propid&~DSPROPERTY_EAX30BUFFER_DEFERRED))
    {
    case DSPROPERTY_EAX30BUFFER_NONE:
    case DSPROPERTY_EAX30BUFFER_ALLPARAMETERS:
    case DSPROPERTY_EAX30BUFFER_OBSTRUCTIONPARAMETERS:
    case DSPROPERTY_EAX30BUFFER_OCCLUSIONPARAMETERS:
    case DSPROPERTY_EAX30BUFFER_EXCLUSIONPARAMETERS:
    case DSPROPERTY_EAX30BUFFER_DIRECT:
    case DSPROPERTY_EAX30BUFFER_DIRECTHF:
    case DSPROPERTY_EAX30BUFFER_ROOM:
    case DSPROPERTY_EAX30BUFFER_ROOMHF:
    case DSPROPERTY_EAX30BUFFER_OBSTRUCTION:
    case DSPROPERTY_EAX30BUFFER_OBSTRUCTIONLFRATIO:
    case DSPROPERTY_EAX30BUFFER_OCCLUSION:
    case DSPROPERTY_EAX30BUFFER_OCCLUSIONLFRATIO:
    case DSPROPERTY_EAX30BUFFER_OCCLUSIONROOMRATIO:
    case DSPROPERTY_EAX30BUFFER_OCCLUSIONDIRECTRATIO:
    case DSPROPERTY_EAX30BUFFER_EXCLUSION:
    case DSPROPERTY_EAX30BUFFER_EXCLUSIONLFRATIO:
    case DSPROPERTY_EAX30BUFFER_OUTSIDEVOLUMEHF:
    case DSPROPERTY_EAX30BUFFER_DOPPLERFACTOR:
    case DSPROPERTY_EAX30BUFFER_ROLLOFFFACTOR:
    case DSPROPERTY_EAX30BUFFER_ROOMROLLOFFFACTOR:
    case DSPROPERTY_EAX30BUFFER_AIRABSORPTIONFACTOR:
    case DSPROPERTY_EAX30BUFFER_FLAGS:
        *pTypeSupport = KSPROPERTY_SUPPORT_GET | KSPROPERTY_SUPPORT_SET;
        return DS_OK;

    default:
        FIXME("Unhandled propid: 0x%08lx\n", propid);
    }
    return E_PROP_ID_UNSUPPORTED;
}


HRESULT EAX3Buffer_Set(DSBuffer *buf, DWORD propid, void *pPropData, ULONG cbPropData)
{
    HRESULT hr;

    hr = DSERR_INVALIDPARAM;
    switch(propid)
    {
    case DSPROPERTY_EAX30BUFFER_NONE: /* not setting any property, just applying */
        hr = DS_OK;
        break;

    case DSPROPERTY_EAX30BUFFER_ALLPARAMETERS:
        if(cbPropData >= sizeof(EAX30BUFFERPROPERTIES))
        {
            union {
                const void *v;
                const EAX30BUFFERPROPERTIES *props;
            } data = { pPropData };
            TRACE("Parameters:\n\tDirect: %ld\n\tDirect HF: %ld\n\tRoom: %ld\n\tRoom HF: %ld\n\t"
                "Obstruction: %ld\n\tObstruction LF Ratio: %f\n\tOcclusion: %ld\n\t"
                "Occlusion LF Ratio: %f\n\tOcclusion Room Ratio: %f\n\t"
                "Occlusion Direct Ratio: %f\n\tExclusion: %ld\n\tExclusion LF Ratio: %f\n\t"
                "Outside Volume HF: %ld\n\tDoppler Factor: %f\n\tRolloff Factor: %f\n\t"
                "Room Rolloff Factor: %f\n\tAir Absorb Factor: %f\n\tFlags: 0x%02lx\n",
                data.props->lDirect, data.props->lDirectHF, data.props->lRoom, data.props->lRoomHF,
                data.props->lObstruction, data.props->flObstructionLFRatio, data.props->lOcclusion,
                data.props->flOcclusionLFRatio, data.props->flOcclusionRoomRatio,
                data.props->flOcclusionDirectRatio, data.props->lExclusion,
                data.props->flExclusionLFRatio, data.props->lOutsideVolumeHF,
                data.props->flDopplerFactor, data.props->flRolloffFactor,
                data.props->flRoomRolloffFactor, data.props->flAirAbsorptionFactor,
                data.props->dwFlags
            );

            buf->deferred.eax = *data.props;
            ApplyFilterParams(buf, data.props, APPLY_DRY_PARAMS|APPLY_WET_PARAMS);

            buf->dirty.bit.dry_filter = 1;
            buf->dirty.bit.send0_filter = 1;
            buf->dirty.bit.doppler = 1;
            buf->dirty.bit.rolloff = 1;
            buf->dirty.bit.room_rolloff = 1;
            buf->dirty.bit.cone_outsidevolumehf = 1;
            buf->dirty.bit.air_absorb = 1;
            buf->dirty.bit.flags = 1;
            hr = DS_OK;
        }
        break;
    case DSPROPERTY_EAX30BUFFER_OBSTRUCTIONPARAMETERS:
        if(cbPropData >= sizeof(EAXOBSTRUCTIONPROPERTIES))
        {
            union {
                const void *v;
                const EAXOBSTRUCTIONPROPERTIES *props;
            } data = { pPropData };
            TRACE("Parameters:\n\tObstruction: %ld\n\tObstruction LF Ratio: %f\n",
                  data.props->lObstruction, data.props->flObstructionLFRatio);

            buf->deferred.eax.lObstruction = data.props->lObstruction;
            buf->deferred.eax.flObstructionLFRatio = data.props->flObstructionLFRatio;
            ApplyFilterParams(buf, &buf->deferred.eax, APPLY_DRY_PARAMS);

            buf->dirty.bit.dry_filter = 1;
            hr = DS_OK;
        }
        break;
    case DSPROPERTY_EAX30BUFFER_OCCLUSIONPARAMETERS:
        if(cbPropData >= sizeof(EAXOCCLUSIONPROPERTIES))
        {
            union {
                const void *v;
                const EAXOCCLUSIONPROPERTIES *props;
            } data = { pPropData };
            TRACE("Parameters:\n\tOcclusion: %ld\n\tOcclusion LF Ratio: %f\n\t"
                "Occlusion Room Ratio: %f\n\tOcclusion Direct Ratio: %f\n",
                data.props->lOcclusion, data.props->flOcclusionLFRatio,
                data.props->flOcclusionRoomRatio, data.props->flOcclusionDirectRatio
            );

            buf->deferred.eax.lOcclusion = data.props->lOcclusion;
            buf->deferred.eax.flOcclusionLFRatio = data.props->flOcclusionLFRatio;
            buf->deferred.eax.flOcclusionRoomRatio = data.props->flOcclusionRoomRatio;
            buf->deferred.eax.flOcclusionDirectRatio = data.props->flOcclusionDirectRatio;
            ApplyFilterParams(buf, &buf->deferred.eax, APPLY_DRY_PARAMS|APPLY_WET_PARAMS);

            buf->dirty.bit.dry_filter = 1;
            buf->dirty.bit.send0_filter = 1;
            hr = DS_OK;
        }
        break;
    case DSPROPERTY_EAX30BUFFER_EXCLUSIONPARAMETERS:
        if(cbPropData >= sizeof(EAXEXCLUSIONPROPERTIES))
        {
            union {
                const void *v;
                const EAXEXCLUSIONPROPERTIES *props;
            } data = { pPropData };
            TRACE("Parameters:\n\tExclusion: %ld\n\tExclusion LF Ratio: %f\n",
                  data.props->lExclusion, data.props->flExclusionLFRatio);

            buf->deferred.eax.lExclusion = data.props->lExclusion;
            buf->deferred.eax.flExclusionLFRatio = data.props->flExclusionLFRatio;
            ApplyFilterParams(buf, &buf->deferred.eax, APPLY_WET_PARAMS);

            buf->dirty.bit.send0_filter = 1;
            hr = DS_OK;
        }
        break;

    case DSPROPERTY_EAX30BUFFER_DIRECT:
        if(cbPropData >= sizeof(long))
        {
            union { const void *v; const long *l; } data = { pPropData };
            TRACE("Direct: %ld\n", *data.l);

            buf->deferred.eax.lDirect = *data.l;
            ApplyFilterParams(buf, &buf->deferred.eax, APPLY_DRY_PARAMS);

            buf->dirty.bit.dry_filter = 1;
            hr = DS_OK;
        }
        break;
    case DSPROPERTY_EAX30BUFFER_DIRECTHF:
        if(cbPropData >= sizeof(long))
        {
            union { const void *v; const long *l; } data = { pPropData };
            TRACE("Direct HF: %ld\n", *data.l);

            buf->deferred.eax.lDirectHF = *data.l;
            ApplyFilterParams(buf, &buf->deferred.eax, APPLY_DRY_PARAMS);

            buf->dirty.bit.dry_filter = 1;
            hr = DS_OK;
        }
        break;

    case DSPROPERTY_EAX30BUFFER_ROOM:
        if(cbPropData >= sizeof(long))
        {
            union { const void *v; const long *l; } data = { pPropData };
            TRACE("Room: %ld\n", *data.l);

            buf->deferred.eax.lRoom = *data.l;
            ApplyFilterParams(buf, &buf->deferred.eax, APPLY_WET_PARAMS);

            buf->dirty.bit.send0_filter = 1;
            hr = DS_OK;
        }
        break;
    case DSPROPERTY_EAX30BUFFER_ROOMHF:
        if(cbPropData >= sizeof(long))
        {
            union { const void *v; const long *l; } data = { pPropData };
            TRACE("Room HF: %ld\n", *data.l);

            buf->deferred.eax.lRoomHF = *data.l;
            ApplyFilterParams(buf, &buf->deferred.eax, APPLY_WET_PARAMS);

            buf->dirty.bit.send0_filter = 1;
            hr = DS_OK;
        }
        break;

    case DSPROPERTY_EAX30BUFFER_OBSTRUCTION:
        if(cbPropData >= sizeof(long))
        {
            union { const void *v; const long *l; } data = { pPropData };
            TRACE("Obstruction: %ld\n", *data.l);

            buf->deferred.eax.lObstruction = *data.l;
            ApplyFilterParams(buf, &buf->deferred.eax, APPLY_DRY_PARAMS);

            buf->dirty.bit.dry_filter = 1;
            hr = DS_OK;
        }
        break;
    case DSPROPERTY_EAX30BUFFER_OBSTRUCTIONLFRATIO:
        if(cbPropData >= sizeof(float))
        {
            union { const void *v; const float *fl; } data = { pPropData };
            TRACE("Obstruction LF Ratio: %f\n", *data.fl);

            buf->deferred.eax.flObstructionLFRatio = *data.fl;
            ApplyFilterParams(buf, &buf->deferred.eax, APPLY_DRY_PARAMS);

            buf->dirty.bit.dry_filter = 1;
            hr = DS_OK;
        }
        break;

    case DSPROPERTY_EAX30BUFFER_OCCLUSION:
        if(cbPropData >= sizeof(long))
        {
            union { const void *v; const long *l; } data = { pPropData };
            TRACE("Occlusion: %ld\n", *data.l);

            buf->deferred.eax.lOcclusion = *data.l;
            ApplyFilterParams(buf, &buf->deferred.eax, APPLY_DRY_PARAMS|APPLY_WET_PARAMS);

            buf->dirty.bit.dry_filter = 1;
            buf->dirty.bit.send0_filter = 1;
            hr = DS_OK;
        }
        break;
    case DSPROPERTY_EAX30BUFFER_OCCLUSIONLFRATIO:
        if(cbPropData >= sizeof(float))
        {
            union { const void *v; const float *fl; } data = { pPropData };
            TRACE("Occlusion LF Ratio: %f\n", *data.fl);

            buf->deferred.eax.flOcclusionLFRatio = *data.fl;
            ApplyFilterParams(buf, &buf->deferred.eax, APPLY_DRY_PARAMS|APPLY_WET_PARAMS);

            buf->dirty.bit.dry_filter = 1;
            buf->dirty.bit.send0_filter = 1;
            hr = DS_OK;
        }
        break;
    case DSPROPERTY_EAX30BUFFER_OCCLUSIONROOMRATIO:
        if(cbPropData >= sizeof(float))
        {
            union { const void *v; const float *fl; } data = { pPropData };
            TRACE("Occlusion Room Ratio: %f\n", *data.fl);

            buf->deferred.eax.flOcclusionRoomRatio = *data.fl;
            ApplyFilterParams(buf, &buf->deferred.eax, APPLY_WET_PARAMS);

            buf->dirty.bit.send0_filter = 1;
            hr = DS_OK;
        }
        break;
    case DSPROPERTY_EAX30BUFFER_OCCLUSIONDIRECTRATIO:
        if(cbPropData >= sizeof(float))
        {
            union { const void *v; const float *fl; } data = { pPropData };
            TRACE("Occlusion Direct Ratio: %f\n", *data.fl);

            buf->deferred.eax.flOcclusionDirectRatio = *data.fl;
            ApplyFilterParams(buf, &buf->deferred.eax, APPLY_DRY_PARAMS);

            buf->dirty.bit.dry_filter = 1;
            hr = DS_OK;
        }
        break;

    case DSPROPERTY_EAX30BUFFER_EXCLUSION:
        if(cbPropData >= sizeof(long))
        {
            union { const void *v; const long *l; } data = { pPropData };
            TRACE("Exclusion: %ld\n", *data.l);

            buf->deferred.eax.lExclusion = *data.l;
            ApplyFilterParams(buf, &buf->deferred.eax, APPLY_WET_PARAMS);

            buf->dirty.bit.send0_filter = 1;
            hr = DS_OK;
        }
        break;
    case DSPROPERTY_EAX30BUFFER_EXCLUSIONLFRATIO:
        if(cbPropData >= sizeof(float))
        {
            union { const void *v; const float *fl; } data = { pPropData };
            TRACE("Exclusion LF Ratio: %f\n", *data.fl);

            buf->deferred.eax.flExclusionLFRatio = *data.fl;
            ApplyFilterParams(buf, &buf->deferred.eax, APPLY_WET_PARAMS);

            buf->dirty.bit.send0_filter = 1;
            hr = DS_OK;
        }
        break;

    case DSPROPERTY_EAX30BUFFER_OUTSIDEVOLUMEHF:
        if(cbPropData >= sizeof(long))
        {
            union { const void *v; const long *l; } data = { pPropData };
            TRACE("Outisde Volume HF: %ld\n", *data.l);

            buf->deferred.eax.lOutsideVolumeHF = *data.l;

            buf->dirty.bit.cone_outsidevolumehf = 1;
            hr = DS_OK;
        }
        break;

    case DSPROPERTY_EAX30BUFFER_DOPPLERFACTOR:
        if(cbPropData >= sizeof(float))
        {
            union { const void *v; const float *fl; } data = { pPropData };
            TRACE("Doppler Factor: %f\n", *data.fl);

            buf->deferred.eax.flDopplerFactor = *data.fl;

            buf->dirty.bit.doppler = 1;
            hr = DS_OK;
        }
        break;

    case DSPROPERTY_EAX30BUFFER_ROLLOFFFACTOR:
        if(cbPropData >= sizeof(float))
        {
            union { const void *v; const float *fl; } data = { pPropData };
            TRACE("Rolloff Factor: %f\n", *data.fl);

            buf->deferred.eax.flRolloffFactor = *data.fl;

            buf->dirty.bit.rolloff = 1;
            hr = DS_OK;
        }
        break;

    case DSPROPERTY_EAX30BUFFER_ROOMROLLOFFFACTOR:
        if(cbPropData >= sizeof(float))
        {
            union { const void *v; const float *fl; } data = { pPropData };
            TRACE("Room Rolloff Factor: %f\n", *data.fl);

            buf->deferred.eax.flRoomRolloffFactor = *data.fl;

            buf->dirty.bit.room_rolloff = 1;
            hr = DS_OK;
        }
        break;

    case DSPROPERTY_EAX30BUFFER_AIRABSORPTIONFACTOR:
        if(cbPropData >= sizeof(float))
        {
            union { const void *v; const float *fl; } data = { pPropData };
            TRACE("Air Absorb Factor: %f\n", *data.fl);

            buf->deferred.eax.flAirAbsorptionFactor = *data.fl;

            buf->dirty.bit.air_absorb = 1;
            hr = DS_OK;
        }
        break;

    case DSPROPERTY_EAX30BUFFER_FLAGS:
        if(cbPropData >= sizeof(DWORD))
        {
            union { const void *v; const DWORD *dw; } data = { pPropData };
            TRACE("Flags: 0x%lx\n", *data.dw);

            buf->deferred.eax.dwFlags = *data.dw;

            buf->dirty.bit.flags = 1;
            hr = DS_OK;
        }
        break;

    default:
        hr = E_PROP_ID_UNSUPPORTED;
        FIXME("Unhandled propid: 0x%08lx\n", propid);
        break;
    }

    return hr;
}

HRESULT EAX3Buffer_Get(DSBuffer *buf, DWORD propid, void *pPropData, ULONG cbPropData, ULONG *pcbReturned)
{
    HRESULT hr;

#define GET_PROP(src, T) do {                              \
    if(cbPropData >= sizeof(T))                            \
    {                                                      \
        union { void *v; T *props; } data = { pPropData }; \
        *data.props = src;                                 \
        *pcbReturned = sizeof(T);                          \
        hr = DS_OK;                                        \
    }                                                      \
} while(0)
    hr = DSERR_INVALIDPARAM;
    switch(propid)
    {
    case DSPROPERTY_EAX30BUFFER_NONE:
        *pcbReturned = 0;
        hr = DS_OK;
        break;

    case DSPROPERTY_EAX30BUFFER_ALLPARAMETERS:
        GET_PROP(buf->current.eax, EAX30BUFFERPROPERTIES);
        break;
    case DSPROPERTY_EAX30BUFFER_OBSTRUCTIONPARAMETERS:
        GET_PROP(EAXSourceObstruction(&buf->current.eax), EAXOBSTRUCTIONPROPERTIES);
        break;
    case DSPROPERTY_EAX30BUFFER_OCCLUSIONPARAMETERS:
        GET_PROP(EAXSourceOcclusion(&buf->current.eax), EAXOCCLUSIONPROPERTIES);
        break;
    case DSPROPERTY_EAX30BUFFER_EXCLUSIONPARAMETERS:
        GET_PROP(EAXSourceExclusion(&buf->current.eax), EAXEXCLUSIONPROPERTIES);
        break;

    case DSPROPERTY_EAX30BUFFER_DIRECT:
        GET_PROP(buf->current.eax.lDirect, long);
        break;
    case DSPROPERTY_EAX30BUFFER_DIRECTHF:
        GET_PROP(buf->current.eax.lDirectHF, long);
        break;

    case DSPROPERTY_EAX30BUFFER_ROOM:
        GET_PROP(buf->current.eax.lRoom, long);
        break;
    case DSPROPERTY_EAX30BUFFER_ROOMHF:
        GET_PROP(buf->current.eax.lRoomHF, long);
        break;

    case DSPROPERTY_EAX30BUFFER_OBSTRUCTION:
        GET_PROP(buf->current.eax.lObstruction, long);
        break;
    case DSPROPERTY_EAX30BUFFER_OBSTRUCTIONLFRATIO:
        GET_PROP(buf->current.eax.flObstructionLFRatio, float);
        break;

    case DSPROPERTY_EAX30BUFFER_OCCLUSION:
        GET_PROP(buf->current.eax.lOcclusion, long);
        break;
    case DSPROPERTY_EAX30BUFFER_OCCLUSIONLFRATIO:
        GET_PROP(buf->current.eax.flOcclusionLFRatio, float);
        break;
    case DSPROPERTY_EAX30BUFFER_OCCLUSIONROOMRATIO:
        GET_PROP(buf->current.eax.flOcclusionRoomRatio, float);
        break;
    case DSPROPERTY_EAX30BUFFER_OCCLUSIONDIRECTRATIO:
        GET_PROP(buf->current.eax.flOcclusionDirectRatio, float);
        break;

    case DSPROPERTY_EAX30BUFFER_EXCLUSION:
        GET_PROP(buf->current.eax.lExclusion, long);
        break;
    case DSPROPERTY_EAX30BUFFER_EXCLUSIONLFRATIO:
        GET_PROP(buf->current.eax.flExclusionLFRatio, float);
        break;

    case DSPROPERTY_EAX30BUFFER_OUTSIDEVOLUMEHF:
        GET_PROP(buf->current.eax.lOutsideVolumeHF, long);
        break;

    case DSPROPERTY_EAX30BUFFER_DOPPLERFACTOR:
        GET_PROP(buf->current.eax.flDopplerFactor, float);
        break;

    case DSPROPERTY_EAX30BUFFER_ROLLOFFFACTOR:
        GET_PROP(buf->current.eax.flRolloffFactor, float);
        break;
    case DSPROPERTY_EAX30BUFFER_ROOMROLLOFFFACTOR:
        GET_PROP(buf->current.eax.flRoomRolloffFactor, float);
        break;

    case DSPROPERTY_EAX30BUFFER_AIRABSORPTIONFACTOR:
        GET_PROP(buf->current.eax.flAirAbsorptionFactor, float);
        break;

    case DSPROPERTY_EAX30BUFFER_FLAGS:
        GET_PROP(buf->current.eax.dwFlags, DWORD);
        break;

    default:
        hr = E_PROP_ID_UNSUPPORTED;
        FIXME("Unhandled propid: 0x%08lx\n", propid);
        break;
    }
#undef GET_PROP

    return hr;
}


/*******************
 * EAX 2 stuff
 ******************/

#define EAX2LISTENERFLAGS_MASK (EAX20LISTENERFLAGS_DECAYTIMESCALE        | \
                                EAX20LISTENERFLAGS_REFLECTIONSSCALE      | \
                                EAX20LISTENERFLAGS_REFLECTIONSDELAYSCALE | \
                                EAX20LISTENERFLAGS_REVERBSCALE           | \
                                EAX20LISTENERFLAGS_REVERBDELAYSCALE      | \
                                EAX20LISTENERFLAGS_DECAYHFLIMIT)

static EAX20LISTENERPROPERTIES EAXRevTo2(const EAXREVERBPROPERTIES *props)
{
    EAX20LISTENERPROPERTIES ret;
    ret.lRoom = props->lRoom;
    ret.lRoomHF = props->lRoomHF;
    ret.flRoomRolloffFactor = props->flRoomRolloffFactor;
    ret.flDecayTime = props->flDecayTime;
    ret.flDecayHFRatio = props->flDecayHFRatio;
    ret.lReflections = props->lReflections;
    ret.flReflectionsDelay = props->flReflectionsDelay;
    ret.lReverb = props->lReverb;
    ret.flReverbDelay = props->flReverbDelay;
    ret.dwEnvironment = props->dwEnvironment;
    ret.flEnvironmentSize = props->flEnvironmentSize;
    ret.flEnvironmentDiffusion = props->flEnvironmentDiffusion;
    ret.flAirAbsorptionHF = props->flAirAbsorptionHF;
    ret.dwFlags = props->dwFlags & EAX2LISTENERFLAGS_MASK;
    return ret;
}

static EAX20BUFFERPROPERTIES EAXSourceTo2(const EAXSOURCEPROPERTIES *props)
{
    EAX20BUFFERPROPERTIES ret;
    ret.lDirect = props->lDirect;
    ret.lDirectHF = props->lDirectHF;
    ret.lRoom = props->lRoom;
    ret.lRoomHF = props->lRoomHF;
    ret.flRoomRolloffFactor = props->flRoomRolloffFactor;
    ret.lObstruction = props->lObstruction;
    ret.flObstructionLFRatio = props->flObstructionLFRatio;
    ret.lOcclusion = props->lOcclusion;
    ret.flOcclusionLFRatio = props->flOcclusionLFRatio;
    ret.flOcclusionRoomRatio = props->flOcclusionRoomRatio;
    ret.lOutsideVolumeHF = props->lOutsideVolumeHF;
    ret.flAirAbsorptionFactor = props->flAirAbsorptionFactor;
    ret.dwFlags = props->dwFlags;
    return ret;
}


HRESULT EAX2_Query(DSPrimary *prim, DWORD propid, ULONG *pTypeSupport)
{
    if(prim->effect[0] == 0)
        return E_PROP_ID_UNSUPPORTED;

    switch((propid&~DSPROPERTY_EAX20LISTENER_DEFERRED))
    {
    case DSPROPERTY_EAX20LISTENER_NONE:
    case DSPROPERTY_EAX20LISTENER_ALLPARAMETERS:
    case DSPROPERTY_EAX20LISTENER_ROOM:
    case DSPROPERTY_EAX20LISTENER_ROOMHF:
    case DSPROPERTY_EAX20LISTENER_ROOMROLLOFFFACTOR:
    case DSPROPERTY_EAX20LISTENER_DECAYTIME:
    case DSPROPERTY_EAX20LISTENER_DECAYHFRATIO:
    case DSPROPERTY_EAX20LISTENER_REFLECTIONS:
    case DSPROPERTY_EAX20LISTENER_REFLECTIONSDELAY:
    case DSPROPERTY_EAX20LISTENER_REVERB:
    case DSPROPERTY_EAX20LISTENER_REVERBDELAY:
    case DSPROPERTY_EAX20LISTENER_ENVIRONMENT:
    case DSPROPERTY_EAX20LISTENER_ENVIRONMENTSIZE:
    case DSPROPERTY_EAX20LISTENER_ENVIRONMENTDIFFUSION:
    case DSPROPERTY_EAX20LISTENER_AIRABSORPTIONHF:
    case DSPROPERTY_EAX20LISTENER_FLAGS:
        *pTypeSupport = KSPROPERTY_SUPPORT_GET | KSPROPERTY_SUPPORT_SET;
        return DS_OK;

    default:
        FIXME("Unhandled propid: 0x%08lx\n", propid);
    }
    return E_PROP_ID_UNSUPPORTED;
}

HRESULT EAX2_Set(DSPrimary *prim, DWORD propid, void *pPropData, ULONG cbPropData)
{
    HRESULT hr;

    if(prim->effect[0] == 0)
        return E_PROP_ID_UNSUPPORTED;
    if(prim->deferred.fxslot[0].effect_type != FXSLOT_EFFECT_REVERB)
    {
        ERR("Trying to set reverb parameters on non-reverb slot\n");
        return DSERR_INVALIDCALL;
    }

    hr = DSERR_INVALIDPARAM;
    switch(propid)
    {
    case DSPROPERTY_EAX20LISTENER_NONE: /* not setting any property, just applying */
        hr = DS_OK;
        break;

    case DSPROPERTY_EAX20LISTENER_ALLPARAMETERS:
        if(cbPropData >= sizeof(EAX20LISTENERPROPERTIES))
        {
            union {
                const void *v;
                const EAX20LISTENERPROPERTIES *props;
            } data = { pPropData };
            EAXREVERBPROPERTIES props = REVERB_PRESET_GENERIC;
            TRACE("Parameters:\n\tEnvironment: %lu\n\tEnvSize: %f\n\tEnvDiffusion: %f\n\t"
                "Room: %ld\n\tRoom HF: %ld\n\tDecay Time: %f\n\tDecay HF Ratio: %f\n\t"
                "Reflections: %ld\n\tReflections Delay: %f\n\tReverb: %ld\n\tReverb Delay: %f\n\t"
                "Air Absorption: %f\n\tRoom Rolloff: %f\n\tFlags: 0x%02lx\n",
                data.props->dwEnvironment, data.props->flEnvironmentSize,
                data.props->flEnvironmentDiffusion, data.props->lRoom, data.props->lRoomHF,
                data.props->flDecayTime, data.props->flDecayHFRatio, data.props->lReflections,
                data.props->flReflectionsDelay, data.props->lReverb, data.props->flReverbDelay,
                data.props->flAirAbsorptionHF, data.props->flRoomRolloffFactor, data.props->dwFlags
            );

            if(data.props->dwEnvironment < EAX_ENVIRONMENT_UNDEFINED)
            {
                props = EnvironmentDefaults[data.props->dwEnvironment];
                props.dwEnvironment = data.props->dwEnvironment;
            }
            props.flEnvironmentSize = data.props->flEnvironmentSize;
            props.flEnvironmentDiffusion = data.props->flEnvironmentDiffusion;
            props.lRoom = data.props->lRoom;
            props.lRoomHF = data.props->lRoomHF;
            props.flDecayTime = data.props->flDecayTime;
            props.flDecayHFRatio = data.props->flDecayHFRatio;
            props.lReflections = data.props->lReflections;
            props.flReflectionsDelay = data.props->flReflectionsDelay;
            props.lReverb = data.props->lReverb;
            props.flReverbDelay = data.props->flReverbDelay;
            props.flAirAbsorptionHF = data.props->flAirAbsorptionHF;
            props.flRoomRolloffFactor = data.props->flRoomRolloffFactor;
            props.dwFlags = data.props->dwFlags;

            prim->deferred.fxslot[0].fx.reverb = props;
            ApplyReverbParams(prim->effect[0], &props);

            FXSLOT_SET_DIRTY(prim->dirty.bit, 0, FXSLOT_EFFECT_BIT);
            hr = DS_OK;
        }
        break;

    case DSPROPERTY_EAX20LISTENER_ROOM:
        if(cbPropData >= sizeof(long))
        {
            union { const void *v; const long *l; } data = { pPropData };
            TRACE("Room: %ld\n", *data.l);

            prim->deferred.fxslot[0].fx.reverb.lRoom = *data.l;
            alEffectf(prim->effect[0], AL_EAXREVERB_GAIN,
                      mB_to_gain(prim->deferred.fxslot[0].fx.reverb.lRoom));
            checkALError();

            FXSLOT_SET_DIRTY(prim->dirty.bit, 0, FXSLOT_EFFECT_BIT);
            hr = DS_OK;
        }
        break;
    case DSPROPERTY_EAX20LISTENER_ROOMHF:
        if(cbPropData >= sizeof(long))
        {
            union { const void *v; const long *l; } data = { pPropData };
            TRACE("Room HF: %ld\n", *data.l);

            prim->deferred.fxslot[0].fx.reverb.lRoomHF = *data.l;
            alEffectf(prim->effect[0], AL_EAXREVERB_GAINHF,
                      mB_to_gain(prim->deferred.fxslot[0].fx.reverb.lRoomHF));
            checkALError();

            FXSLOT_SET_DIRTY(prim->dirty.bit, 0, FXSLOT_EFFECT_BIT);
            hr = DS_OK;
        }
        break;

    case DSPROPERTY_EAX20LISTENER_ROOMROLLOFFFACTOR:
        if(cbPropData >= sizeof(float))
        {
            union { const void *v; const float *fl; } data = { pPropData };
            TRACE("Room Rolloff Factor: %f\n", *data.fl);

            prim->deferred.fxslot[0].fx.reverb.flRoomRolloffFactor = *data.fl;
            alEffectf(prim->effect[0], AL_EAXREVERB_ROOM_ROLLOFF_FACTOR,
                      prim->deferred.fxslot[0].fx.reverb.flRoomRolloffFactor);
            checkALError();

            FXSLOT_SET_DIRTY(prim->dirty.bit, 0, FXSLOT_EFFECT_BIT);
            hr = DS_OK;
        }
        break;

    case DSPROPERTY_EAX20LISTENER_DECAYTIME:
        if(cbPropData >= sizeof(float))
        {
            union { const void *v; const float *fl; } data = { pPropData };
            TRACE("Decay Time: %f\n", *data.fl);

            prim->deferred.fxslot[0].fx.reverb.flDecayTime = *data.fl;
            alEffectf(prim->effect[0], AL_EAXREVERB_DECAY_TIME,
                      prim->deferred.fxslot[0].fx.reverb.flDecayTime);
            checkALError();

            FXSLOT_SET_DIRTY(prim->dirty.bit, 0, FXSLOT_EFFECT_BIT);
            hr = DS_OK;
        }
        break;
    case DSPROPERTY_EAX20LISTENER_DECAYHFRATIO:
        if(cbPropData >= sizeof(float))
        {
            union { const void *v; const float *fl; } data = { pPropData };
            TRACE("Decay HF Ratio: %f\n", *data.fl);

            prim->deferred.fxslot[0].fx.reverb.flDecayHFRatio = *data.fl;
            alEffectf(prim->effect[0], AL_EAXREVERB_DECAY_HFRATIO,
                      prim->deferred.fxslot[0].fx.reverb.flDecayHFRatio);
            checkALError();

            FXSLOT_SET_DIRTY(prim->dirty.bit, 0, FXSLOT_EFFECT_BIT);
            hr = DS_OK;
        }
        break;

    case DSPROPERTY_EAX20LISTENER_REFLECTIONS:
        if(cbPropData >= sizeof(long))
        {
            union { const void *v; const long *l; } data = { pPropData };
            TRACE("Reflections: %ld\n", *data.l);

            prim->deferred.fxslot[0].fx.reverb.lReflections = *data.l;
            alEffectf(prim->effect[0], AL_EAXREVERB_REFLECTIONS_GAIN,
                      mB_to_gain(prim->deferred.fxslot[0].fx.reverb.lReflections));
            checkALError();

            FXSLOT_SET_DIRTY(prim->dirty.bit, 0, FXSLOT_EFFECT_BIT);
            hr = DS_OK;
        }
        break;
    case DSPROPERTY_EAX20LISTENER_REFLECTIONSDELAY:
        if(cbPropData >= sizeof(float))
        {
            union { const void *v; const float *fl; } data = { pPropData };
            TRACE("Reflections Delay: %f\n", *data.fl);

            prim->deferred.fxslot[0].fx.reverb.flReflectionsDelay = *data.fl;
            alEffectf(prim->effect[0], AL_EAXREVERB_REFLECTIONS_DELAY,
                      prim->deferred.fxslot[0].fx.reverb.flReflectionsDelay);
            checkALError();

            FXSLOT_SET_DIRTY(prim->dirty.bit, 0, FXSLOT_EFFECT_BIT);
            hr = DS_OK;
        }
        break;

    case DSPROPERTY_EAX20LISTENER_REVERB:
        if(cbPropData >= sizeof(long))
        {
            union { const void *v; const long *l; } data = { pPropData };
            TRACE("Reverb: %ld\n", *data.l);

            prim->deferred.fxslot[0].fx.reverb.lReverb = *data.l;
            alEffectf(prim->effect[0], AL_EAXREVERB_LATE_REVERB_GAIN,
                      mB_to_gain(prim->deferred.fxslot[0].fx.reverb.lReverb));
            checkALError();

            FXSLOT_SET_DIRTY(prim->dirty.bit, 0, FXSLOT_EFFECT_BIT);
            hr = DS_OK;
        }
        break;
    case DSPROPERTY_EAX20LISTENER_REVERBDELAY:
        if(cbPropData >= sizeof(float))
        {
            union { const void *v; const float *fl; } data = { pPropData };
            TRACE("Reverb Delay: %f\n", *data.fl);

            prim->deferred.fxslot[0].fx.reverb.flReverbDelay = *data.fl;
            alEffectf(prim->effect[0], AL_EAXREVERB_LATE_REVERB_DELAY,
                      prim->deferred.fxslot[0].fx.reverb.flReverbDelay);
            checkALError();

            FXSLOT_SET_DIRTY(prim->dirty.bit, 0, FXSLOT_EFFECT_BIT);
            hr = DS_OK;
        }
        break;

    case DSPROPERTY_EAX20LISTENER_ENVIRONMENT:
        if(cbPropData >= sizeof(DWORD))
        {
            union { const void *v; const DWORD *dw; } data = { pPropData };
            TRACE("Environment: %lu\n", *data.dw);
            if(*data.dw < EAX_ENVIRONMENT_UNDEFINED)
            {
                prim->deferred.fxslot[0].fx.reverb = EnvironmentDefaults[*data.dw];
                ApplyReverbParams(prim->effect[0], &EnvironmentDefaults[*data.dw]);

                FXSLOT_SET_DIRTY(prim->dirty.bit, 0, FXSLOT_EFFECT_BIT);
                hr = DS_OK;
            }
        }
        break;

    case DSPROPERTY_EAX20LISTENER_ENVIRONMENTSIZE:
        if(cbPropData >= sizeof(float))
        {
            union { const void *v; const float *fl; } data = { pPropData };
            TRACE("Environment Size: %f\n", *data.fl);

            RescaleEnvSize(&prim->deferred.fxslot[0].fx.reverb, clampF(*data.fl, 1.0f, 100.0f));
            ApplyReverbParams(prim->effect[0], &prim->deferred.fxslot[0].fx.reverb);

            FXSLOT_SET_DIRTY(prim->dirty.bit, 0, FXSLOT_EFFECT_BIT);
            hr = DS_OK;
        }
        break;
    case DSPROPERTY_EAX20LISTENER_ENVIRONMENTDIFFUSION:
        if(cbPropData >= sizeof(float))
        {
            union { const void *v; const float *fl; } data = { pPropData };
            TRACE("Environment Diffusion: %f\n", *data.fl);

            prim->deferred.fxslot[0].fx.reverb.flEnvironmentDiffusion = *data.fl;
            alEffectf(prim->effect[0], AL_EAXREVERB_DIFFUSION,
                      prim->deferred.fxslot[0].fx.reverb.flEnvironmentDiffusion);
            checkALError();

            FXSLOT_SET_DIRTY(prim->dirty.bit, 0, FXSLOT_EFFECT_BIT);
            hr = DS_OK;
        }
        break;

    case DSPROPERTY_EAX20LISTENER_AIRABSORPTIONHF:
        if(cbPropData >= sizeof(float))
        {
            union { const void *v; const float *fl; } data = { pPropData };
            TRACE("Air Absorption HF: %f\n", *data.fl);

            prim->deferred.fxslot[0].fx.reverb.flAirAbsorptionHF = *data.fl;
            alEffectf(prim->effect[0], AL_EAXREVERB_AIR_ABSORPTION_GAINHF,
                      mB_to_gain(prim->deferred.fxslot[0].fx.reverb.flAirAbsorptionHF));
            checkALError();

            FXSLOT_SET_DIRTY(prim->dirty.bit, 0, FXSLOT_EFFECT_BIT);
            hr = DS_OK;
        }
        break;

    case DSPROPERTY_EAX20LISTENER_FLAGS:
        if(cbPropData >= sizeof(DWORD))
        {
            union { const void *v; const DWORD *dw; } data = { pPropData };
            TRACE("Flags: %lu\n", *data.dw);

            prim->deferred.fxslot[0].fx.reverb.dwFlags = *data.dw;
            alEffecti(prim->effect[0], AL_EAXREVERB_DECAY_HFLIMIT,
                      (prim->deferred.fxslot[0].fx.reverb.dwFlags&EAX30LISTENERFLAGS_DECAYHFLIMIT) ?
                      AL_TRUE : AL_FALSE);
            checkALError();

            FXSLOT_SET_DIRTY(prim->dirty.bit, 0, FXSLOT_EFFECT_BIT);
            hr = DS_OK;
        }
        break;

    default:
        hr = E_PROP_ID_UNSUPPORTED;
        FIXME("Unhandled propid: 0x%08lx\n", propid);
        break;
    }

    return hr;
}

HRESULT EAX2_Get(DSPrimary *prim, DWORD propid, void *pPropData, ULONG cbPropData, ULONG *pcbReturned)
{
    HRESULT hr;

    if(prim->effect[0] == 0)
        return E_PROP_ID_UNSUPPORTED;
    if(prim->deferred.fxslot[0].effect_type != FXSLOT_EFFECT_REVERB)
    {
        ERR("Trying to get reverb parameters on non-reverb slot\n");
        return DSERR_INVALIDCALL;
    }

#define GET_PROP(src, T) do {                              \
    if(cbPropData >= sizeof(T))                            \
    {                                                      \
        union { void *v; T *props; } data = { pPropData }; \
        *data.props = src;                                 \
        *pcbReturned = sizeof(T);                          \
        hr = DS_OK;                                        \
    }                                                      \
} while(0)
    hr = DSERR_INVALIDPARAM;
    switch(propid)
    {
    case DSPROPERTY_EAX20LISTENER_NONE:
        *pcbReturned = 0;
        hr = DS_OK;
        break;

    case DSPROPERTY_EAX20LISTENER_ALLPARAMETERS:
        GET_PROP(EAXRevTo2(&prim->deferred.fxslot[0].fx.reverb), EAX20LISTENERPROPERTIES);
        break;

    case DSPROPERTY_EAX20LISTENER_ROOM:
        GET_PROP(prim->deferred.fxslot[0].fx.reverb.lRoom, long);
        break;
    case DSPROPERTY_EAX20LISTENER_ROOMHF:
        GET_PROP(prim->deferred.fxslot[0].fx.reverb.lRoomHF, long);
        break;

    case DSPROPERTY_EAX20LISTENER_ROOMROLLOFFFACTOR:
        GET_PROP(prim->deferred.fxslot[0].fx.reverb.flRoomRolloffFactor, float);
        break;

    case DSPROPERTY_EAX20LISTENER_DECAYTIME:
        GET_PROP(prim->deferred.fxslot[0].fx.reverb.flDecayTime, float);
        break;
    case DSPROPERTY_EAX20LISTENER_DECAYHFRATIO:
        GET_PROP(prim->deferred.fxslot[0].fx.reverb.flDecayHFRatio, float);
        break;

    case DSPROPERTY_EAX20LISTENER_REFLECTIONS:
        GET_PROP(prim->deferred.fxslot[0].fx.reverb.lReflections, long);
        break;
    case DSPROPERTY_EAX20LISTENER_REFLECTIONSDELAY:
        GET_PROP(prim->deferred.fxslot[0].fx.reverb.flReflectionsDelay, float);
        break;

    case DSPROPERTY_EAX20LISTENER_REVERB:
        GET_PROP(prim->deferred.fxslot[0].fx.reverb.lReverb, long);
        break;
    case DSPROPERTY_EAX20LISTENER_REVERBDELAY:
        GET_PROP(prim->deferred.fxslot[0].fx.reverb.flReverbDelay, float);
        break;

    case DSPROPERTY_EAX20LISTENER_ENVIRONMENT:
        GET_PROP(prim->deferred.fxslot[0].fx.reverb.dwEnvironment, DWORD);
        break;

    case DSPROPERTY_EAX20LISTENER_ENVIRONMENTSIZE:
        GET_PROP(prim->deferred.fxslot[0].fx.reverb.flEnvironmentSize, float);
        break;
    case DSPROPERTY_EAX20LISTENER_ENVIRONMENTDIFFUSION:
        GET_PROP(prim->deferred.fxslot[0].fx.reverb.flEnvironmentDiffusion, float);
        break;

    case DSPROPERTY_EAX20LISTENER_AIRABSORPTIONHF:
        GET_PROP(prim->deferred.fxslot[0].fx.reverb.flAirAbsorptionHF, float);
        break;

    case DSPROPERTY_EAX20LISTENER_FLAGS:
        GET_PROP(prim->deferred.fxslot[0].fx.reverb.dwFlags&EAX2LISTENERFLAGS_MASK, DWORD);
        break;

    default:
        hr = E_PROP_ID_UNSUPPORTED;
        FIXME("Unhandled listener propid: 0x%08lx\n", propid);
        break;
    }
#undef GET_PROP

    return hr;
}


HRESULT EAX2Buffer_Query(DSBuffer *buf, DWORD propid, ULONG *pTypeSupport)
{
    (void)buf;

    switch((propid&~DSPROPERTY_EAX20BUFFER_DEFERRED))
    {
    case DSPROPERTY_EAX20BUFFER_NONE:
    case DSPROPERTY_EAX20BUFFER_ALLPARAMETERS:
    case DSPROPERTY_EAX20BUFFER_DIRECT:
    case DSPROPERTY_EAX20BUFFER_DIRECTHF:
    case DSPROPERTY_EAX20BUFFER_ROOM:
    case DSPROPERTY_EAX20BUFFER_ROOMHF:
    case DSPROPERTY_EAX20BUFFER_ROOMROLLOFFFACTOR:
    case DSPROPERTY_EAX20BUFFER_OBSTRUCTION:
    case DSPROPERTY_EAX20BUFFER_OBSTRUCTIONLFRATIO:
    case DSPROPERTY_EAX20BUFFER_OCCLUSION:
    case DSPROPERTY_EAX20BUFFER_OCCLUSIONLFRATIO:
    case DSPROPERTY_EAX20BUFFER_OCCLUSIONROOMRATIO:
    case DSPROPERTY_EAX20BUFFER_OUTSIDEVOLUMEHF:
    case DSPROPERTY_EAX20BUFFER_AIRABSORPTIONFACTOR:
    case DSPROPERTY_EAX20BUFFER_FLAGS:
        *pTypeSupport = KSPROPERTY_SUPPORT_GET | KSPROPERTY_SUPPORT_SET;
        return DS_OK;

    default:
        FIXME("Unhandled propid: 0x%08lx\n", propid);
    }
    return E_PROP_ID_UNSUPPORTED;
}

HRESULT EAX2Buffer_Set(DSBuffer *buf, DWORD propid, void *pPropData, ULONG cbPropData)
{
    HRESULT hr;

    hr = DSERR_INVALIDPARAM;
    switch(propid)
    {
    case DSPROPERTY_EAX20BUFFER_NONE: /* not setting any property, just applying */
        hr = DS_OK;
        break;

    case DSPROPERTY_EAX20BUFFER_ALLPARAMETERS:
        if(cbPropData >= sizeof(EAX20BUFFERPROPERTIES))
        {
            union {
                const void *v;
                const EAX20BUFFERPROPERTIES *props;
            } data = { pPropData };
            TRACE("Parameters:\n\tDirect: %ld\n\tDirect HF: %ld\n\tRoom: %ld\n\tRoom HF: %ld\n\t"
                "Room Rolloff Factor: %f\n\tObstruction: %ld\n\tObstruction LF Ratio: %f\n\t"
                "Occlusion: %ld\n\tOcclusion LF Ratio: %f\n\tOcclusion Room Ratio: %f\n\t"
                "Outside Volume HF: %ld\n\tAir Absorb Factor: %f\n\tFlags: 0x%02lx\n",
                data.props->lDirect, data.props->lDirectHF, data.props->lRoom, data.props->lRoomHF,
                data.props->flRoomRolloffFactor, data.props->lObstruction,
                data.props->flObstructionLFRatio, data.props->lOcclusion,
                data.props->flOcclusionLFRatio, data.props->flOcclusionRoomRatio,
                data.props->lOutsideVolumeHF, data.props->flAirAbsorptionFactor,
                data.props->dwFlags
            );

            buf->deferred.eax.lDirect = data.props->lDirect;
            buf->deferred.eax.lDirectHF = data.props->lDirectHF;
            buf->deferred.eax.lRoom = data.props->lRoom;
            buf->deferred.eax.lRoomHF = data.props->lRoomHF;
            buf->deferred.eax.flRoomRolloffFactor = data.props->flRoomRolloffFactor;
            buf->deferred.eax.lObstruction = data.props->lObstruction;
            buf->deferred.eax.flObstructionLFRatio = data.props->flObstructionLFRatio;
            buf->deferred.eax.lOcclusion = data.props->lOcclusion;
            buf->deferred.eax.flOcclusionLFRatio = data.props->flOcclusionLFRatio;
            buf->deferred.eax.flOcclusionRoomRatio = data.props->flOcclusionRoomRatio;
            buf->deferred.eax.lOutsideVolumeHF = data.props->lOutsideVolumeHF;
            buf->deferred.eax.flAirAbsorptionFactor = data.props->flAirAbsorptionFactor;
            buf->deferred.eax.dwFlags = data.props->dwFlags;
            ApplyFilterParams(buf, &buf->deferred.eax, APPLY_DRY_PARAMS|APPLY_WET_PARAMS);

            buf->dirty.bit.dry_filter = 1;
            buf->dirty.bit.send0_filter = 1;
            buf->dirty.bit.room_rolloff = 1;
            buf->dirty.bit.cone_outsidevolumehf = 1;
            buf->dirty.bit.air_absorb = 1;
            buf->dirty.bit.flags = 1;
            hr = DS_OK;
        }
        break;

    case DSPROPERTY_EAX20BUFFER_DIRECT:
        if(cbPropData >= sizeof(long))
        {
            union { const void *v; const long *l; } data = { pPropData };
            TRACE("Direct: %ld\n", *data.l);

            buf->deferred.eax.lDirect = *data.l;
            ApplyFilterParams(buf, &buf->deferred.eax, APPLY_DRY_PARAMS);

            buf->dirty.bit.dry_filter = 1;
            hr = DS_OK;
        }
        break;
    case DSPROPERTY_EAX20BUFFER_DIRECTHF:
        if(cbPropData >= sizeof(long))
        {
            union { const void *v; const long *l; } data = { pPropData };
            TRACE("Direct HF: %ld\n", *data.l);

            buf->deferred.eax.lDirectHF = *data.l;
            ApplyFilterParams(buf, &buf->deferred.eax, APPLY_DRY_PARAMS);

            buf->dirty.bit.dry_filter = 1;
            hr = DS_OK;
        }
        break;

    case DSPROPERTY_EAX20BUFFER_ROOM:
        if(cbPropData >= sizeof(long))
        {
            union { const void *v; const long *l; } data = { pPropData };
            TRACE("Room: %ld\n", *data.l);

            buf->deferred.eax.lRoom = *data.l;
            ApplyFilterParams(buf, &buf->deferred.eax, APPLY_WET_PARAMS);

            buf->dirty.bit.send0_filter = 1;
            hr = DS_OK;
        }
        break;
    case DSPROPERTY_EAX20BUFFER_ROOMHF:
        if(cbPropData >= sizeof(long))
        {
            union { const void *v; const long *l; } data = { pPropData };
            TRACE("Room HF: %ld\n", *data.l);

            buf->deferred.eax.lRoomHF = *data.l;
            ApplyFilterParams(buf, &buf->deferred.eax, APPLY_WET_PARAMS);

            buf->dirty.bit.send0_filter = 1;
            hr = DS_OK;
        }
        break;

    case DSPROPERTY_EAX20BUFFER_ROOMROLLOFFFACTOR:
        if(cbPropData >= sizeof(float))
        {
            union { const void *v; const float *fl; } data = { pPropData };
            TRACE("Room Rolloff Factor: %f\n", *data.fl);

            buf->deferred.eax.flRoomRolloffFactor = *data.fl;

            buf->dirty.bit.room_rolloff = 1;
            hr = DS_OK;
        }
        break;

    case DSPROPERTY_EAX20BUFFER_OBSTRUCTION:
        if(cbPropData >= sizeof(long))
        {
            union { const void *v; const long *l; } data = { pPropData };
            TRACE("Obstruction: %ld\n", *data.l);

            buf->deferred.eax.lObstruction = *data.l;
            ApplyFilterParams(buf, &buf->deferred.eax, APPLY_DRY_PARAMS);

            buf->dirty.bit.dry_filter = 1;
            hr = DS_OK;
        }
        break;
    case DSPROPERTY_EAX20BUFFER_OBSTRUCTIONLFRATIO:
        if(cbPropData >= sizeof(float))
        {
            union { const void *v; const float *fl; } data = { pPropData };
            TRACE("Obstruction LF Ratio: %f\n", *data.fl);

            buf->deferred.eax.flObstructionLFRatio = *data.fl;
            ApplyFilterParams(buf, &buf->deferred.eax, APPLY_DRY_PARAMS);

            buf->dirty.bit.dry_filter = 1;
            hr = DS_OK;
        }
        break;

    case DSPROPERTY_EAX20BUFFER_OCCLUSION:
        if(cbPropData >= sizeof(long))
        {
            union { const void *v; const long *l; } data = { pPropData };
            TRACE("Occlusion: %ld\n", *data.l);

            buf->deferred.eax.lOcclusion = *data.l;
            ApplyFilterParams(buf, &buf->deferred.eax, APPLY_DRY_PARAMS|APPLY_WET_PARAMS);

            buf->dirty.bit.dry_filter = 1;
            buf->dirty.bit.send0_filter = 1;
            hr = DS_OK;
        }
        break;
    case DSPROPERTY_EAX20BUFFER_OCCLUSIONLFRATIO:
        if(cbPropData >= sizeof(float))
        {
            union { const void *v; const float *fl; } data = { pPropData };
            TRACE("Occlusion LF Ratio: %f\n", *data.fl);

            buf->deferred.eax.flOcclusionLFRatio = *data.fl;
            ApplyFilterParams(buf, &buf->deferred.eax, APPLY_DRY_PARAMS|APPLY_WET_PARAMS);

            buf->dirty.bit.dry_filter = 1;
            buf->dirty.bit.send0_filter = 1;
            hr = DS_OK;
        }
        break;
    case DSPROPERTY_EAX20BUFFER_OCCLUSIONROOMRATIO:
        if(cbPropData >= sizeof(float))
        {
            union { const void *v; const float *fl; } data = { pPropData };
            TRACE("Occlusion Room Ratio: %f\n", *data.fl);

            buf->deferred.eax.flOcclusionRoomRatio = *data.fl;
            ApplyFilterParams(buf, &buf->deferred.eax, APPLY_WET_PARAMS);

            buf->dirty.bit.send0_filter = 1;
            hr = DS_OK;
        }
        break;

    case DSPROPERTY_EAX20BUFFER_OUTSIDEVOLUMEHF:
        if(cbPropData >= sizeof(long))
        {
            union { const void *v; const long *l; } data = { pPropData };
            TRACE("Outisde Volume HF: %ld\n", *data.l);

            buf->deferred.eax.lOutsideVolumeHF = *data.l;

            buf->dirty.bit.cone_outsidevolumehf = 1;
            hr = DS_OK;
        }
        break;

    case DSPROPERTY_EAX20BUFFER_AIRABSORPTIONFACTOR:
        if(cbPropData >= sizeof(float))
        {
            union { const void *v; const float *fl; } data = { pPropData };
            TRACE("Air Absorb Factor: %f\n", *data.fl);

            buf->deferred.eax.flAirAbsorptionFactor = *data.fl;

            buf->dirty.bit.air_absorb = 1;
            hr = DS_OK;
        }
        break;

    case DSPROPERTY_EAX20BUFFER_FLAGS:
        if(cbPropData >= sizeof(DWORD))
        {
            union { const void *v; const DWORD *dw; } data = { pPropData };
            TRACE("Flags: 0x%lx\n", *data.dw);

            buf->deferred.eax.dwFlags = *data.dw;

            buf->dirty.bit.flags = 1;
            hr = DS_OK;
        }
        break;

    default:
        hr = E_PROP_ID_UNSUPPORTED;
        FIXME("Unhandled propid: 0x%08lx\n", propid);
        break;
    }

    return hr;
}

HRESULT EAX2Buffer_Get(DSBuffer *buf, DWORD propid, void *pPropData, ULONG cbPropData, ULONG *pcbReturned)
{
    HRESULT hr;

#define GET_PROP(src, T) do {                              \
    if(cbPropData >= sizeof(T))                            \
    {                                                      \
        union { void *v; T *props; } data = { pPropData }; \
        *data.props = src;                                 \
        *pcbReturned = sizeof(T);                          \
        hr = DS_OK;                                        \
    }                                                      \
} while(0)
    hr = DSERR_INVALIDPARAM;
    switch(propid)
    {
    case DSPROPERTY_EAX20BUFFER_NONE:
        *pcbReturned = 0;
        hr = DS_OK;
        break;

    case DSPROPERTY_EAX20BUFFER_ALLPARAMETERS:
        GET_PROP(EAXSourceTo2(&buf->current.eax), EAX20BUFFERPROPERTIES);
        break;

    case DSPROPERTY_EAX20BUFFER_DIRECT:
        GET_PROP(buf->current.eax.lDirect, long);
        break;
    case DSPROPERTY_EAX20BUFFER_DIRECTHF:
        GET_PROP(buf->current.eax.lDirectHF, long);
        break;

    case DSPROPERTY_EAX20BUFFER_ROOM:
        GET_PROP(buf->current.eax.lRoom, long);
        break;
    case DSPROPERTY_EAX20BUFFER_ROOMHF:
        GET_PROP(buf->current.eax.lRoomHF, long);
        break;

    case DSPROPERTY_EAX20BUFFER_ROOMROLLOFFFACTOR:
        GET_PROP(buf->current.eax.flRoomRolloffFactor, float);
        break;

    case DSPROPERTY_EAX20BUFFER_OBSTRUCTION:
        GET_PROP(buf->current.eax.lObstruction, long);
        break;
    case DSPROPERTY_EAX20BUFFER_OBSTRUCTIONLFRATIO:
        GET_PROP(buf->current.eax.flObstructionLFRatio, float);
        break;

    case DSPROPERTY_EAX20BUFFER_OCCLUSION:
        GET_PROP(buf->current.eax.lOcclusion, long);
        break;
    case DSPROPERTY_EAX20BUFFER_OCCLUSIONLFRATIO:
        GET_PROP(buf->current.eax.flOcclusionLFRatio, float);
        break;
    case DSPROPERTY_EAX20BUFFER_OCCLUSIONROOMRATIO:
        GET_PROP(buf->current.eax.flOcclusionRoomRatio, float);
        break;

    case DSPROPERTY_EAX20BUFFER_OUTSIDEVOLUMEHF:
        GET_PROP(buf->current.eax.lOutsideVolumeHF, long);
        break;

    case DSPROPERTY_EAX20BUFFER_AIRABSORPTIONFACTOR:
        GET_PROP(buf->current.eax.flAirAbsorptionFactor, float);
        break;

    case DSPROPERTY_EAX20BUFFER_FLAGS:
        GET_PROP(buf->current.eax.dwFlags, DWORD);
        break;

    default:
        hr = E_PROP_ID_UNSUPPORTED;
        FIXME("Unhandled propid: 0x%08lx\n", propid);
        break;
    }
#undef GET_PROP

    return hr;
}


/*******************
 * EAX 1 stuff
 ******************/

HRESULT EAX1_Query(DSPrimary *prim, DWORD propid, ULONG *pTypeSupport)
{
    if(prim->effect[0] == 0)
        return E_PROP_ID_UNSUPPORTED;

    switch(propid)
    {
    case DSPROPERTY_EAX10LISTENER_ALL:
    case DSPROPERTY_EAX10LISTENER_ENVIRONMENT:
    case DSPROPERTY_EAX10LISTENER_VOLUME:
    case DSPROPERTY_EAX10LISTENER_DECAYTIME:
    case DSPROPERTY_EAX10LISTENER_DAMPING:
        *pTypeSupport = KSPROPERTY_SUPPORT_GET | KSPROPERTY_SUPPORT_SET;
        return DS_OK;

    default:
        FIXME("Unhandled propid: 0x%08lx\n", propid);
    }
    return E_PROP_ID_UNSUPPORTED;
}

HRESULT EAX1_Set(DSPrimary *prim, DWORD propid, void *pPropData, ULONG cbPropData)
{
    static const float eax1_env_volume[EAX_ENVIRONMENT_UNDEFINED] = {
        0.5f, 0.25f, 0.417f, 0.653f, 0.208f, 0.5f, 0.403f, 0.5f, 0.5f,
        0.361f, 0.5f, 0.153f, 0.361f, 0.444f, 0.25f, 0.111f, 0.111f,
        0.194f, 1.0f, 0.097f, 0.208f, 0.652f, 1.0f, 0.875f, 0.139f, 0.486f
    };
    static const float eax1_env_dampening[EAX_ENVIRONMENT_UNDEFINED] = {
        0.5f, 0.0f, 0.666f, 0.166f, 0.0f, 0.888f, 0.5f, 0.5f, 1.304f,
        0.332f, 0.3f, 2.0f, 0.0f, 0.638f, 0.776f, 0.472f, 0.224f, 0.472f,
        0.5f, 0.224f, 1.5f, 0.25f, 0.0f, 1.388f, 0.666f, 0.806f
    };
    HRESULT hr;

    if(prim->effect[0] == 0)
        return E_PROP_ID_UNSUPPORTED;
    if(prim->deferred.fxslot[0].effect_type != FXSLOT_EFFECT_REVERB)
    {
        ERR("Trying to set reverb parameters on non-reverb slot\n");
        return DSERR_INVALIDCALL;
    }

    hr = DSERR_INVALIDPARAM;
    switch(propid)
    {
    case DSPROPERTY_EAX10LISTENER_ALL:
        if(cbPropData >= sizeof(EAX10LISTENERPROPERTIES))
        {
            union {
                const void *v;
                const EAX10LISTENERPROPERTIES *props;
            } data = { pPropData };
            TRACE("Parameters:\n\tEnvironment: %lu\n\tVolume: %f\n\tDecay Time: %f\n\t"
                "Damping: %f\n", data.props->dwEnvironment, data.props->fVolume,
                data.props->fDecayTime, data.props->fDamping
            );

            if(data.props->dwEnvironment < EAX_ENVIRONMENT_UNDEFINED)
            {
                /* NOTE: I'm not quite sure how to handle the volume. It's
                 * important to deal with since it can have a notable impact on
                 * the output levels, but given the default EAX1 environment
                 * volumes, they don't align with the gain/room volume for
                 * EAX2+ environments. Presuming the default volumes are
                 * correct, it's possible the reverb implementation was
                 * different and relied on different gains to get the intended
                 * output levels.
                 *
                 * Rather than just blindly applying the volume, we take the
                 * difference from the EAX1 environment's default volume and
                 * apply that as an offset to the EAX2 environment's volume.
                 */
                EAXREVERBPROPERTIES env = EnvironmentDefaults[data.props->dwEnvironment];
                long db_vol = clampI(
                    gain_to_mB(data.props->fVolume / eax1_env_volume[data.props->dwEnvironment]),
                    -10000, 10000
                );
                env.lRoom = clampI(env.lRoom + db_vol, -10000, 0);
                env.flDecayTime = data.props->fDecayTime;

                prim->deferred.fxslot[0].fx.reverb = env;
                prim->deferred.eax1_volume = data.props->fVolume;
                prim->deferred.eax1_dampening = data.props->fDamping;
                ApplyReverbParams(prim->effect[0], &env);

                FXSLOT_SET_DIRTY(prim->dirty.bit, 0, FXSLOT_EFFECT_BIT);
                hr = DS_OK;
            }
        }
        break;

    case DSPROPERTY_EAX10LISTENER_ENVIRONMENT:
        if(cbPropData >= sizeof(DWORD))
        {
            union { const void *v; const DWORD *dw; } data = { pPropData };
            TRACE("Environment: %lu\n", *data.dw);

            if(*data.dw < EAX_ENVIRONMENT_UNDEFINED)
            {
                prim->deferred.fxslot[0].fx.reverb = EnvironmentDefaults[*data.dw];
                prim->deferred.eax1_volume = eax1_env_volume[*data.dw];
                prim->deferred.eax1_dampening = eax1_env_dampening[*data.dw];
                ApplyReverbParams(prim->effect[0], &EnvironmentDefaults[*data.dw]);

                FXSLOT_SET_DIRTY(prim->dirty.bit, 0, FXSLOT_EFFECT_BIT);
                hr = DS_OK;
            }
        }
        break;

    case DSPROPERTY_EAX10LISTENER_VOLUME:
        if(cbPropData >= sizeof(float))
        {
            union { const void *v; const float *fl; } data = { pPropData };
            long db_vol = clampI(
                gain_to_mB(*data.fl / eax1_env_volume[prim->deferred.fxslot[0].fx.reverb.dwEnvironment]),
                -10000, 10000
            );
            long room_vol = clampI(
                EnvironmentDefaults[prim->deferred.fxslot[0].fx.reverb.dwEnvironment].lRoom + db_vol,
                -10000, 0
            );
            TRACE("Volume: %f\n", *data.fl);

            prim->deferred.fxslot[0].fx.reverb.lRoom = room_vol;
            prim->deferred.eax1_volume = *data.fl;
            alEffectf(prim->effect[0], AL_EAXREVERB_GAIN, mB_to_gain(room_vol));
            checkALError();

            FXSLOT_SET_DIRTY(prim->dirty.bit, 0, FXSLOT_EFFECT_BIT);
            hr = DS_OK;
        }
        break;
    case DSPROPERTY_EAX10LISTENER_DECAYTIME:
        if(cbPropData >= sizeof(float))
        {
            union { const void *v; const float *fl; } data = { pPropData };
            TRACE("Decay Time: %f\n", *data.fl);

            prim->deferred.fxslot[0].fx.reverb.flDecayTime = *data.fl;
            alEffectf(prim->effect[0], AL_EAXREVERB_DECAY_TIME,
                      prim->deferred.fxslot[0].fx.reverb.flDecayTime);
            checkALError();

            FXSLOT_SET_DIRTY(prim->dirty.bit, 0, FXSLOT_EFFECT_BIT);
            hr = DS_OK;
        }
        break;
    case DSPROPERTY_EAX10LISTENER_DAMPING:
        if(cbPropData >= sizeof(float))
        {
            union { const void *v; const float *fl; } data = { pPropData };
            TRACE("Damping: %f\n", *data.fl);

            prim->deferred.eax1_dampening = *data.fl;

            hr = DS_OK;
        }
        break;

    default:
        hr = E_PROP_ID_UNSUPPORTED;
        FIXME("Unhandled propid: 0x%08lx\n", propid);
        break;
    }

    return hr;
}

HRESULT EAX1_Get(DSPrimary *prim, DWORD propid, void *pPropData, ULONG cbPropData, ULONG *pcbReturned)
{
    HRESULT hr;

    if(prim->effect[0] == 0)
        return E_PROP_ID_UNSUPPORTED;
    if(prim->deferred.fxslot[0].effect_type != FXSLOT_EFFECT_REVERB)
    {
        ERR("Trying to get reverb parameters on non-reverb slot\n");
        return DSERR_INVALIDCALL;
    }

    hr = DSERR_INVALIDPARAM;
    switch(propid)
    {
    case DSPROPERTY_EAX10LISTENER_ALL:
        if(cbPropData >= sizeof(EAX10LISTENERPROPERTIES))
        {
            union {
                void *v;
                EAX10LISTENERPROPERTIES *props;
            } data = { pPropData };

            data.props->dwEnvironment = prim->deferred.fxslot[0].fx.reverb.dwEnvironment;
            data.props->fVolume = prim->deferred.eax1_volume;
            data.props->fDecayTime = prim->deferred.fxslot[0].fx.reverb.flDecayTime;
            data.props->fDamping = prim->deferred.eax1_dampening;

            *pcbReturned = sizeof(EAX10LISTENERPROPERTIES);
            hr = DS_OK;
        }
        break;

    case DSPROPERTY_EAX10LISTENER_ENVIRONMENT:
        if(cbPropData >= sizeof(DWORD))
        {
            union { void *v; DWORD *dw; } data = { pPropData };

            *data.dw = prim->deferred.fxslot[0].fx.reverb.dwEnvironment;

            *pcbReturned = sizeof(DWORD);
            hr = DS_OK;
        }
        break;

    case DSPROPERTY_EAX10LISTENER_VOLUME:
        if(cbPropData >= sizeof(float))
        {
            union { void *v; float *fl; } data = { pPropData };

            *data.fl = prim->deferred.eax1_volume;

            *pcbReturned = sizeof(float);
            hr = DS_OK;
        }
        break;

    case DSPROPERTY_EAX10LISTENER_DECAYTIME:
        if(cbPropData >= sizeof(float))
        {
            union { void *v; float *fl; } data = { pPropData };

            *data.fl = prim->deferred.fxslot[0].fx.reverb.flDecayTime;

            *pcbReturned = sizeof(float);
            hr = DS_OK;
        }
        break;

    case DSPROPERTY_EAX10LISTENER_DAMPING:
        if(cbPropData >= sizeof(float))
        {
            union { void *v; float *fl; } data = { pPropData };

            *data.fl = prim->deferred.eax1_dampening;

            *pcbReturned = sizeof(float);
            hr = DS_OK;
        }
        break;

    default:
        hr = E_PROP_ID_UNSUPPORTED;
        FIXME("Unhandled propid: 0x%08lx\n", propid);
        break;
    }

    return hr;
}


HRESULT EAX1Buffer_Query(DSBuffer *buf, DWORD propid, ULONG *pTypeSupport)
{
    (void)buf;

    switch(propid)
    {
    case DSPROPERTY_EAX10BUFFER_ALL:
    case DSPROPERTY_EAX10BUFFER_REVERBMIX:
        *pTypeSupport = KSPROPERTY_SUPPORT_GET | KSPROPERTY_SUPPORT_SET;
        return DS_OK;

    default:
        FIXME("Unhandled propid: 0x%08lx\n", propid);
    }
    return E_PROP_ID_UNSUPPORTED;
}

HRESULT EAX1Buffer_Set(DSBuffer *buf, DWORD propid, void *pPropData, ULONG cbPropData)
{
    HRESULT hr;

    hr = DSERR_INVALIDPARAM;
    switch(propid)
    {
    /* NOTE: DSPROPERTY_EAX10BUFFER_ALL is for EAX10BUFFERPROPERTIES, however
     * that struct just contains the single ReverbMix float property.
     */
    case DSPROPERTY_EAX10BUFFER_ALL:
    case DSPROPERTY_EAX10BUFFER_REVERBMIX:
        if(cbPropData >= sizeof(float))
        {
            union { const void *v; const float *fl; } data = { pPropData };
            TRACE("Reverb Mix: %f\n", *data.fl);

            buf->deferred.eax.lRoom = gain_to_mB(*data.fl);
            buf->deferred.eax1_reverbmix = *data.fl;
            ApplyFilterParams(buf, &buf->deferred.eax, APPLY_WET_PARAMS);

            buf->dirty.bit.send0_filter = 1;
            hr = DS_OK;
        }
        break;

    default:
        hr = E_PROP_ID_UNSUPPORTED;
        FIXME("Unhandled propid: 0x%08lx\n", propid);
        break;
    }

    return hr;
}

HRESULT EAX1Buffer_Get(DSBuffer *buf, DWORD propid, void *pPropData, ULONG cbPropData, ULONG *pcbReturned)
{
    HRESULT hr;

    hr = DSERR_INVALIDPARAM;
    switch(propid)
    {
    case DSPROPERTY_EAX10BUFFER_ALL:
    case DSPROPERTY_EAX10BUFFER_REVERBMIX:
        if(cbPropData >= sizeof(float))
        {
            union { void *v; float *fl; } data = { pPropData };

            *data.fl = buf->current.eax1_reverbmix;
            *pcbReturned = sizeof(float);
            hr = DS_OK;
        }
        break;

    default:
        hr = E_PROP_ID_UNSUPPORTED;
        FIXME("Unhandled propid: 0x%08lx\n", propid);
        break;
    }

    return hr;
}
