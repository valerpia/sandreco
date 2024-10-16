#include "SANDGeoManager.h"
#include "SANDTrackerModuleConfig.h"

#include <iostream>
#include <fstream>

#include <iomanip>


#include <TGeoTrd2.h>
#include <TGeoTube.h>
#include <TGeoBBox.h>
#include <TObjString.h>
#include <TRandom3.h>
#include <TH2D.h>
#include <TLine.h>
#include <TCanvas.h>
#include <TMatrixD.h>
#include <TStyle.h>

Counter counter_;

int SANDGeoManager::encode_ecal_barrel_cell_local_id(int layer, int cell) const
{
  return cell * 100 + layer;
}

int SANDGeoManager::encode_ecal_endcap_cell_local_id(int layer, int cell) const
{
  return cell * 100 + layer;
}

std::pair<int, int> SANDGeoManager::decode_ecal_barrel_cell_local_id(int id)
    const
{
  int cell = id / 100;
  int layer = id % 100;
  return std::make_pair(layer, cell);
}

std::pair<int, int> SANDGeoManager::decode_ecal_endcap_cell_local_id(int id)
    const
{
  int cell = id / 100;
  int layer = id % 100;
  return std::make_pair(layer, cell);
}

std::vector<double> SANDGeoManager::get_levels_z(double half_module_height)
    const
{
  // z edge of the cells
  std::vector<double> zlevel;
  zlevel.push_back(-half_module_height);

  for (int i = 0; i < sand_geometry::ecal::number_of_layers; i++) {
    zlevel.push_back(zlevel.back() + sand_geometry::ecal::layer_thickness[i]);
  }
  return zlevel;
}

std::map<int, TVector3>
    SANDGeoManager::get_ecal_barrel_cell_center_local_position(
        const std::vector<double>& zlevels, double m, double q) const
{
  // z position of the center of the cells
  std::map<int, TVector3> ecal_barrel_cell_center_local_positions;
  for (auto i = 0u; i < zlevels.size() - 1u; i++) {
    auto z_this_layer = 0.5 * (zlevels.at(i) + zlevels.at(i + 1));

    // total module width at the z position of the center of the cell
    double x_module_width_at_z = 2 * (m * z_this_layer + q);

    // cell width at the z position of the center of the cell
    double x_cell_width = x_module_width_at_z /
                          sand_geometry::ecal::number_of_cells_per_barrel_layer;

    // position of the center of the cells
    for (int j = 0; j < sand_geometry::ecal::number_of_cells_per_barrel_layer;
         j++) {
      auto x = x_cell_width * (j + 0.5) - x_module_width_at_z * 0.5;
      auto y = 0.;
      auto z = z_this_layer;
      auto id = encode_ecal_barrel_cell_local_id(i, j);
      ecal_barrel_cell_center_local_positions[id] = TVector3(x, y, z);
    }
  }
  return ecal_barrel_cell_center_local_positions;
}

std::map<int, TVector3>
    SANDGeoManager::get_ecal_endcap_cell_center_local_position(
        const std::vector<double>& zlevels, double rmin, double rmax) const
{
  // z position of the center of the cells
  std::map<int, TVector3> ecal_endcap_cell_center_local_positions;
  for (auto i = 0u; i < zlevels.size() - 1u; i++) {
    auto z_this_layer = 0.5 * (zlevels.at(i) + zlevels.at(i + 1));

    // cell width at the z position of the center of the cell
    double x_cell_width =
        2 * rmax / sand_geometry::ecal::number_of_cells_per_endcap_layer;

    // position of the center of the cells
    for (int j = 0; j < sand_geometry::ecal::number_of_cells_per_endcap_layer;
         j++) {
      auto x = x_cell_width * (j + 0.5) - rmax;
      auto y = 0.;
      auto z = z_this_layer;
      auto id = encode_ecal_endcap_cell_local_id(i, j);
      ecal_endcap_cell_center_local_positions[id] = TVector3(x, y, z);
    }
  }
  return ecal_endcap_cell_center_local_positions;
}

int SANDGeoManager::encode_ecal_cell_id(int detector_id, int module_id,
                                        int layer_id, int cell_local_id)
{
  return cell_local_id + 100 * layer_id + 1000 * module_id +
         detector_id * 100000;
}

void SANDGeoManager::decode_ecal_cell_id(int cell_global_id, int& detector_id,
                                         int& module_id, int& layer_id,
                                         int& cell_local_id)
{
  detector_id = cell_global_id / 100000;
  cell_global_id -= detector_id * 100000;

  module_id = cell_global_id / 1000;
  cell_global_id -= module_id * 1000;

  layer_id = cell_global_id / 100;
  cell_global_id -= layer_id * 100;

  cell_local_id = cell_global_id;
}

TString SANDGeoManager::FindNextActiveLayer(const double* starting_point,
                                            const double* direction) const
{
  // step from current point haed till you find an Active layer
  int max_nof_steps = 3;

  int nof_steps = 0;

  geo_->SetCurrentPoint(starting_point[0], starting_point[1],
                        starting_point[2]);

  geo_->SetCurrentDirection(direction[0], direction[1], direction[2]);

  TString current_node = geo_->GetCurrentNode()->GetName();

  while (!current_node.Contains("Active")) {
    geo_->FindNextBoundaryAndStep();
    current_node = geo_->GetCurrentNode()->GetName();
    std::cout << current_node << "\n";
    if (current_node.Contains("Passive"))
      current_node.ReplaceAll("Passive", "Active");
    if ((nof_steps > max_nof_steps) &&
        (!current_node.Contains(
              "Active"))) {  // invert direction to find Active volume
      double _direction[3] = {-direction[0], -direction[1], -direction[2]};
      FindNextActiveLayer(starting_point, _direction);
    }
    nof_steps++;
  }
  return current_node;
}

bool SANDGeoManager::is_ecal_barrel(const TString& volume_name) const
{
  // something like: volECALActiveSlab_21_PV_0
  return volume_name.Contains("volECAL") == true &&
         volume_name.Contains("Active") == true &&
         volume_name.Contains("end") == false;
}

bool SANDGeoManager::is_ecal_endcap(const TString& volume_name) const
{
  // something like: endvolECALActiveSlab_0_PV_0
  return volume_name.Contains("endvolECAL") == true &&
         volume_name.Contains("Active") == true;
}

bool SANDGeoManager::check_and_process_ecal_path(TString& volume_path) const
{
  // ENDCAP ==> something like:
  // "/volWorld_PV_1/rockBox_lv_PV_0/volDetEnclosure_PV_0/volSAND_PV_0/MagIntVol_volume_PV_0/kloe_calo_volume_PV_0/ECAL_lv_PV_18/volECALActiveSlab_21_PV_0"
  // BARREL ==> something like:
  // "/volWorld_PV_1/rockBox_lv_PV_0/volDetEnclosure_PV_0/volSAND_PV_0/MagIntVol_volume_PV_0/kloe_calo_volume_PV_0/ECAL_end_lv_PV_0/endvolECALActiveSlab_0_PV_0"
  TObjArray* obj = volume_path.Tokenize("/");

  int size = obj->GetEntries();
  if (size < 8) {
    return false;
  };

  // BARREL => ECAL_lv_PV_18
  // ENDCAP => ECAL_end_lv_PV_0
  volume_path = ((TObjString*)obj->At(6))->GetString();
  delete obj;

  return true;
}

void SANDGeoManager::get_ecal_barrel_module_and_layer(
    const TString& volume_name, const TString& volume_path, int& detector_id,
    int& module_id, int& plane_id) const
{
  TObjArray* obja1 =
      volume_name.Tokenize("_");  // BARREL => volECALActiveSlab_21_PV_0
  TObjArray* obja2 = volume_path.Tokenize("_");  // BARREL => ECAL_lv_PV_18

  // top module => modID == 0
  // increasing modID counterclockwise as seen from positive x
  //(i.e. z(modID==1) < z(modID==0) & z(modID==0) < z(modID==23))
  detector_id = 2;
  module_id = ((TObjString*)obja2->At(3))->GetString().Atoi();
  int slab_id = ((TObjString*)obja1->At(1))->GetString().Atoi();  // 21

  delete obja1;
  delete obja2;

  // planeID==0 -> smallest slab -> internal
  // planeID==208 -> biggest slab -> external
  plane_id = slab_id / 40;

  if (plane_id > 4) plane_id = 4;

  if (detector_id != 2) {
    std::cout << "invalid detector id : " << detector_id << "\n";
    throw "";
  } else if (module_id > 23) {
    std::cout << "invalid module_id  : " << module_id << "\n";
    throw "";
  } else if (slab_id > 208) {
    std::cout << "invalid slab_id  : " << slab_id << "\n";
    throw "";
  } else if (plane_id > 5) {
    std::cout << "invalid plane_id  : " << plane_id << "\n";
    throw "";
  }
}

