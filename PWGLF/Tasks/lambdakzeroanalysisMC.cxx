// Copyright 2019-2020 CERN and copyright holders of ALICE O2.
// See https://alice-o2.web.cern.ch/copyright for details of the copyright holders.
// All rights not expressly granted are reserved.
//
// This software is distributed under the terms of the GNU General Public
// License v3 (GPL Version 3), copied verbatim in the file "COPYING".
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.
//
// Adaptation of V0 analysis task for run on MC data
// ========================
//
// This code loops over a V0Data table and produces some
// standard analysis output. It requires either
// the lambdakzerofinder or the lambdakzerobuilder tasks
// to have been executed in the workflow (before).
//
//    Comments, questions, complaints, suggestions?
//    Please write to:
//    aimeric.landou@cern.ch (MC adaptation)
//    david.dobrigkeit.chinellato@cern.ch (original lambdakzeroanalysis task)

#include "Framework/runDataProcessing.h"
#include "Framework/AnalysisTask.h"
#include "Framework/AnalysisDataModel.h"
#include "Framework/ASoAHelpers.h"
#include "ReconstructionDataFormats/Track.h"
#include "Common/Core/RecoDecay.h"
#include "Common/Core/trackUtilities.h"
#include "Common/DataModel/StrangenessTables.h"
#include "Common/Core/TrackSelection.h"
#include "Common/DataModel/TrackSelectionTables.h"
#include "Common/DataModel/EventSelection.h"
#include "Common/DataModel/Centrality.h"
#include "PID/PIDResponse.h"

#include <TFile.h>
#include <TH2F.h>
#include <TProfile.h>
#include <TLorentzVector.h>
#include <Math/Vector4D.h>
#include <TPDGCode.h>
#include <TDatabasePDG.h>
#include <cmath>
#include <array>
#include <cstdlib>
#include "Framework/ASoAHelpers.h"

using namespace o2;
using namespace o2::framework;
using namespace o2::framework::expressions;
using std::array;

using MyTracks = soa::Join<aod::Tracks, aod::McTrackLabels, aod::TracksExtra, aod::pidTPCPr>;

struct lambdakzeroQa {
  //Basic checks
  HistogramRegistry registry{
    "registry",
    {
      {"hMassK0ShortMCportion", "hMassK0ShortMCportion", {HistType::kTH1F, {{800, 0.0f, 3.0f, "Inv. Mass (GeV/c^{2})"}}}},
      {"hPtDiffK0ShortMCportion", "hPtDiffK0ShortMCportion", {HistType::kTH1F, {{200, 0.0f, 10.0f, "#it{p}_{T} (GeV/c)"}}}},

      {"hV0Radius", "hV0Radius", {HistType::kTH1F, {{1000, 0.0f, 100.0f, "cm"}}}},
      {"hV0CosPA", "hV0CosPA", {HistType::kTH1F, {{1000, 0.95f, 1.0f}}}},
      {"hDCAPosToPV", "hDCAPosToPV", {HistType::kTH1F, {{1000, -10.0f, 10.0f, "cm"}}}},
      {"hDCANegToPV", "hDCANegToPV", {HistType::kTH1F, {{1000, -10.0f, 10.0f, "cm"}}}},
      {"hDCAV0Dau", "hDCAV0Dau", {HistType::kTH1F, {{1000, 0.0f, 10.0f, "cm^{2}"}}}},
    },
  };

  void init(InitContext const&)
  {
    AxisSpec massAxisK0Short = {600, 0.0f, 3.0f, "Inv. Mass (GeV/c^{2})"};
    AxisSpec massAxisLambda = {600, 0.0f, 3.0f, "Inv. Mass (GeV/c^{2})"};

    registry.add("hMassK0Short", "hMassK0Short", {HistType::kTH1F, {massAxisK0Short}});
    registry.add("hMassLambda", "hMassLambda", {HistType::kTH1F, {massAxisLambda}});
    registry.add("hMassAntiLambda", "hMassAntiLambda", {HistType::kTH1F, {massAxisLambda}});
  }

