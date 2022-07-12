//----------------------------------*-C++-*----------------------------------//
// Copyright 2022 UT-Battelle, LLC, and other Celeritas developers.
// See the top-level COPYRIGHT file for details.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)
//---------------------------------------------------------------------------//
//! \file celeritas/TestEm3Base.cc
//---------------------------------------------------------------------------//
#include "TestEm3Base.hh"

#include <string>

#include "celeritas/em/FluctuationParams.hh"
#include "celeritas/em/process/BremsstrahlungProcess.hh"
#include "celeritas/em/process/ComptonProcess.hh"
#include "celeritas/em/process/EIonizationProcess.hh"
#include "celeritas/em/process/EPlusAnnihilationProcess.hh"
#include "celeritas/em/process/GammaConversionProcess.hh"
#include "celeritas/em/process/MultipleScatteringProcess.hh"
#include "celeritas/em/process/PhotoelectricProcess.hh"
#include "celeritas/ext/GeantImporter.hh"
#include "celeritas/ext/GeantSetup.hh"
#include "celeritas/geo/GeoMaterialParams.hh"
#include "celeritas/geo/GeoParams.hh"
#include "celeritas/global/ActionManager.hh"
#include "celeritas/io/ImportData.hh"
#include "celeritas/mat/MaterialParams.hh"
#include "celeritas/phys/CutoffParams.hh"
#include "celeritas/phys/ImportedProcessAdapter.hh"
#include "celeritas/phys/ParticleParams.hh"
#include "celeritas/phys/PhysicsParams.hh"
#include "celeritas/random/RngParams.hh"

using namespace celeritas;

namespace celeritas_test
{
//---------------------------------------------------------------------------//
namespace
{
//---------------------------------------------------------------------------//
std::string gdml_filename(const char* basename)
{
    return std::string(basename) + std::string(".gdml");
}

//---------------------------------------------------------------------------//
ImportData load_import_data(std::string filename)
{
    GeantSetupOptions options;
    options.physics            = GeantSetupPhysicsList::em_basic;
    options.em_bins_per_decade = 14;
    GeantImporter import(GeantSetup(filename, options));
    return import();
}

//---------------------------------------------------------------------------//
} // namespace

//---------------------------------------------------------------------------//
// PROTECTED MEMBER FUNCTIONS
//---------------------------------------------------------------------------//
auto TestEm3Base::build_material() -> SPConstMaterial
{
    return MaterialParams::from_import(this->imported_data());
}

//---------------------------------------------------------------------------//
auto TestEm3Base::build_geomaterial() -> SPConstGeoMaterial
{
    GeoMaterialParams::Input input;
    input.geometry       = this->geometry();
    input.materials      = this->material();
    const auto& imported = this->imported_data();

    input.volume_to_mat.resize(imported.volumes.size());
    input.volume_labels.resize(imported.volumes.size());
    for (auto volume_idx :
         range<VolumeId::size_type>(input.volume_to_mat.size()))
    {
        input.volume_to_mat[volume_idx]
            = MaterialId{imported.volumes[volume_idx].material_id};
        input.volume_labels[volume_idx]
            = Label::from_geant(imported.volumes[volume_idx].name);
    }
    return std::make_shared<GeoMaterialParams>(std::move(input));
}

//---------------------------------------------------------------------------//
auto TestEm3Base::build_particle() -> SPConstParticle
{
    return ParticleParams::from_import(this->imported_data());
}

//---------------------------------------------------------------------------//
auto TestEm3Base::build_cutoff() -> SPConstCutoff
{
    return CutoffParams::from_import(
        this->imported_data(), this->particle(), this->material());
}

//---------------------------------------------------------------------------//
auto TestEm3Base::build_physics() -> SPConstPhysics
{
    PhysicsParams::Input input;
    input.materials      = this->material();
    input.particles      = this->particle();
    input.options        = this->build_physics_options();
    input.action_manager = this->action_mgr().get();

    // For now, always use energy fluctuations
    input.fluctuation = std::make_shared<FluctuationParams>(input.particles,
                                                            input.materials);

    BremsstrahlungProcess::Options brem_options;
    brem_options.combined_model  = true;
    brem_options.enable_lpm      = true;
    brem_options.use_integral_xs = true;

    GammaConversionProcess::Options conv_options;
    conv_options.enable_lpm = true;

    EPlusAnnihilationProcess::Options epgg_options;
    epgg_options.use_integral_xs = true;

    EIonizationProcess::Options ioni_options;
    ioni_options.use_integral_xs = true;

    auto process_data
        = std::make_shared<ImportedProcesses>(this->imported_data().processes);
    input.processes.push_back(
        std::make_shared<ComptonProcess>(input.particles, process_data));
    input.processes.push_back(std::make_shared<PhotoelectricProcess>(
        input.particles, input.materials, process_data));
    input.processes.push_back(std::make_shared<GammaConversionProcess>(
        input.particles, process_data, conv_options));
    input.processes.push_back(std::make_shared<EPlusAnnihilationProcess>(
        input.particles, epgg_options));
    input.processes.push_back(std::make_shared<EIonizationProcess>(
        input.particles, process_data, ioni_options));
    input.processes.push_back(std::make_shared<BremsstrahlungProcess>(
        input.particles, input.materials, process_data, brem_options));
    if (this->enable_msc())
    {
        input.processes.push_back(std::make_shared<MultipleScatteringProcess>(
            input.particles, input.materials, process_data));
    }
    return std::make_shared<PhysicsParams>(std::move(input));
}

//---------------------------------------------------------------------------//
auto TestEm3Base::build_physics_options() const -> PhysicsOptions
{
    PhysicsOptions options;
    options.secondary_stack_factor = this->secondary_stack_factor();
    return options;
}

//---------------------------------------------------------------------------//
// Lazily set up and load geant4
auto TestEm3Base::imported_data() const -> const ImportData&
{
    static ImportData imported = load_import_data(this->test_data_path(
        "celeritas", gdml_filename(this->geometry_basename()).c_str()));
    return imported;
}

//---------------------------------------------------------------------------//
} // namespace celeritas_test