#include <iostream>
#include <fstream>
#include <TAxis.h>
#include <TRandom3.h>

#include "SANDRecoUtils.h"

#include "TFile.h"
#include "TTree.h"
#include "TCanvas.h"
#include "TGraph2D.h"
#include "TGraph.h"
#include "TLegend.h"
#include "TG4Event.h"
#include "TH1D.h"
#include "TG4HitSegment.h"

/*
    CODE SKELETON:
    - DEFAULT SIMULATION SETTINGS
    - CONSTANS
    - GLOBAL VARIABLES
    - INPUT/OUTPUT FUNCTIONS
    - EVENT SELECTION
    - DIGITIZATION
    - EDEP DIGITIZATION
    - FITTING
*/

// DEFAULT SIMULATION SETTINGS_________________________________________________

bool INCLUDE_SIGNAL_PROPAGATION = true;

bool INCLUDE_HIT_TIME = true;

bool INCLUDE_TDC_SMEARING = false;

bool _DEBUG_ = false;

bool USE_NON_SMEARED_TRACK = false;

unsigned int MIN_NOF_XZ_HITS = 5;

unsigned int MIN_NOF_ZY_HITS = 5;

// CONSTANS____________________________________________________________________

const double SAND_CENTER_X = 0.;

const double SAND_CENTER_Y = -2384.73;

const double SAND_CENTER_Z = 23910.;

// !! HARDCODED (I know you are judging me, stop it)
// does not include frames
const double SAND_TRACKER_X_LENGTH = 3220.0;

// distance in mm from SUPERMOD frames
const double FIDUCIAL_CUT = 100.;

// height in mm
const double SUPERMOD_Y_HEIGHT[5] = {3755.16996258, // A1, A2
                                     3550.97659024, // B1, B2
                                     3129.97277395, // C1, C2
                                     1262.30218417,  // X1    
                                     2466.05424949 // X0   
};

const double MYLAR_2_MYLAR_DIST = 10.02; // mm

const double SENSE_2_SENSE_DIST = 20.28; // mm

const double TDC_SMEARING = 1.; // ns, default disabled

const double NEUTRINO_INTERACTION_TIME = 1.; // ns

const double LIGHT_VELOCITY = 299.792458; // mm/ns

// GLOBAL VARIABLES____________________________________________________________

TG4Event* evEdep = nullptr;

TG4Event* evDigit = nullptr;

TGeoManager* geo = nullptr;

std::vector<dg_wire>* RecoUtils::event_digits = nullptr;

std::vector<TG4HitSegment> event_drift_hits;

// pointer to vertical wires of event_digits
std::vector<dg_wire*> vertical_fired_wires;

// pointer to horizontal wires of event_digits
std::vector<dg_wire*> horizontal_fired_wires;

std::vector<Line2D>* track_segments_ZY = nullptr;

std::vector<Line2D>* track_segments_XZ = nullptr;

// COLORS ______________________________________________________________________

Color::Modifier def(Color::FG_DEFAULT);
Color::Modifier red(Color::FG_RED); // warnings
Color::Modifier green(Color::FG_GREEN); // opeartions
Color::Modifier blue(Color::FG_BLUE); // results

void LOG(TString i, const char* out){
    if(i.Contains("I")){ // info
        std::cout << green << "[INFO] " << out << def << std::endl;
        std::cout << "\n";
    }else if(i.Contains("i")){
        std::cout << green << " |___ " << out << def << std::endl;
        std::cout << "\n";
    }else if(i.Contains("R")){ // result
        std::cout << blue << "[RESULT] " << out << def << std::endl;
        std::cout << "\n";
    }else if(i.Contains("W")){ // waring
        std::cout << red << "[WARNING] " << out << def << std::endl;
        std::cout << "\n";
    }else{
        std::cout << out;
    }
}

// _____________________________________________________________________________

// INPUT/OUTPUT FUNCTIONS_______________________________________________________

void help_input(){
    std::cout << "\n";
    std::cout << red << "./buil/bin/test_reconstructionNLL "
              << "-edep <EDep file> "
              << "-digit <digitization file> "
              << "-wireinfo <WireInfo file> "
              << "-o <fOuptut.root> "
              << "[signal_propagation] [hit_time] [debug] [track_no_smear]\n";
    std::cout << "\n";
    std::cout << "<WireInfo file>      : see tests/wireinfo.txt \n";
    // std::cout << "--signal_propagation : include signal_propagation in digitization \n";
    // std::cout << "--hit_time           : include hit time in digitization \n";
    // std::cout << "--track_no_smear     : reconstruct non smeared track (NO E_LOSS NO MCS)\n";
    std::cout << "--debug              : debug mode " << def << std::endl;
}

void ReadWireInfos(const std::string fInput, std::vector<dg_wire>& wire_infos){
    std::ifstream stream(fInput);
    std::string line;
    std::string wire_value;
    int w_id, w_hor;
    double w_x, w_y, w_z, w_l;
    std::getline(stream, line); // needed to skip header
    while(std::getline(stream, line))
    {
        std::stringstream lineStream(line);
        std::vector<double> wire_values;
        while (getline(lineStream, wire_value, ','))
        {
            double value = std::stod(wire_value);
            wire_values.push_back(value);
        }
        dg_wire wire;
        wire.did = wire_values[0];
        wire.x = wire_values[1];
        wire.y = wire_values[2];
        wire.z = wire_values[3];
        wire.wire_length = wire_values[4];
        /*
            if on txt file you read hor(ientation) == 0,
            means horizontal. Here I invert the values
            to give to hor the mmeaning of horizontal
        */
        wire.hor =  (wire_values[5]==0) ? 1 : 0;
        wire_infos.push_back(wire);
    }
    stream.close();
}

// EVENT SELECTION_____________________________________________________________

std::vector<dg_wire*> SelectWireFiredByTraj(int trj_index){

    std::vector<dg_wire*> selected_wires;

    auto hits = evEdep->SegmentDetectors["DriftVolume"];

    if (!RecoUtils::event_digits) {
        // std::cerr << "Error: event_digits is null!" << std::endl;
        std::cout << "Error: event_digits is null!" << std::endl;
        return selected_wires;
    }

    // run over event fired wires
    for (auto& wire : *RecoUtils::event_digits) {

        bool trj_contributed_to_this = true;

        // run over hits contribuiting to this wire
        for(auto hit_index : wire.hindex){
        
            // run over hits contributors
            for(const auto& contrib_id : hits[hit_index].Contrib){

                // pass only wire whose hits have contrubution
                // from the trajectory with index traj index
                if(evEdep->Trajectories[contrib_id].GetTrackId() != trj_index){

                    trj_contributed_to_this = false;

                    break;
                }
            }
        }
        if(trj_contributed_to_this) selected_wires.push_back(&wire);     
    }
    return selected_wires;
}