  void process(aod::Collision const& collision, aod::V0Datas const& fullV0s, aod::McParticles const& mcParticles, MyTracks const& tracks)
  {
    for (auto& v0 : fullV0s) {
      registry.fill(HIST("hMassK0Short"), v0.mK0Short());
      registry.fill(HIST("hMassLambda"), v0.mLambda());
      registry.fill(HIST("hMassAntiLambda"), v0.mAntiLambda());

      registry.fill(HIST("hV0Radius"), v0.v0radius());
      registry.fill(HIST("hV0CosPA"), v0.v0cosPA(collision.posX(), collision.posY(), collision.posZ()));
      registry.fill(HIST("hDCAPosToPV"), v0.dcapostopv());
      registry.fill(HIST("hDCANegToPV"), v0.dcanegtopv());
      registry.fill(HIST("hDCAV0Dau"), v0.dcaV0daughters());

      auto reconegtrack = v0.negTrack_as<MyTracks>();
      auto recopostrack = v0.posTrack_as<MyTracks>();
      if (!reconegtrack.has_mcParticle() || !recopostrack.has_mcParticle()) {
        continue;
      }
      auto mcnegtrack = reconegtrack.mcParticle_as<aod::McParticles>();
      auto mcpostrack = recopostrack.mcParticle_as<aod::McParticles>();
      if (!mcnegtrack.has_mothers() || !mcpostrack.has_mothers()) {
        continue;
      }
      for (auto& particleMotherOfNeg : mcnegtrack.mothers_as<aod::McParticles>()) {
        for (auto& particleMotherOfPos : mcpostrack.mothers_as<aod::McParticles>()) {
          // LOGF(info, "MotherNeg = %d, MotherPos = %d", particleMotherOfNeg.pdgCode(), particleMotherOfPos.pdgCode());
          if (particleMotherOfNeg.isPhysicalPrimary() && particleMotherOfNeg == particleMotherOfPos && particleMotherOfNeg.pdgCode() == 310) {
            registry.fill(HIST("hMassK0ShortMCportion"), v0.mK0Short());
            registry.fill(HIST("hPtDiffK0ShortMCportion"), particleMotherOfNeg.pt());
          }
        }
      }
    }
  }
};

struct lambdakzeroAnalysisMc {

  HistogramRegistry registry{
    "registry",
    {
      {"h3dMassK0Short", "h3dMassK0Short", {HistType::kTH3F, {{20, 0.0f, 100.0f, "Cent (%)"}, {200, 0.0f, 10.0f, "#it{p}_{T} (GeV/c)"}, {200, 0.450f, 0.550f, "Inv. Mass (GeV/c^{2})"}}}},
      {"h3dMassLambda", "h3dMassLambda", {HistType::kTH3F, {{20, 0.0f, 100.0f, "Cent (%)"}, {200, 0.0f, 10.0f, "#it{p}_{T} (GeV/c)"}, {200, 1.015f, 1.215f, "Inv. Mass (GeV/c^{2})"}}}},
      {"h3dMassAntiLambda", "h3dMassAntiLambda", {HistType::kTH3F, {{20, 0.0f, 100.0f, "Cent (%)"}, {200, 0.0f, 10.0f, "#it{p}_{T} (GeV/c)"}, {200, 1.015f, 1.215f, "Inv. Mass (GeV/c^{2})"}}}},
      {"h3dMassK0Short_MC_truePt", "h3dMassK0Short_MC_truePt", {HistType::kTH3F, {{20, 0.0f, 100.0f, "Cent (%)"}, {200, 0.0f, 10.0f, "#it{p}_{T} (GeV/c)"}, {200, 0.450f, 0.550f, "Inv. Mass (GeV/c^{2})"}}}},
      {"h3dMassLambda_MC_truePt", "h3dMassLambda_MC_truePt", {HistType::kTH3F, {{20, 0.0f, 100.0f, "Cent (%)"}, {200, 0.0f, 10.0f, "#it{p}_{T} (GeV/c)"}, {200, 1.015f, 1.215f, "Inv. Mass (GeV/c^{2})"}}}},
      {"h3dMassAntiLambda_MC_truePt", "h3dMassAntiLambda_MC_truePt", {HistType::kTH3F, {{20, 0.0f, 100.0f, "Cent (%)"}, {200, 0.0f, 10.0f, "#it{p}_{T} (GeV/c)"}, {200, 1.015f, 1.215f, "Inv. Mass (GeV/c^{2})"}}}},
      {"MCmomID_Lambda", "MCmomID_Lambda", {HistType::kTH1I, {{4000000, 0, 4000000}}}},
      {"MCmomID_AntiLambda", "MCmomID_AntiLambda", {HistType::kTH1I, {{4000000, 0, 4000000}}}},
      {"MCmomID_K0Short", "MCmomID_K0Short", {HistType::kTH1I, {{4000000, 0, 4000000}}}},
      {"V0loopFiltersCounts", "V0loopFiltersCounts", {HistType::kTH1F, {{11, 0.0f, 11.0f}}}},

      {"hLambdaFeedDownMatrix", "hLambdaFeedDownMatrix", {HistType::kTH2F, {{200, 0.0f, 10.0f, "#it{p}_{T}^{#Lambda} (GeV/c)"}, {200, 0.0f, 10.0f, "#it{p}_{T}^{#Omega-} (GeV/c)"}}}},
      {"hAntiLambdaFeedDownMatrix", "hAntiLambdaFeedDownMatrix", {HistType::kTH2F, {{200, 0.0f, 10.0f, "#it{p}_{T}^{#bar{#Lambda}} (GeV/c)"}, {200, 0.0f, 10.0f, "#it{p}_{T}^{#Omega+} (GeV/c)"}}}},

      {"hSelectedEventCounter", "hSelectedEventCounter", {HistType::kTH1F, {{1, 0.0f, 1.0f}}}},
    },
  };
  //_MC_truePt histograms: additional .pdgcode cut, and fill true Pt instead of reconstructed Pt

