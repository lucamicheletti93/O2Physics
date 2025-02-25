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

#ifndef O2_ANALYSIS_DIFFRACTION_SELECTOR_H_
#define O2_ANALYSIS_DIFFRACTION_SELECTOR_H_

#include "Framework/DataTypes.h"
#include "Common/DataModel/EventSelection.h"
#include "Common/DataModel/TrackSelectionTables.h"
#include "Common/Core/PID/PIDResponse.h"

using namespace o2;
using namespace o2::framework;
using namespace o2::framework::expressions;

template <typename TC>
bool hasGoodPID(cutHolder diffCuts, TC track);

template <typename T>
T compatibleBCs(soa::Join<aod::Collisions, aod::EvSels>::iterator const& collision, int ndt, T const& bcs);

// add here Selectors for different types of diffractive events
// Selector for Double Gap events
struct DGSelector {
 public:
  DGSelector() = default;

  // Function to check if collisions passes filter
  template <typename CC, typename BC, typename BCs, typename TCs, typename FWs>
  int IsSelected(cutHolder diffCuts, CC const& collision, BC& bc, BCs& bcRange, TCs& tracks, FWs& fwdtracks)
  {
    LOGF(debug, "Collision %f BC %i", collision.collisionTime(), bc.globalBC());
    LOGF(debug, "Number of close BCs: %i", bcRange.size());

    // check that there are no FIT signals in any of the compatible BCs
    // Double Gap (DG) condition
    for (auto& bc : bcRange) {
      if (bc.has_ft0() || bc.has_fv0a() || bc.has_fdd()) {
        return 1;
      }
    }

    // Number of tracks
    // All tracks in vertex
    LOGF(debug, "Number of tracks: %i in vertex: %i", tracks.size(), collision.numContrib());
    if (collision.numContrib() != tracks.size()) {
      return 2;
    }
    if (tracks.size() < diffCuts.minNTracks() || tracks.size() > diffCuts.maxNTracks()) {
      return 3;
    }

    // all tracks must be global tracks
    // and some need to have a TOF hit
    // chaeck also net charge
    auto goodTracks = true;
    int nTracksWithTOFHit = 0;
    auto netCharge = 0;
    LOGF(debug, "Tracks");
    for (auto& track : tracks) {
      goodTracks &= (track.isGlobalTrack() > 0);

      // update netCharge
      netCharge += track.sign();

      // TOF hit with good match quality
      if (track.hasTOF() && track.tofChi2() < diffCuts.maxTOFChi2()) {
        nTracksWithTOFHit++;
      }
      LOGF(debug, "   global %i, TOF [%i] signal %f / chi2: %f", track.isGlobalTrack(), (track.hasTOF() ? 1 : 0), track.tofSignal(), track.tofChi2());
    }
    LOGF(debug, "  Tracks net charge %i, with TOF hit %i / %i", netCharge, nTracksWithTOFHit, diffCuts.minNTracksWithTOFHit());
    goodTracks &= (nTracksWithTOFHit >= diffCuts.minNTracksWithTOFHit());
    goodTracks &= (netCharge >= diffCuts.minNetCharge() && netCharge <= diffCuts.maxNetCharge());
    if (!goodTracks) {
      return 4;
    }

    // only tracks with good PID
    auto goodPID = true;
    for (auto& track : tracks) {
      goodPID &= hasGoodPID(diffCuts, track);
    }
    if (!goodPID) {
      return 5;
    }

    // check no activity in muon arm
    LOGF(debug, "Muons %i", fwdtracks.size());
    for (auto& muon : fwdtracks) {
      LOGF(debug, "  %i / %f / %f / %f", muon.trackType(), muon.eta(), muon.pt(), muon.p());
    }
    if (fwdtracks.size() > 0) {
      return 6;
    }

    // if we arrive here then the event is good!
    return 0;
  };
};

// The associations between collsisions and BCs can be ambiguous.
// By default a collision is associated with the BC closest in time.
// The collision time t_coll is determined by the tracks which are used to
// reconstruct the vertex. t_coll has an uncertainty dt_coll.
// Any BC with a BC time t_BC falling within a time window of +- ndt*dt_coll
// around t_coll could potentially be the true BC. ndt is typically 4.

