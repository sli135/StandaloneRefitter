/*
Refit-signals was originally written as an EXOAnalysis module; however, it quickly became clear that
the code could work much more efficiently if we collected many events together to handle in parallel.
The bottleneck to the code is multiplying vectors by the noise blocks of A

Note that this module DOES rearrange the ordering of events.  If you're not referring to events by
run/event number, well, you should.  (We index our trees by run/event number, for your convenience.)

The usability is less than EXOAnalysis, with fewer options settable on the command line.
It's possible I'll address this, but no promises.

Should be called like:
./Refit <InputProcessedFile> <InputWaveformFile> <OutputFile>
*/


#include "EXORefitSignals.hh"
#include "EXOUtilities/EXOEventData.hh"
#include "EXOCalibUtilities/EXOCalibManager.hh"
#include "EXOAnalysisManager/EXOTreeInputModule.hh"
#include "EXOAnalysisManager/EXOTreeOutputModule.hh"
#include "TFile.h"
#include "TTree.h"
#include <iostream>
#include <cstdlib>
#include <fstream>

int main(int argc, char** argv)
{
  std::cout<<"Entered program."<<std::endl;
  std::string ProcessedFileName;
  std::string RawFileName;
  std::string OutFileName;
  Long64_t StartEntry = 0;
  Long64_t NumEntries = 100;
  double Threshold = 10;

  if(argc == 2) {
    std::cout<<"Reading arguments from file \""<<argv[1]<<"\"."<<std::endl;
    std::ifstream OptionFile(argv[1]);
    OptionFile >> ProcessedFileName
               >> RawFileName
               >> OutFileName
               >> StartEntry
               >> NumEntries
               >> Threshold;
  }
  else {
    assert(argc >= 4);
    ProcessedFileName = argv[1];
    RawFileName = argv[2];
    OutFileName = argv[3];
    if(argc >= 5) StartEntry = std::atol(argv[4]);
    if(argc >= 6) NumEntries = std::atol(argv[5]);
    if(argc >= 7) Threshold = std::atof(argv[6]);
  }

  std::cout<<"Input processed file: "<<ProcessedFileName<<std::endl;
  std::cout<<"Input raw file: "<<RawFileName<<std::endl;
  std::cout<<"Output file: "<<OutFileName<<std::endl;
  std::cout<<"Starting at entry "<<StartEntry<<std::endl;
  std::cout<<"Handle "<<NumEntries<<" entries."<<std::endl;

  EXOTreeInputModule InputModule;
  std::cout<<"About to set filename."<<std::endl;
  InputModule.SetFilename(ProcessedFileName);
  std::cout<<"Successfully set filename."<<std::endl;
  TFile WaveformFile(RawFileName.c_str());
  TTree* WaveformTree = dynamic_cast<TTree*>(WaveformFile.Get("tree"));

  EXOTreeOutputModule OutputModule;
  OutputModule.SetOutputFilename(OutFileName);
  OutputModule.Initialize();
  OutputModule.BeginOfRun(NULL); // OK, fine -- shortcut here, I assume input has only one run.

  EXORefitSignals RefitSig(InputModule, *WaveformTree, OutputModule);
  EXOCalibManager::GetCalibManager().SetMetadataAccessType("text");
#if defined HOPPER
  RefitSig.SetNoiseFilename("/scratch2/scratchdirs/claytond/noise_manyruns_withuwires_100000.dat");
#elif defined EDISON
  RefitSig.SetNoiseFilename("/scratch1/scratchdirs/claytond/noise_manyruns_withuwires_100000.dat");
#elif defined SLAC
  RefitSig.SetNoiseFilename("/nfs/slac/g/exo_data4/users/cgd8d/rhel5-64/noise_manyruns_withuwires_100000.dat");
#endif
  RefitSig.SetRThreshold(Threshold);
  RefitSig.fVerbose = true;
  RefitSig.Initialize();

#ifdef USE_THREADS
  std::cout<<"Using "<<NUM_THREADS<<" threads."<<std::endl;
#else
  std::cout<<"Sequential code."<<std::endl;
#endif
#ifdef USE_LOCKFREE
  std::cout<<"Using the boost::lockfree library."<<std::endl;
#endif

  for(Long64_t entryNum = StartEntry; entryNum < StartEntry + NumEntries; entryNum++) {
    if(entryNum % 10 == 0) std::cout << "Grabbing entry " << entryNum << std::endl;
    EXOEventData* ED = InputModule.GetEvent(entryNum);
    if(ED == NULL) break;
    RefitSig.AcceptEvent(ED, entryNum);
  }

  RefitSig.FlushEvents();
  OutputModule.ShutDown();
}