void SplitWiresHorVer(const std::vector<dg_wire*>& wires) {
    for (auto* wire : wires) {
        if (wire->hor == 0) {
            vertical_fired_wires.push_back(wire);
        } else if (wire->hor == 1) {
            horizontal_fired_wires.push_back(wire);
        }
    }
}

bool PassSelectionNofHits(unsigned int nof_hor_fired_wires, unsigned int nof_ver_fired_wires){
    if((nof_hor_fired_wires < MIN_NOF_ZY_HITS)|(nof_ver_fired_wires < MIN_NOF_XZ_HITS)){
        LOG("W", TString::Format("Skipping events with not enough fired wires (hor : %d, ver : %d)", 
                                nof_hor_fired_wires, nof_ver_fired_wires).Data());
        return false;
    }else{
        return true;
    }
}

std::string GetVolumeFromCoordinates(std::string units, double x, double y, double z){
    
    if(!geo){
        std::cout<<"GEO not initialized in "<<__FILE__<<" "<<__LINE__<<"\n";
        throw "";
    }
    if(units == "cm"){ // convert to mm
        x = x * 10.;
        y = y * 10.;
        z = z * 10.;
    }else if(units == "m"){ // convert to mm
        x = x * 1000.;
        y = y * 1000.;
        z = z * 1000.;
    }

    auto navigator = geo->GetCurrentNavigator();

    if(!navigator) navigator = geo->AddNavigator();

    // TGeoManager wants cm units
    auto volume = navigator->FindNode(x, y, z)->GetVolume()->GetName();

    return volume;
}

bool IsInSMODFiducialVol(int smod, double x, double y, double z){
    // !!! ALL LENGTHS REQUIRED IN MM !!!
    bool pass_x = (fabs(x - SAND_CENTER_X) <= SAND_TRACKER_X_LENGTH/2. - FIDUCIAL_CUT);
    bool pass_zy = (fabs(y - SAND_CENTER_Y) <= SUPERMOD_Y_HEIGHT[smod]/2. - FIDUCIAL_CUT);
    return pass_x * pass_zy;
}

bool IsInFiducialVolume(std::string units, double x, double y, double z){
    // std::cout << "x " << x << ", y " << y << ", z " << z << "\n";
    TString volName_ = GetVolumeFromCoordinates(units, x, y, z);
    if(units == "m"){ // convert to mm
        x = x*1e3; y = y*1e3; z = z*1e3;
    }else if(units == "cm"){ // convert to mm
        x = x*1e1; y = y*1e1; z = z*1e1;
    }else{}
    std::cout << "volName : "<< volName_ << "\n";
    if(volName_.Contains("Frame")){
        return false;
    }else if(volName_.Contains("_A")){ // one of 2 supermod A
        return IsInSMODFiducialVol(0, x, y, z);
    }else if(volName_.Contains("_B")){ // one of 2 supermod B
        return IsInSMODFiducialVol(1, x, y, z);
    }else if(volName_.Contains("_C")){ // one of 2 supermod C
        return IsInSMODFiducialVol(2, x, y, z);
    }else if(volName_.Contains("_X0")){ // dwstream supermod X0
        return IsInSMODFiducialVol(3, x, y, z);
    }else if(volName_.Contains("_X1")){ // dwstream supermod X1
        return IsInSMODFiducialVol(4, x, y, z);
    }else{
        return false;
    }
}

// DIGITIZATION________________________________________________________________

void CreateDigitsFromHelix(Helix& h, 
                           std::vector<dg_wire>& wire_infos, 
                           std::vector<dg_wire>& fired_wires){
    /* 
        - run over wires
        - set helix range
        - call RecoUtils::GetMinImpactParameter(const Helix& helix, const Line& line)
        - if it is < 5*sqrt(2) mm ( = half diagonal of a cell size of 10 mm)
          and the helix point is inside SAND tracker x range
            - fill dg_wire info with
            - add to event_wire_digits
    */
    fired_wires.clear();
    // usefull in case t_hit included in TDC
    double hit_timer = NEUTRINO_INTERACTION_TIME;
    double last_s_min = 0.;
    //
   // scan all the wires TDC and select only the onces with the closest impact par
   for(auto& w : wire_infos){

    Line l = RecoUtils::GetLineFromWire(w);

    h.SetHelixRangeFromDigit(w);

    if(h.LowLim() > 0) continue;

    /* define point on the helix and on the line corresponding to the impact parameter
       t_min is how much do I move from the wire center to get to the line point closest to the helix.
       It has a sign.
    */
    double s_min, t_min;

    bool HasMinimized = false;

    double impact_par = RecoUtils::GetMinImpactParameter(h, l, s_min, t_min, HasMinimized);

    if((impact_par <= SENSE_2_SENSE_DIST * 0.5 * sqrt(2)) & (fabs(t_min)*l.GetDirectionVector().Mag() <= w.wire_length)){ // conditions for fired_wire

        w.drift_time = impact_par / sand_reco::stt::v_drift;

        w.tdc = w.drift_time;

        if(INCLUDE_TDC_SMEARING)
            w.tdc = RecoUtils::SmearVariable(w.drift_time, TDC_SMEARING, 1)[0];
        
        if(INCLUDE_SIGNAL_PROPAGATION){
            // ATTENCTION WE ARE ASSUMING t_min CALCULATE WRT TO WIRE CENTER
            // CHECK GetMinImpactParameter
            w.signal_time = (t_min + w.wire_length/2.) / sand_reco::stt::v_signal_inwire;
            w.tdc += w.signal_time;
        }

        if(INCLUDE_HIT_TIME){
            if(last_s_min!=0) // for first fired hit hit_timer=NEUTRINO_INTERACTION_TIME
                hit_timer += (h.GetPointAt(s_min) - h.GetPointAt(last_s_min)).Mag() / LIGHT_VELOCITY;
            w.t_hit = hit_timer;
            w.tdc += w.t_hit;
            last_s_min = s_min;
        }
        fired_wires.push_back(w);
    }
   }
   // oreder the wire hit by z and give the a hit time
//    SortWires(fired_wires);
}

