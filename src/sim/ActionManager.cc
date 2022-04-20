//----------------------------------*-C++-*----------------------------------//
// Copyright 2022 UT-Battelle, LLC, and other Celeritas developers.
// See the top-level COPYRIGHT file for details.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)
//---------------------------------------------------------------------------//
//! \file ActionManager.cc
//---------------------------------------------------------------------------//
#include "ActionManager.hh"

namespace celeritas
{
//---------------------------------------------------------------------------//
/*!
 * Register an implicit action.
 */
void ActionManager::insert(SPConstImplicit action)
{
    CELER_EXPECT(action);
    this->insert_impl(std::move(action), nullptr);
}

//---------------------------------------------------------------------------//
/*!
 * Register an explicit action.
 */
void ActionManager::insert(SPConstExplicit action)
{
    CELER_EXPECT(action);
    // Get explicit action pointer
    PConstExplicit expl = action.get();
    this->insert_impl(std::move(action), expl);
}

//---------------------------------------------------------------------------//
// Call the given action ID with host or device data.
template<MemSpace M>
void ActionManager::invoke(ActionId id, const CoreRef<M>& data) const
{
    CELER_ASSERT(id < actions_.size());
    PConstExplicit action = actions_[id.unchecked_get()].expl;
    CELER_VALIDATE(action,
                   << "action '" << actions_[id.unchecked_get()].label
                   << "' is implicit and cannot be invoked");
    action->execute(data);
}

//---------------------------------------------------------------------------//
/*!
 * Find the action corresponding to an label.
 */
ActionId ActionManager::find_action(const std::string& label) const
{
    auto iter = action_ids_.find(label);
    if (iter == action_ids_.end())
        return {};
    return iter->second;
}

//---------------------------------------------------------------------------//
// HELPER FUNCTIONS
//---------------------------------------------------------------------------//
void ActionManager::insert_impl(SPConstAction&& action, PConstExplicit expl)
{
    CELER_ASSERT(action);

    auto label = action->label();
    CELER_VALIDATE(!label.empty(), << "action label is empty");

    auto id = action->action_id();
    CELER_VALIDATE(id == this->next_id(),
                   << "incorrect action id {" << id.unchecked_get()
                   << "} for action '" << label << "' (should be {"
                   << this->next_id().get() << "})");
    auto iter_inserted = action_ids_.insert({label, id});
    CELER_VALIDATE(iter_inserted.second,
                   << "duplicate action label '" << label << "'");

    actions_.emplace_back(
        ActionData{std::move(label), std::move(action), expl});
}

//---------------------------------------------------------------------------//
// Explicit template instantiation
//---------------------------------------------------------------------------//

template void
ActionManager::invoke(ActionId, const CoreRef<MemSpace::host>&) const;
template void
ActionManager::invoke(ActionId, const CoreRef<MemSpace::device>&) const;

//---------------------------------------------------------------------------//
} // namespace celeritas