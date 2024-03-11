////////////////////////////////////////////////////////////////////////
// Class:       PMTGain
// Module Type: analyzer
// File:        PMTGain_module.cc
//
// Analyzer to read optical waveforms
//
// Authors: A. Szelc, H. Parkinson, A. Hamer, based on framework by L. Paulucci and F. Marinho
////////////////////////////////////////////////////////////////////////

#include <algorithm>
#include <vector>
#include <cmath>
#include <memory>
#include <string>
#include <iostream>

#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Principal/Run.h"
#include "art/Framework/Principal/SubRun.h"
#include "art_root_io/TFileService.h"
#include "canvas/Utilities/InputTag.h"
#include "fhiclcpp/ParameterSet.h"
#include "messagefacility/MessageLogger/MessageLogger.h"

#include "canvas/Utilities/Exception.h"
#include "art/Framework/Services/Registry/ServiceHandle.h"
#include "larcore/Geometry/Geometry.h"

#include "lardata/DetectorInfoServices/DetectorClocksService.h"
#include "lardata/DetectorInfoServices/DetectorClocksServiceStandard.h"
#include "lardata/DetectorInfoServices/DetectorPropertiesService.h"
#include "lardata/DetectorInfoServices/LArPropertiesService.h"
#include "lardataobj/RawData/OpDetWaveform.h"
#include "sbndcode/Utilities/SignalShapingServiceSBND.h"
#include "lardataobj/Simulation/sim.h"
#include "lardataobj/Simulation/SimChannel.h"
#include "lardataobj/Simulation/SimPhotons.h"

#include "TH1D.h"
#include "TFile.h"
#include "TTree.h"

#include "sbndcode/OpDetSim/sbndPDMapAlg.hh"

namespace PDSCali{

  class PMTGain;

  class PMTGain : public art::EDAnalyzer {
  public:
    explicit PMTGain(fhicl::ParameterSet const & p);
    // The destructor generated by the compiler is fine for classes
    // without bare pointers or other resource use.

    // Plugins should not be copied or assigned.
    PMTGain(PMTGain const &) = delete;
    PMTGain(PMTGain &&) = delete;
    PMTGain & operator = (PMTGain const &) = delete;
    PMTGain & operator = (PMTGain &&) = delete;

    // Required functions.
    void analyze(art::Event const & e) override;

    //Selected optional functions
    void beginJob() override;
    void endJob() override;

    opdet::sbndPDMapAlg pdMap; //map for photon detector types  USE THIS TO SELECT ONLY PMTs!!

  private:

    size_t fEvNumber;
    size_t fChNumber;
    double fSampling;
    double fSampling_Daphne;
    double fStartTime;
    double fEndTime;
    //TTree *fWaveformTree;

    // Declare member data here.
    std::string fInputModuleName;
    std::vector<std::string> fOpDetsToPlot;
    std::stringstream histname;
    std::string opdetType;
    std::string opdetElectronics;
    int fLowbin; // bins holding the start and end of the singlePE waveform. 
    int fHibin; //sample range around the peak
    int fNstdev; //number of noise stdevs to set the threshold to 
    int fSpe_region_start; //number of bins after which SPE search starts
    double fNbmax_factor; //set noise analysis range 1 (pre peaks)
    double fNbmin_factor;
    double fN2bmin_factor; //set noise analysis range 2 (post peaks)
    int fManual_bound_hi;
    int fManual_bound_lo; //these might need to be doubles?? check (H) 
    int fEventid; //event id as integer
    bool fAll_events; //do all events or just the selected one specified by eventid
    bool fCut; //make the 100ns cut (make false-we don't use this anymore- A. Bullock) 
    bool fDo_avgspe;
    bool fDo_amp;
    bool fDo_integ; //which analysis do we want to do?


    // Histograms to save:
    std::vector<TH1D *> avgspe;
    std::vector<TH1D *> amp;
    std::vector<TH1D *> integ0;
    std::vector<TH1D *> integ1;
    std::vector<TH1D *> integ2;
    std::vector<TH1D *> integ3;
    std::vector<TH1D *> integ4;
    std::vector<TH1D *> integ5; 
    std::vector<int> navspes;
    int NBINS;