std::vector<TG4HitSegment> FilterHits(const std::vector<TG4HitSegment>& hits, const int PDG){
    /*
    filter hits whose PrimaryId is equal to PDG
    */
    
    std::vector<TG4HitSegment> filtered_hits;

    for (auto& hit : hits)
    {
        bool contribution = false;
        for(auto& id : hit.Contrib){
            if((evEdep->Trajectories[id].GetPDGCode()==PDG)&&(evEdep->Trajectories[id].GetParentId()==-1)){
                contribution = true;
                break;
                }
        }
        if(contribution) filtered_hits.push_back(hit);
    }
    return filtered_hits;
}

void UpdateFiredWires(std::vector<dg_wire>& fired_wires, dg_wire new_fired){
    
    bool found = false;
    //(f.did == new_fired.did) && (f.tdc > new_fired.tdc)
    for(auto& f : fired_wires){
        if((f.did == new_fired.did)){ // wire already fired
            found = true;
            // if tdc is shorter than the one saved, update otherwise discard wire
            if(f.tdc > new_fired.tdc){
                f.tdc = new_fired.tdc;
                f.drift_time = new_fired.drift_time;
                f.signal_time = new_fired.signal_time;
                f.t_hit = new_fired.t_hit;
            }
        }
    }
    if(!found) fired_wires.push_back(new_fired);
}

// EDEP DIGITIZATION___________________________________________________________

void SortWiresByTime(std::vector<dg_wire>& wires){
    // sort fired wires by hit time (MC truth): usefull to subract assumed t_hit from measured TDC
    std::sort(wires.begin(), wires.end(), [](const dg_wire &w1, const dg_wire &w2) {
    return w1.t_hit < w2.t_hit;
    });
}

void CreateDigitsFromEDep(const std::vector<TG4HitSegment>& hits,
                          int PDG_hits_filter,
                          const std::vector<dg_wire>& wire_infos, 
                          std::vector<dg_wire>& fired_wires){
    /*,
    Perform digitization of edepsim hits
    */
    fired_wires.clear();
    
    // filter only muon hit segments
    LOG("i", "Select muon hits from MC truth");
    auto muon_hits = FilterHits(hits, PDG_hits_filter);

    for(auto& hit : muon_hits){
        for(auto& wire : wire_infos){

            auto hit_middle = (hit.GetStart()+hit.GetStop())*0.5;

            // first scan along z to find the wire plane
            bool is_hit_in_wire_plane = fabs(wire.z - hit_middle.Z()) < MYLAR_2_MYLAR_DIST * 0.5;
            
            if(is_hit_in_wire_plane){ // pass if hit z coodinate is found in the wire plane

                Line wire_line = RecoUtils::GetLineFromWire(wire);
                
                Line hit_line = Line(hit);

                double closest_2_hit, closest_2_wire;

                double hit_2_wire_dist = RecoUtils::GetSegmentSegmentDistance(wire_line, hit_line, closest_2_hit, closest_2_wire);

                TVector3 w_point = wire_line.GetPointAt(closest_2_hit);
                
                TVector3 h_point = hit_line.GetPointAt(closest_2_wire);

                bool is_hit_in_cell = (wire.hor == true) ? 
                    fabs(w_point.Y() - h_point.Y()) < SENSE_2_SENSE_DIST * 0.5 : fabs(w_point.X() - h_point.X()) <= SENSE_2_SENSE_DIST * 0.5;

                if(is_hit_in_cell){ // // pass if hit y (or x) coodinate is found in the hor (or vertical) wire cell

                    auto fired = RecoUtils::Copy(wire);
                    
                    fired.drift_time = hit_2_wire_dist / sand_reco::stt::v_drift;

                    fired.signal_time = (w_point - wire_line.GetLineUpperLimit()).Mag() / sand_reco::stt::v_signal_inwire;
                    
                    fired.t_hit = ((hit.GetStop() + hit.GetStart())*0.5 + (hit.GetStop() - hit.GetStart())*closest_2_wire).T();

                    fired.tdc = fired.drift_time + fired.signal_time + fired.t_hit;
                    
                    UpdateFiredWires(fired_wires, fired);
                }
            }
        }
    }
}

// FITTING_____________________________________________________________________

Parameter CreateParam(std::string name, 
                      int id,
                      bool fixed_in_fit,
                      double initial_guess,
                      double value,
                      double error){
    Parameter p;
    p.name = name;
    p.id = id;
    p.fixed_in_fit = fixed_in_fit;
    p.initial_guess = initial_guess;
    p.value = value;
    p.error = error;
    return p;
}

void GetTrackFirstGuess(Circle& circle, Line2D& line){
    /*
        First Guess for particle trajectory is give by a fit of the 
        wires coordinates (circle fit in the ZY bending plane and
        linear fit on the XZ plane)
    */
    LOG("i", "Track First Guess : fitting wire coordinates on XZ PLANE");
    line = RecoUtils::WiresLinearFit(vertical_fired_wires);
    LOG("R", TString::Format("first guess : Slope (m) = %f, Intercept (q) =  %f", 
                            line.m(), line.q()).Data());

    LOG("i", "Track First Guess : fitting wire coordinates on ZY PLANE");
    circle = RecoUtils::WiresCircleFit(horizontal_fired_wires);
    LOG("R", TString::Format("first guess : Center = (%f, %f), R =  %f", 
                            circle.center_x(), circle.center_y(), circle.R()).Data());
    
}

double WireZdistance(const dg_wire& wire1, const dg_wire& wire2) {
    return std::fabs(wire1.z - wire2.z);
}

dg_wire FindClosestZWire(const dg_wire& input_wire, 
                        const std::vector<dg_wire*>& wires){
    /*
        return wire with opposit orientation 
        closest to the input_wire along
        the Z axis.
    */
    dg_wire closest;
    double minDistance = std::numeric_limits<double>::max();
    for(const auto& w : wires){
        // compute input_wire - w distance
        double dist = WireZdistance(input_wire, *w);
        if(dist < minDistance){
            // closest wire on the right
            minDistance = dist;
            closest = *w;
        }
    }
    return closest;
}

double GetMissingCoordinate(dg_wire& horizontal_wire, const Line2D& track_guess){
    // get horizontal wire x coordinate from line first guess
    return track_guess.GetXFromY(horizontal_wire.z);
}

double GetMissingCoordinate(dg_wire& vertical_wire, const Circle& track_guess){
    // get vertical wire y coordinate from circle first guess
    // circle intersect the vertical wire in two points
    double y1 = track_guess.GetUpperSemiCircle()->Eval(vertical_wire.z);
    double y2 = track_guess.GetLowerSemiCircle()->Eval(vertical_wire.z);
    // to decide which one of the two is the one we are looking for
    // consider the closest horizontal fired wires
    dg_wire closest_horizontal = FindClosestZWire(vertical_wire, horizontal_fired_wires);
    if(fabs(closest_horizontal.y - y1) < fabs(closest_horizontal.y - y2)){
        return y1;
    }else{
        return y2;
    }
}

