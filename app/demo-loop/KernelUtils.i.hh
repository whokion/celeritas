//----------------------------------*-C++-*----------------------------------//
// Copyright 2021 UT-Battelle, LLC, and other Celeritas developers.
// See the top-level COPYRIGHT file for details.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)
//---------------------------------------------------------------------------//
//! \file KernelUtils.i.hh
//---------------------------------------------------------------------------//
#pragma once

#include "base/Macros.hh"
#include "geometry/LinearPropagator.hh"
#include "random/distributions/ExponentialDistribution.hh"

namespace demo_loop
{
//---------------------------------------------------------------------------//
/*!
 * Sample mean free path and calculate physics step limits.
 */
template<class Rng>
CELER_FUNCTION void calc_step_limits(const MaterialTrackView& mat,
                                     const ParticleTrackView& particle,
                                     PhysicsTrackView&        phys,
                                     SimTrackView&            sim,
                                     Rng&                     rng)
{
    // Sample mean free path
    if (!phys.has_interaction_mfp())
    {
        ExponentialDistribution<real_type> sample_exponential;
        phys.interaction_mfp(sample_exponential(rng));
    }

    // Calculate physics step limits and total macro xs
    real_type step = calc_tabulated_physics_step(mat, particle, phys);
    if (particle.is_stopped())
    {
        if (phys.macro_xs() == 0)
        {
            // If the particle is stopped and cannot undergo a discrete
            // interaction, kill it
            sim.alive(false);
            return;
        }
        // Set the interaction length and mfp to zero for active stopped
        // particles
        step = 0;
        phys.interaction_mfp(0);
    }
    phys.step_length(step);
}

//---------------------------------------------------------------------------//
/*!
 * Propagate up to the step length or next boundary, calculate the energy loss
 * over the step, and select the model for the discrete interaction.
 */
template<class Rng>
CELER_FUNCTION void move_and_select_model(GeoTrackView&      geo,
                                          ParticleTrackView& particle,
                                          PhysicsTrackView&  phys,
                                          SimTrackView&      sim,
                                          Rng&               rng,
                                          real_type*         edep,
                                          Interaction*       result)
{
    // Actual distance, limited by along-step length or geometry
    real_type step = phys.step_length();
    if (step > 0)
    {
        // Store the current volume
        auto pre_step_volume = geo.volume_id();

        // Propagate up to the step length or next boundary
        LinearPropagator propagate(&geo);
        auto             geo_step = propagate(step);
        step                      = geo_step.distance;

        // Particle entered a new volume before reaching the interaction point
        if (geo_step.volume != pre_step_volume)
        {
            *result = Interaction::from_unchanged(particle.energy(), geo.dir());
            result->action = Action::entered_volume;
        }
    }
    phys.step_length(phys.step_length() - step);

    // Kill the track if it's outside the valid geometry region
    if (geo.is_outside())
    {
        result->action = Action::escaped;
        sim.alive(false);
    }

    // Calculate energy loss over the step length
    auto eloss = calc_energy_loss(particle, phys, step);
    *edep += eloss.value();
    particle.energy(
        ParticleTrackView::Energy{particle.energy().value() - eloss.value()});

    // Reduce the remaining mean free path
    real_type mfp = phys.interaction_mfp() - step * phys.macro_xs();
    phys.interaction_mfp(soft_zero(mfp) ? 0 : mfp);

    ModelId model{};
    if (phys.interaction_mfp() <= 0)
    {
        // Reached the interaction point: sample the process and determine the
        // corresponding model
        auto ppid_mid = select_process_and_model(particle, phys, rng);
        model         = ppid_mid.model;
    }
    phys.model_id(model);
}

//---------------------------------------------------------------------------//
/*!
 * Apply secondary cutoffs and process interaction change.
 */
CELER_FUNCTION void post_process(const CutoffView&  cutoffs,
                                 GeoTrackView&      geo,
                                 ParticleTrackView& particle,
                                 PhysicsTrackView&  phys,
                                 SimTrackView&      sim,
                                 real_type*         edep,
                                 const Interaction& result)
{
    // Update the track state from the interaction
    // TODO: handle recoverable errors
    CELER_ASSERT(result);
    if (action_killed(result.action))
    {
        sim.alive(false);
    }
    else if (!action_unchanged(result.action))
    {
        particle.energy(result.energy);
        geo.set_dir(result.direction);
    }

    // Deposit energy from interaction
    *edep += result.energy_deposition.value();

    // Kill secondaries with energy below the production threshold and deposit
    // their energy
    for (auto& secondary : result.secondaries)
    {
        if (secondary.energy < cutoffs.energy(secondary.particle_id))
        {
            *edep += secondary.energy.value();
            secondary = {};
        }
    }

    // Reset the physics state if a discrete interaction occured
    if (phys.model_id())
    {
        phys = {};
    }
}

//---------------------------------------------------------------------------//
} // namespace demo_loop