    // these two variables should be defined via .fcl 
    bool fUseAllPMTs;      //if true then NPMTs is 120. If false, it's smaller, and uses size of vector defined just below.
    std::vector<unsigned int> fSelectedPMTs;

    std::vector<int> PMTIndexingVector;

     art::ServiceHandle<art::TFileService> tfs;     
    
     int  GetPMTIndex(int wvf_ch);
  };


  PMTGain::PMTGain(fhicl::ParameterSet const & p)
    :
    EDAnalyzer(p) { // ,
    // More initializers here.
  
      
     
      
    fInputModuleName = p.get< std::string >("InputModule" );
    fOpDetsToPlot    = p.get<std::vector<std::string> >("OpDetsToPlot");
    fUseAllPMTs = p.get< bool >("UseAllPMTs" );
    fSelectedPMTs    = p.get<std::vector<unsigned int> >("SelectedPMTs",{0});
    fLowbin = p.get< int >("lowbin");
    fHibin = p.get < int >("hibin"); 
    fNstdev = p.get < int >("nstdev"); 
    fSpe_region_start = p.get < int >("spe_region_start");
    fNbmax_factor = p.get < double >("nbmax_factor"); 
    fNbmin_factor = p.get < double >("nbmin_factor");
    fN2bmin_factor = p.get < double >("n2bmin_factor"); 
    fManual_bound_hi = p.get < int >("manual_bound_hi");
    fManual_bound_lo = p.get < int >("manual_bound_lo");
    fEventid = p.get < int >("eventid");
    fAll_events = p.get < bool >("all_events");
    fCut = p.get < bool >("cut");
    fDo_avgspe = p.get < bool >("do_avgspe");
    fDo_amp = p.get < bool >("do_amp");
    fDo_integ = p.get <bool >("do_integ");


    auto const clockData = art::ServiceHandle<detinfo::DetectorClocksService const>()->DataForJob();
    fSampling = clockData.OpticalClock().Frequency(); // MHz
    fSampling_Daphne = p.get<double>("DaphneFrequency" );
  

 
    const Double_t NPDS=312;    //this should be calculated given .fcl parameters, if we want to do a smaller number of PMTs.
                                 //Holly - PMTs + xARAPUCAS = 312



    unsigned int tot_pmt_counter=0;
    unsigned int sel_pmt_counter=0;
    
   for(int iPDS=0;iPDS<NPDS;iPDS++)
     {
     if (pdMap.isPDType(iPDS, "xarapuca_vuv") || pdMap.isPDType(iPDS, "xarapuca_vis"))
	     {
             // Skip ihist that passes the if statement
	     continue;
	      }
     else{
         
         if(fUseAllPMTs  || (!fUseAllPMTs && fSelectedPMTs[sel_pmt_counter]==tot_pmt_counter))
		{

 		 PMTIndexingVector.push_back(iPDS);        //this now has the list of PMT channels. i
		 sel_pmt_counter++;
		}


        tot_pmt_counter++;   //increment index of PMT.
        if(!fUseAllPMTs && sel_pmt_counter >= fSelectedPMTs.size())
		break;                   // if this happens, we're done.
	}     


    }   // now PMTIndexingVector contains the numbers of the channel numbers we want to save. Its size is the size of loops
  


    NBINS=fHibin+fLowbin+1; //number of total bins around the peak
    
    // create the histograms needed by the code.
   
    int count = 0;

    for(unsigned int ihist=0;ihist<PMTIndexingVector.size();ihist++)
    {

     avgspe.push_back(tfs->make< TH1D >(Form("avgspe_opchannel_%d", PMTIndexingVector[ihist]), Form("Average SPE Shape, channel %d;Samples from peak;Count",PMTIndexingVector[ihist]), NBINS, -fLowbin, fHibin)); // create histogram for average shape                
     navspes.push_back(0);    

     amp.push_back(tfs->make< TH1D >(Form("amp_opchannel_%d",PMTIndexingVector[ihist]), Form("Amplitude of SPEs, channel %d;Amplitude[ADC];Count",PMTIndexingVector[ihist]), 50, 0, 200)); // create histogram for amplitude
      // THIS IS WHERE I CAN CHANGE MY BINNING: there are currently 50
     integ0.push_back(tfs->make< TH1D >(Form("integ_opchannel_%d_zeromode",PMTIndexingVector[ihist]), Form("'Zero-Mode' Integral of SPEs, channel %d;Integral value [ADC*samples];Count",PMTIndexingVector[ihist]), 50, 0, 500)); // create histogram for integral (zero mode, no local baseline subtraction)
  
     integ1.push_back(tfs->make< TH1D >(Form("integ_opchannel_%d_threshmode",PMTIndexingVector[ihist]), Form("'Threshold-Mode' Integral of SPEs, channel %d;Integral value [ADC*samples];Count",PMTIndexingVector[ihist]), 50, 0, 500)); // create histogram for integral (threshold mode, no local baseline subtraction)

     integ2.push_back(tfs->make< TH1D >(Form("integ_opchannel_%d_manualmode",PMTIndexingVector[ihist]), Form("'Manual-Mode' Integral of SPEs, channel %d;Integral value [ADC*samples];Count",PMTIndexingVector[ihist]), 50, 0, 500)); // create histogram for integral (manual mode, no local baseline subtraction)
   
     integ3.push_back(tfs->make< TH1D >(Form("integ_opchannel_%d_zeromodeB",PMTIndexingVector[ihist]), Form("'Zero-Mode' Integral of SPEs, channel %d;Integral value [ADC*samples];Count",PMTIndexingVector[ihist]), 50, 0, 500)); // create histogram for integral (zero mode, local baseline subtraction)

     integ4.push_back(tfs->make< TH1D >(Form("integ_opchannel_%d_threshmodeB",PMTIndexingVector[ihist]), Form("'Threshold-Mode' Integral of SPEs, channel %d;Integral value [ADC*samples];Count",PMTIndexingVector[ihist]), 50, 0, 500)); // create histogram for integral (threshold mode, local baseline subtraction)

     integ5.push_back(tfs->make< TH1D >(Form("integ_opchannel_%d_manualmodeB",PMTIndexingVector[ihist]), Form("'Manual-Mode' Integral of SPEs, channel %d;Integral value [ADC*samples];Count",PMTIndexingVector[ihist]), 50, 0, 500)); // create histogram for integral (manual mode, local baseline subtraction)
     
     count++;


   }
    
  

  }