void SANDGeoManager::get_ecal_endcap_module_and_layer(
    const TString& volume_name, const TString& volume_path, int& detector_id,
    int& module_id, int& plane_id) const
{
  TObjArray* obja1 =
      volume_name.Tokenize("_");  // ENDCAP => endvolECALActiveSlab_0_PV_0
  TObjArray* obja2 = volume_path.Tokenize("_");  // ENDCAP => ECAL_end_lv_PV_0

  module_id = ((TObjString*)obja2->At(4))->GetString().Atoi();
  int slab_id = ((TObjString*)obja1->At(1))->GetString().Atoi();

  // mod == 40 -> left  -> detID = 1
  // mod == 30 -> right -> detID = 3
  // (see issue: https://baltig.infn.it/dune/sand-reco/-/issues/18)
  if (module_id == 0) {
    detector_id = 1;
    module_id = 40;
  } else if (module_id == 1) {
    detector_id = 3;
    module_id = 30;
  }
  delete obja1;
  delete obja2;

  // planeID==0 -> internal
  // planeID==208 -> external
  plane_id = slab_id / 40;

  if (plane_id > 4) plane_id = 4;
}

void SANDGeoManager::get_ecal_barrel_cell_local_id(double x, double y, double z,
                                                   const TGeoNode* const node,
                                                   int& cell_local_id) const
{
  double master[3];
  double local[3];
  master[0] = x;
  master[1] = y;
  master[2] = z;

  geo_->GetCurrentNavigator()->MasterToLocal(master, local);

  TString shape_name = node->GetVolume()->GetShape()->GetName();

  if (shape_name != "TGeoTrd2") {
    std::cout << __FILE__ << " " << __LINE__ << "\n";
    std::cout << "invalid shape : " << shape_name << "\n";
    throw "";
  }

  TGeoTrd2* trd = (TGeoTrd2*)node->GetVolume()->GetShape();

  double dx1 = trd->GetDx1();
  double dx2 = trd->GetDx2();
  double dz = trd->GetDz();

  // http://geant4-userdoc.web.cern.ch/geant4-userdoc/UsersGuides/ForApplicationDeveloper/html/Detector/Geometry/geomSolids.html
  // if z = -dz -> dx = 2*dx1
  // if z =  dz -> dx = 2*dx2
  // semilarghezza della slab di scintillatore alla quota Plocal[2]
  double dx = 0.5 * local[2] / dz * (dx2 - dx1) + 0.5 * (dx2 + dx1);

  // Cell width at z = Plocal[2]
  double cell_width =
      2. * dx / sand_geometry::ecal::number_of_cells_per_barrel_layer;

  // cellID = distanza dall'estremo diviso larghezza cella
  cell_local_id = (local[0] + dx) / cell_width;

  if (cell_local_id > 11) {
    std::cout << __FILE__ << " " << __LINE__ << "\n";
    std::cout << "current node : "
              << geo_->GetCurrentNavigator()->GetCurrentNode()->GetName()
              << "\n";
    std::cout << "invalid cell_local_id : " << cell_local_id << "\n";
    throw "";
  }
}

void SANDGeoManager::get_ecal_endcap_cell_local_id(double x, double y, double z,
                                                   const TGeoNode* const node,
                                                   int& cell_local_id) const
{
  double master[3];
  double local[3];
  master[0] = x;
  master[1] = y;
  master[2] = z;

  geo_->GetCurrentNavigator()->MasterToLocal(master, local);

  TGeoTube* tub = (TGeoTube*)node->GetVolume()->GetShape();

  double rmin = tub->GetRmin();
  double rmax = tub->GetRmax();
  double dz = tub->GetDz();

  cell_local_id =
      int((local[0] / rmax + 1.) *
          sand_geometry::ecal::number_of_cells_per_endcap_layer * 0.5);
}

void SANDGeoManager::set_ecal_info()
{
  // https://root.cern.ch/root/htmldoc/guides/users-guide/Geometry.html#shapes
  // GetDx1() half length in x at -Dz
  // GetDx2() half length in x at +Dz
  // Dx1 < Dx2 => -Dz corresponds to minor width => internal side
  TGeoTrd2* mod =
      (TGeoTrd2*)geo_->FindVolumeFast(sand_geometry::ecal::barrel_module_name)
          ->GetShape();
  double ecal_barrel_xmin = mod->GetDx1();
  double ecal_barrel_xmax = mod->GetDx2();
  double ecal_barrel_dz = mod->GetDz();
  double ecal_barrel_dy = mod->GetDy1();

  TGeoTube* ec =
      (TGeoTube*)geo_->FindVolumeFast(sand_geometry::ecal::endcap_module_name)
          ->GetShape();
  double ecal_endcap_rmax = ec->GetRmax();  // Maximum radius = 2000
  double ecal_endcap_rmin = ec->GetRmin();

  // get z of the levels between the layers
  auto z_levels = get_levels_z(ecal_barrel_dz);

  // get slop of the edge of the barrel module
  auto ecal_barrel_edge_slope =
      0.5 * (ecal_barrel_xmax - ecal_barrel_xmin) / ecal_barrel_dz;
  auto ecal_barrel_edge_position = 0.5 * (ecal_barrel_xmax + ecal_barrel_xmin);

  // eval barrel cell center global position
  auto ecal_barrel_cell_center_local_positions =
      get_ecal_barrel_cell_center_local_position(
          z_levels, ecal_barrel_edge_slope, ecal_barrel_edge_position);

  double local[3];
  double master[3];

  for (int module_id = 0;
       module_id < sand_geometry::ecal::number_of_barrel_modules; module_id++) {
    for (auto cell_position : ecal_barrel_cell_center_local_positions) {
      local[0] = cell_position.second.X();
      local[1] = cell_position.second.Y();
      local[2] = cell_position.second.Z();

      auto cell_and_layer_id =
          decode_ecal_barrel_cell_local_id(cell_position.first);
      auto layer_id = cell_and_layer_id.first;
      auto cell_local_id = cell_and_layer_id.second;

      geo_->cd(TString::Format(sand_geometry::ecal::path_barrel_template,
                               module_id).Data());
      geo_->LocalToMaster(local, master);

      // here we create new cellInfo
      int detector_id = 2;
      int cell_unique_id =
          encode_ecal_cell_id(detector_id, module_id, layer_id, cell_local_id);
      cellmap_[cell_unique_id] = SANDECALCellInfo(
          cell_unique_id, master[0], master[1], master[2], ecal_barrel_dy,
          SANDECALCellInfo::Orient::kHorizontal);
    }
  }

  // eval barrel cell center global position
  auto ecal_endcap_cell_center_local_positions =
      get_ecal_endcap_cell_center_local_position(z_levels, ecal_endcap_rmin,
                                                 ecal_endcap_rmax);

  for (auto module_id : sand_geometry::ecal::endcap_module_ids) {
    for (auto cell_position : ecal_endcap_cell_center_local_positions) {
      local[0] = cell_position.second.X();
      local[1] = cell_position.second.Y();
      local[2] = cell_position.second.Z();

      double cell_length = ecal_endcap_rmax *
                           TMath::Sin(TMath::ACos(local[0] / ecal_endcap_rmax));

      auto cell_and_layer_id =
          decode_ecal_endcap_cell_local_id(cell_position.first);
      auto layer_id = cell_and_layer_id.first;
      auto cell_local_id = cell_and_layer_id.second;
      int detector_id = 0;

      if (module_id == 30) {
        detector_id = 3;
        geo_->cd(sand_geometry::ecal::path_endcapR_template);
      } else if (module_id == 40) {
        detector_id = 1;
        geo_->cd(sand_geometry::ecal::path_endcapL_template);
      }

      geo_->LocalToMaster(local, master);

      // here we create new cellInfo
      int cell_unique_id =
          encode_ecal_cell_id(detector_id, module_id, layer_id, cell_local_id);
      cellmap_[cell_unique_id] =
          SANDECALCellInfo(cell_unique_id, master[0], master[1], master[2],
                           cell_length, SANDECALCellInfo::Orient::kVertical);
    }
  }
}

