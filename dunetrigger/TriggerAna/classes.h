#ifndef TPTOMCDICT_H
#define TPTOMCDICT_H

#include <vector>
#include "canvas/Persistency/Common/Wrapper.h"
#include "canvas/Persistency/Common/Assns.h"
#include "nusimdata/SimulationBase/MCParticle.h"
#include "detdataformats/trigger/TriggerPrimitive.hpp" 
#include "lardataobj/Simulation/SimChannel.h" 

namespace {
  struct dictionary {

    sim::TrackIDE dummy_ide;
    std::vector<sim::TrackIDE> dummy_vec_ide;

    art::Assns<dunedaq::trgdataformats::TriggerPrimitive, simb::MCParticle, sim::TrackIDE> assn_tf_f;
    art::Assns<dunedaq::trgdataformats::TriggerPrimitive, simb::MCParticle, void> assn_tf_v;
    art::Assns<simb::MCParticle, dunedaq::trgdataformats::TriggerPrimitive, sim::TrackIDE> assn_ft_f;
    art::Assns<simb::MCParticle, dunedaq::trgdataformats::TriggerPrimitive, void> assn_ft_v;

    art::Wrapper<art::Assns<dunedaq::trgdataformats::TriggerPrimitive, simb::MCParticle, sim::TrackIDE>> wrap_tf_f;
    art::Wrapper<art::Assns<dunedaq::trgdataformats::TriggerPrimitive, simb::MCParticle, void>> wrap_tf_v;
    art::Wrapper<art::Assns<simb::MCParticle, dunedaq::trgdataformats::TriggerPrimitive, sim::TrackIDE>> wrap_ft_f;
    art::Wrapper<art::Assns<simb::MCParticle, dunedaq::trgdataformats::TriggerPrimitive, void>> wrap_ft_v;
  };
}

#endif // TPTOMCDICT_H
