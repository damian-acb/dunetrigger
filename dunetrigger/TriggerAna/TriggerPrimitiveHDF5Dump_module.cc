/**
 * @file TriggerPrimitiveHDF5Dump_module.cc
 *
 * @brief This module is an analyzer that writes trigger primitives and their backtracked
 * G4 truth information to an HDF5 file.
 *
 * For every event a group "Events/<event>" is created in the output file, holding the
 * datasets "TriggerPrimitives" (one row per TP/TrackIDE pair), "MCParticles" (one row per
 * simb::MCParticle), "MCNeutrino" (one row per neutrino interaction) and, if requested,
 * one "SimEnergyDeposit_<instance>" dataset per sim::SimEnergyDeposit product instance.
 *
 * Trigger primitives are matched to energy depositions over the sample window of the TP
 * shifted by a per-view offset.
 *
 * SimChannels and the MCParticle -> MCTruth association are read directly from the event
 * rather than through BackTrackerService / ParticleInventoryService. Those services build
 * their maps from an sPreProcessEvent callback, i.e. before any module has run, so they are
 * empty whenever the products come from producers in the same job. Reading the products
 * ourselves works the same whether this runs standalone or in the producer path.
 */
////////////////////////////////////////////////////////////////////////
// Class:       TriggerPrimitiveHDF5Dump
// Plugin Type: analyzer (Unknown Unknown)
// File:        TriggerPrimitiveHDF5Dump_module.cc
////////////////////////////////////////////////////////////////////////

#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Principal/Provenance.h"
#include "art/Framework/Principal/Selector.h"
#include "art/Framework/Services/Registry/ServiceHandle.h"
#include "canvas/Persistency/Common/FindOneP.h"
#include "canvas/Persistency/Provenance/ProductID.h"
#include "canvas/Utilities/InputTag.h"
#include "fhiclcpp/ParameterSet.h"

#include "detdataformats/trigger/TriggerPrimitive.hpp"

#include "larcore/Geometry/WireReadout.h"
#include "larcoreobj/SimpleTypesAndConstants/RawTypes.h"
#include "lardata/DetectorInfoServices/DetectorClocksService.h"
#include "lardata/DetectorInfoServices/DetectorPropertiesService.h"
#include "lardataobj/Simulation/SimChannel.h"
#include "lardataobj/Simulation/SimEnergyDeposit.h"

#include "nusimdata/SimulationBase/MCNeutrino.h"
#include "nusimdata/SimulationBase/MCParticle.h"
#include "nusimdata/SimulationBase/MCTruth.h"

#include "dunetrigger/TriggerSim/TPAlgTools/TPAlgTPCTool.hh"
#include "dunetrigger/TriggerSim/Verbosity.hh"

#include "hep_hpc/hdf5/File.hpp"
#include "hep_hpc/hdf5/Group.hpp"
#include "hep_hpc/hdf5/Ntuple.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

constexpr int INVALID_TRACK_ID = -9999;
constexpr char UNKNOWN_GENERATOR[] = "unknown";
constexpr char DEFAULT_SED_INSTANCE[] = "DefaultVolume";
constexpr size_t GENERATOR_LABEL_SIZE = 40;
constexpr size_t NTUPLE_BUFFER_SIZE = 1000;
constexpr size_t NU_NTUPLE_BUFFER_SIZE = 10;

namespace dunetrigger {
  class TriggerPrimitiveHDF5Dump;
}

class dunetrigger::TriggerPrimitiveHDF5Dump : public art::EDAnalyzer {
public:
  explicit TriggerPrimitiveHDF5Dump(fhicl::ParameterSet const &p);
  // The compiler-generated destructor is fine for non-base
  // classes without bare pointers or other resource use.

  // Plugins should not be copied or assigned.
  TriggerPrimitiveHDF5Dump(TriggerPrimitiveHDF5Dump const &) = delete;
  TriggerPrimitiveHDF5Dump(TriggerPrimitiveHDF5Dump &&) = delete;
  TriggerPrimitiveHDF5Dump &operator=(TriggerPrimitiveHDF5Dump const &) = delete;
  TriggerPrimitiveHDF5Dump &operator=(TriggerPrimitiveHDF5Dump &&) = delete;

  // Required functions.
  void analyze(art::Event const &e) override;