plane_iterator SANDGeoManager::get_plane_info(SANDTrackerPlaneID plane_global_id) const
{
  SANDTrackerModuleID module_unique_id;
  SANDTrackerPlaneID  plane_local_id, plane_type;

  decode_plane_id(plane_global_id, module_unique_id, 
                  plane_local_id, plane_type);
  return _id_to_plane.at(plane_global_id);
}

plane_iterator SANDGeoManager::get_plane_info(SANDTrackerCellID cell_global_id) const
{
  SANDTrackerPlaneID  plane_global_id;
  SANDTrackerCellID   cell_local_id;

  decode_cell_id(cell_global_id, plane_global_id, cell_local_id);
  return _id_to_plane.at(plane_global_id);
}

std::map<SANDTrackerCellID, SANDTrackerCell>::const_iterator SANDGeoManager::get_cell_info(SANDTrackerCellID cell_global_id) const
{
  SANDTrackerModuleID module_unique_id;
  SANDTrackerPlaneID  plane_global_id, plane_local_id, plane_type;
  SANDTrackerCellID   cell_local_id;

  decode_cell_id(cell_global_id, plane_global_id, cell_local_id);
  decode_plane_id(plane_global_id, module_unique_id, 
                  plane_local_id, plane_type);
  return _id_to_plane.at(plane_global_id)->getCell(cell_global_id);
}

SANDTrackerCellID SANDGeoManager::encode_cell_id(SANDTrackerPlaneID plane_global_id, SANDTrackerCellID cell_local_id)
{
  return plane_global_id() * 10000 + cell_local_id();
}

void SANDGeoManager::decode_cell_id(SANDTrackerCellID   cell_global_id, 
                                    SANDTrackerPlaneID& plane_global_id,
                                    SANDTrackerCellID&  cell_local_id)
{
  plane_global_id = cell_global_id() / 10000;  // global id
  cell_local_id = cell_global_id() % 10000;
}

SANDTrackerPlaneID SANDGeoManager::encode_plane_id(SANDTrackerModuleID unique_module_id,
                                     SANDTrackerPlaneID plane_replica_id, SANDTrackerPlaneID plane_type)
{
  return unique_module_id() + (2 * plane_replica_id() + plane_type()) * 10 + plane_type();
}

void SANDGeoManager::decode_plane_id(SANDTrackerPlaneID plane_global_id, SANDTrackerModuleID& unique_module_id, 
                                     SANDTrackerPlaneID& plane_replica_id, SANDTrackerPlaneID& plane_type)
{
  unique_module_id = plane_global_id() / 100 * 100;
  SANDTrackerPlaneID local_plane_id = plane_global_id() - unique_module_id();
  plane_type = local_plane_id() % 10;
  plane_replica_id = ((local_plane_id() / 10) - plane_type()) / 2;
}

SANDTrackerModuleID SANDGeoManager::encode_module_id(SANDTrackerModuleID supermodule_id, SANDTrackerModuleID module_id, SANDTrackerModuleID module_replica_id)
{
  return supermodule_id() * 1E5 + (module_id() * 10 + module_replica_id()) * 100;
}

void SANDGeoManager::decode_module_id(SANDTrackerModuleID unique_module_id, SANDTrackerModuleID& supermodule_id, 
                                      SANDTrackerModuleID& module_id, SANDTrackerModuleID& module_replica_id)
{
  supermodule_id = unique_module_id() / 1E5;
  SANDTrackerModuleID local_module_id = (unique_module_id() - supermodule_id() * 1E5) / 100;
  module_id = local_module_id() / 10;
  module_replica_id = local_module_id() % 10;
}

bool SANDGeoManager::is_stt_tube(const TString& volume_name) const
{
  return volume_name.Contains(stt_tube_regex_);
}

bool SANDGeoManager::is_stt_plane(const TString& volume_name) const
{
  return volume_name.Contains(stt_plane_regex_);
}
bool SANDGeoManager::is_drift_plane(const TString& volume_name) const
{
  return volume_name.Contains(drift_plane_regex_);
}

SANDTrackerModuleID SANDGeoManager::get_stt_module_id(const TString& volume_path) const
{
  auto supermodule_matches = stt_supermodule_regex_.MatchS(volume_path);

  long supermodule_id;
  if (supermodule_matches->GetEntries() == 0) {
    supermodule_id = 0;
  } else {
    // To Do: Currently there are no supermodules in the stt geometry
  }

  auto module_matches = stt_module_regex_.MatchS(volume_path);
  long module_id =
      (reinterpret_cast<TObjString*>(module_matches->At(2)))->GetString().Atoi();
  long module_replica_id = (reinterpret_cast<TObjString*>(module_matches->At(3)))
                            ->GetString()
                            .Atoi();
  return encode_module_id(SANDTrackerModuleID(supermodule_id), 
                          SANDTrackerModuleID(module_id), 
                          SANDTrackerModuleID(module_replica_id));
}

SANDTrackerPlaneID SANDGeoManager::get_stt_plane_id(const TString& volume_path, bool justLocal = false) const
{

  auto plane_matches = stt_plane_regex_.MatchS(volume_path);

  if (plane_matches->GetEntries() < 5) {
    // Sometimes the volume path returned by the TGeoManager does not match
    // with the expected one for a tube...to be investigated!!!
    // std::cout << "Error: volume path for STT digit not expected!! returning
    // default value (0) for stt plane id" << std::endl;
    delete plane_matches;
    return 0;
  }

  int plane_replica_id =
      (reinterpret_cast<TObjString*>(plane_matches->At(4)))->GetString().Atoi();

  int plane_type = (reinterpret_cast<TObjString*>(plane_matches->At(3)))
                           ->GetString()
                           .EqualTo("XX")
                       ? 2
                       : 1;

  delete plane_matches;
  if (justLocal) {
    return plane_type;
  } else {
    SANDTrackerModuleID unique_module_id = get_stt_module_id(volume_path);
    return encode_plane_id(unique_module_id, SANDTrackerPlaneID(plane_replica_id), SANDTrackerPlaneID(plane_type));
  }
}

SANDTrackerModuleID SANDGeoManager::get_drift_supermodule_id(const TString& volume_path) const
{
  // upstram -> downstrea,
  // Trk, C1, B1, A1, A0, B0, C0, X0, X1
  //   0,  1,  2,  3,  4,  5,  6,  7,  8
  auto supermodule_matches = supermodule_regex_.MatchS(volume_path);
  TString supermodule_name =
      (reinterpret_cast<TObjString*>(supermodule_matches->At(2)))->GetString();
  int supermodule_replica =
      (reinterpret_cast<TObjString*>(supermodule_matches->At(3)))
          ->GetString()
          .Atoi();
  int supermodule_id;
  if (supermodule_name.Contains("X0")) {
    supermodule_id = 8;
  } else if (supermodule_name.Contains("X1")) {
    supermodule_id = 7;
  } else if (supermodule_name.Contains("C")) {
    supermodule_id = supermodule_replica ? 1 : 6;
  } else if (supermodule_name.Contains("B")) {
    supermodule_id = supermodule_replica ? 2 : 5;
  } else if (supermodule_name.Contains("A")) {
    supermodule_id = supermodule_replica ? 3 : 4;
  } else {
    supermodule_id = 0;
  }

  delete supermodule_matches;

  return SANDTrackerModuleID(supermodule_id);
}

SANDTrackerModuleID SANDGeoManager::get_drift_module_replica_id(const TString& volume_path)
    const
{
  auto matches = module_regex_.MatchS(volume_path);
  auto type = (reinterpret_cast<TObjString*>(matches->At(1)))->GetString();
  long id = (reinterpret_cast<TObjString*>(matches->At(3)))->GetString().Atoi();
  if (type == "C") {
    id = 9;
  }
  return SANDTrackerModuleID(id);
}

