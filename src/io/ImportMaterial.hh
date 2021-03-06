//----------------------------------*-C++-*----------------------------------//
// Copyright 2020 UT-Battelle, LLC, and other Celeritas developers.
// See the top-level COPYRIGHT file for details.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)
//---------------------------------------------------------------------------//
//! \file ImportMaterial.hh
//---------------------------------------------------------------------------//
#pragma once

#include <string>
#include <map>
#include <vector>

#include "ImportElement.hh"
#include "base/Types.hh"

namespace celeritas
{
//---------------------------------------------------------------------------//
/*!
 * Enum for storing G4State enumerators.
 * [See G4Material.hh]
 */
enum class ImportMaterialState
{
    not_defined,
    solid,
    liquid,
    gas
};

//---------------------------------------------------------------------------//
/*!
 * Store particle production cut.
 */
struct ImportProductionCut
{
    double energy; //!< [MeV]
    double range;  //!< [cm]
};

//---------------------------------------------------------------------------//
/*!
 * Store elemental composition of a given material.
 */
struct ImportMatElemComponent
{
    unsigned int element_id;    //!< Index of element in ImportElement
    double       mass_fraction; //!< [g/cm^3]
    double       number_fraction;
};

//---------------------------------------------------------------------------//
/*!
 * Store material data.
 *
 * \sa ImportData
 */
struct ImportMaterial
{
    std::string                         name;
    ImportMaterialState                 state;
    double                              temperature;        //!< [K]
    double                              density;            //!< [g/cm^3]
    double                              electron_density;   //!< [1/cm^3]
    double                              number_density;     //!< [1/cm^3]
    double                              radiation_length;   //!< [cm]
    double                              nuclear_int_length; //!< [cm]
    std::map<int, ImportProductionCut>  pdg_cutoffs;        //!< Cutoff per PDG
    std::vector<ImportMatElemComponent> elements;
};

//---------------------------------------------------------------------------//
} // namespace celeritas