  // Selected optional functions.
  void beginJob() override;

private:
  using Ntuple_tp = hep_hpc::hdf5::Ntuple<int, int, uint64_t, int, int, int, double,
                                          double, int, int, double>;
  using Ntuple_mc = hep_hpc::hdf5::Ntuple<int, int, std::array<char, GENERATOR_LABEL_SIZE>, double,
                                          int, double, double, double, double>;
  using Ntuple_sed =
      hep_hpc::hdf5::Ntuple<int, int, int, double, double, double, double, double, double>;
  using Ntuple_nu = hep_hpc::hdf5::Ntuple<int, double, int, int, int, int>;

  int view_window_offset(geo::View_t view) const;

  void write_mcparticles(art::Event const &e, int event) const;
  void write_mcneutrinos(art::Event const &e, int event) const;
  void write_simenergydeposits(art::Event const &e, int event) const;
  void write_triggerprimitives(art::Event const &e, int event) const;

  art::InputTag tp_tag;
  art::InputTag g4_tag;
  art::InputTag atmos_tag;
  art::InputTag simchannel_tag;
  std::string output_filename;

  std::array<int, 3> bt_view_offsets;

  bool dump_mcparticles;
  bool dump_mcneutrinos;
  bool dump_sed;
  int verbosity_;

  std::unique_ptr<hep_hpc::hdf5::File> h5_file;
  std::unique_ptr<hep_hpc::hdf5::Group> events_group;
};

dunetrigger::TriggerPrimitiveHDF5Dump::TriggerPrimitiveHDF5Dump(fhicl::ParameterSet const &p)
    : EDAnalyzer{p}
    , tp_tag(p.get<art::InputTag>("tp_tag"))
    , g4_tag(p.get<art::InputTag>("g4_tag"))
    , atmos_tag(p.get<art::InputTag>("atmos_tag", "generator"))
    , simchannel_tag(p.get<art::InputTag>("simchannel_tag", "tpcrawdecoder:simpleSC"))
    , output_filename(p.get<std::string>("output_filename"))
    , bt_view_offsets{p.get<int>("U_window_offset", 0), p.get<int>("V_window_offset", 0),
                      p.get<int>("X_window_offset", 0)}
    , dump_mcparticles(p.get<bool>("dump_mcparticles", true))
    , dump_mcneutrinos(p.get<bool>("dump_mcneutrinos", true))
    , dump_sed(p.get<bool>("dump_sed", false))
    , verbosity_(p.get<int>("verbosity", 0))
{
  consumes<std::vector<dunedaq::trgdataformats::TriggerPrimitive>>(tp_tag);
  consumes<std::vector<sim::SimChannel>>(simchannel_tag);
  if (dump_mcparticles) {
    consumes<std::vector<simb::MCParticle>>(g4_tag);
    consumes<art::Assns<simb::MCTruth, simb::MCParticle>>(g4_tag);
  }
  if (dump_mcneutrinos) consumes<std::vector<simb::MCTruth>>(atmos_tag);
}

void dunetrigger::TriggerPrimitiveHDF5Dump::beginJob() {
  h5_file = std::make_unique<hep_hpc::hdf5::File>(output_filename, H5F_ACC_TRUNC);
  events_group = std::make_unique<hep_hpc::hdf5::Group>(*h5_file, "Events");
}

int dunetrigger::TriggerPrimitiveHDF5Dump::view_window_offset(geo::View_t view) const {
  switch (view) {
    case geo::kU:
      return bt_view_offsets[0];
    case geo::kV:
      return bt_view_offsets[1];
    case geo::kW:
      return bt_view_offsets[2];
    default:
      return 0;
  }
}