bool SANDGeoManager::isSwire(const TString& volume_path) const
{
  return (volume_path.Contains("Swire"));
}

SANDWireID SANDGeoManager::get_wire_id(const TString& volume_path) const
{
  auto matches = wire_regex_.MatchS(volume_path);
  long id = (reinterpret_cast<TObjString*>(matches->At(5)))->GetString().Atoi();
  return SANDWireID(id);
}

SANDTrackerModuleID SANDGeoManager::get_drift_module_id(const TString& volume_path) const
{
  SANDTrackerModuleID supermodule_id(get_drift_supermodule_id(volume_path));
  SANDTrackerModuleID module_id(0);
  SANDTrackerModuleID module_replica_id(0);
  if (supermodule_id() != 0) {
    module_replica_id = get_drift_module_replica_id(volume_path);
  }
  return encode_module_id(supermodule_id, module_id, module_replica_id);
}

SANDTrackerPlaneID SANDGeoManager::get_drift_plane_id(const TString& volume_path,
                                       bool JustLocalId = false) const
{
  auto plane_matches = drift_plane_regex_.MatchS(volume_path);

  if (plane_matches->GetEntries() == 0) {
    delete plane_matches;
    return 0;
  }

  int plane_type =
      (reinterpret_cast<TObjString*>(plane_matches->At(2)))->GetString().Atoi();
  int plane_replica_id =
      (reinterpret_cast<TObjString*>(plane_matches->At(4)))->GetString().Atoi();

  if (JustLocalId) {
    return plane_type;
  } else {
    SANDTrackerModuleID unique_module_id(get_drift_module_id(volume_path));
    return encode_plane_id(unique_module_id, plane_replica_id, plane_type);
  }
}

std::vector<TVector2> SANDGeoManager::getLocalLinePlaneIntersections(const TVector2& local_2d_position,
                                               const SANDTrackerPlane& plane)
{
  TVector2 local_plane_x_axis = RotatedToLocal(TVector2(1, 0), plane);
  std::vector<TVector2> vertices = plane.getPlaneVertices();

  std::vector<TVector2> intersection_points;

  TVector2 intersection(0, 0);
    if (getLineSegmentIntersection(local_2d_position, local_plane_x_axis, 
                               vertices[0], vertices[1], intersection)) {
      intersection_points.push_back(intersection);
    };
    if (getLineSegmentIntersection(local_2d_position, local_plane_x_axis, 
                               vertices[1], vertices[2], intersection)) {
      intersection_points.push_back(intersection);
    };
    if (getLineSegmentIntersection(local_2d_position, local_plane_x_axis, 
                               vertices[2], vertices[3], intersection)) {
      intersection_points.push_back(intersection);
    };
    if (getLineSegmentIntersection(local_2d_position, local_plane_x_axis, 
                               vertices[3], vertices[0], intersection)) {
      intersection_points.push_back(intersection);
    };
  return intersection_points;
}

std::vector<TVector2> SANDGeoManager::getGlobalLinePlaneIntersections(const TVector2& local_2d_position,
                                               const SANDTrackerPlane& plane)
{
  std::vector<TVector2> intersection_points = getLocalLinePlaneIntersections(local_2d_position, plane);

  for(auto& intersection:intersection_points) {
    intersection = LocalToGlobal(intersection, plane);
  }

  return intersection_points;
}

void SANDGeoManager::set_stt_plane_info(const TGeoNode* const node,
                                        const TGeoHMatrix& matrix)
{
  TString node_path = gGeoManager->GetPath();
  SANDTrackerPlaneID stt_plane_unique_id = get_stt_plane_id(node_path);
  SANDTrackerPlaneID stt_plane_local_id  = get_stt_plane_id(node_path, true);

  _planes.push_back(SANDTrackerPlane(stt_plane_unique_id, stt_plane_local_id));
  _id_to_plane[_planes.back().uid()] = std::prev(_planes.end());

  auto& plane = _planes.back();
  double angle = TrackerModuleConfiguration::STT::_id_to_angle[std::to_string(stt_plane_local_id())];

  plane.setRotation(angle);
  
  TGeoMatrix* plane_matrix = node->GetMatrix();
  TGeoHMatrix plane_hmatrix = matrix * (*plane_matrix);
  TGeoBBox* plane_shape = (TGeoBBox*)node->GetVolume()->GetShape();
  TVector3 plane_dimension;
  if(angle == 0) {
    plane_dimension.SetX(2 * plane_shape->GetDZ());
    plane_dimension.SetY(2 * plane_shape->GetDY());
  } else {
    plane_dimension.SetY(2 * plane_shape->GetDZ());
    plane_dimension.SetX(2 * plane_shape->GetDY());
  }
  plane_dimension.SetZ(2 * plane_shape->GetDX());

  TVector3 plane_position;
  plane_position.SetX(matrix.GetTranslation()[0]);
  plane_position.SetY(matrix.GetTranslation()[1]);
  plane_position.SetZ(matrix.GetTranslation()[2]);

  plane.setPosition(plane_position);
  plane.setDimension(plane_dimension);
  
  plane.computePlaneVertices();
  plane.computeMaxTransversePosition();

  set_stt_wire_info(plane, node, matrix);
}

void SANDGeoManager::set_stt_wire_info(SANDTrackerPlane& plane,
                                       const TGeoNode* const node,
                                       const TGeoHMatrix& matrix)
{
  TVector2 local_plane_x_axis = RotatedToLocal(TVector2(1, 0), plane);
  std::vector<TVector2> vertices = plane.getPlaneVertices();

  for (int i = 0; i < node->GetNdaughters(); i++) {
    SANDWireInfo w;

    auto tube_node = node->GetDaughter(i);
    auto tube_matches = stt_tube_regex_.MatchS(tube_node->GetName());
    int tube_id = (reinterpret_cast<TObjString*>(tube_matches->At(4)))
                      ->GetString()
                      .Atoi();
    delete tube_matches;

    SANDTrackerCellID cell_unique_id = encode_cell_id(plane.uid(), SANDTrackerCellID(tube_id));
    w.id(SANDWireID(cell_unique_id()));
    w.type(SANDWireInfo::Type::kSignal);

    TGeoMatrix* tube_matrix = tube_node->GetMatrix();
    TGeoHMatrix tube_hmatrix = matrix * (*tube_matrix);

    TVector3 tube_position;
    tube_position.SetX(tube_hmatrix.GetTranslation()[0]);
    tube_position.SetY(tube_hmatrix.GetTranslation()[1]);
    tube_position.SetZ(tube_hmatrix.GetTranslation()[2]);
    TVector2 local_2d_position = GlobalToLocal(TVector2(tube_position.X(), tube_position.Y()), plane);
    
    std::vector<TVector2> intersection_points = 
            getGlobalLinePlaneIntersections(local_2d_position, plane);
    for (const auto& point:intersection_points) {
      TVector3 intersection(point.X(), point.Y(), tube_position.Z());
      w.setPoint(intersection);
    }

    if (w.getPoints().size() == 2) {
      w.center((w.getFirstPoint() + w.getSecondPoint()) * 0.5);
      w.length((w.getSecondPoint() - w.getFirstPoint()).Mag());

      if (fabs(w.getFirstPoint().Y() - plane.getPosition().Y() + vertices[0].Y()) < 1E-3) {
        w.readout_end(SANDWireInfo::ReadoutEnd::kFirst);
      }
      if (fabs(w.getFirstPoint().X() - plane.getPosition().X() + vertices[0].X()) < 1E-3) {
        w.readout_end(SANDWireInfo::ReadoutEnd::kFirst);
      }
      if (fabs(w.getSecondPoint().Y() - plane.getPosition().Y() + vertices[0].Y()) < 1E-3) {
        w.readout_end(SANDWireInfo::ReadoutEnd::kSecond);
      }
      if (fabs(w.getSecondPoint().X() - plane.getPosition().X() + vertices[0].X()) < 1E-3) {
        w.readout_end(SANDWireInfo::ReadoutEnd::kSecond);
      }
    }

    TGeoTubeSeg* tube_shape = (TGeoTubeSeg*)tube_node->GetVolume()->GetShape();
    TVector2 rotated_2d_position = LocalToRotated(local_2d_position, plane);
    plane.addCell(rotated_2d_position.Y(), 
                  SANDTrackerCell(cell_unique_id, w, 2. * tube_shape->GetRmax(), 2. * tube_shape->GetRmax(), 
                  TrackerModuleConfiguration::STT::_id_to_velocity[std::to_string(plane.lid()())]));
  }
}

