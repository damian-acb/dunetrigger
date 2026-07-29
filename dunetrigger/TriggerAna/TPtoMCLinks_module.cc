#include "art/Framework/Core/EDProducer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Services/Registry/ServiceHandle.h"
#include "canvas/Persistency/Common/Assns.h"
#include "canvas/Persistency/Common/Ptr.h"
#include "cetlib_except/exception.h"

#include "fhiclcpp/ParameterSet.h"
#include "fhiclcpp/types/Atom.h"
#include "fhiclcpp/types/Table.h"

#include "nusimdata/SimulationBase/MCParticle.h"
#include "detdataformats/trigger/TriggerPrimitive.hpp"
#include "lardataobj/Simulation/SimChannel.h"
#include "lardataobj/Simulation/sim.h"

#include "lardata/DetectorInfoServices/DetectorClocksService.h"
#include "larcore/Geometry/Geometry.h"
#include "larcore/Geometry/WireReadout.h"
#include "larsim/MCCheater/BackTrackerService.h"
#include "larsim/MCCheater/ParticleInventoryService.h"

#include "dunetrigger/TriggerSim/TPAlgTools/TPAlgTPCTool.hh"

#include <map>
#include <vector>
#include <algorithm>
#include <iostream>

namespace dune {

  class TPtoMCLinks : public art::EDProducer {
  public:
    // 1. FHiCL Configuration
    struct Config {
      using Name = fhicl::Name;
      using Comment = fhicl::Comment;

      fhicl::Atom<std::string> TPLabel{
        Name("TPLabel"), Comment("Module label for dunedaq Trigger Primitives")
      };
      fhicl::Atom<std::string> G4Label{
        Name("G4Label"), Comment("Module label for Geant4 MCParticles (e.g., 'largeant')")
      };
      
      // Time offsets (in TDC ticks) to align the DAQ time with the Sim time per plane
      fhicl::Atom<int> OffsetU{Name("OffsetU"), Comment("TDC offset for U plane"), 0};
      fhicl::Atom<int> OffsetV{Name("OffsetV"), Comment("TDC offset for V plane"), 0};
      fhicl::Atom<int> OffsetZ{Name("OffsetZ"), Comment("TDC offset for W/Z Collection plane"), 0};
    };

    using Parameters = art::EDProducer::Table<Config>;

    explicit TPtoMCLinks(Parameters const& p);
    void produce(art::Event& event) override;

  private:
    std::string fTPLabel;
    std::string fG4Label;
    int fOffsetU;
    int fOffsetV;
    int fOffsetZ;

  };

  TPtoMCLinks::TPtoMCLinks(Parameters const& p) 
    : EDProducer{p}
    , fTPLabel(p().TPLabel())
    , fG4Label(p().G4Label())
    , fOffsetU(p().OffsetU())
    , fOffsetV(p().OffsetV())
    , fOffsetZ(p().OffsetZ())
  {
    produces<art::Assns<dunedaq::trgdataformats::TriggerPrimitive, simb::MCParticle, sim::TrackIDE>>();
  }

  void TPtoMCLinks::produce(art::Event& event) 
  {
    auto assnCol = std::make_unique<art::Assns<dunedaq::trgdataformats::TriggerPrimitive, simb::MCParticle, sim::TrackIDE>>();

    auto tpHandle = event.getValidHandle<std::vector<dunedaq::trgdataformats::TriggerPrimitive>>(fTPLabel);
    auto mcHandle = event.getValidHandle<std::vector<simb::MCParticle>>(fG4Label);

    art::ServiceHandle<geo::Geometry const> geo;
    auto const& wire_serv= art::ServiceHandle<geo::WireReadout>()->Get();
    art::ServiceHandle<cheat::BackTrackerService const> bt_serv;
    art::ServiceHandle<cheat::ParticleInventoryService const> pi_serv;

    std::map<int, art::Ptr<simb::MCParticle>> trackIdToMCPtr;
    for (size_t i = 0; i < mcHandle->size(); ++i) {
      art::Ptr<simb::MCParticle> mcPtr(mcHandle, i);
      trackIdToMCPtr[mcPtr->TrackId()] = mcPtr;
    }

    for (size_t i = 0; i < tpHandle->size(); ++i) {

      art::Ptr<dunedaq::trgdataformats::TriggerPrimitive> tpPtr(tpHandle, i);

      auto rop_id = wire_serv.ChannelToROP(tpPtr->channel);
      geo::View_t view = wire_serv.View(rop_id);
      int offset = 0;
      if      (view == geo::kU) offset = fOffsetU;
      else if (view == geo::kV) offset = fOffsetV;
      else if (view == geo::kW || view == geo::kZ) offset = fOffsetZ;


      int sample_start = tpPtr->time_start / dunetrigger::TPAlgTPCTool::ADC_SAMPLING_RATE_IN_DTS;
      int samples_over_threshold = tpPtr->time_over_threshold / dunetrigger::TPAlgTPCTool::ADC_SAMPLING_RATE_IN_DTS;
      int sample_end = sample_start + samples_over_threshold;

      sample_start += offset;
      sample_end += offset;

      sample_start = std::max(0, sample_start);
      sample_end = std::max(0, sample_end);

      art::Ptr<sim::SimChannel> sim_channel = bt_serv->FindSimChannel(tpPtr->channel);
      std::vector<sim::TrackIDE> track_ides = sim_channel->TrackIDEs(sample_start, sample_end);
      
      if (track_ides.empty()){
          std::cout<<"Warning: No matched trackIDEs were found for trigger primittive at channel: "<< tpPtr->channel << " ,and time: " << tpPtr->time_peak << std::endl;
          continue;
      }
      for (const sim::TrackIDE& track_ide: track_ides){

        if (trackIdToMCPtr.find(track_ide.trackID) != trackIdToMCPtr.end()) {
          art::Ptr<simb::MCParticle> mcPtr = trackIdToMCPtr[track_ide.trackID];
        
          assnCol->addSingle(tpPtr, mcPtr, track_ide);
        }

      }

    }

    event.put(std::move(assnCol));
  }

}

DEFINE_ART_MODULE(dune::TPtoMCLinks)