template<typename T>
void TDC2ImpactPar(dg_wire& wire, const T& track_guess){
    /*
        Covert the measure TDC into a impact
        parameter (distance track - wire) that
        has produced the measured TDC.
    */
   wire.t_hit_measured = wire.t_hit;

   if(INCLUDE_HIT_TIME){
     wire.t_hit_measured = wire.t_hit; // use MC truth for now
   }

   if(INCLUDE_SIGNAL_PROPAGATION){
    /*
        the coordinate on the (horizontal / vertical) wire from 
        which the signal propagates is approximatively given by  
        the closest (vertical / horizontal) fired wire in space.
    */
        if(wire.hor==true){ // horizontal
            double x_coordinate = GetMissingCoordinate(wire, track_guess);
            if(std::isnan(x_coordinate)) x_coordinate = wire.y;
            TVector3 signal_origin_on_wire = {x_coordinate, wire.y, wire.z};
            wire.missing_coordinate = x_coordinate;
            Line wire_line = RecoUtils::GetLineFromWire(wire);
            wire.signal_time_measured = (signal_origin_on_wire - wire_line.GetLineUpperLimit()).Mag() / sand_reco::stt::v_signal_inwire;
        }else{ // vertical
            double y_coordinate = GetMissingCoordinate(wire, track_guess);
            if(std::isnan(y_coordinate)) y_coordinate = wire.y; 
            TVector3 signal_origin_on_wire = {wire.x, y_coordinate, wire.z};
            wire.missing_coordinate = y_coordinate;
            Line wire_line = RecoUtils::GetLineFromWire(wire);
            wire.signal_time_measured = (signal_origin_on_wire - wire_line.GetLineUpperLimit()).Mag() / sand_reco::stt::v_signal_inwire;
        }
   }
   wire.drift_time_measured = wire.tdc - wire.signal_time_measured - wire.t_hit_measured;
}

template<typename T>
void TDC2DriftDistance(std::vector<dg_wire*>& fired_wires, const T& first_guess){
    /*
        Convert the observed TDC into a drift distance
        for each fired_wires. Store the infos in the wire
        attributes: t_hit_measured, signal_time_measured,
        drift_time_measured.
        
        NOTE : Used the track guess to infer signal time
    */
   for (auto& wire : fired_wires)
   {
        TDC2ImpactPar(*wire, first_guess);
        if(_DEBUG_){
            std::cout << "wire id " << wire->did << " is horizontal : " << wire->hor <<"\n";
            LOG("R", TString::Format("SIGNAL time: true %f, measured %f", wire->signal_time, wire->signal_time_measured).Data());
            std::cout << "\n";
        }
   }
}

std::vector<Circle> Wire2DriftCircle(const std::vector<dg_wire*>& wires){
    std::vector<Circle> drift_cirlces;
    for(const auto& wire : wires){
        if(wire->hor == true){ // horizontal
            Circle c(wire->z, wire->y, wire->drift_time_measured * sand_reco::stt::v_drift);
            drift_cirlces.push_back(c);
        }else{ // vertical
            Circle c(wire->x, wire->z, wire->drift_time_measured * sand_reco::stt::v_drift);
            drift_cirlces.push_back(c);
        }
    }
    return drift_cirlces;
}

// SEGMENT FITTING_____________________________________________________________

// std::vector<Line2D> GetTrackSegments(const std::vector<Circle>& drift_circles, int nof_circle_used){
  
//   std::vector<Line2D> track_segments;
//   std::vector<Circle> first_n_circles;
//   for(auto i=0u; i < nof_circle_used; i++){
//     const Circle& c = drift_circles[i];
//     first_n_circles.push_back(c);
//   }
//   track_segments.push_back(RecoUtils::GetBestTangent2NCircles(first_n_circles));
//   return track_segments;
// }

std::vector<Line2D> GetTrackSegments(const std::vector<Circle>& drift_circles){
    std::vector<Line2D> track_segments;
    unsigned int nof_circles = drift_circles.size();
    for(auto i=0u; i < nof_circles - 2; i++){
        const Circle& first = drift_circles[i];
        const Circle& second = drift_circles[i+1];
        const Circle& third = drift_circles[i+2];
        auto tangent12 = RecoUtils::GetTangetTo3Circles(first, second, third);
        track_segments.push_back(tangent12);
        // if(i==0){
        //     first.PrintCircleInfo();
        //     second.PrintCircleInfo();
        //     third.PrintCircleInfo();
        // }
    }
    return track_segments;
}

// FITTING ON ZY PLANE_________________________________________________________

// CIRLCE FITTING______________________________________________________________

double GetImpactParamiterCircle(Circle c, const dg_wire& wire){
    TVector2 wire_center = {wire.z, wire.y};
    return  c.Distance2Point(wire_center);
}

double FunctorNLL_Circle(const double* p){
    
    double zc = p[0];
    double yc = p[1];
    double R = p[2];

    double nll         = 0.;
    const double sigma = 0.2; // 200 mu_m = 0.2 mm

    Circle c(zc, yc, R);

    double i = 1;
    for(auto& wire : horizontal_fired_wires){

        // r_estimated = impact parameter estimated as distance sin function - wire
        double r_estimated = GetImpactParamiterCircle(c, *wire);

        // r_observed = impact parameter from the observed tdc
        double r_observed = wire->drift_time_measured * sand_reco::stt::v_drift;

        nll += (r_estimated - r_observed) * (r_estimated - r_observed) / (sigma * sigma);

        i++;
    }
    // return nll/(horizontal_fired_wires.size());
    return nll;
}