const TVector2 SANDGeoManager::pointInRotatedSystem(TVector2 v, double angle) const
{
  TVector2 rotated_v;
  rotated_v.SetX( v.X() * cos(angle) + v.Y() * sin(angle));
  rotated_v.SetY(-v.X() * sin(angle) + v.Y() * cos(angle));

  return rotated_v;
}
const TVector2 SANDGeoManager::GlobalToLocal(TVector2 global, const SANDTrackerPlane& plane) const
{
  return TVector2(global.X() - plane.getPosition().X(), global.Y() - plane.getPosition().Y());
}
const TVector2 SANDGeoManager::LocalToRotated(TVector2 local, const SANDTrackerPlane& plane) const
{
  return pointInRotatedSystem(local, plane.getRotation());
}
const TVector2 SANDGeoManager::GlobalToRotated(TVector2 global, const SANDTrackerPlane& plane) const
{
  return LocalToRotated(GlobalToLocal(global, plane), plane);
}
const TVector2 SANDGeoManager::RotatedToLocal(TVector2 rotated, const SANDTrackerPlane& plane) const
{
  return pointInRotatedSystem(rotated, -plane.getRotation());
}
const TVector2 SANDGeoManager::LocalToGlobal(TVector2 local, const SANDTrackerPlane& plane) const
{
  return TVector2(local.X() + plane.getPosition().X(), local.Y() + plane.getPosition().Y());
}
const TVector2 SANDGeoManager::RotatedToGlobal(TVector2 rotated, const SANDTrackerPlane& plane) const
{
  return LocalToGlobal(RotatedToLocal(rotated, plane), plane);
}


bool SANDGeoManager::getLineSegmentIntersection(TVector2 p, TVector2 dir, TVector2 A, TVector2 B, TVector2& intersection)
{
  double delta_x = A.X() - B.X();
  double delta_y = A.Y() - B.Y();
  double det = dir.X() * delta_y  - dir.Y() * delta_x;

  if (fabs(det) < 1E-9) {
    // std::cout << "Line and segment are parallel." << std::endl;
    return false;
  } else {
    
    double t = ((A.X() - p.X()) * delta_y - (A.Y() - p.Y()) * delta_x) / det;
    double s = ((p.X() - A.X()) * dir.Y() - (p.Y() - A.Y()) * dir.X()) / det;

    if (s >= 0 && s <= 1) {
      intersection.SetX(p.X() + t * dir.X());
      intersection.SetY(p.Y() + t * dir.Y());
      return true;
    }

    return false;
  }
}

bool SANDGeoManager::getLineSegmentIntersection(TVector2 p, TVector2 dir, TVector2 A, TVector2 B, TVector3& intersection)
{
  double delta_x = A.X() - B.X();
  double delta_y = A.Y() - B.Y();
  double det = dir.X() * delta_y  - dir.Y() * delta_x;

  if (fabs(det) < 1E-9) {
    // std::cout << "Line and segment are parallel." << std::endl;
    return false;
  } else {
    
    double t = ((A.X() - p.X()) * delta_y - (A.Y() - p.Y()) * delta_x) / det;
    double s = ((p.X() - A.X()) * dir.Y() - (p.Y() - A.Y()) * dir.X()) / det;

    if (s >= 0 && s <= 1) {
      intersection.SetX(p.X() + t * dir.X());
      intersection.SetY(p.Y() + t * dir.Y());
      return true;
    }

    return false;
  }
}

void SANDGeoManager::set_drift_plane_info(const TGeoNode* const node,
                                          const TGeoHMatrix& matrix)
{
  // To Do:
  // Check rotation of modules in the geometry.
  // Currently the rotation of the wire is obtained by rotating 
  // the whole plane. This results is the x-y dimensions being swapped
  TString node_path = gGeoManager->GetPath();

  SANDTrackerPlaneID drift_plane_unique_id = get_drift_plane_id(node_path);
  SANDTrackerPlaneID drift_plane_local_id  = get_drift_plane_id(node_path, true);  // 0,1 or 2

  _planes.push_back(SANDTrackerPlane(drift_plane_unique_id, drift_plane_local_id));
  _id_to_plane[_planes.back().uid()] = std::prev(_planes.end());

  auto& plane = _planes.back();
  double angle = TrackerModuleConfiguration::Drift::_id_to_angle[std::to_string(drift_plane_local_id())];

  plane.setRotation(angle);

  TGeoBBox* plane_shape = (TGeoBBox*)node->GetVolume()->GetShape();
  TVector3 plane_dimension;
  if(angle == 0) {
    plane_dimension.SetX(2 * plane_shape->GetDZ());
    plane_dimension.SetY(2 * plane_shape->GetDY());
  } else {
    plane_dimension.SetY(2 * plane_shape->GetDZ());
    plane_dimension.SetX(2 * plane_shape->GetDY());
  }
  plane_dimension.SetZ(2 * plane_shape->GetDX());

  TVector3 plane_position;
  plane_position.SetX(matrix.GetTranslation()[0]);
  plane_position.SetY(matrix.GetTranslation()[1]);
  plane_position.SetZ(matrix.GetTranslation()[2]);

  plane.setPosition(plane_position);
  plane.setDimension(plane_dimension);
  
  plane.computePlaneVertices();
  plane.computeMaxTransversePosition();

  set_drift_wire_info(plane);
}

void SANDGeoManager::set_drift_wire_info(SANDTrackerPlane& plane)
{

  TVector2 local_plane_x_axis = RotatedToLocal(TVector2(1, 0), plane);
  // local_plane_x_axis.Print();
  
  std::vector<TVector2> vertices = plane.getPlaneVertices();

  double transverse_position = plane.getMaxTransverseCoord() - TrackerModuleConfiguration::Drift::_id_to_offset[std::to_string(plane.lid()())];
  long wire_id = 0;
  while (transverse_position > -plane.getMaxTransverseCoord()) {
    SANDWireInfo w;

    SANDTrackerCellID cell_unique_id = encode_cell_id(plane.uid(), SANDTrackerCellID(wire_id));
    w.id(SANDWireID(cell_unique_id()));
    w.type(SANDWireInfo::Type::kSignal);
    TVector2 local_2d_position = RotatedToLocal(TVector2(0, transverse_position), plane);
    std::vector<TVector2> intersection_points = 
            getGlobalLinePlaneIntersections(local_2d_position, plane);
    for (const auto& point:intersection_points) {
      TVector3 intersection(point.X(), point.Y(), plane.getPosition().Z());
      w.setPoint(intersection);
    }

    if (w.getPoints().size() == 2) {
      w.center((w.getFirstPoint() + w.getSecondPoint()) * 0.5);
      w.length((w.getSecondPoint() - w.getFirstPoint()).Mag());
      if (fabs(w.getFirstPoint().Y() - plane.getPosition().Y() + vertices[0].Y()) < 1E-3) {
        w.readout_end(SANDWireInfo::ReadoutEnd::kFirst);
      }
      if (fabs(w.getFirstPoint().X() - plane.getPosition().X() + vertices[0].X()) < 1E-3) {
        w.readout_end(SANDWireInfo::ReadoutEnd::kFirst);
      }
      if (fabs(w.getSecondPoint().Y() - plane.getPosition().Y() + vertices[0].Y()) < 1E-3) {
        w.readout_end(SANDWireInfo::ReadoutEnd::kSecond);
      }
      if (fabs(w.getSecondPoint().X() - plane.getPosition().X() + vertices[0].X()) < 1E-3) {
        w.readout_end(SANDWireInfo::ReadoutEnd::kSecond);
      }
    }

    if (w.length() > TrackerModuleConfiguration::Drift::_id_to_length[std::to_string(plane.lid()())]) {
      plane.addCell(transverse_position, 
                    SANDTrackerCell(cell_unique_id, w, 
                    TrackerModuleConfiguration::Drift::_id_to_offset[std::to_string(plane.lid()())],
                    plane.getDimension().Z(),
                    TrackerModuleConfiguration::Drift::_id_to_velocity[std::to_string(plane.lid()())]));
      wire_id++;
    }
    transverse_position -= TrackerModuleConfiguration::Drift::_id_to_spacing[std::to_string(plane.lid()())];

  }

}