void dunetrigger::TriggerPrimitiveHDF5Dump::write_mcparticles(art::Event const &e,
                                                              int event) const {
  auto mc_handle = e.getValidHandle<std::vector<simb::MCParticle>>(g4_tag);

  // Read the MCParticle -> MCTruth association straight from the event instead of asking
  // ParticleInventoryService. The service fills its maps from an sPreProcessEvent callback,
  // so they are empty when the products come from producers in this same job.
  art::FindOneP<simb::MCTruth> mc_to_truth(mc_handle, e, g4_tag);

  Ntuple_mc::column_info_t const cols_mc{"trackID",  "PDG", "GeneratorLabel", "InitialEnergy",
                                         "motherID", "Vx",  "Vz",             "EndX",
                                         "EndZ"};
  Ntuple_mc ntuple(*h5_file, "Events/" + std::to_string(event) + "/MCParticles", cols_mc,
                   NTUPLE_BUFFER_SIZE);

  // The generator that produced a particle is only recoverable through the module label of
  // the MCTruth product its chain hangs off of.
  std::map<art::ProductID, std::string> id_to_label;
  for (auto const &handle : e.getMany<std::vector<simb::MCTruth>>()) {
    id_to_label[handle.id()] = handle.provenance()->moduleLabel();
  }

  std::array<char, GENERATOR_LABEL_SIZE> gen_label;

  for (size_t i = 0; i < mc_handle->size(); ++i) {
    simb::MCParticle const &mc = (*mc_handle)[i];

    art::Ptr<simb::MCTruth> const &mctruth_ptr = mc_to_truth.at(i);
    std::string generator_label = UNKNOWN_GENERATOR;

    if (mctruth_ptr.isNonnull() && id_to_label.count(mctruth_ptr.id())) {
      generator_label = id_to_label[mctruth_ptr.id()];
    }

    gen_label.fill('\0');
    std::strncpy(gen_label.data(), generator_label.c_str(), GENERATOR_LABEL_SIZE - 1);

    ntuple.insert(mc.TrackId(), mc.PdgCode(), gen_label, mc.E(), mc.Mother(), mc.Vx(), mc.Vz(),
                  mc.EndX(), mc.EndZ());
  }

  if (verbosity_ >= Verbosity::kDebug)
    std::cout << "Wrote " << mc_handle->size() << " MCParticles" << std::endl;
}

void dunetrigger::TriggerPrimitiveHDF5Dump::write_mcneutrinos(art::Event const &e,
                                                              int event) const {
  auto nu_handle = e.getValidHandle<std::vector<simb::MCTruth>>(atmos_tag);

  Ntuple_nu::column_info_t const cols_nu{"NuPDG", "NuEnergy",        "CCNC",
                                         "Mode",  "InteractionType", "TargetPDG"};
  Ntuple_nu ntuple(*h5_file, "Events/" + std::to_string(event) + "/MCNeutrino", cols_nu,
                   NU_NTUPLE_BUFFER_SIZE);

  for (auto const &truth : *nu_handle) {
    if (!truth.NeutrinoSet()) continue;

    simb::MCNeutrino const &nu = truth.GetNeutrino();
    ntuple.insert(nu.Nu().PdgCode(), nu.Nu().E(), nu.CCNC(), nu.Mode(), nu.InteractionType(),
                  nu.Target());
  }

  if (verbosity_ >= Verbosity::kDebug)
    std::cout << "Wrote MCNeutrinos from " << nu_handle->size() << " MCTruths" << std::endl;
}

void dunetrigger::TriggerPrimitiveHDF5Dump::write_simenergydeposits(art::Event const &e,
                                                                    int event) const {
  Ntuple_sed::column_info_t const cols_sed{"trackID",   "PDG",       "NumElectrons",
                                           "Energy",    "MidPointX", "MidPointY",
                                           "MidPointZ", "Time",      "StepLength"};

  // largeant writes one SimEnergyDeposit product per detector volume, so each instance gets
  // its own dataset.
  for (auto const &sed_handle :
       e.getMany<std::vector<sim::SimEnergyDeposit>>(art::ModuleLabelSelector(g4_tag.label()))) {
    if (sed_handle->empty()) continue;

    std::string instance_label = sed_handle.provenance()->productInstanceName();
    if (instance_label.empty()) instance_label = DEFAULT_SED_INSTANCE;

    if (verbosity_ >= Verbosity::kDebug)
      std::cout << "Writing SimEnergyDeposits for instance " << instance_label << std::endl;

    Ntuple_sed ntuple(*h5_file,
                      "Events/" + std::to_string(event) + "/SimEnergyDeposit_" + instance_label,
                      cols_sed, NTUPLE_BUFFER_SIZE);

    for (auto const &sed : *sed_handle) {
      ntuple.insert(sed.TrackID(), sed.PdgCode(), sed.NumElectrons(), sed.Energy(),
                    sed.MidPointX(), sed.MidPointY(), sed.MidPointZ(), sed.Time(),
                    sed.StepLength());
    }
  }
}