  void PMTGain::beginJob()
  {

  }

  void PMTGain::analyze(art::Event const & e)
  {
    // Implementation of required member function here.
    std::cout << "My module on event #" << e.id().event() << std::endl;

   
    fEvNumber = e.id().event();

    art::Handle< std::vector< raw::OpDetWaveform > > waveHandle;
    e.getByLabel(fInputModuleName, waveHandle);

    if(!waveHandle.isValid() || waveHandle->empty() )  {
      std::cout << Form("Did not find any G4 photons from a producer: %s", "largeant") << std::endl;
      return;
    }

    
  //CONFIG
  //bool do_avgspe=true, do_amp=true, do_integ=true; // which analysis do we want to do?
 
  double pulse_peak;
  double pulse_t_start;
  double pulse_t_end;
  double pulse_t_peak;
  std::vector<double> pulse_t_peak_v;
  int total_nspe = 0;

   std::cout << "Number of waveforms: " << waveHandle->size() << std::endl;

  int success=0; //number of successful waveform analyses
  int failed=0; //number of failed waveform analyses

    
    
    
    
    std::cout << "Number of waveforms: " << waveHandle->size() << std::endl;
  
    std::cout << "fOpDetsToPlot:\t";
    for (auto const& opdet : fOpDetsToPlot){std::cout << opdet << " ";}
    std::cout << std::endl;

    int hist_id = 0;
    int pmt_counter=0;
    std::unordered_map<int, int> fPMTChannelGainMap;
    std::unordered_map<int, bool> channelInserted;
    for(auto const& wvf : (*waveHandle)) {
      auto wvf_ch= wvf.ChannelNumber(); // I am creating a local variable wvf_ch

// (pdMap.isPDType(wvf_ch, "xarapuca_vuv") || pdMap.isPDType(wvf_ch, "xarapuca_vis")) continue; //TEST THIS
//      if (channelInserted.find(wvf_ch) != channelInserted.end()) {
//        continue;  // Skip if already inserted
//    }
//      fPMTChannelGainMap.insert(std::make_pair(wvf_ch, pmt_counter));
//      channelInserted[wvf_ch] = true;
      opdetType = pdMap.pdType(wvf_ch);
      opdetElectronics = pdMap.electronicsType(wvf_ch);
//     
      pmt_counter = GetPMTIndex(wvf_ch); 

     //std::cout << "++++ PMT counter " << pmt_counter << "  vector size " << PMTIndexingVector.size() << std::endl;

      if(pmt_counter == -1) continue;   //if channel not found, then let's not do anything. 

      if (std::find(fOpDetsToPlot.begin(), fOpDetsToPlot.end(), opdetType) == fOpDetsToPlot.end()) {continue;}
      histname.str(std::string());
      histname << "event_" << fEvNumber
               << "_opchannel_" << wvf_ch
               << "_" << opdetType
               << "_" << hist_id;

      fStartTime = wvf.TimeStamp(); //in us
      if (opdetElectronics == "daphne"){
				fEndTime = double(wvf.size()) / fSampling_Daphne + fStartTime;
			} //in us
			else{
        fEndTime = double(wvf.size()) / fSampling + fStartTime;
      } //in us

      //Create a new histogram
      TH1D *wvfHist = tfs->make< TH1D >(histname.str().c_str(), TString::Format(";t - %f (#mus);", fStartTime), wvf.size(), fStartTime, fEndTime);
      for(unsigned int i = 0; i < wvf.size(); i++) {
        wvfHist->SetBinContent(i + 1, (double)wvf[i]);
      }
      hist_id++;
  
      //////////////////////////////////////////////////////////////////////////////

 

  //INITIAL PRINTOUT
  std::cout << "======" << "SPE ANALYSIS" << "======" << std::endl << "Developed by abullock and hollyp for SBND, 2023-2024." << std::endl << "Channel selected: " << wvf_ch << std::endl;
  if (!fAll_events) {std::cout << "Event selected: " << fEventid << std::endl;} else {std::cout << "All events selected." << std::endl;}
  std::cout << "Launching..." << std::endl;



  //GET INFO ON THE WAVEFORM  // need to fix this, Andrzej
//   Double_t wvf_tlo = wvf->GetXaxis()->GetBinLowEdge(1); // get lower bound of t on histogram
//   Double_t wvf_thi = wvf->GetXaxis()->GetBinUpEdge(wvf_nbins); // get upper bound of t on histogram
   Int_t wvf_nbins = wvf.size();
   //Double_t wvf_tl = wvf.TimeStamp(); //in us
   //Double_t wvf_thi = double(wvf.size()) / fSampling + fStartTime; NEEDS SORTING (H)


  //NOISE ANALYSIS
  double thresh; //tspectrum threshold will be determined by noise analysis
//  wvf->Add(wvf, -2); //flip histogram

  Double_t highestval = 999999;    // this will be the highest value, once we flip the polarity of the wvf, but for now, it's going to be minimum. A.
  Int_t highestbin = -1; //which bin has the highest peak? (or lowest for now), A.
  for(unsigned int ix=0;ix<wvf.size();ix++)
  {
      if((double)wvf[ix]<highestval)
      {
       highestval=  (double)wvf[ix];
       highestbin=ix;
      }
  }
  

//// What we should have is a pretrigger parameter, fcl file and that should replace the highestbin alogorithm. Andrzej  
  
// this needs to be set by numbers/.fcl parameters (Andrzej) -- it is now :)  (H)
  Int_t noisebinmin = fNbmin_factor*highestbin; 
  Int_t noisebinmax = fNbmax_factor*highestbin; //set noise analysis range 1 (pre peaks)
  Int_t noisebin2min = fN2bmin_factor*wvf_nbins; //set noise analysis range 2 (post peaks), upper bound is just wvf_nbins
  int m = 0; //number of noise values recorded
  Double_t total = 0; Double_t totalvar = 0; //for calculation
  //total across noise region 1
  for (int i=noisebinmin; i<=noisebinmax; i++) { //iterate across noise region
   Double_t val = (double)wvf[i]; //get value in bin
   total += val; //sum all values
   m++;
  }
  //total across noise region 2
  for (int i=noisebin2min; i<=wvf_nbins; i++) { //iterate across noise region
    Double_t val = (double)wvf[i]; //get value in bin
    total += val; //sum all values
    m++;
  }
  //calculating mean
  Double_t mean = total / m; //this mean describes the baseline and the average value over the noise


 std::vector<double> wvfm(wvf_nbins, 0);
 for (int i=0; i<wvf_nbins; i++) { 
	wvfm.at(i)=mean-(double)wvf[i];
	}

size_t numElements = wvfm.size();

    // Print the number of elements
    std::cout << "Number of elements in wvfm: " << numElements << std::endl;
   
size_t vectorSize = wvfm.size();

    // Print the size of the vector
    std::cout << "Vector size: " << vectorSize << std::endl;
 
  //total variance across noise region 1
  for (int i=noisebinmin; i<=noisebinmax; i++) { //iterate across noise region
   Double_t val = wvfm.at(i); //get value in bin
   totalvar += (val)*(val); //sum together the variances of each bin
  }
  //total variance across noise region 2
  for (int i=noisebin2min-1; i<wvf_nbins; i++) { //iterate across noise region
    Double_t val = wvfm.at(i); //get value in bin
    totalvar += (val)*(val); //sum together the variances of each bin
  }
  //calculating stdev
  Double_t stdev = sqrt(totalvar/m); //stdev of noise
  highestval = wvfm.at(highestbin); //height of the highest peak, from which the threshold will be determined
  // redefining highestval to be positive, Andrzej
  
  thresh = stdev * fNstdev ; //threshold defined as some number of stdevs above mean, as a fraction of the highest peak height
  //cout << "Threshold set to: " << thresh << endl;

// Andrzej: why would you base your threshold on the highest value? 

  //LYNN'S CODE (pasted in from line 490 at https://github.com/SBNSoftware/sbndcode/blob/5bf42ecca1bf9e8ec6477ec7b5390aa25ffab94f/sbndcode/Trigger/PMT/pmtSoftwareTriggerProducer_module.cc#L490 )

   
  bool fire = false; // bool for if pulse has been detected
  int counter = 0; // counts the bin of the waveform
  int peak_count = 0;
  
  double start_threshold = thresh; //maybe put all of these outside the loop? 
  double end_threshold = thresh; 


  pulse_t_peak_v.clear();
  pulse_peak = 0; pulse_t_start = 0; pulse_t_end = 0; pulse_t_peak = 0;
  counter = fSpe_region_start;
  for (unsigned int i=fSpe_region_start; i<wvfm.size(); i++){
    //for (auto const &adc : wvfm){ 
    auto const &adc = wvfm[counter];
//   std:: cout << " adc/thresh " << adc <<  " " << start_threshold << std::endl;

    if ( !fire && ((double)adc) > start_threshold ){ // if its a new pulse 
      fire = true;
      //vic: i move t_start back one, this helps with porch
      pulse_t_start = counter - 1 > 0 ? counter - 1 : counter;    
      //std::cout << counter << " " << (double)adc << std::endl;
     }
  
     else if( fire && ((double)adc) < end_threshold ){ // found end of a pulse
       fire = false;
       //vic: i move t_start forward one, this helps with tail
       pulse_t_end = counter < ((int)wvfm.size())  ? counter : counter - 1;
       //std::cout << "pulse length = " << (pulse_t_end - pulse_t_start) << std::endl;
       if (pulse_t_end - pulse_t_start > 2){
	 //std::cout << counter << " " << (double)adc << std::endl;
	 //std::cout << "pulse length = " << (pulse_t_end - pulse_t_start) << std::endl;
	 pulse_t_peak_v.push_back(pulse_t_peak);
	 peak_count++;
       }
       pulse_peak = 0; pulse_t_start = 0; pulse_t_end = 0; pulse_t_peak = 0;
 
     }   
  
     else if(fire){ // if we're in a pulse 
      // pulse_area += (baseline-(double)adc);
       //if ((thresh-(double)adc) > pulse_peak) { // Found a new maximum:
       if ((double)adc > pulse_peak){
         pulse_peak = ((double)adc);
         pulse_t_peak = counter;
       }
       //std::cout << counter << " " << (double)adc << std::endl;
     }
    //std::cout << counter << " " << (double)adc << std::endl;
    counter++;

     if (peak_count==200) {std::cout << "  Analysis Failure: Threshold setting unsuccessful." << std::endl; failed++; continue;}
   }

  
   if(fire){ // Take care of a pulse that did not finish within the readout window.
     fire = false;
     pulse_t_end = counter - 1;
     pulse_peak = 0; pulse_t_start = 0; pulse_t_end = 0; pulse_t_peak = 0;
   }
 

  //TSPECTRUM USED TO BE HERE

  auto wvf_pt = pulse_t_peak_v;


  //ISOLATE SPE PEAKS FROM ALL THE PEAKS -- potentially edit/remove this block because our threshold window for peak searching is already the SPE range
  int nspe = 0; //how many peaks are within the spe region
  for (int i=0; i<peak_count; i++) { //loop through all peak positions
    nspe++;
  }
  
  /////// why is this translation even done? Andrzej do we need this code?
  
  std::vector<Double_t >wvf_spet;
  wvf_spet.resize(nspe); int j=0; //create array for them
  for (int i=0; i<peak_count; i++) { //loop through all peak positions 
      wvf_spet[j] = wvf_pt[i]; //add SPEs to the array

      j++;
   
  }
  if (nspe==0) {std::cout << "  Analysis Failure: No SPEs found in this waveform." << std::endl; failed++; continue;} //if no SPEs are found
 
 
std::cout<< "avgspe.size: " << avgspe.size() << std::endl; 
std::cout<< "wvfm channel: " << wvf_ch << std::endl;


  //AVERAGE SPE SHAPE HISTOGRAM
if (fDo_avgspe) {
  for (int i=0; i<nspe; i++) { //iterate through the found spe positions
      Int_t peakbin = wvf_spet[i]; // get bin associated with peak time 
   if(peakbin-fLowbin <0 || peakbin+fHibin>wvf_nbins) //aszelc addition—check to make sure the bin numbers do not cause an error
      {continue;}

//selection addition——prevent any peak with another peak 100ns before it from being added
    bool selected = true;
    for (int l=0; l<nspe; l++){
      Double_t separation = wvf_spet[i] - wvf_spet[l];
      if (separation<0.1 && separation>0) {selected=false;}
    }
    if (fCut==false) {selected = true;} //bypasses the selection above
    if (selected) {


        for (int j=1; j<=NBINS; j++) { //loop over range surrounding peak
            Double_t le_bin = (double)wvfm[peakbin-fLowbin+j]; //add the values to the histogram
            avgspe[pmt_counter]->AddBinContent(j,le_bin);
            }
    navspes[pmt_counter]++; //added one!
    } //close if(selected)
  }
  //this will get normalized after the key loop
}

  //AMPLITUDES OF SPES
if (fDo_amp) {
  for (int i=0; i<nspe; i++) { //iterate through the found spe positions
    Int_t peakbin = wvf_spet[i]; //get bin associated with peak time
    Double_t peakheight = wvfm.at(peakbin);
    amp[pmt_counter]->Fill(peakheight); //add to histogram
    if (!fDo_avgspe) {navspes[pmt_counter]++;} //count total spes here if we aren't already
  }
}


//THIS IS WHERE IT FAILS
  //INTEGRALS OF SPES
if (fDo_integ) {
  //without baseline subtraction
  for (int i=0; i<nspe; i++) { //iterate through the found spe positions
    Int_t peakbin = wvf_spet[i]; //get bin associated with peak time
    if(peakbin-fLowbin <0 || peakbin+fHibin>wvf_nbins) //aszelc addition—check to make sure the bin numbers do not cause an error
      continue;
    std::cout << wvf_nbins << " wvf_nbins" << std::endl;
    std::cout << wvfm.size() << " size of wvfm" << std::endl;
    //zeromode bounds
    int ilo=0, ihi=0; //set bounds initially
    Double_t val = wvfm.at(peakbin); //edited for baseline subtraction
    while (val>(10)) { //check whether a bin is below noise threshold (indicating a bound on the peak); previously 0 (H)
      ilo++;
      val = wvfm.at(peakbin-ilo); //go one bin left
     // std::cout << "In ilo loop - peakbinIndex: " << peakbin-ilo << ", ilo: " << ilo << ", val: " << val << std::endl;
    }
    val = wvfm.at(peakbin); //reset for other bound
    while (val>(10)) { //repeat the process for the other bound
      ihi++;
      val = wvfm.at(peakbin+ihi); //go one bin right   
      //std::cout << "In ihi loop - peakbinIndex: " << peakbin+ihi << ", ihi: " << ihi << ", val: " << val << std::endl; 
      }
    //std::cout << "TEST AFTER LOOP 1" << std::endl; //THIS DOES NOT PRINT: IT FAILS BEFORE HERE (H)
    ilo--; ihi--; //the bounds search stops at one greater than the true bounds
    Double_t integral = 0;
    for (int j=ilo*-1; j<=ihi; j++) { //loop over range surrounding peak
      //std::cout << peakbin+j << " = PEAKBIN+J" << std::endl;
      Double_t le_bin = wvfm.at(peakbin+j); //add the values to the histogram
      integral += le_bin;
    }
    integ0[pmt_counter]->Fill(integral); //add integral
    
    //threshmode bounds
    ilo=0, ihi=0; //set bounds initially
    val = wvfm.at(peakbin);
    while (val>(stdev*fNstdev)) { //check whether a bin is below noise threshold (indicating a bound on the peak)
      ilo++;       
      val = wvfm.at(peakbin-ilo); //go one bin left
    }
    val = wvfm.at(peakbin); //reset for other bound
    while (val>(stdev*fNstdev)) { //repeat the process for the other bound
      ihi++;
      val = wvfm.at(peakbin+ihi); //go one bin right
    }
    ilo--; ihi--; //the bounds search stops at one greater than the true bounds
    integral = 0;
    for (int j=ilo*-1; j<=ihi; j++) { //loop over range surrounding peak
      Double_t le_bin = wvfm.at(peakbin+j); //add the values to the histogram
      integral += le_bin;
    }
    integ1[pmt_counter]->Fill(integral); //add integral 
   
   
    //manualmode bounds
    ilo=fManual_bound_lo, ihi=fManual_bound_hi; //set bounds manually
    integral = 0;
    for (int j=ilo*-1; j<=ihi; j++) { //loop over range surrounding peak
      Double_t le_bin = wvfm.at(peakbin+j); //add the values to the histogram
      integral += le_bin;
    }
    integ2[pmt_counter]->Fill(integral); //add integral
    if (!fDo_avgspe && !fDo_amp) {navspes[pmt_counter]++;} //count total spes here if we aren't already
  }

  //with baseline subtraction
  for (int i=0; i<nspe; i++) { //iterate through the found spe positions
    Int_t peakbin = wvf_spet[i]; //get bin associated with peak time
    if(peakbin-fLowbin <0 || peakbin+fHibin>wvf_nbins) //aszelc addition—check to make sure the bin numbers do not cause an error
      continue;
    //local baseline calculation
    Double_t vallow = wvfm.at(peakbin-50);
    Double_t valhi = wvfm.at(peakbin+50);
    Double_t bsl = (vallow + valhi) / 2; //average the two
    //zeromode bounds
    int ilo=0, ihi=0; //set bounds initially
    Double_t val = wvfm.at(peakbin) - bsl;
    while (val>(0)) { //check whether a bin is below noise threshold (indicating a bound on the peak)
      ilo++;
      val = wvfm.at(peakbin-ilo) - bsl; //go one bin left
      if (ilo==50) {break;} //keep it from going too wide
    }
    val = wvfm.at(peakbin) - bsl; //reset for other bound
    while (val>(0)) { //repeat the process for the other bound
      ihi++;
      val = wvfm.at(peakbin+ihi) - bsl; //go one bin right
      if (ihi==50) {break;} //keep it from going too wide
    }
    ilo--; ihi--; //the bounds search stops at one greater than the true bounds
    Double_t integral = 0;
    for (int j=ilo*-1; j<=ihi; j++) { //loop over range surrounding peak
      Double_t le_bin = wvfm.at(peakbin+j) - bsl; //add the values to the histogram
      integral += le_bin;
    }
    integ3[pmt_counter]->Fill(integral); //add integral
    //threshmode bounds
    ilo=0, ihi=0; //set bounds initially
    val = wvfm.at(peakbin) - bsl;
    while (val>(stdev*fNstdev)) { //check whether a bin is below noise threshold (indicating a bound on the peak)
      ilo++;
      val = wvfm.at(peakbin-ilo) - bsl; //go one bin left
      if (ilo==50) {break;} //keep it from going too wide
    }
    val = wvfm.at(peakbin) - bsl; //reset for other bound
    while (val>(stdev*fNstdev)) { //repeat the process for the other bound
      ihi++;
      val = wvfm.at(peakbin+ihi) - bsl; //go one bin right
      if (ihi==50) {break;} //keep it from going too wide
    }
    ilo--; ihi--; //the bounds search stops at one greater than the true bounds
    integral = 0;
    for (int j=ilo*-1; j<=ihi; j++) { //loop over range surrounding peak
      Double_t le_bin = wvfm.at(peakbin+j) - bsl; //add the values to the histogram
      integral += le_bin;
    }
    integ4[pmt_counter]->Fill(integral); //add integral 
    //manualmode bounds
    ilo=fManual_bound_lo, ihi=fManual_bound_hi; //set bounds manually
    integral = 0;
    for (int j=ilo*-1; j<=ihi; j++) { //loop over range surrounding peak
      Double_t le_bin = wvfm.at(peakbin+j) - bsl; //add the values to the histogram
      integral += le_bin;
    }
    integ5[pmt_counter]->Fill(integral); //add integral
  }
}
 
  success++;

 std::cout << "  Analysis successful. " << nspe << " SPEs found." << std::endl;
  total_nspe += nspe; 
} //end waveform loop




 
//FINAL PRINTOUT
std::cout << "======" << std::endl << "Analyses complete." << std::endl  << " SPEs analyzed from " << success << " waveforms. Analysis failed on " << failed << " waveforms." << std::endl; 

std::cout << "Total SPEs found: " << total_nspe << std::endl;
//}
      
      
} //end analyze function
        