void SANDGeoManager::set_wire_info(const TGeoHMatrix& matrix)
{
  TGeoNode* node = gGeoManager->GetCurrentNode();
  TString node_name = node->GetName();
  TGeoMatrix* node_matrix = node->GetMatrix();
  TGeoHMatrix node_hmatrix = matrix * (*node_matrix);
  if (is_drift_plane(node_name)) {
    set_drift_plane_info(node, node_hmatrix);
  } else if (is_stt_plane(node_name)) {
    set_stt_plane_info(node, node_hmatrix);
  } else {
    for (int i = 0; i < node->GetNdaughters(); i++) {
      gGeoManager->CdDown(i);
      set_wire_info(node_hmatrix);
      gGeoManager->CdUp();
    }
  }
}

double SANDGeoManager::getMinDistanceBetweenSegments(TVector3 a, TVector3 b,
                                                     TVector3 c, TVector3 d)
{
  TVector3 s(b - a);
  TVector3 r(d - c);

  TVector3 diff = a - c;
  double A = s.Dot(s);    // s . s
  double B = s.Dot(r);    // s . r
  double C = r.Dot(r);    // r . r
  double D = s.Dot(diff);    // s . (a - c)
  double E = r.Dot(diff);    // r . (a - c)

  double det = A * C - B * B;
  if ( (fabs(det) > 1E-9)) {
    double t = (B * E - C * D) / det;
    double t_prime = (A * E - B * D) / det;

    t = std::max(0.0, std::min(1.0, t));
    t_prime = std::max(0.0, std::min(1.0, t_prime));
    
    TVector3 point1 = a + t * (b - a);
    TVector3 point2 = c + t_prime * (d - c);
    
    if (t == 0 || t == 1) {
      TVector3 AP = point1 - c;
      t_prime = AP.Dot(r) / r.Mag2();
      t_prime = std::max(0.0, std::min(1.0, t_prime));
    }
    
    if (t_prime == 0 || t_prime == 1) {
      TVector3 AP = point2 - a;
      t = AP.Dot(s) / s.Mag2();
      t = std::max(0.0, std::min(1.0, t));
    }

    point1 = a + t * (b - a);
    point2 = c + t_prime * (d - c);

    return (point1 - point2).Mag();
  } else {
    TVector3 w(a - c);
    double t = w.Dot(r) / r.Mag2();
    TVector3 p_closest = c + t * r;
    return (a - p_closest).Mag();
  }


}

// To Do: I don't like geometry being a string.. maybe use an enum?
// Notice: Currently a single dz and dy are considered. If planes will have 
//        different thickness or different wire smaplings, this won't work
void SANDGeoManager::fill_adjacent_cells(std::string geometry)
{
  double dz; 
  double dy;
  auto first_cell  = _planes.at(0).getIdToCellMap().begin();
  if (geometry == "STT") {
    first_cell->second.size(dy, dz);
    dz = dz * sqrt(3) / 2.;
  } else {
    first_cell->second.size(dy, dz);
  }
  
  double max_distance = sqrt(dy*dy + dz*dz) + 0.1;
  std::cout << "max_distance " << dy << " " << dz << " " << max_distance << std::endl;

  for(auto plane_it = _planes.begin(); plane_it != _planes.end(); plane_it++) {
    // std::cout << "Checking plane " << plane_it->uid()() << std::endl;
    int c = 0;
    for(auto next_plane_it = plane_it; c < 3 && next_plane_it != _planes.end(); next_plane_it++) {
      // std::cout << "with plane " << next_plane_it->uid()() << std::endl;
      c++;
      auto&      plane_it_cells =      plane_it->getIdToCellMap();
      auto& next_plane_it_cells = next_plane_it->getIdToCellMap();

        
      for(auto& plane_cell:plane_it_cells) {
        for(auto& next_plane_cell:next_plane_it_cells) {
          if(plane_cell.first == next_plane_cell.first) {
            continue;
          }

          double distance = getMinDistanceBetweenSegments(plane_cell.second.wire().getFirstPoint(),
                                                          plane_cell.second.wire().getSecondPoint(),
                                                          next_plane_cell.second.wire().getFirstPoint(),
                                                          next_plane_cell.second.wire().getSecondPoint());
          // std::cout << c << " " << distance << " " << plane_cell.first() << " " << next_plane_cell.first() << std::endl;
          if (distance < max_distance) {
            plane_cell.second.addAdjacentCell(&(next_plane_cell.second));
            next_plane_cell.second.addAdjacentCell(&(plane_cell.second));
          }
        }
      }
    }
    // break;
  }
}

void SANDGeoManager::rearrange_planes()
{
  _id_to_plane.clear();
  std::sort(_planes.begin(), _planes.end(), 
            [](const SANDTrackerPlane& p1, const SANDTrackerPlane& p2)
              {return p1.getPosition().Z() < p2.getPosition().Z();});

  for (auto it = _planes.begin();
            it != _planes.end(); ++it) {
    _id_to_plane[it->uid()] = it;
  }
}

void SANDGeoManager::set_wire_info()
{
  geo_->CdTop();
  TGeoHMatrix matrix = *gGeoIdentity;
  std::string geometry;
  set_wire_info(matrix);
  if (geo_->FindVolumeFast("STTtracker_PV")) {
    std::cout << "using SAND tracker : STT\n";
    geometry = "STT";
  } else {
    std::cout << "using SAND tracker : DRIFT CHAMBER\n";
    geometry = "DRIFT";
  }
  rearrange_planes();
  fill_adjacent_cells(geometry);
  std::cout << "writing wiremap_ info on separate file\n";
  std::cout << "wiremap_ size: " << wiremap_.size() << std::endl;
  WriteMapOnFile(geometry, wiremap_);
  PrintModulesInfo(0);
  DrawModulesInfo();
}

void SANDGeoManager::PrintModulesInfo(int verbose)
{
  std::cout << "There are " << _planes.size() << " planes in the geometry:" << std::endl;
  for (const auto& p:_planes) {
    std::cout << "  - Plane " << p.uid()() << std::endl;
    std::cout << "    Wire rotation: " << p.getRotation() << std::endl;
    std::cout << "    Center position: " << p.getPosition().X() << " " 
                                         << p.getPosition().Y() << " " 
                                         << p.getPosition().Z() << std::endl;
    std::cout << "    Dimensions: "    << p.getDimension().X() << " " 
                                       << p.getDimension().Y() << " " 
                                       << p.getDimension().Z() << std::endl;
    std::cout << "    List of cells (" << p.nCells() << "):"  << std::endl;
    if (verbose >= 1) {
      for(const auto& c:p.getIdToCellMap()) {
        std::cout << "      " << c.first() << std::endl;
        std::cout << "        Center: " << c.second.wire().center().X() << " "
                                        << c.second.wire().center().Y() << " "
                                        << c.second.wire().center().Z() << std::endl;
        std::cout << "        Length: " << c.second.wire().length()     << std::endl;
        std::cout << "        Point1: " << c.second.wire().getFirstPoint().X() << " "
                                        << c.second.wire().getFirstPoint().Y() << " "
                                        << c.second.wire().getFirstPoint().Z() << std::endl;
        std::cout << "        Point2: " << c.second.wire().getSecondPoint().X() << " "
                                        << c.second.wire().getSecondPoint().Y() << " "
                                        << c.second.wire().getSecondPoint().Z() << std::endl;
        std::cout << "        Adjacent ids: ";
        for (const auto& adj:c.second.getAdjacentCell()) std::cout << adj->id()() << " ";
        std::cout << std::endl;

      }
    }
  }
}