  ConfigurableAxis dcaBinning{"dca-binning", {200, 0.0f, 1.0f}, ""};
  ConfigurableAxis ptBinning{"pt-binning", {200, 0.0f, 10.0f}, ""};
  ConfigurableAxis massK0Shortbinning{"K0S-mass-binning", {200, 0.450f, 0.550f}, ""};
  ConfigurableAxis massLambdabinning{"Lambda-mass-binning", {200, 1.015f, 1.215f}, ""};

  void init(InitContext const&)
  {
    AxisSpec dcaAxis = {dcaBinning, "DCA (cm)"};
    AxisSpec ptAxis = {ptBinning, "#it{p}_{T} (GeV/c)"};
    AxisSpec massAxisK0Short = {massK0Shortbinning, "Inv. Mass (GeV/c^{2})"};
    AxisSpec massAxisLambda = {massLambdabinning, "Inv. Mass (GeV/c^{2})"};

    registry.add("h3dMassK0ShortDca", "h3dMassK0ShortDca", {HistType::kTH3F, {dcaAxis, ptAxis, massAxisK0Short}});
    registry.add("h3dMassLambdaDca", "h3dMassLambdaDca", {HistType::kTH3F, {dcaAxis, ptAxis, massAxisLambda}});
    registry.add("h3dMassAntiLambdaDca", "h3dMassAntiLambdaDca", {HistType::kTH3F, {dcaAxis, ptAxis, massAxisLambda}});
    registry.add("h3dMassK0ShortDca_MC_truePt", "h3dMassK0ShortDca_MC_truePt", {HistType::kTH3F, {dcaAxis, ptAxis, massAxisK0Short}});
    registry.add("h3dMassLambdaDca_MC_truePt", "h3dMassLambdaDca_MC_truePt", {HistType::kTH3F, {dcaAxis, ptAxis, massAxisLambda}});
    registry.add("h3dMassAntiLambdaDca_MC_truePt", "h3dMassAntiLambdaDca_MC_truePt", {HistType::kTH3F, {dcaAxis, ptAxis, massAxisLambda}});

    registry.get<TH1>(HIST("V0loopFiltersCounts"))->GetXaxis()->SetBinLabel(1, "V0 Candidates");
    registry.get<TH1>(HIST("V0loopFiltersCounts"))->GetXaxis()->SetBinLabel(2, "V0Radius and CosPA");
    registry.get<TH1>(HIST("V0loopFiltersCounts"))->GetXaxis()->SetBinLabel(4, "Lambda Rapidity");
    registry.get<TH1>(HIST("V0loopFiltersCounts"))->GetXaxis()->SetBinLabel(5, "Lambda lifetime cut");
    registry.get<TH1>(HIST("V0loopFiltersCounts"))->GetXaxis()->SetBinLabel(6, "Lambda TPC PID cut");
    registry.get<TH1>(HIST("V0loopFiltersCounts"))->GetXaxis()->SetBinLabel(8, "K0S Rapidity");
    registry.get<TH1>(HIST("V0loopFiltersCounts"))->GetXaxis()->SetBinLabel(9, "K0S lifetime cut");
    registry.get<TH1>(HIST("V0loopFiltersCounts"))->GetXaxis()->SetBinLabel(10, "K0S Armenteros cut");
  }