Circle FitZYDriftCircles(Circle& first_guess,
                      MinuitFitInfos& fit_infos){
    /*
        Fit observed TDCs on ZY plane with a Circle.
        Parameter of the circle:
        - (zc, yc) center of the fitted circle
        - R radius of the circle
    */
    double zc = first_guess.center_x();
    double yc = first_guess.center_y();
    double R = first_guess.R();
 
    ROOT::Math::Functor functor(&FunctorNLL_Circle, 3);
    
    ROOT::Math::Minimizer* minimizer = ROOT::Math::Factory::CreateMinimizer("Minuit", "Migrad");

    minimizer->SetFunction(functor);

    // zc
    minimizer->SetVariable(0, "zc", zc, 5);
    // yc
    minimizer->SetVariable(1, "yc", yc, 5);
    // R
    minimizer->SetVariable(2, "R", R, 200); // 200 mm -> 35 MeV difference

    // minimization settings
    if(_DEBUG_) minimizer->SetPrintLevel(4);

    // start minimization
    minimizer->Minimize();

    // retrieve result of the minimization and errors
    const double* pars = minimizer->X();
    const double* parsErrors = minimizer->Errors();
    
    // fill output object fit_infos
    Parameter center_z = CreateParam("zc", 0, false, zc, pars[0], parsErrors[0]);
    Parameter center_y = CreateParam("yc", 1, false, yc, pars[1], parsErrors[1]);
    Parameter radius   = CreateParam("R", 2, false, R, pars[2], parsErrors[2]);
    fit_infos.Auxiliary_name = "Circular_fit_ZY";
    fit_infos.fitted_parameters = {center_z, center_y, radius};
    fit_infos.TMinuitFinalStatus = minimizer->Status();
    fit_infos.NIterations = minimizer->NIterations();
    fit_infos.MinValue = minimizer->MinValue();

    // track from final fit
    Circle c_reco(center_z.value, center_y.value, radius.value);

    // print results of the minimization
    minimizer->PrintResults();

    return c_reco;
}

// FITTING ON XZ PLANE_________________________________________________________

// linear fit
double FunctorNLL_Line(const double* p){
    double m = p[0];
    double q = p[1];

    Line2D test_line(m, q);
    
    double nll = 0.;
    double sigma = 0.2;
    double i = 1;

    for(auto& wire : vertical_fired_wires){

        // r_estimated = impact parameter estimated as 2D distance line - wire
        double r_estimated = test_line.Distance2Point({wire->x, wire->z});

        // r_observed = impact parameter from the observed tdc
        double r_observed = wire->drift_time_measured * sand_reco::stt::v_drift;

        nll += (r_estimated - r_observed) * (r_estimated - r_observed) / (sigma * sigma);

        // std::cout << "wire id : " << wire.did
        //           << ", impact par : true " << wire.drift_time * sand_reco::stt::v_drift
        //           << ", r_observed : " << r_observed
        //           << ", r_estimated : " << r_estimated << "\n";

        i++;
    }
    // return sqrt(nll)/(vertical_fired_wires.size());
    return nll;
}

Line2D FitXZDriftCircles(Line2D first_guess,
                       MinuitFitInfos& fit_infos){
    /*
        Fit observed TDCs on XZ plane with a Line.
        Parameter of the Line:
        y = m*x + q
        - m : slope
        - q : intercept
    */
    ROOT::Math::Functor functor(&FunctorNLL_Line, 2);

    ROOT::Math::Minimizer* minimizer = ROOT::Math::Factory::CreateMinimizer("Minuit", "Migrad");

    minimizer->SetFunction(functor);

    minimizer->SetVariable(0, "m", first_guess.m(), 0.001);
    
    minimizer->SetVariable(1, "q", first_guess.q(), 1.);

    // minimization settings
    if(_DEBUG_) minimizer->SetPrintLevel(4);

    minimizer->Minimize();

    // retrieve result of the minimization and errors
    const double* pars = minimizer->X();
    const double* parsErrors = minimizer->Errors();

    // fill output object fit_infos
    Parameter m = CreateParam("m", 0, false, first_guess.m(), pars[0], parsErrors[0]);
    Parameter q = CreateParam("q", 1, false, first_guess.q(), pars[1], parsErrors[1]);
    fit_infos.Auxiliary_name = "Linear_fit_XZ";
    fit_infos.fitted_parameters = {m, q};
    fit_infos.TMinuitFinalStatus = minimizer->Status();
    fit_infos.NIterations = minimizer->NIterations();
    fit_infos.MinValue = minimizer->MinValue();
    
    // print results of the minimization
    minimizer->PrintResults();

    Line2D l(m.value, q.value);

    return l;
}

void FitDriftCircles(Circle& circle_ZY_plane, 
                     Line2D& line_XZ_plane,
                     MinuitFitInfos& fit_ZY,
                     MinuitFitInfos& fit_XZ
                     ){
        LOG("ii", "Fitting drift circles starting from first guess");
        std::cout << blue << "\n";
        line_XZ_plane = FitXZDriftCircles(line_XZ_plane, fit_XZ);
        std::cout << def << "\n";
            
        LOG("ii", "Fitting drift circles starting from first guess");
        std::cout << blue << "\n";
        circle_ZY_plane = FitZYDriftCircles(circle_ZY_plane, fit_ZY);
        std::cout << def << "\n";
}

// sin fit

double GetImpactParameterSin(TF1* TestSin, const dg_wire& wire){
    /*
        Use numerical Newton method to find distance sin function to a point 
        in (wire.x, wire.z) in 2D space. TestSin = A sin(Bx + C) + D, functi
        on to be minimized is the euclidian distance: 
        - D(x) = (x_wire - x)**2 + (z_wire - TestSin(x))**2
        - D_prime(x) = 2(x_wire - x) + 2(z_wire - TestSin(x))*TestSin_prime(x)
        - TestSin_prime(x) = A B cos(Bx + C)
    */
    double x_wire = wire.x;
    double z_wire = wire.z;
    double x_min = TestSin->GetXmin();
    double x_max = TestSin->GetXmax();

    TF1* TestSin_prime = new TF1("TestSin_prime", "[0]*[1]*cos([1]*x + [2])", x_min, x_max);

    TestSin_prime->SetParameters(TestSin->GetParameter(0),
                                 TestSin->GetParameter(1),
                                 TestSin->GetParameter(2));
    /*
        distance_sin_wire : distance curve TestSin to a point (x_wire, z_wire)
        - variable x (space coordinate)
        - parameter (x_wire, z_wire) coordinate f the wire to which the distace
          is calculated
    */
    TF1* distance_sin_wire = new TF1("distance_sin_wire", [&](double*x, double *par){
        double sin_at_x = TestSin->Eval(x[0]);
        double x_w = par[0];
        double z_w = par[1];
        return sqrt((x[0] - x_w)*(x[0] - x_w) + (sin_at_x - z_w)*(sin_at_x - z_w));
    }, x_min, x_max, 2);              
    distance_sin_wire->SetParameters(x_wire, z_wire);
    
    // derivative of distance
    TF1* distance_sin_wire_prime = new TF1("distance_sin_wire_prime", [&](double*x, double *par){
        double distance = distance_sin_wire->Eval(x[0]);
        double sin_at_x = TestSin->Eval(x[0]);
        double derivative_at_x = TestSin_prime->Eval(x[0]);
        double x_w = par[0];
        double z_w = par[1];
        return 1./(2.*distance)*(2*(x[0] - x_w) + 2*(sin_at_x - z_w)*derivative_at_x);
    }, x_min, x_max, 2);
    distance_sin_wire_prime->SetParameters(x_wire, z_wire);

    double x_guess = x_wire;

    double impact_par = RecoUtils::NewtonRaphson2D(distance_sin_wire, distance_sin_wire_prime, x_guess, 0.2, 1000);

    return impact_par;
}