template <typename T>
T compatibleBCs(soa::Join<aod::Collisions, aod::EvSels>::iterator const& collision, int ndt, T const& bcs)
{
  LOGF(debug, "Collision time / resolution [ns]: %f / %f", collision.collisionTime(), collision.collisionTimeRes());

  auto bcIter = collision.bc_as<T>();

  // due to the filling scheme the most probably BC may not be the one estimated from the collision time
  uint64_t mostProbableBC = bcIter.globalBC();
  uint64_t meanBC = mostProbableBC - std::lround(collision.collisionTime() / o2::constants::lhc::LHCBunchSpacingNS);
  int deltaBC = std::ceil(collision.collisionTimeRes() / o2::constants::lhc::LHCBunchSpacingNS * ndt);
  int64_t minBC = meanBC - deltaBC;
  uint64_t maxBC = meanBC + deltaBC;
  if (minBC < 0) {
    minBC = 0;
  }

  // find slice of BCs table with BC in [minBC, maxBC]
  int64_t maxBCId = bcIter.globalIndex();
  int moveCount = 0; // optimize to avoid to re-create the iterator
  while (bcIter != bcs.end() && bcIter.globalBC() <= maxBC && (int64_t)bcIter.globalBC() >= minBC) {
    LOGF(debug, "Table id %d BC %llu", bcIter.globalIndex(), bcIter.globalBC());
    maxBCId = bcIter.globalIndex();
    ++bcIter;
    ++moveCount;
  }

  bcIter.moveByIndex(-moveCount); // Move back to original position
  int64_t minBCId = collision.bcId();
  while (bcIter != bcs.begin() && bcIter.globalBC() <= maxBC && (int64_t)bcIter.globalBC() >= minBC) {
    LOGF(debug, "Table id %d BC %llu", bcIter.globalIndex(), bcIter.globalBC());
    minBCId = bcIter.globalIndex();
    --bcIter;
  }

  LOGF(debug, "  BC range: %i (%d) - %i (%d)", minBC, minBCId, maxBC, maxBCId);

  T slice{{bcs.asArrowTable()->Slice(minBCId, maxBCId - minBCId + 1)}, (uint64_t)minBCId};
  bcs.copyIndexBindings(slice);
  return slice;
}

// function to check if track provides good PID information
// Checks the nSigma for any particle assumption to be within limits.
template <typename TC>
bool hasGoodPID(cutHolder diffCuts, TC track)
{
  // El, Mu, Pi, Ka, and Pr are considered
  // at least one nSigma must be within set limits
  LOGF(debug, "TPC PID %f / %f / %f / %f / %f",
       track.tpcNSigmaEl(),
       track.tpcNSigmaMu(),
       track.tpcNSigmaPi(),
       track.tpcNSigmaKa(),
       track.tpcNSigmaPr());
  if (TMath::Abs(track.tpcNSigmaEl()) < diffCuts.maxnSigmaTPC()) {
    return true;
  }
  if (TMath::Abs(track.tpcNSigmaMu()) < diffCuts.maxnSigmaTPC()) {
    return true;
  }
  if (TMath::Abs(track.tpcNSigmaPi()) < diffCuts.maxnSigmaTPC()) {
    return true;
  }
  if (TMath::Abs(track.tpcNSigmaKa()) < diffCuts.maxnSigmaTPC()) {
    return true;
  }
  if (TMath::Abs(track.tpcNSigmaPr()) < diffCuts.maxnSigmaTPC()) {
    return true;
  }

  if (track.hasTOF()) {
    LOGF(debug, "TOF PID %f / %f / %f / %f / %f",
         track.tofNSigmaEl(),
         track.tofNSigmaMu(),
         track.tofNSigmaPi(),
         track.tofNSigmaKa(),
         track.tofNSigmaPr());
    if (TMath::Abs(track.tofNSigmaEl()) < diffCuts.maxnSigmaTOF()) {
      return true;
    }
    if (TMath::Abs(track.tofNSigmaMu()) < diffCuts.maxnSigmaTOF()) {
      return true;
    }
    if (TMath::Abs(track.tofNSigmaPi()) < diffCuts.maxnSigmaTOF()) {
      return true;
    }
    if (TMath::Abs(track.tofNSigmaKa()) < diffCuts.maxnSigmaTOF()) {
      return true;
    }
    if (TMath::Abs(track.tofNSigmaPr()) < diffCuts.maxnSigmaTOF()) {
      return true;
    }
  }
  return false;
}

#endif // O2_ANALYSIS_DIFFRACTION_SELECTOR_H_