  //Selection criteria
  Configurable<double> v0cospa{"v0cospa", 0.995, "V0 CosPA"}; //double -> N.B. dcos(x)/dx = 0 at x=0)
  Configurable<float> dcav0dau{"dcav0dau", 1.0, "DCA V0 Daughters"};
  Configurable<float> dcanegtopv{"dcanegtopv", .1, "DCA Neg To PV"};
  Configurable<float> dcapostopv{"dcapostopv", .1, "DCA Pos To PV"};
  Configurable<float> v0radius{"v0radius", 5.0, "v0radius"};
  Configurable<float> rapidity{"rapidity", 0.5, "rapidity"};
  Configurable<int> saveDcaHist{"saveDcaHist", 0, "saveDcaHist"};
  Configurable<float> TpcPidNsigmaCut{"TpcPidNsigmaCut", 5, "TpcPidNsigmaCut"};
  Configurable<bool> boolArmenterosCut{"boolArmenterosCut", true, "cut on Armenteros-Podolanski graph"};
  Configurable<float> paramArmenterosCut{"paramArmenterosCut", 0.2, "parameter Armenteros Cut"};
  Configurable<bool> eventSelection{"eventSelection", true, "event selection"};

  static constexpr float defaultLifetimeCuts[1][2] = {{25., 20.}};
  Configurable<LabeledArray<float>> lifetimecut{"lifetimecut", {defaultLifetimeCuts[0], 2, {"lifetimecutLambda", "lifetimecutK0S"}}, "lifetimecut"};

  Filter preFilterV0 = nabs(aod::v0data::dcapostopv) > dcapostopv&& nabs(aod::v0data::dcanegtopv) > dcanegtopv&& aod::v0data::dcaV0daughters < dcav0dau;