double NLL_Sin(TF1* TestSin, std::vector<dg_wire*>& wires){
    /*
        run over vertical fired wires
        - get impact par from TDC
        - compare it with wire - track 
          min distance
    */
    static int calls   = 0;
    double nll         = 0.;
    const double sigma = 0.2; // 200 mu_m = 0.2 mm
    
    for(auto& wire : wires){

        // r_estimated = impact parameter estimated as distance sin function - wire
        double r_estimated = GetImpactParameterSin(TestSin, *wire);

        // r_observed = impact parameter from the observed tdc
        double r_observed = wire->drift_time_measured * sand_reco::stt::v_drift;

        nll += (r_estimated - r_observed) * (r_estimated - r_observed) / (sigma * sigma);

        // std::cout << "impact par : true " << wire.drift_time * sand_reco::stt::v_drift
        //                << ", r_observed : " << r_observed
        //                << ", r_estimated : " << r_estimated << "\n";

    }
    calls ++;
    return sqrt(nll)/wires.size();
}

double FunctorNLL_Sin(const double* p){
    
    double amplitude = p[0];
    double frequency = p[1];
    double phase = p[2];
    double offset = p[3];
    // fixed
    double x_min = p[4];
    double x_max = p[5];
    
    TF1* TestSin = new TF1("TestSin", "[0]*sin([1]*x + [2]) + [3]", x_min, x_max);
    
    // Set initial parameters for the fit
    TestSin->SetParameters(amplitude, 
                           frequency, 
                           phase, 
                           offset);
    
    return NLL_Sin(TestSin, vertical_fired_wires);
}

TF1* GetRecoXZTrack(TF1* first_guess,
                    MinuitFitInfos& fit_infos){
    /*
        Fit TDCs on XZ plane using a sin function.
        Parameter of the sin function:
        A * sin(B * x + C) + D
        - A: amplitude 
        - B: frequency
        - C: phase
        - D: offset
    */
    double amplitude = first_guess->GetParameter(0); // A
    double frequency = first_guess->GetParameter(1); // B
    double phase = first_guess->GetParameter(2); // C
    double offset = first_guess->GetParameter(3); // D
    double x_min = first_guess->GetXmin();
    double x_max = first_guess->GetXmax();
    
    ROOT::Math::Functor functor(&FunctorNLL_Sin, 6);                  

    ROOT::Math::Minimizer* minimizer = ROOT::Math::Factory::CreateMinimizer("Minuit", "Migrad");

    minimizer->SetFunction(functor);

    // A
    minimizer->SetLimitedVariable(0, "A", amplitude, amplitude*0.01, amplitude*0.8,amplitude*1.2);
    // B
    minimizer->SetLimitedVariable(1, "B", frequency, frequency*0.01, frequency*0.8, frequency*1.2);
    // C
    // minimizer->SetLimitedVariable(2, "C", phase, phase*0.01, phase*0.8, phase*1.2);
    // D
    // minimizer->SetLimitedVariable(3, "D", offset, offset*0.01, offset*0.8, offset*1.2);

    // extreme [x_min, x_max]
    minimizer->SetFixedVariable(2, "C", phase);
    minimizer->SetFixedVariable(3, "D", offset);
    minimizer->SetFixedVariable(4, "x_min", x_min);
    minimizer->SetFixedVariable(5, "x_max", x_max);

    // minimization settings
    if(_DEBUG_) minimizer->SetPrintLevel(4);

    // start minimization
    minimizer->Minimize();

    // retrieve result of the minimization and errors
    const double* pars = minimizer->X();
    const double* parsErrors = minimizer->Errors();
    
    // fill output object fit_infos
    Parameter A = CreateParam("amplitude", 0, false, amplitude, pars[0], parsErrors[0]);                                        
    Parameter B = CreateParam("frequency", 1, false, frequency, pars[1], parsErrors[1]);                                        
    Parameter C = CreateParam("phase", 2, true, phase, pars[2], parsErrors[2]);                                        
    Parameter D = CreateParam("offset", 3, true, offset, pars[3], parsErrors[3]);
    fit_infos.Auxiliary_name = "Sin_Fit_XZ";                          
    fit_infos.fitted_parameters = {A,B,C,D};                          
    fit_infos.TMinuitFinalStatus = minimizer->Status();
    fit_infos.NIterations = minimizer->NIterations();
    fit_infos.MinValue = minimizer->MinValue();

    // final fit
    TF1* SinFinalFit = new TF1("SinFinalFit", "[0]*sin([1]*x + [2]) + [3]", x_min, x_max);
    SinFinalFit->SetParameters(pars[0],pars[1],pars[2],pars[3]);

    // print results of the minimization
    minimizer->PrintResults();

    return SinFinalFit;
}

// Helix Reconstruct(Circle FittedCircle,
//                   Line2D FittedLine,
//                   const std::vector<dg_wire*>& hor_wires,
//                   const std::vector<dg_wire*>& ver_wires
//                   ){
//     /*
//         Define the reconstructed helix from
//         the separate reconstruction of the 
//         track in the XZ plane and ZY plane
//     */
//     double zc = FittedCircle.center_x();
//     double yc = FittedCircle.center_y();
//     double R = FittedCircle.R();
//     double m = FittedLine.m();
//     double q = FittedLine.q();
    
//     bool forward_track = (ver_wires[0]->z < ver_wires[1]->z) ? 1 : 0;

//     TVector2 vertex_XZ = {ver_wires[0]->x, ver_wires[0]->x * m + q};