void dunetrigger::TriggerPrimitiveHDF5Dump::write_triggerprimitives(art::Event const &e,
                                                                    int event) const {
  auto const clockData = art::ServiceHandle<detinfo::DetectorClocksService const>()->DataFor(e);
  auto const detProp =
      art::ServiceHandle<detinfo::DetectorPropertiesService const>()->DataFor(e, clockData);
  auto const &wire_serv = art::ServiceHandle<geo::WireReadout>()->Get();

  auto tp_handle = e.getValidHandle<std::vector<dunedaq::trgdataformats::TriggerPrimitive>>(tp_tag);

  // Index the SimChannels by channel ourselves rather than going through BackTrackerService,
  // whose map is built before any module runs and so is empty when the SimChannels are made
  // by a producer in this same job. sim::SimChannel::TrackIDEs does the rest of the work the
  // BackTracker would have done.
  auto sc_handle = e.getValidHandle<std::vector<sim::SimChannel>>(simchannel_tag);
  std::unordered_map<raw::ChannelID_t, sim::SimChannel const *> simchannels;
  simchannels.reserve(sc_handle->size());
  for (auto const &sc : *sc_handle) simchannels[sc.Channel()] = &sc;

  Ntuple_tp::column_info_t const cols_tp{
      "channel", "samples_over_threshold", "time_start", "samples_to_peak", "adc_integral",
      "adc_peak", "x", "z", "view", "trackIDE_trackID",
      "trackIDE_fraction"};
  Ntuple_tp ntuple(*h5_file, "Events/" + std::to_string(event) + "/TriggerPrimitives", cols_tp,
                   NTUPLE_BUFFER_SIZE);

  for (auto const &tp : *tp_handle) {
    auto const rop_id = wire_serv.ChannelToROP(tp.channel);
    geo::View_t const view = wire_serv.View(rop_id);
    int const offset = view_window_offset(view);

    int const samples_over_threshold =
        tp.time_over_threshold / TPAlgTPCTool::ADC_SAMPLING_RATE_IN_DTS;
    int const samples_to_peak =
        (tp.time_peak - tp.time_start) / TPAlgTPCTool::ADC_SAMPLING_RATE_IN_DTS;

    int sample_start = tp.time_start / TPAlgTPCTool::ADC_SAMPLING_RATE_IN_DTS;
    int sample_end = sample_start + samples_over_threshold;
    sample_start += offset;
    sample_end += offset;
    sample_start = std::max(0, sample_start);
    sample_end = std::max(0, sample_end);

    auto const sc_it = simchannels.find(tp.channel);
    std::vector<sim::TrackIDE> track_ides;
    if (sc_it != simchannels.end())
      track_ides = sc_it->second->TrackIDEs(sample_start, sample_end);

    geo::WireID const &wid = wire_serv.ChannelToWire(tp.channel).front();
    double const sample_peak = tp.time_peak / TPAlgTPCTool::ADC_SAMPLING_RATE_IN_DTS;

    double const z = wire_serv.Wire(wid).GetCenter().Z();
    double const x = detProp.ConvertTicksToX(sample_peak, wid.Plane, wid.TPC, wid.Cryostat);

    if (track_ides.empty()) {
      if (verbosity_ >= Verbosity::kVerbose)
        std::cout << "No matched TrackIDEs for TP on channel " << tp.channel << " at time "
                  << tp.time_peak << std::endl;
      ntuple.insert(tp.channel, samples_over_threshold, tp.time_start, samples_to_peak,
                    tp.adc_integral, tp.adc_peak, x, z, view,
                    INVALID_TRACK_ID, INVALID_TRACK_ID);
      continue;
    }

    for (sim::TrackIDE const &track_ide : track_ides) {
      if (verbosity_ >= Verbosity::kVerbose)
        std::cout << "G4 MC particle ID: " << track_ide.trackID << std::endl;

      ntuple.insert(tp.channel, samples_over_threshold, tp.time_start, samples_to_peak,
                    tp.adc_integral, tp.adc_peak, x, z, view,
                    track_ide.trackID, track_ide.energyFrac);
    }
  }

  if (verbosity_ >= Verbosity::kDebug)
    std::cout << "Wrote " << tp_handle->size() << " TriggerPrimitives" << std::endl;
}

void dunetrigger::TriggerPrimitiveHDF5Dump::analyze(art::Event const &e) {
  int const event = e.id().event();

  if (verbosity_ >= Verbosity::kInfo)
    std::cout << "Event: " << event << std::endl;

  // Creating the per-event group is what makes the "Events/<event>" path exist for the
  // datasets below, so it has to outlive them.
  hep_hpc::hdf5::Group event_group(*events_group, std::to_string(event));

  if (dump_mcparticles) write_mcparticles(e, event);
  if (dump_mcneutrinos) write_mcneutrinos(e, event);
  if (dump_sed) write_simenergydeposits(e, event);
  write_triggerprimitives(e, event);
}

DEFINE_ART_MODULE(dunetrigger::TriggerPrimitiveHDF5Dump)