      //////////////////////////////////////////////////////////////////////////////
      
   
  

  int  PMTGain::GetPMTIndex(int wvf_ch)
   {


        for(unsigned int ix=0;ix<PMTIndexingVector.size();ix++)
	   { if(PMTIndexingVector[ix]==wvf_ch)
		return ix;
           }
        return -1;

   }
  
  
  
  
  
  
  
  void PMTGain::endJob()
  {

     //NORMALIZE AVGSPE
      std::cout << "Normalising average SPEs..." << std::endl;

	for( unsigned int ihist=0;ihist<PMTIndexingVector.size();ihist++){  //first loop on all relevant histograms
	      for( int ix=1;ix<=avgspe[ihist]->GetSize();ix++)
		{
		     Double_t le_bin=avgspe[ihist]->GetBinContent(ix); //get a bin
		     avgspe[ihist]->SetBinContent(ix, le_bin/navspes[ihist]);
                     //Double_t norm = -1 * le_bin * (navspes[ihist]-1) / navspes[ihist]; //amount to subtract to reduce le_bin to le_bin/navspes: CHECK THIS IS CORRECT-- maybe do 1 not -1
		     //avgspe[ihist]->AddBinContent(ix, norm); //add it   // Andrzej: I'm not sure I understand what's going on here? 
		} // end loop on smples
	}   // end loop on histograms.
  }

  DEFINE_ART_MODULE(PDSCali::PMTGain)

}