//     TVector2 vertex_ZY;
//     double upper_y = FittedCircle.GetUpperSemiCircle()->Eval(hor_wires[0]->z);
//     double lower_y = FittedCircle.GetLowerSemiCircle()->Eval(hor_wires[0]->z);
//     if(fabs(upper_y - hor_wires[0]->y) < fabs(lower_y - hor_wires[0]->y)){
//        vertex_ZY = {hor_wires[0]->z, upper_y};
//      }else{
//        vertex_ZY = {hor_wires[0]->z, lower_y};
//      }
    
    
//     double Phi0 = FittedCircle.GetAngleFromPoint(vertex_ZY.X(), vertex_ZY.Y());
//     auto track_direction_ZY = FittedCircle.GetDerivativeAt(vertex_ZY.X(), vertex_ZY.Y()).Unit();
//     TVector2 pt = R*0.3*0.6 * track_direction_ZY;
//     /*
//         reconstructed pt is the value of the tangent0 
//         to the circle in the first point of the curve
//     */
//     double pz = (forward_track) ? -1. * fabs(pt.X()) : fabs(pt.X());
//     double py = pt.Y();
//     double px = pz / FittedLine.m();
    
//     LOG("R", TString::Format("Reconstructed muon (px, py, pz) : (%f, %f, %f)",px,py,pz).Data());

//     double dip = atan2(px, pt.Mod());
    
//     Helix h(R, dip, Phi0, 1, {vertex_XZ.X(), vertex_ZY.Y(), vertex_XZ.Y()});

//     return h;
// }

// MAIN________________________________________________________________________

