include_directories("${dunedetdataformats_DIR}/../../../include") # there's a neater way?
include_directories($ENV{NUSIMDATA_INC})
include_directories($ENV{LARDATAOBJ_INC})
include_directories($ENV{LARCOREOBJ_INC})

add_subdirectory(fcl)
add_subdirectory(exe)

art_make(BASENAME_ONLY MODULE_LIBRARIES
  larcore::Geometry_Geometry_service
  lardataobj::RawData
  lardataobj::Simulation
  dunecore::DuneObj
  lardata::ArtDataHelper
  nusimdata::SimulationBase
  art::Framework_Core
  art::Framework_Principal
  art::Framework_Services_Registry
  art_root_io::tfile_support
  ROOT::Core
  art_root_io::TFileService_service
  art::Persistency_Common
  art::Persistency_Provenance
  art::Utilities
  canvas::canvas
  messagefacility::MF_MessageLogger
  cetlib::cetlib
  cetlib_except::cetlib_except
  ROOT::Tree
  larsim::MCCheater_BackTrackerService_service
  larsim::MCCheater_ParticleInventoryService_service
  triggeralgs_module
  lardataalg::DetectorInfo
  DICTIONARY_LIBRARIES
    canvas
    nusimdata::SimulationBase
    lardataobj::Simulation
    lardataobj_Simulation_dict
)

install_fhicl()
install_headers()
install_source()
