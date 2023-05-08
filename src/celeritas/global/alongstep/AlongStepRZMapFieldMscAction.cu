//---------------------------------*-CUDA-*----------------------------------//
// Copyright 2023 UT-Battelle, LLC, and other Celeritas developers.
// See the top-level COPYRIGHT file for details.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)
//---------------------------------------------------------------------------//
//! \file celeritas/global/alongstep/AlongStepRZMapFieldMscAction.cu
//---------------------------------------------------------------------------//
#include "AlongStepRZMapFieldMscAction.hh"

#include "corecel/device_runtime_api.h"
#include "corecel/Assert.hh"
#include "corecel/Types.hh"
#include "corecel/sys/Device.hh"
#include "corecel/sys/KernelParamCalculator.device.hh"
#include "celeritas/em/FluctuationParams.hh"
#include "celeritas/em/UrbanMscParams.hh"
#include "celeritas/em/data/FluctuationData.hh"
#include "celeritas/em/data/UrbanMscData.hh"
#include "celeritas/em/msc/UrbanMsc.hh"
#include "celeritas/field/DormandPrinceStepper.hh"
#include "celeritas/field/MakeMagFieldPropagator.hh"
#include "celeritas/field/RZMapField.hh"
#include "celeritas/field/RZMapFieldData.hh"
#include "celeritas/field/RZMapFieldParams.hh"
#include "celeritas/global/CoreParams.hh"
#include "celeritas/global/CoreState.hh"
#include "celeritas/global/TrackLauncher.hh"

#include "detail/AlongStepImpl.hh"
#include "detail/FluctELoss.hh"

namespace celeritas
{
namespace
{
//---------------------------------------------------------------------------//
// TODO: these are now "unique" if MSC is in use: reuse across along-step
__global__ void
along_step_apply_msc_step_limit_kernel(DeviceCRef<CoreParamsData> const params,
                                       DeviceRef<CoreStateData> const state,
                                       DeviceCRef<UrbanMscData> const msc_data)
{
    auto launch
        = make_active_track_launcher(params,
                                     state,
                                     detail::apply_msc_step_limit<UrbanMsc>,
                                     UrbanMsc{msc_data});
    launch(KernelParamCalculator::thread_id());
}

//---------------------------------------------------------------------------//
__global__ void along_step_apply_rzmap_propagation_kernel(
    DeviceCRef<CoreParamsData> const params,
    DeviceRef<CoreStateData> const state,
    DeviceCRef<RZMapFieldParamsData> const field)
{
    auto launch = make_active_track_launcher(
        params,
        state,
        detail::ApplyPropagation{},
        [&field](ParticleTrackView const& particle, GeoTrackView* geo) {
            return make_mag_field_propagator<DormandPrinceStepper>(
                RZMapField(field), field.options, particle, geo);
        });
    launch(KernelParamCalculator::thread_id());
}

//---------------------------------------------------------------------------//
__global__ void
along_step_apply_msc_kernel(DeviceCRef<CoreParamsData> const params,
                            DeviceRef<CoreStateData> const state,
                            DeviceCRef<UrbanMscData> const msc_data)
{
    auto launch = make_active_track_launcher(
        params, state, detail::apply_msc<UrbanMsc>, UrbanMsc{msc_data});
    launch(KernelParamCalculator::thread_id());
}

//---------------------------------------------------------------------------//
__global__ void
along_step_update_time_kernel(DeviceCRef<CoreParamsData> const params,
                              DeviceRef<CoreStateData> const state)
{
    auto launch
        = make_active_track_launcher(params, state, detail::update_time);
    launch(KernelParamCalculator::thread_id());
}

//---------------------------------------------------------------------------//
__global__ void
along_step_apply_fluct_eloss_kernel(DeviceCRef<CoreParamsData> const params,
                                    DeviceRef<CoreStateData> const state,
                                    NativeCRef<FluctuationData> const fluct)
{
    using detail::FluctELoss;

    auto launch = make_active_track_launcher(
        params, state, detail::apply_eloss<FluctELoss>, FluctELoss{fluct});
    launch(KernelParamCalculator::thread_id());
}

//---------------------------------------------------------------------------//
__global__ void
along_step_update_track_kernel(DeviceCRef<CoreParamsData> const params,
                               DeviceRef<CoreStateData> const state)
{
    auto launch
        = make_active_track_launcher(params, state, detail::update_track);
    launch(KernelParamCalculator::thread_id());
}

//---------------------------------------------------------------------------//
}  // namespace

//---------------------------------------------------------------------------//
/*!
 * Launch the along-step action on device.
 */
void AlongStepRZMapFieldMscAction::execute(CoreParams const& params,
                                           CoreStateDevice& state) const
{
    CELER_LAUNCH_KERNEL(along_step_apply_msc_step_limit,
                        celeritas::device().default_block_size(),
                        state.size(),
                        params.ref<MemSpace::native>(),
                        state.ref(),
                        msc_->device_ref());
    CELER_LAUNCH_KERNEL(along_step_apply_rzmap_propagation,
                        celeritas::device().default_block_size(),
                        state.size(),
                        params.ref<MemSpace::native>(),
                        state.ref(),
                        field_->device_ref());
    CELER_LAUNCH_KERNEL(along_step_apply_msc,
                        celeritas::device().default_block_size(),
                        state.size(),
                        params.ref<MemSpace::native>(),
                        state.ref(),
                        msc_->device_ref());
    CELER_LAUNCH_KERNEL(along_step_update_time,
                        celeritas::device().default_block_size(),
                        state.size(),
                        params.ref<MemSpace::native>(),
                        state.ref());
    CELER_LAUNCH_KERNEL(along_step_apply_fluct_eloss,
                        celeritas::device().default_block_size(),
                        state.size(),
                        params.ref<MemSpace::native>(),
                        state.ref(),
                        fluct_->device_ref());
    CELER_LAUNCH_KERNEL(along_step_update_track,
                        celeritas::device().default_block_size(),
                        state.size(),
                        params.ref<MemSpace::native>(),
                        state.ref());
}

//---------------------------------------------------------------------------//
}  // namespace celeritas