int main(int argc, char* argv[]){

    if (argc < 3 || argc > 10) {
        help_input();
    return -1;
    }
    /* 
       provide edepsim input to run the reconstruction on non-smeared muon tracks
       (for testing used file "/storage/gpfs_data/neutrino/users/gi/sand-reco/tests/test_drift_edep.root")
    */
    const char* fEDepInput = "";
    
    /*
       provide digit file input to run the reconstruction on edep smeared muon tracks
    */
    const char* fDigitInput = "";
    
    const char* fOutput = "";
    
    /* 
       look up table previously created in the Digitization as csv file that contains
       all info about wires in the geometry
       for testing used file  "/storage/gpfs_data/neutrino/users/gi/sand-reco/tests/wireinfo.txt"
    */
    const char* fWireInfo = "";

    int index = 1;

    std::string trackerType = "DriftVolume";
    
    LOG("","\n");
    LOG("I","Reading inputs");

    while (index < argc)
    {
        TString opt = argv[index];
        if(opt.CompareTo("-edep")==0){
            try
            {
                fEDepInput = argv[++index];
                std::cout << "edep file       " << fEDepInput << std::endl;
            }
            catch(const std::exception& e)
            {
                std::cerr << e.what() << '\n';
                return 1;
            }
        }else if(opt.CompareTo("-digit")==0){
            try
            {
                fDigitInput = argv[++index];
                std::cout << "digit file  " << fDigitInput << std::endl;
            }
            catch(const std::exception& e)
            {
                std::cerr << e.what() << '\n';
            }
        }else if(opt.CompareTo("-wireinfo")==0){
            try
            {
                fWireInfo = argv[++index];
                std::cout << "wire_info file  " << fWireInfo << std::endl;
            }
            catch(const std::exception& e)
            {
                std::cerr << e.what() << '\n';
                return 1;
            }
        }else if(opt.CompareTo("-o")==0){
            try
            {
                fOutput = argv[++index];
                std::cout << "Output file     " << fOutput << std::endl;
            }
            catch(const std::exception& e)
            {
                std::cerr << e.what() << '\n';
                return 1;
            }
        }else if(opt.CompareTo("--debug")==0){
            _DEBUG_ = true;
        }else{
            auto ui = argv[++index];
            std::cout<<"unknown input "<< ui << "\n";
            return 1; 
        }
        index++;
    }

    std::cout << "\n";
    
    TFile fEDep(fEDepInput, "READ");
    
    TFile fDigit(fDigitInput, "READ");
    
    TFile fout(fOutput, "RECREATE");
    
    TTree* tEdep = (TTree*)fEDep.Get("EDepSimEvents");
    
    LOG("I","Open TGeoManager from edepsim file");
    geo = (TGeoManager*)fEDep.Get("EDepSimGeometry");
    //geo = TGeoManager::Import("/storage/gpfs_data/neutrino/users/gi/dunendggd/SAND_opt3_DRIFT1.root");
    
    TTree* tDigit = (TTree*)fDigit.Get("tDigit");
    
    TTree tout("tReco", "tReco");
    
    RecoObject reco_object;
    
    std::vector<dg_wire> fired_wires;
    
    std::vector<dg_wire> wire_infos;

    unsigned int edep_event_index;

    bool KeepThisEvent = false;

    std::string fEDepInputStr = fEDepInput;

    std::string fDigitInputStr = fDigitInput;
    
    tEdep->SetBranchAddress("Event", &evEdep);
    tDigit->SetBranchAddress("Event", &evDigit);
    // tDigit->AddFriend(tEdep);
    tDigit->SetBranchAddress("fired_wires", &RecoUtils::event_digits);
    
    tout.Branch("edep_file_input", &fEDepInputStr);
    
    tout.Branch("digit_file_input", &fDigitInputStr);
    
    tout.Branch("edep_event_index", &edep_event_index, "edep_event_index/i");
    
    tout.Branch("KeepThisEvent", &KeepThisEvent, "KeepThisEvent/O");
    
    tout.Branch("reco_object", "reco_object", &reco_object);
    
    LOG("I","Loading wires lookup table");
    ReadWireInfos(fWireInfo, wire_infos);

    // int j = 0;
    for(auto i=0u; i < tEdep->GetEntries(); i++)
    {
        // if(i != 33u && i != 111u && i != 127u && i != 166u) continue;
        LOG("I", TString::Format("********************** PROCESSING EDEP EVENT %d **********************", i).Data());
        
        KeepThisEvent = false;
        
        edep_event_index = i;
        
        RecoUtils::event_digits->clear();
        
        tEdep->GetEntry(i);
        tDigit->GetEntry(i);

        // tDigit->GetEntry(j);
        // j++;

        auto muon_trj = evEdep->Trajectories[0];

        // Discard event if no muon(antimuon) is found
        KeepThisEvent = (abs(muon_trj.GetPDGCode()) != 13);

        if (!KeepThisEvent) {
            // Discard event if vertex not in fiducial volume
            auto vertex = evEdep->Primaries[0];
            KeepThisEvent = IsInFiducialVolume("mm", vertex.GetPosition().X(), vertex.GetPosition().Y(), vertex.GetPosition().Z());
        }

        if (!KeepThisEvent) {
            LOG("W", TString::Format("Skipping Event %d, reason: %s", i,
                (abs(muon_trj.GetPDGCode()) != 13) ? "first trajectory is neither mu- nor mu+" : "not in fiducial volume").Data());
            tout.Fill();  // Fill empty event to keep 1-1 correspondence with input file
            continue;
        }

        event_drift_hits = evEdep->SegmentDetectors["DriftVolume"];

        std::cout << "event number : " << i 
                  << ", muon_momentum : (" 
                  << muon_trj.GetInitialMomentum().X() << ", " 
                  << muon_trj.GetInitialMomentum().Y() << ", " 
                  << muon_trj.GetInitialMomentum().Z() << ", " 
                  << muon_trj.GetInitialMomentum().T() << ")\n";
        LOG("","\n");

        Helix true_helix(muon_trj);

        LOG("I", "Reading Fired Wires From File Digit");
        std::cout << "Event total fired wires : " << RecoUtils::event_digits->size() << "\n";

        // clear vectors from previous events
        horizontal_fired_wires.clear();
        vertical_fired_wires.clear();

        LOG("I", TString::Format("FAKE PATTERN RECO : Select fired_wires that belongs to %d", muon_trj.GetTrackId()).Data());
        std::vector<dg_wire*> selected_wires = SelectWireFiredByTraj(muon_trj.GetTrackId());

        LOG("I", "Group wires in vertical and horizontal");
        SplitWiresHorVer(selected_wires);

        LOG("ii", TString::Format("number of horizontal fired wires for trackid %d : %d", muon_trj.GetTrackId(), (int)horizontal_fired_wires.size()).Data());
        LOG("ii", TString::Format("number of vertical fired wires for trackid %d : %d", muon_trj.GetTrackId(), (int)vertical_fired_wires.size()).Data());
        LOG("I", "Check event with enough hits");
        
        KeepThisEvent = PassSelectionNofHits(horizontal_fired_wires.size(), vertical_fired_wires.size());
        
        if(!KeepThisEvent){
            LOG("W", TString::Format("Skipping Event %d not enough hits", i).Data());
            tout.Fill();
            continue;
        };

        true_helix.PrintHelixPars();

        MinuitFitInfos fit_XZ, fit_ZY;
        Circle circle_ZY_plane;
        Line2D line_XZ_plane;
        TVector3 particle_momentum;
        Helix reco_helix;
        unsigned int nof_cycles = 3;

        LOG("I", "Track First Guess (seed): fitting wire coordinates");
        GetTrackFirstGuess(circle_ZY_plane, line_XZ_plane);

        for (auto cycle = 0u; cycle < nof_cycles; cycle++)
        {
            LOG("I", TString::Format("---------> Starting cycle number %d ", cycle).Data());

            LOG("I","Converting measured TDC into drift time ");
            
            LOG("ii","Converting horizontal wires");
            TDC2DriftDistance<Line2D>(horizontal_fired_wires, line_XZ_plane);
            
            LOG("ii","Converting vertical wires");
            TDC2DriftDistance<Circle>(vertical_fired_wires, circle_ZY_plane);
            
            double deviation = 0.;
            for(auto w : selected_wires) deviation += (w->signal_time - w->signal_time_measured);
            LOG("W", TString::Format("Sum (t_signal_propagation_time_true - t_signal_propagation_time_assumed) %f [ns]", deviation).Data());
                                
            LOG("I", "Creating drift circles from drift time");
            std::vector<Circle> vertical_drift_circles = Wire2DriftCircle(vertical_fired_wires);
            std::vector<Circle> horizontal_drift_circles = Wire2DriftCircle(horizontal_fired_wires);

            LOG("I", "Fitting drift circles with a NLL method");
            FitDriftCircles(circle_ZY_plane, line_XZ_plane, fit_ZY, fit_XZ);
        }

        LOG("I","Reconstructed Helix from Circle (ZY plane) and Line (XZ plane)");
        // reco_helix.Setx0(true_helix.x0());
        // reco_helix.Seth(true_helix.h());
        /*
            To be modified in the future:
                pass vertex and helicity of the true helix
        */
        reco_helix = RecoUtils::GetHelixFromCircleLine(circle_ZY_plane, line_XZ_plane, true_helix, particle_momentum);
        reco_helix.PrintHelixPars();                                     
        LOG("R", TString::Format("True momentum: (%f, %f, %f)",  muon_trj.GetInitialMomentum().X(),  muon_trj.GetInitialMomentum().Y(),  muon_trj.GetInitialMomentum().Z()));
        LOG("R", TString::Format("Reco momentum: (%f, %f, %f)", particle_momentum.X(), particle_momentum.Y(), particle_momentum.Z()));
        // throw "";
        LOG("I", "Filling output tree");
        reco_object.fit_infos_xz = fit_XZ;
        reco_object.fit_infos_zy = fit_ZY;

        // reco_object.fired_wires = *RecoUtils::event_digits;
        reco_object.fired_wires.clear();
        for(dg_wire* wire : selected_wires){
            reco_object.fired_wires.push_back(*wire);
        }

        reco_object.true_helix = true_helix;
        reco_object.pt_true = true_helix.R()*0.3*0.6;
        reco_object.reco_helix = reco_helix;
        reco_object.pt_reco = reco_helix.R()*0.3*0.6;
        reco_object.p_true = {muon_trj.GetInitialMomentum().X(),
                              muon_trj.GetInitialMomentum().Y(), 
                              muon_trj.GetInitialMomentum().Z()};
        reco_object.p_reco = {particle_momentum.X(), 
                              particle_momentum.Y(), 
                              particle_momentum.Z()};
        
        tout.Fill();
    }
    fout.cd();

    tout.Write();

    fout.Close();
}

// reco_object.track_segments_ZY = *track_segments_ZY;
// reco_object.track_segments_XZ = *track_segments_XZ;

// LOG("I","Find local tangets to drift circles in XZ plane");
// LOG("I","Find local tangets to first 3 drift circles in XZ plane");
// auto segmentsXZ = GetTrackSegments(vertical_drift_circles, MIN_NOF_XZ_HITS);
// track_segments_XZ = &segmentsXZ;
// LOG("R", TString::Format("Tangent to first TDC XZ plane m = %f, q = %f", segmentsXZ[0].m(), segmentsXZ[0].q()).Data());

// LOG("I","Find local tangets to first 5 drift circles in XZ plane");
// auto segmentsZY = GetTrackSegments(horizontal_drift_circles, MIN_NOF_ZY_HITS);
// track_segments_ZY = &segmentsZY;
// LOG("R", TString::Format("Tangent to first TDC XZ plane m = %f, q = %f", segmentsZY[0].m(), segmentsZY[0].q()).Data());