  void processRun3(soa::Join<aod::Collisions, aod::EvSels>::iterator const& collision, soa::Filtered<aod::V0Datas> const& fullV0s, aod::McParticles const& mcParticles, MyTracks const& tracks)
  // void process(soa::Join<aod::Collisions, aod::EvSels, aod::CentV0Ms>::iterator const& collision, soa::Filtered<aod::V0Datas> const& fullV0s, aod::McParticles const& mcParticles, MyTracks const& tracks)
  {
    if (eventSelection && !collision.sel8()) {
      return;
    }
    registry.fill(HIST("hSelectedEventCounter"), 0.5);

    for (auto& v0 : fullV0s) {
      //   FIXME: could not find out how to filter cosPA and radius variables (dynamic columns)
      registry.fill(HIST("V0loopFiltersCounts"), 0.5);
      if (v0.v0radius() > v0radius && v0.v0cosPA(collision.posX(), collision.posY(), collision.posZ()) > v0cospa) {
        registry.fill(HIST("V0loopFiltersCounts"), 1.5);

        auto reconegtrack = v0.negTrack_as<MyTracks>();
        auto recopostrack = v0.posTrack_as<MyTracks>();
        if (!reconegtrack.has_mcParticle() || !recopostrack.has_mcParticle()) {
          continue;
        }
        auto mcnegtrack = reconegtrack.mcParticle_as<aod::McParticles>();
        auto mcpostrack = recopostrack.mcParticle_as<aod::McParticles>();

        if (TMath::Abs(v0.yLambda()) < rapidity) {
          registry.fill(HIST("V0loopFiltersCounts"), 3.5);
          if (v0.distovertotmom(collision.posX(), collision.posY(), collision.posZ()) * RecoDecay::getMassPDG(kLambda0) < lifetimecut->get("lifetimecutLambda")) {
            registry.fill(HIST("V0loopFiltersCounts"), 4.5);
            if (TMath::Abs(v0.posTrack_as<MyTracks>().tpcNSigmaPr()) < TpcPidNsigmaCut) { //previous 900Gev pp analysis had nSigma< 5 for pt<0.7Gev and tpcNSigmaStorePi<3 for pt>0.7GeV; and no cut on K0S
              registry.fill(HIST("V0loopFiltersCounts"), 5.5);
              // registry.fill(HIST("h3dMassLambda"), collision.centV0M(), v0.pt(), v0.mLambda());
              registry.fill(HIST("h3dMassLambda"), 0., v0.pt(), v0.mLambda());
              registry.fill(HIST("h3dMassAntiLambda"), 0., v0.pt(), v0.mAntiLambda());

              for (auto& particleMotherOfNeg : mcnegtrack.mothers_as<aod::McParticles>()) {
                for (auto& particleMotherOfPos : mcpostrack.mothers_as<aod::McParticles>()) {
                  if (particleMotherOfNeg == particleMotherOfPos && particleMotherOfNeg.pdgCode() == 3122) {
                    if (particleMotherOfNeg.has_mothers()) {
                      auto particleGrandMothersOfNegTable = particleMotherOfNeg.mothers_as<aod::McParticles>();
                      auto particleGrandMotherOfNeg = particleGrandMothersOfNegTable[0];
                      if (particleGrandMotherOfNeg.pdgCode() == 3312) {
                        registry.fill(HIST("hLambdaFeedDownMatrix"), particleMotherOfNeg.pt(), particleGrandMotherOfNeg.pt());
                      }
                    }
                  }
                  if (particleMotherOfNeg == particleMotherOfPos && particleMotherOfNeg.pdgCode() == -3122) {
                    if (particleMotherOfNeg.isPhysicalPrimary()) {
                      registry.fill(HIST("h3dMassAntiLambda_MC_truePt"), 0., particleMotherOfNeg.pt(), v0.mAntiLambda());
                    }
                    if (particleMotherOfNeg.has_mothers()) {
                      auto particleGrandMothersOfNegTable = particleMotherOfNeg.mothers_as<aod::McParticles>();
                      auto particleGrandMotherOfNeg = particleGrandMothersOfNegTable[0];
                      if (particleGrandMotherOfNeg.pdgCode() == -3312) {
                        registry.fill(HIST("hAntiLambdaFeedDownMatrix"), particleMotherOfNeg.pt(), particleGrandMotherOfNeg.pt());
                      }
                    }
                  }
                  if (saveDcaHist == 1) {
                    registry.fill(HIST("h3dMassLambdaDca"), v0.dcaV0daughters(), v0.pt(), v0.mLambda());
                    registry.fill(HIST("h3dMassAntiLambdaDca"), v0.dcaV0daughters(), v0.pt(), v0.mAntiLambda());

                    if (particleMotherOfNeg.isPhysicalPrimary() && particleMotherOfNeg == particleMotherOfPos && particleMotherOfNeg.pdgCode() == 3122) {
                      registry.fill(HIST("h3dMassLambdaDca_MC_truePt"), v0.dcaV0daughters(), particleMotherOfNeg.pt(), v0.mLambda());
                    }
                    if (particleMotherOfNeg.isPhysicalPrimary() && particleMotherOfNeg == particleMotherOfPos && particleMotherOfNeg.pdgCode() == -3122) {
                      registry.fill(HIST("h3dMassAntiLambdaDca_MC_truePt"), v0.dcaV0daughters(), particleMotherOfNeg.pt(), v0.mAntiLambda());
                    }
                  }
                }
              }
            }
          }
        }
        if (TMath::Abs(v0.yK0Short()) < rapidity) {
          registry.fill(HIST("V0loopFiltersCounts"), 7.5);
          if (v0.distovertotmom(collision.posX(), collision.posY(), collision.posZ()) * RecoDecay::getMassPDG(kK0Short) < lifetimecut->get("lifetimecutK0S")) {
            registry.fill(HIST("V0loopFiltersCounts"), 8.5);
            if ((v0.qtarm() > paramArmenterosCut * v0.alpha()) || !boolArmenterosCut) {
              registry.fill(HIST("V0loopFiltersCounts"), 9.5);
              registry.fill(HIST("h3dMassK0Short"), 0., v0.pt(), v0.mK0Short());

              for (auto& particleMotherOfNeg : mcnegtrack.mothers_as<aod::McParticles>()) {
                for (auto& particleMotherOfPos : mcpostrack.mothers_as<aod::McParticles>()) {
                  if (particleMotherOfNeg.isPhysicalPrimary() && particleMotherOfNeg == particleMotherOfPos && particleMotherOfNeg.pdgCode() == 310) {
                    registry.fill(HIST("h3dMassK0Short_MC_truePt"), 0., particleMotherOfNeg.pt(), v0.mK0Short());
                  }
                  if (saveDcaHist == 1) {
                    registry.fill(HIST("h3dMassK0ShortDca"), v0.dcaV0daughters(), v0.pt(), v0.mK0Short());
                    if (particleMotherOfNeg.isPhysicalPrimary() && particleMotherOfNeg == particleMotherOfPos && particleMotherOfNeg.pdgCode() == 310) {
                      registry.fill(HIST("h3dMassK0ShortDca_MC_truePt"), v0.dcaV0daughters(), particleMotherOfNeg.pt(), v0.mK0Short());
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
  PROCESS_SWITCH(lambdakzeroAnalysisMc, processRun3, "Process Run 3 data", true);

  void processRun2(soa::Join<aod::Collisions, aod::EvSels, aod::CentV0Ms>::iterator const& collision, soa::Filtered<aod::V0Datas> const& fullV0s, aod::McParticles const& mcParticles, MyTracks const& tracks)
  {
    if (!collision.alias()[kINT7]) {
      return;
    }
    if (eventSelection && !collision.sel7()) {
      return;
    }
    registry.fill(HIST("hSelectedEventCounter"), 0.5);

    for (auto& v0 : fullV0s) {
      //   FIXME: could not find out how to filter cosPA and radius variables (dynamic columns)
      registry.fill(HIST("V0loopFiltersCounts"), 0.5);
      if (v0.v0radius() > v0radius && v0.v0cosPA(collision.posX(), collision.posY(), collision.posZ()) > v0cospa) {
        registry.fill(HIST("V0loopFiltersCounts"), 1.5);

        auto reconegtrack = v0.negTrack_as<MyTracks>();
        auto recopostrack = v0.posTrack_as<MyTracks>();

        auto mcnegtrack = reconegtrack.mcParticle_as<aod::McParticles>();
        auto mcpostrack = recopostrack.mcParticle_as<aod::McParticles>();

        if (TMath::Abs(v0.yLambda()) < rapidity) {
          registry.fill(HIST("V0loopFiltersCounts"), 3.5);
          if (v0.distovertotmom(collision.posX(), collision.posY(), collision.posZ()) * RecoDecay::getMassPDG(kLambda0) < lifetimecut->get("lifetimecutLambda")) {
            registry.fill(HIST("V0loopFiltersCounts"), 4.5);
            registry.fill(HIST("h3dMassLambda"), collision.centV0M(), v0.pt(), v0.mLambda());
            registry.fill(HIST("h3dMassAntiLambda"), collision.centV0M(), v0.pt(), v0.mAntiLambda());

            for (auto& particleMotherOfNeg : mcnegtrack.mothers_as<aod::McParticles>()) {
              for (auto& particleMotherOfPos : mcpostrack.mothers_as<aod::McParticles>()) {
                if (particleMotherOfNeg.isPhysicalPrimary() && particleMotherOfNeg == particleMotherOfPos && particleMotherOfNeg.pdgCode() == 3122) {
                  registry.fill(HIST("h3dMassLambda_MC_truePt"), collision.centV0M(), particleMotherOfNeg.pt(), v0.mLambda());
                }
                if (particleMotherOfNeg.isPhysicalPrimary() && particleMotherOfNeg == particleMotherOfPos && particleMotherOfNeg.pdgCode() == -3122) {
                  registry.fill(HIST("h3dMassAntiLambda_MC_truePt"), collision.centV0M(), particleMotherOfNeg.pt(), v0.mAntiLambda());
                }
                if (saveDcaHist == 1) {
                  registry.fill(HIST("h3dMassLambdaDca"), v0.dcaV0daughters(), v0.pt(), v0.mLambda());
                  registry.fill(HIST("h3dMassAntiLambdaDca"), v0.dcaV0daughters(), v0.pt(), v0.mAntiLambda());

                  if (particleMotherOfNeg.isPhysicalPrimary() && particleMotherOfNeg == particleMotherOfPos && particleMotherOfNeg.pdgCode() == 3122) {
                    registry.fill(HIST("h3dMassLambdaDca_MC_truePt"), v0.dcaV0daughters(), particleMotherOfNeg.pt(), v0.mLambda());
                  }
                  if (particleMotherOfNeg.isPhysicalPrimary() && particleMotherOfNeg == particleMotherOfPos && particleMotherOfNeg.pdgCode() == -3122) {
                    registry.fill(HIST("h3dMassAntiLambdaDca_MC_truePt"), v0.dcaV0daughters(), particleMotherOfNeg.pt(), v0.mAntiLambda());
                  }
                }
              }
            }
          }
        }
        if (TMath::Abs(v0.yK0Short()) < rapidity) {
          registry.fill(HIST("V0loopFiltersCounts"), 6.5);
          if (v0.distovertotmom(collision.posX(), collision.posY(), collision.posZ()) * RecoDecay::getMassPDG(kK0Short) < lifetimecut->get("lifetimecutK0S")) {
            registry.fill(HIST("V0loopFiltersCounts"), 7.5);
            registry.fill(HIST("h3dMassK0Short"), collision.centV0M(), v0.pt(), v0.mK0Short());

            for (auto& particleMotherOfNeg : mcnegtrack.mothers_as<aod::McParticles>()) {
              for (auto& particleMotherOfPos : mcpostrack.mothers_as<aod::McParticles>()) {
                if (particleMotherOfNeg.isPhysicalPrimary() && particleMotherOfNeg == particleMotherOfPos && particleMotherOfNeg.pdgCode() == 310) {
                  registry.fill(HIST("h3dMassK0Short_MC_truePt"), collision.centV0M(), particleMotherOfNeg.pt(), v0.mK0Short());
                }
                if (saveDcaHist == 1) {
                  registry.fill(HIST("h3dMassK0ShortDca"), v0.dcaV0daughters(), v0.pt(), v0.mK0Short());
                  if (particleMotherOfNeg.isPhysicalPrimary() && particleMotherOfNeg == particleMotherOfPos && particleMotherOfNeg.pdgCode() == 310) {
                    registry.fill(HIST("h3dMassK0ShortDca_MC_truePt"), v0.dcaV0daughters(), particleMotherOfNeg.pt(), v0.mK0Short());
                  }
                }
              }
            }
          }
        }
      }
    }
  }
  PROCESS_SWITCH(lambdakzeroAnalysisMc, processRun2, "Process Run 2 data", false);
};

struct lambdakzeroParticleCountMc {
  //Basic checks
  HistogramRegistry registry{
    "registry",
    {
      {"hK0ShortCount", "hK0ShortCount", {HistType::kTH1F, {{2, 0.0f, 2.0f}}}},
      {"hLambdaCount", "hLambdaCount", {HistType::kTH1F, {{2, 0.0f, 2.0f}}}},
      {"hAntiLambdaCount", "hAntiLambdaCount", {HistType::kTH1F, {{2, 0.0f, 2.0f}}}},
      {"hK0ShortCount_PtDiff", "hK0ShortCount_PtDiff", {HistType::kTH1F, {{200, 0.0f, 10.0f}}}},
      {"hLambdaCount_PtDiff", "hLambdaCount_PtDiff", {HistType::kTH1F, {{200, 0.0f, 10.0f}}}},
      {"hAntiLambdaCount_PtDiff", "hAntiLambdaCount_PtDiff", {HistType::kTH1F, {{200, 0.0f, 10.0f}}}},

      {"hSelAndRecoMcCollCounter", "hSelAndRecoMcCollCounter", {HistType::kTH1F, {{1, 0.0f, 1.0f}}}},
      {"hTotalMcCollCounter", "hTotalMcCollCounter", {HistType::kTH1F, {{1, 0.0f, 1.0f}}}},

    },
  };

  void init(InitContext&)
  {
    registry.get<TH1>(HIST("hK0ShortCount"))->GetXaxis()->SetBinLabel(1, "primary K0S mothers");
    registry.get<TH1>(HIST("hK0ShortCount"))->GetXaxis()->SetBinLabel(2, "decaying into V0");
    registry.get<TH1>(HIST("hLambdaCount"))->GetXaxis()->SetBinLabel(1, "primary Lambda mothers");
    registry.get<TH1>(HIST("hLambdaCount"))->GetXaxis()->SetBinLabel(2, "decaying into V0");
    registry.get<TH1>(HIST("hAntiLambdaCount"))->GetXaxis()->SetBinLabel(1, "primary AntiLambda mothers");
    registry.get<TH1>(HIST("hAntiLambdaCount"))->GetXaxis()->SetBinLabel(2, "decaying into V0");
  }

  Configurable<float> rapidityMCcut{"rapidityMCcut", 0.5, "rapidity cut MC count"};
  Configurable<bool> eventSelectionMC{"eventSelectionMC", true, "event selection MC count"};

  void process(aod::McCollision const& mcCollision, aod::McParticles const& mcParticles, const soa::SmallGroups<o2::soa::Join<o2::aod::Collisions, o2::aod::McCollisionLabels, o2::aod::EvSels>>& collisions)
  {
    std::vector<int64_t> SelectedEvents(collisions.size());
    int nevts = 0;
    for (const auto& collision : collisions) {
      if (eventSelectionMC && !collision.sel8()) {
        continue;
      }
      SelectedEvents[nevts++] = collision.mcCollision_as<aod::McCollisions>().globalIndex();
    }
    SelectedEvents.resize(nevts);

    const auto evtReconstructedAndSelected = std::find(SelectedEvents.begin(), SelectedEvents.end(), mcCollision.globalIndex()) != SelectedEvents.end();

    registry.fill(HIST("hTotalMcCollCounter"), 0.5);
    if (!evtReconstructedAndSelected) { // Check that the event is reconstructed and that the reconstructed events pass the selection
      return;
    }
    registry.fill(HIST("hSelAndRecoMcCollCounter"), 0.5);

    for (auto& mcparticle : mcParticles) {
      if (TMath::Abs(mcparticle.y()) < rapidityMCcut) {
        if (mcparticle.isPhysicalPrimary()) {
          if (mcparticle.pdgCode() == 310) {
            registry.fill(HIST("hK0ShortCount"), 0.5);
            for (auto& mcparticleDaughter0 : mcparticle.daughters_as<aod::McParticles>()) {
              for (auto& mcparticleDaughter1 : mcparticle.daughters_as<aod::McParticles>()) {
                if (mcparticleDaughter0.pdgCode() == 211 && mcparticleDaughter1.pdgCode() == -211) {
                  registry.fill(HIST("hK0ShortCount"), 1.5);
                  registry.fill(HIST("hK0ShortCount_PtDiff"), mcparticle.pt());
                }
              }
            }
          }
          if (mcparticle.pdgCode() == 3122) {
            registry.fill(HIST("hLambdaCount"), 0.5);
            for (auto& mcparticleDaughter0 : mcparticle.daughters_as<aod::McParticles>()) {
              for (auto& mcparticleDaughter1 : mcparticle.daughters_as<aod::McParticles>()) {
                if (mcparticleDaughter0.pdgCode() == -211 && mcparticleDaughter1.pdgCode() == 2212) {
                  registry.fill(HIST("hLambdaCount"), 1.5);
                  registry.fill(HIST("hLambdaCount_PtDiff"), mcparticle.pt());
                }
              }
            }
          }
          if (mcparticle.pdgCode() == -3122) {
            registry.fill(HIST("hAntiLambdaCount"), 0.5);
            for (auto& mcparticleDaughter0 : mcparticle.daughters_as<aod::McParticles>()) {
              for (auto& mcparticleDaughter1 : mcparticle.daughters_as<aod::McParticles>()) {
                if (mcparticleDaughter0.pdgCode() == 211 && mcparticleDaughter1.pdgCode() == -2212) {
                  registry.fill(HIST("hAntiLambdaCount"), 1.5);
                  registry.fill(HIST("hAntiLambdaCount_PtDiff"), mcparticle.pt());
                }
              }
            }
          }
        }
      }
    }
  }
};


WorkflowSpec defineDataProcessing(ConfigContext const& cfgc)
{
  return WorkflowSpec{
    adaptAnalysisTask<lambdakzeroAnalysisMc>(cfgc),
    adaptAnalysisTask<lambdakzeroQa>(cfgc),
    adaptAnalysisTask<lambdakzeroParticleCountMc>(cfgc)};
}
