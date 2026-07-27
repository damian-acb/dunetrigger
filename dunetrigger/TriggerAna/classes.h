#ifndef TPTOMCDICT_H
#define TPTOMCDICT_H

#include "canvas/Persistency/Common/Wrapper.h"
#include "canvas/Persistency/Common/Assns.h"
#include "nusimdata/SimulationBase/MCParticle.h"
#include "detdataformats/trigger/TriggerPrimitive.hpp" 

namespace {
  struct dictionary {
    art::Assns<dunedaq::trgdataformats::TriggerPrimitive, simb::MCParticle, float> assn_tf_f;
    art::Assns<simb::MCParticle, dunedaq::trgdataformats::TriggerPrimitive, float> assn_ft_f;
    art::Assns<dunedaq::trgdataformats::TriggerPrimitive, simb::MCParticle, void>  assn_tf_v;
    art::Assns<simb::MCParticle, dunedaq::trgdataformats::TriggerPrimitive, void>  assn_ft_v;

    art::Wrapper<art::Assns<dunedaq::trgdataformats::TriggerPrimitive, simb::MCParticle, float>> wrap_tf_f;
    art::Wrapper<art::Assns<simb::MCParticle, dunedaq::trgdataformats::TriggerPrimitive, float>> wrap_ft_f;
  };
}

#endif // TPTOMCDICT_H