void SANDGeoManager::DrawModulesInfo()
{
  gStyle->SetOptStat(0);
  TCanvas cc("", "", 1000, 1000);
  cc.cd();

  auto plane = _planes[3];
  
  TH2D h("","", 10, plane.getPosition().X() - plane.getDimension().X() / 2, plane.getPosition().X() + plane.getDimension().X() / 2, 
                10, plane.getPosition().Y() - plane.getDimension().Y() / 2, plane.getPosition().Y() + plane.getDimension().Y() / 2);

  h.Draw();

  for (const auto& c:plane.getIdToCellMap()) {
    TLine* l = new TLine(c.second.wire().getFirstPoint().X(), c.second.wire().getFirstPoint().Y(),
                         c.second.wire().getSecondPoint().X(), c.second.wire().getSecondPoint().Y());
    l->Draw("same");
  }
  
  cc.SaveAs("plane.png");
}

void SANDGeoManager::WriteMapOnFile(std::string fName,
                                    const std::map<SANDWireID, SANDWireInfo>& map)
{
  std::fstream file_wireinfo;
  file_wireinfo.open(fName + "_info.txt", std::ios::out);
  file_wireinfo << "id,x,y,z,length,orientation,ax,ay,az\n";
  for (auto& wire : map) {
    SANDWireInfo w = wire.second;
    int orientation =
        (w.orientation() == SANDWireInfo::Orient::kVertical) ? 1 : 0;
    file_wireinfo << std::setprecision(20) << wire.first() << "," << w.x() << ","
                  << w.y() << "," << w.z() << "," << w.length() << ","
                  << orientation << "," << w.ax() << "," << w.ay() << ","
                  << w.az() << "\n";
  }
  file_wireinfo.close();
}

void Counter::IncrementCounter(std::string k)
{
  hit_counter_[k]++;
}

void SANDGeoManager::PrintCounter()
{
  for (auto c : counter_.hit_counter_)
    std::cout << "\n" << c.first << " : " << c.second << "\n";
}
void SANDGeoManager::init(TGeoManager* const geo)
{
  geo_ = geo;
  counter_.hit_counter_.clear();
  cellmap_.clear();
  wiremap_.clear();
  wire_tranverse_position_map_.clear();
  _planes.clear();
  _id_to_plane.clear();
  set_ecal_info();
  set_wire_info();
}

void SANDGeoManager::SetGeoCurrentPoint(double x, double y, double z) const
{
  double p[3] = {x, y, z};
  geo_->SetCurrentPoint(p);
}

void SANDGeoManager::SetGeoCurrentDirection(double x, double y, double z) const
{
  geo_->SetCurrentDirection(x, y, z);
}

void SANDGeoManager::InitVolume(volume& v) const
{
  auto p = geo_->GetCurrentPoint();
  v.geo_volume = geo_->FindNode(p[0], p[1], p[2])->GetVolume();
  v.volume_path = geo_->GetPath();
  if (v.volume_path.Contains("Active")) {
    v.IsActive = true;
  } else {
    v.IsActive = false;
  }
}

void SANDGeoManager::LOGVolumeInfo(volume& v) const
{
  auto p = geo_->GetCurrentPoint();
  std::cout << "Current Point " << p[0] << ", " << p[1] << ", " << p[2] << "\n";
  std::cout << "volume path " << v.volume_path << "\n";
  std::cout << "is active volume ? :" << v.IsActive << "\n";
}

int SANDGeoManager::get_ecal_cell_id(double x, double y, double z) const
{
  if (geo_ == 0) {
    std::cout << "ERROR: TGeoManager pointer not initialized" << std::endl;
    throw "";
  }

  TGeoNode* node = geo_->FindNode(x, y, z);

  if (node == 0) return -999;

  TString volume_name = node->GetName();
  TString volume_path = geo_->GetPath();

  if (!volume_name.Contains("Active")) {
    if (volume_name.Contains("Passive")) {
      volume_name.ReplaceAll("Passive", "Active");
      volume_path.ReplaceAll("Passive", "Active");
    } else if (volume_name.Contains("end")) {
      std::cout << __FILE__ << " " << __LINE__ << "\n";
      // auto n=geo_->FindNormalFast();
      double n[3] = {1., 0., 0.};
      geo_->SetCurrentDirection(n[0], n[1], n[2]);
      std::cout << std::setprecision(15) << "calling FindNextActiveLayer for "
                << volume_name << " x y z " << x << ", " << y << ", " << z
                << "\n";
      std::cout << std::setprecision(15) << " GetCurrentPoint  x y z "
                << geo_->GetCurrentPoint()[0] << ", "
                << geo_->GetCurrentPoint()[1] << ", "
                << geo_->GetCurrentPoint()[2] << "\n";
      std::cout << std::setprecision(15) << " GetCurrentDirection  x y z "
                << n[0] << ", " << n[1] << ", " << n[2] << "\n";
      volume_name = FindNextActiveLayer(geo_->GetCurrentPoint(),
                                        geo_->GetCurrentDirection());
      auto p = geo_->GetCurrentPoint();
      std::cout << "after calling FindNextActiveLayer volume_name : "
                << volume_name << "\n";
      std::cout << "after calling FindNode current point : "
                << geo_->FindNode(p[0], p[1], p[2])->GetName() << "\n";
      volume_path.Append("/");
      volume_path.Append(volume_name);
    } else {  // manage cases like "ECAL_lv_18_PV_0" and "Frame_C_PV_0"
      std::cout << __FILE__ << " " << __LINE__ << "\n";
      std::cout << std::setprecision(15) << "calling FindNextActiveLayer for "
                << volume_name << " x y z " << x << ", " << y << ", " << z
                << "\n";
      std::cout << std::setprecision(15) << " GetCurrentPoint  x y z "
                << geo_->GetCurrentPoint()[0] << ", "
                << geo_->GetCurrentPoint()[1] << ", "
                << geo_->GetCurrentPoint()[2] << "\n";
      std::cout << std::setprecision(15) << " GetCurrentDirection  x y z "
                << geo_->GetCurrentDirection()[0] << ", "
                << geo_->GetCurrentDirection()[1] << ", "
                << geo_->GetCurrentDirection()[2] << "\n";
      volume_name = FindNextActiveLayer(geo_->GetCurrentPoint(),
                                        geo_->GetCurrentDirection());
      auto p = geo_->GetCurrentPoint();
      std::cout << "after calling FindNextActiveLayer volume_name : "
                << volume_name << "\n";
      std::cout << "after calling FindNode current point : "
                << geo_->FindNode(p[0], p[1], p[2])->GetName() << "\n";
      volume_path.Append("/");
      volume_path.Append(volume_name);
    }
    geo_->cd(volume_path);
    node = geo_->GetCurrentNode();
  }

  if (!((TString)node->GetName()).Contains("Active")) {
    std::cout << __FILE__ << " " << __LINE__ << "\n";
    std::cout << "invalid current node (is not ecal active layer): "
              << node->GetName() << "\n";
    throw "";
  }
  if (check_and_process_ecal_path(volume_path) == false) return -999;
  //////

  int detector_id;
  int module_id;
  int layer_id;
  int cell_local_id;

  // barrel modules
  if (is_ecal_barrel(volume_name)) {

    get_ecal_barrel_module_and_layer(volume_name, volume_path, detector_id,
                                     module_id, layer_id);
    get_ecal_barrel_cell_local_id(x, y, z, node, cell_local_id);
  }
  // end cap modules
  else if (is_ecal_endcap(volume_name)) {

    get_ecal_endcap_module_and_layer(volume_name, volume_path, detector_id,
                                     module_id, layer_id);
    get_ecal_endcap_cell_local_id(x, y, z, node, cell_local_id);
  } else {
    return -999;
  }

  int cell_unique_id =
      encode_ecal_cell_id(detector_id, module_id, layer_id, cell_local_id);
  return cell_unique_id;
}

double SANDGeoManager::GetHitCellDistance(TVector2 rotated_yz_hit_position, 
                                        std::map<SANDTrackerCellID, SANDTrackerCell>::const_iterator cell_it, 
                                        const SANDTrackerPlane& plane) const
{
  TVector2 global_wire_xy_position(cell_it->second.wire().center().X(), cell_it->second.wire().center().Y());
  TVector2 rotated_wire_2d_position = GlobalToRotated(global_wire_xy_position, plane);
  
  TVector2 rotated_yz_wire_position(rotated_wire_2d_position.Y(), 
                                    cell_it->second.wire().center().Z() - plane.getPosition().Z());
  return (rotated_yz_hit_position - rotated_yz_wire_position).Mod();
}

SANDTrackerCellID SANDGeoManager::GetClosestCellToHit(TVector3 hit_center, const SANDTrackerPlane& plane, bool checkCloseCells = false) const
{
  TVector2 global_hit_xy_position(hit_center.X(), hit_center.Y());
  TVector2 rotated_hit_xy_position = GlobalToRotated(global_hit_xy_position, plane);
  double transverse_coord = rotated_hit_xy_position.Y();
  
  TVector2 rotated_yz_hit_position(transverse_coord, hit_center.Z() - plane.getPosition().Z());

  std::map<SANDTrackerCellID, SANDTrackerCell>::const_iterator cell_it = plane.getLowerBoundCell(transverse_coord);
  if (cell_it == plane.getIdToCellMapEnd()) {
    cell_it = plane.getIdToCellMap().begin();
  }
  std::map<SANDTrackerCellID, SANDTrackerCell>::const_iterator next_cell_it = std::next(cell_it);
  if (next_cell_it == plane.getIdToCellMapEnd()) {
    next_cell_it = plane.getIdToCellMap().begin();
  }
 
  double distance1 = 1E9;
  double distance2 = 1E9;
  while (true) {
    distance1 = GetHitCellDistance(rotated_yz_hit_position, cell_it, plane);
    distance2 = GetHitCellDistance(rotated_yz_hit_position, next_cell_it, plane);

    if (checkCloseCells) {
      double h,w;
      cell_it->second.size(w,h);
      w /= 2;
      h /= 2;
      if (distance1 > w && distance2 > w) {
        if (cell_it != plane.getIdToCellMap().cbegin()) {
          cell_it--;
        } 
        if (std::next(next_cell_it) != plane.getIdToCellMap().end()) {
          next_cell_it++;
        }
        if (cell_it == plane.getIdToCellMap().cbegin() && 
            std::next(next_cell_it) == plane.getIdToCellMap().cend()) {
          break;
        }
      } else {
        break;
      }
    } else {
      break;
    }
  }

  return (distance1 < distance2) ? cell_it->first : next_cell_it->first;
}

SANDTrackerCellID SANDGeoManager::get_stt_tube_id(double x, double y, double z) const
{
  if (geo_ == 0) {
    std::cout << "ERROR: TGeoManager pointer not initialized" << std::endl;
    return -999;
  }

  TGeoNode* node = geo_->FindNode(x, y, z);

  TString node_path = gGeoManager->GetPath();
  SANDTrackerPlaneID stt_plane_unique_id = get_stt_plane_id(node_path);

  auto& plane = _planes.at(GetPlaneIndex(stt_plane_unique_id)());

  TVector3 hit_center(x, y, z);
  return GetClosestCellToHit(hit_center, plane, true);
}

long SANDGeoManager::print_stt_tube_id(double x, double y, double z) const
{
  if (geo_ == 0) {
    std::cout << "ERROR: TGeoManager pointer not initialized" << std::endl;
    return -999;
  }

  TGeoNode* node = geo_->FindNode(x, y, z);
  TString volume_name = node->GetName();
  std::cout << volume_name << "\n";
  return -1;
}

long SANDGeoManager::get_wire_id(long drift_plane_id, double z,
                                 double transverse_coord) const
{
  // return the id of the closest wire to the poin x y z
  if (geo_ == 0) {
    std::cout << "ERROR: TGeoManager pointer not initialized" << std::endl;
    return -999;
  }
  long wire_id = -999;

  std::map<double, long>::const_iterator it =
      wire_tranverse_position_map_.at(drift_plane_id)
          .lower_bound(transverse_coord);
  // return the wire with the smallest coordinate grater than transverse_coord

  if (it == wire_tranverse_position_map_.at(drift_plane_id).begin()) {
    wire_id = wire_tranverse_position_map_.at(drift_plane_id).begin()->second;
  } else if (it == wire_tranverse_position_map_.at(drift_plane_id).end()) {
    wire_id = wire_tranverse_position_map_.at(drift_plane_id).rbegin()->second;
  } else {
    SANDWireInfo wire1 = wiremap_.at(it->second);
    SANDWireInfo wire2 = wiremap_.at(std::prev(it)->second);

    TVector2 v1;
    TVector2 v2;

    v1.SetX(wire1.z());
    v2.SetX(wire2.z());

    SANDTrackerPlaneID drift_plane_local_id = get_drift_plane_id(geo_->GetPath(), true);

    if (drift_plane_local_id() == 2) {
      v1.SetY(wire1.x());
      v2.SetY(wire2.x());
    } else {
      v1.SetY(wire1.y());
      v2.SetY(wire2.y());
    }

    TVector2 v(z, transverse_coord);

    if ((v - v1).Mod() > (v - v2).Mod()) {
      wire_id = std::prev(it)->second;
    } else {
      wire_id = it->second;
    }
  }
  return wire_id;
}

bool SANDGeoManager::IsOnEdge(TVector3 point) const
{
  bool OnEdge = 0;
  counter_.IncrementCounter("total");
  TString volume = geo_->FindNode(point.X(), point.Y(), point.Z())->GetName();

  if (!is_drift_plane(volume)) {
    counter_.IncrementCounter("edge");
    OnEdge = 1;
  }
  return OnEdge;
}

TVector3 SANDGeoManager::SmearPoint(TVector3 point, double epsilon) const
{
  TRandom3 ran;
  double theta = ran.Uniform(0, TMath::Pi());
  double phi = ran.Uniform(0, 2 * TMath::Pi());
  double x = point.X() + epsilon * TMath::Sin(theta) * TMath::Cos(phi);
  double y = point.Y() + epsilon * TMath::Sin(theta) * TMath::Sin(phi);
  double z = point.Z() + epsilon * TMath::Cos(theta);
  return {x, y, z};
}

TVector3 SANDGeoManager::FindClosestDrift(TVector3 point,
                                          double epsilon = 0.01) const
// move by 10 micron
{
  bool drift_found = 0;
  TVector3 smeared_point;
  int trials = 0;
  while (!drift_found) {
    trials++;
    smeared_point = SmearPoint(point, epsilon);
    TString volume = geo_->FindNode(smeared_point.X(), smeared_point.Y(),
                                    smeared_point.Z())->GetName();
    if (is_drift_plane(volume)) drift_found = 1;
    if (trials > 1000) {
      std::cout << "not able to find closest drift";
      break;
    }
  }
  return {smeared_point.X(), smeared_point.Y(), smeared_point.Z()};
}

std::vector<SANDTrackerCellID> SANDGeoManager::get_segment_ids(const TG4HitSegment& hseg)
    const
{

  // What are these?
  IsOnEdge(hseg.Start.Vect());
  IsOnEdge(hseg.Stop.Vect());

  auto middle = (hseg.Start + hseg.Stop) * 0.5;

  if (IsOnEdge(middle.Vect())) {
    counter_.IncrementCounter("weird");
    // middle = {FindClosestDrift(middle.Vect()), middle.T()};
    long particle_id = hseg.GetPrimaryId();
    return {SANDTrackerCellID(-999), SANDTrackerCellID(particle_id)};
  }

  TGeoNode* node = geo_->FindNode(middle.X(), middle.Y(), middle.Z());
  TString node_path = gGeoManager->GetPath();
  SANDTrackerPlaneID drift_plane_unique_id = get_drift_plane_id(node_path);

  // To Do: use the map?
  auto& plane = _planes.at(GetPlaneIndex(drift_plane_unique_id)());

  SANDTrackerCellID cell_id_start = GetClosestCellToHit(hseg.Start.Vect(), plane);
  SANDTrackerCellID cell_id_stop  = GetClosestCellToHit(hseg.Stop.Vect(),  plane);

  return {cell_id_start, cell_id_stop};
}
